use crate::common::{is_windows_subsystem_for_linux, read_blocked};
use crate::env::{EnvStack, Environment};
use crate::fd_readable_set::FdReadableSet;
use crate::flog::FLOG;
use crate::reader::reader_current_data;
use crate::threads::{iothread_port, iothread_service_main};
use crate::universal_notifier::default_notifier;
use crate::wchar::prelude::*;
use crate::wutil::encoding::{mbrtowc, zero_mbstate};
use crate::wutil::fish_wcstol;
use std::collections::VecDeque;
use std::os::fd::RawFd;
use std::ptr;
use std::sync::atomic::{AtomicUsize, Ordering};

// The range of key codes for inputrc-style keyboard functions.
pub const R_END_INPUT_FUNCTIONS: usize = (ReadlineCmd::ReverseRepeatJump.repr as usize) + 1;

// TODO: move CharInputStyle and ReadlineCmd here once they no longer must be exposed to C++.
pub use crate::input_ffi::{CharInputStyle, ReadlineCmd};

/// Represents an event on the character input stream.
#[derive(Debug, Copy, Clone)]
pub enum CharEventType {
    /// A character was entered.
    Char(char),

    /// A readline event.
    Readline(ReadlineCmd),

    /// end-of-file was reached.
    Eof,

    /// An event was handled internally, or an interrupt was received. Check to see if the reader
    /// loop should exit.
    CheckExit,
}

#[derive(Debug, Clone)]
pub struct CharEvent {
    pub evt: CharEventType,

    // The style to use when inserting characters into the command line.
    //  todo!("This is only needed if the type is Readline")
    pub input_style: CharInputStyle,

    /// The sequence of characters in the input mapping which generated this event.
    /// Note that the generic self-insert case does not have any characters, so this would be empty.
    pub seq: WString,
}

impl CharEvent {
    pub fn is_char(&self) -> bool {
        matches!(self.evt, CharEventType::Char(_))
    }

    pub fn is_eof(&self) -> bool {
        matches!(self.evt, CharEventType::Eof)
    }

    pub fn is_check_exit(&self) -> bool {
        matches!(self.evt, CharEventType::CheckExit)
    }

    pub fn is_readline(&self) -> bool {
        matches!(self.evt, CharEventType::Readline(_))
    }

    pub fn get_char(&self) -> char {
        let CharEventType::Char(c) = self.evt else {
            panic!("Not a char type");
        };
        c
    }

    pub fn maybe_char(&self) -> Option<char> {
        if let CharEventType::Char(c) = self.evt {
            Some(c)
        } else {
            None
        }
    }

    pub fn get_readline(&self) -> ReadlineCmd {
        let CharEventType::Readline(c) = self.evt else {
            panic!("Not a readline type");
        };
        c
    }

    pub fn from_char(c: char) -> CharEvent {
        CharEvent {
            evt: CharEventType::Char(c),
            input_style: CharInputStyle::Normal,
            seq: WString::new(),
        }
    }

    pub fn from_readline(cmd: ReadlineCmd) -> CharEvent {
        Self::from_readline_seq(cmd, WString::new())
    }

    pub fn from_readline_seq(cmd: ReadlineCmd, seq: WString) -> CharEvent {
        CharEvent {
            evt: CharEventType::Readline(cmd),
            input_style: CharInputStyle::Normal,
            seq,
        }
    }

    pub fn from_check_exit() -> CharEvent {
        CharEvent {
            evt: CharEventType::CheckExit,
            input_style: CharInputStyle::Normal,
            seq: WString::new(),
        }
    }

    pub fn from_eof() -> CharEvent {
        CharEvent {
            evt: CharEventType::Eof,
            input_style: CharInputStyle::Normal,
            seq: WString::new(),
        }
    }
}

/// Time in milliseconds to wait for another byte to be available for reading
/// after \x1B is read before assuming that escape key was pressed, and not an
/// escape sequence.
const WAIT_ON_ESCAPE_DEFAULT: usize = 30;
static WAIT_ON_ESCAPE_MS: AtomicUsize = AtomicUsize::new(WAIT_ON_ESCAPE_DEFAULT);

const WAIT_ON_SEQUENCE_KEY_INFINITE: usize = usize::MAX;
static WAIT_ON_SEQUENCE_KEY_MS: AtomicUsize = AtomicUsize::new(WAIT_ON_SEQUENCE_KEY_INFINITE);

/// Internal function used by readch to read one byte.
/// This calls select() on three fds: input (e.g. stdin), the ioport notifier fd (for main thread
/// requests), and the uvar notifier. This returns either the byte which was read, or one of the
/// special values below.
enum ReadbResult {
    // A byte was successfully read.
    Byte(u8),

    // The in fd has been closed.
    Eof,

    // select() was interrupted by a signal.
    Interrupted,

    // Our uvar notifier reported a change (either through poll() or its fd).
    UvarNotified,

    // Our ioport reported a change, so service main thread requests.
    IOPortNotified,
}

fn readb(in_fd: RawFd) -> ReadbResult {
    assert!(in_fd >= 0, "Invalid in fd");
    let mut fdset = FdReadableSet::new();
    loop {
        fdset.clear();
        fdset.add(in_fd);

        // Add the completion ioport.
        let ioport_fd = iothread_port();
        fdset.add(ioport_fd);

        // Get the uvar notifier fd (possibly none).
        let notifier = default_notifier();
        let notifier_fd = notifier.notification_fd();
        if let Some(notifier_fd) = notifier.notification_fd() {
            fdset.add(notifier_fd);
        }

        // Here's where we call select().
        let select_res = fdset.check_readable(FdReadableSet::kNoTimeout);
        if select_res < 0 {
            let err = errno::errno().0;
            if err == libc::EINTR || err == libc::EAGAIN {
                // A signal.
                return ReadbResult::Interrupted;
            } else {
                // Some fd was invalid, so probably the tty has been closed.
                return ReadbResult::Eof;
            }
        }

        // select() did not return an error, so we may have a readable fd.
        // The priority order is: uvars, stdin, ioport.
        // Check to see if we want a universal variable barrier.
        if let Some(notifier_fd) = notifier_fd {
            if fdset.test(notifier_fd) && notifier.notification_fd_became_readable(notifier_fd) {
                return ReadbResult::UvarNotified;
            }
        }

        // Check stdin.
        if fdset.test(in_fd) {
            let mut arr: [u8; 1] = [0];
            if read_blocked(in_fd, &mut arr) != 1 {
                // The terminal has been closed.
                return ReadbResult::Eof;
            }
            // The common path is to return a u8.
            return ReadbResult::Byte(arr[0]);
        }

        // Check for iothread completions only if there is no data to be read from the stdin.
        // This gives priority to the foreground.
        if fdset.test(ioport_fd) {
            return ReadbResult::IOPortNotified;
        }
    }
}

// Update the wait_on_escape_ms value in response to the fish_escape_delay_ms user variable being
// set.
pub fn update_wait_on_escape_ms(vars: &EnvStack) {
    let fish_escape_delay_ms = vars.get_unless_empty(L!("fish_escape_delay_ms"));
    let Some(fish_escape_delay_ms) = fish_escape_delay_ms else {
        WAIT_ON_ESCAPE_MS.store(WAIT_ON_ESCAPE_DEFAULT, Ordering::Relaxed);
        return;
    };
    let fish_escape_delay_ms = fish_escape_delay_ms.as_string();
    match fish_wcstol(&fish_escape_delay_ms) {
        Ok(val) if (10..5000).contains(&val) => {
            WAIT_ON_ESCAPE_MS.store(val.try_into().unwrap(), Ordering::Relaxed);
        }
        _ => {
            eprintln!(
                concat!(
                    "ignoring fish_escape_delay_ms: value '{}' ",
                    "is not an integer or is < 10 or >= 5000 ms"
                ),
                fish_escape_delay_ms
            )
        }
    }
}

// Update the wait_on_sequence_key_ms value in response to the fish_sequence_key_delay_ms user
// variable being set.
pub fn update_wait_on_sequence_key_ms(vars: &EnvStack) {
    let sequence_key_time_ms = vars.get_unless_empty(L!("fish_sequence_key_delay_ms"));
    let Some(sequence_key_time_ms) = sequence_key_time_ms else {
        WAIT_ON_SEQUENCE_KEY_MS.store(WAIT_ON_SEQUENCE_KEY_INFINITE, Ordering::Relaxed);
        return;
    };
    let sequence_key_time_ms = sequence_key_time_ms.as_string();
    match fish_wcstol(&sequence_key_time_ms) {
        Ok(val) if (10..5000).contains(&val) => {
            WAIT_ON_SEQUENCE_KEY_MS.store(val.try_into().unwrap(), Ordering::Relaxed);
        }
        _ => {
            eprintln!(
                concat!(
                    "ignoring fish_sequence_key_delay_ms: value '{}' ",
                    "is not an integer or is < 10 or >= 5000 ms"
                ),
                sequence_key_time_ms
            )
        }
    }
}

/// A trait which knows how to produce a stream of input events.
/// Note this is conceptually a "base class" with override points.
pub trait InputEventQueuer {
    /// \return the next event in the queue, or none if the queue is empty.
    fn try_pop(&mut self) -> Option<CharEvent> {
        self.get_queue_mut().pop_front()
    }

    /// Function used by input_readch to read bytes from stdin until enough bytes have been read to
    /// convert them to a wchar_t. Conversion is done using mbrtowc. If a character has previously
    /// been read and then 'unread' using \c input_common_unreadch, that character is returned.
    fn readch(&mut self) -> CharEvent {
        let mut res: char = '\0';
        let mut state = zero_mbstate();
        loop {
            // Do we have something enqueued already?
            // Note this may be initially true, or it may become true through calls to
            // iothread_service_main() or env_universal_barrier() below.
            if let Some(mevt) = self.try_pop() {
                return mevt;
            }

            // We are going to block; but first allow any override to inject events.
            self.prepare_to_select();
            if let Some(mevt) = self.try_pop() {
                return mevt;
            }

            let rr = readb(self.get_in_fd());
            match rr {
                ReadbResult::Eof => {
                    return CharEvent::from_eof();
                }

                ReadbResult::Interrupted => {
                    // FIXME: here signals may break multibyte sequences.
                    self.select_interrupted();
                }

                ReadbResult::UvarNotified => {
                    self.uvar_change_notified();
                }

                ReadbResult::IOPortNotified => {
                    iothread_service_main(reader_current_data().unwrap());
                }

                ReadbResult::Byte(read_byte) => {
                    if crate::compat::MB_CUR_MAX() == 1 {
                        // single-byte locale, all values are legal
                        // FIXME: this looks wrong, this falsely assumes that
                        // the single-byte locale is compatible with Unicode upper-ASCII.
                        res = read_byte.into();
                        return CharEvent::from_char(res);
                    }
                    let sz = unsafe {
                        mbrtowc(
                            std::ptr::addr_of_mut!(res).cast(),
                            std::ptr::addr_of!(read_byte).cast(),
                            1,
                            &mut state,
                        )
                    } as isize;
                    match sz {
                        -1 => {
                            FLOG!(reader, "Illegal input");
                            return CharEvent::from_check_exit();
                        }
                        -2 => {
                            // Sequence not yet complete.
                        }
                        0 => {
                            // Actual nul char.
                            return CharEvent::from_char('\0');
                        }
                        _ => {
                            // Sequence complete.
                            return CharEvent::from_char(res);
                        }
                    }
                }
            }
        }
    }

    fn readch_timed_esc(&mut self) -> Option<CharEvent> {
        self.readch_timed(WAIT_ON_ESCAPE_MS.load(Ordering::Relaxed))
    }

    fn readch_timed_sequence_key(&mut self) -> Option<CharEvent> {
        let wait_on_sequence_key_ms = WAIT_ON_SEQUENCE_KEY_MS.load(Ordering::Relaxed);
        if wait_on_sequence_key_ms == WAIT_ON_SEQUENCE_KEY_INFINITE {
            return Some(self.readch());
        }
        self.readch_timed(wait_on_sequence_key_ms)
    }

    /// Like readch(), except it will wait at most wait_time_ms milliseconds for a
    /// character to be available for reading.
    /// \return None on timeout, the event on success.
    fn readch_timed(&mut self, wait_time_ms: usize) -> Option<CharEvent> {
        if let Some(evt) = self.try_pop() {
            return Some(evt);
        }

        // We are not prepared to handle a signal immediately; we only want to know if we get input on
        // our fd before the timeout. Use pselect to block all signals; we will handle signals
        // before the next call to readch().
        let mut sigs: libc::sigset_t = unsafe { std::mem::zeroed() };
        unsafe { libc::sigfillset(&mut sigs) };

        // pselect expects timeouts in nanoseconds.
        const NSEC_PER_MSEC: u64 = 1000 * 1000;
        const NSEC_PER_SEC: u64 = NSEC_PER_MSEC * 1000;
        let wait_nsec: u64 = (wait_time_ms as u64) * NSEC_PER_MSEC;
        let timeout = libc::timespec {
            tv_sec: (wait_nsec / NSEC_PER_SEC).try_into().unwrap(),
            tv_nsec: (wait_nsec % NSEC_PER_SEC).try_into().unwrap(),
        };

        // We have one fd of interest.
        let mut fdset: libc::fd_set = unsafe { std::mem::zeroed() };
        let in_fd = self.get_in_fd();
        unsafe {
            libc::FD_ZERO(&mut fdset);
            libc::FD_SET(in_fd, &mut fdset);
        };
        let res = unsafe {
            libc::pselect(
                in_fd + 1,
                &mut fdset,
                ptr::null_mut(),
                ptr::null_mut(),
                &timeout,
                &sigs,
            )
        };

        // Prevent signal starvation on WSL causing the `torn_escapes.py` test to fail
        if is_windows_subsystem_for_linux() {
            // Merely querying the current thread's sigmask is sufficient to deliver a pending signal
            let _ = unsafe { libc::pthread_sigmask(0, ptr::null(), &mut sigs) };
        }
        if res > 0 {
            return Some(self.readch());
        }
        None
    }

    /// Return our queue. These are "abstract" methods to be implemented by concrete types.
    fn get_queue(&self) -> &VecDeque<CharEvent>;
    fn get_queue_mut(&mut self) -> &mut VecDeque<CharEvent>;

    /// Return the fd corresponding to stdin.
    fn get_in_fd(&self) -> RawFd;

    /// Enqueue a character or a readline function to the queue of unread characters that
    /// readch will return before actually reading from fd 0.
    fn push_back(&mut self, ch: CharEvent) {
        self.get_queue_mut().push_back(ch);
    }

    /// Add a character or a readline function to the front of the queue of unread characters.  This
    /// will be the next character returned by readch.
    fn push_front(&mut self, ch: CharEvent) {
        self.get_queue_mut().push_front(ch);
    }

    /// Find the first sequence of non-char events, and promote them to the front.
    fn promote_interruptions_to_front(&mut self) {
        // Find the first sequence of non-char events.
        // EOF is considered a char: we don't want to pull EOF in front of real chars.
        let queue = self.get_queue_mut();
        let is_char = |evt: &CharEvent| evt.is_char() || evt.is_eof();
        // Find the index of the first non-char event.
        // If there's none, we're done.
        let Some(first): Option<usize> = queue.iter().position(|e| !is_char(e)) else {
            return;
        };
        let last = queue
            .range(first..)
            .position(is_char)
            .map_or(queue.len(), |x| x + first);
        // Move the non-char events to the front, retaining their order.
        let elems: Vec<CharEvent> = queue.drain(first..last).collect();
        for elem in elems.into_iter().rev() {
            queue.push_front(elem);
        }
    }

    /// Add multiple readline events to the front of the queue of unread characters.
    /// The order of the provided events is not changed, i.e. they are not inserted in reverse
    /// order. That is, the first element in evts will be the first element returned.
    fn insert_front<I>(&mut self, evts: I)
    where
        I: IntoIterator<Item = CharEvent>,
        I::IntoIter: DoubleEndedIterator,
    {
        let queue = self.get_queue_mut();
        let iter = evts.into_iter().rev();
        queue.reserve(iter.size_hint().0);
        for evt in iter {
            queue.push_front(evt);
        }
    }

    /// Forget all enqueued readline events in the front of the queue.
    fn drop_leading_readline_events(&mut self) {
        let queue = self.get_queue_mut();
        while let Some(evt) = queue.front() {
            if evt.is_readline() {
                queue.pop_front();
            } else {
                break;
            }
        }
    }

    /// Override point for when we are about to (potentially) block in select(). The default does
    /// nothing.
    fn prepare_to_select(&mut self) {}

    /// Override point for when when select() is interrupted by a signal. The default does nothing.
    fn select_interrupted(&mut self) {}

    /// Override point for when when select() is interrupted by the universal variable notifier.
    /// The default does nothing.
    fn uvar_change_notified(&mut self) {}

    /// \return if we have any lookahead.
    fn has_lookahead(&self) -> bool {
        !self.get_queue().is_empty()
    }
}

/// A simple, concrete implementation of InputEventQueuer.
pub struct InputEventQueue {
    queue: VecDeque<CharEvent>,
    in_fd: RawFd,
}

impl InputEventQueue {
    pub fn new(in_fd: RawFd) -> InputEventQueue {
        InputEventQueue {
            queue: VecDeque::new(),
            in_fd,
        }
    }
}

impl InputEventQueuer for InputEventQueue {
    fn get_queue(&self) -> &VecDeque<CharEvent> {
        &self.queue
    }

    fn get_queue_mut(&mut self) -> &mut VecDeque<CharEvent> {
        &mut self.queue
    }

    fn get_in_fd(&self) -> RawFd {
        self.in_fd
    }
}

#[test]
fn test_push_front_back() {
    let mut queue = InputEventQueue::new(0);
    queue.push_front(CharEvent::from_char('a'));
    queue.push_front(CharEvent::from_char('b'));
    queue.push_back(CharEvent::from_char('c'));
    queue.push_back(CharEvent::from_char('d'));
    assert_eq!(queue.try_pop().unwrap().get_char(), 'b');
    assert_eq!(queue.try_pop().unwrap().get_char(), 'a');
    assert_eq!(queue.try_pop().unwrap().get_char(), 'c');
    assert_eq!(queue.try_pop().unwrap().get_char(), 'd');
    assert!(queue.try_pop().is_none());
}

#[test]
fn test_promote_interruptions_to_front() {
    let mut queue = InputEventQueue::new(0);
    queue.push_back(CharEvent::from_char('a'));
    queue.push_back(CharEvent::from_char('b'));
    queue.push_back(CharEvent::from_readline(ReadlineCmd::Undo));
    queue.push_back(CharEvent::from_readline(ReadlineCmd::Redo));
    queue.push_back(CharEvent::from_char('c'));
    queue.push_back(CharEvent::from_char('d'));
    queue.promote_interruptions_to_front();

    assert_eq!(queue.try_pop().unwrap().get_readline(), ReadlineCmd::Undo);
    assert_eq!(queue.try_pop().unwrap().get_readline(), ReadlineCmd::Redo);
    assert_eq!(queue.try_pop().unwrap().get_char(), 'a');
    assert_eq!(queue.try_pop().unwrap().get_char(), 'b');
    assert_eq!(queue.try_pop().unwrap().get_char(), 'c');
    assert_eq!(queue.try_pop().unwrap().get_char(), 'd');
    assert!(!queue.has_lookahead());

    queue.push_back(CharEvent::from_char('e'));
    queue.promote_interruptions_to_front();
    assert_eq!(queue.try_pop().unwrap().get_char(), 'e');
    assert!(!queue.has_lookahead());
}

#[test]
fn test_insert_front() {
    let mut queue = InputEventQueue::new(0);
    queue.push_back(CharEvent::from_char('a'));
    queue.push_back(CharEvent::from_char('b'));

    let events = vec![
        CharEvent::from_char('A'),
        CharEvent::from_char('B'),
        CharEvent::from_char('C'),
    ];
    queue.insert_front(events);
    assert_eq!(queue.try_pop().unwrap().get_char(), 'A');
    assert_eq!(queue.try_pop().unwrap().get_char(), 'B');
    assert_eq!(queue.try_pop().unwrap().get_char(), 'C');
    assert_eq!(queue.try_pop().unwrap().get_char(), 'a');
    assert_eq!(queue.try_pop().unwrap().get_char(), 'b');
}
