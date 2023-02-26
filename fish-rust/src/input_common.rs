#![allow(unused_imports)]
use crate::wchar_ffi::{WCharFromFFI, WCharToFFI};
use crate::{
    env::flags,
    fd_readable_set::{self, new_fd_readable_set},
    fds::kNoTimeout,
    ffi::{self, moveit, Repin},
    flog::{categories::iothread, FLOG},
    wchar::{wchar_t, WString, L},
    wutil::{fish_wcstoi, format::printf, wgettext, wgettext_fmt},
};
use autocxx::WithinUniquePtr;
use cxx::{CxxWString, UniquePtr};
use errno::errno;
use libc::{sigset_t, EAGAIN, EINTR};
use std::pin::Pin;
use std::{
    collections::VecDeque,
    ffi::c_int,
    fmt::Debug,
    io::{self, Read},
    mem,
    os::fd::RawFd,
    ptr::{null, null_mut},
};
use widestring::WideChar;

#[cxx::bridge]
mod input_common_ffi {
    extern "Rust" {
        fn update_wait_on_escape_ms_ffi(escape_time_ms: &CxxWString);
    }
}

/// Time in milliseconds to wait for another byte to be available for reading
/// after \x1B is read before assuming that escape key was pressed, and not an
/// escape sequence.
const WAIT_ON_ESCAPE_DEFAULT: i64 = 30;
static mut wait_on_escape_ms: i64 = WAIT_ON_ESCAPE_DEFAULT;

/// The range of key codes for inputrc-style keyboard functions.
const R_END_INPUT_FUNCTIONS: u16 = ReadlineCmd::reverse_repeat_jump as u16 + 1;

/// Internal function used by readch to read one byte.
/// This calls select() on three fds: input (e.g. stdin), the ioport notifier fd (for main thread
/// requests), and the uvar notifier. This returns either the byte which was read, or one of the
/// special values below.
mod readb_result {
    // The in fd has been closed.
    pub const readb_eof: i32 = -1;

    // select() was interrupted by a signal.
    pub const readb_interrupted: i32 = -2;

    // Our uvar notifier reported a change (either through poll() or its fd).
    pub const readb_uvar_notified: i32 = -3;

    // Our ioport reported a change, so service main thread requests.
    pub const readb_ioport_notified: i32 = -4;
}

fn readb(in_fd: RawFd) -> i32 {
    assert!(in_fd >= 0, "Invalid in fd");
    let notifier = unsafe { &mut *ffi::default_notifier_ffi() };
    let mut fdset = new_fd_readable_set();
    loop {
        fdset.clear();
        fdset.add(in_fd);

        // Add the completion ioport.
        let ioport_fd = ffi::iothread_port().into();
        fdset.add(ioport_fd);

        // Get the uvar notifier fd (possibly none).
        let notifier_fd = notifier.notification_fd().into();
        fdset.add(notifier_fd);

        // Get its suggested delay (possibly none).
        // Note a 0 here means do not poll.
        let mut timeout = kNoTimeout;
        let usecs_delay = notifier.usec_delay_between_polls().into();
        if usecs_delay > 0 {
            timeout = usecs_delay;
        }

        // Here's where we call select().
        let select_res = fdset.check_readable(timeout);
        if select_res < 0 {
            let err: i32 = errno().into();
            if err == EINTR || err == EAGAIN {
                // A signal.
                return readb_result::readb_interrupted;
            } else {
                // Some fd was invalid, so probably the tty has been closed.
                return readb_result::readb_eof;
            }
        }

        // select() did not return an error, so we may have a readable fd.
        // The priority order is: uvars, stdin, ioport.
        // Check to see if we want a universal variable barrier.
        // This may come about through readability, or through a call to poll().
        if (fdset.test(notifier_fd)
            && notifier
                .pin()
                .notification_fd_became_readable(notifier_fd.into()))
            || notifier.pin().poll()
        {
            return readb_result::readb_uvar_notified;
        }

        // Check stdin.
        if fdset.test(in_fd) {
            let mut arr = [0 as u8];
            if ffi::read_blocked(in_fd.into(), arr.as_mut_ptr() as *mut autocxx::c_void, 1)
                != 1.into()
            {
                // The terminal has been closed.
                return readb_result::readb_eof;
            }
            // The common path is to return a (non-negative) char.
            return arr[0] as i32;
        }

        // Check for iothread completions only if there is no data to be read from the stdin.
        // This gives priority to the foreground.
        if fdset.test(ioport_fd) {
            return readb_result::readb_ioport_notified;
        }
    }
}

#[repr(u16)]
#[derive(Debug, PartialEq, Eq, Clone, Copy)]
enum ReadlineCmd {
    beginning_of_line,
    end_of_line,
    forward_char,
    backward_char,
    forward_single_char,
    forward_word,
    backward_word,
    forward_bigword,
    backward_bigword,
    nextd_or_forward_word,
    prevd_or_backward_word,
    history_search_backward,
    history_search_forward,
    history_prefix_search_backward,
    history_prefix_search_forward,
    history_pager,
    delete_char,
    backward_delete_char,
    kill_line,
    yank,
    yank_pop,
    complete,
    complete_and_search,
    pager_toggle_search,
    beginning_of_history,
    end_of_history,
    backward_kill_line,
    kill_whole_line,
    kill_inner_line,
    kill_word,
    kill_bigword,
    backward_kill_word,
    backward_kill_path_component,
    backward_kill_bigword,
    history_token_search_backward,
    history_token_search_forward,
    self_insert,
    self_insert_notfirst,
    transpose_chars,
    transpose_words,
    upcase_word,
    downcase_word,
    capitalize_word,
    togglecase_char,
    togglecase_selection,
    execute,
    beginning_of_buffer,
    end_of_buffer,
    repaint_mode,
    repaint,
    force_repaint,
    up_line,
    down_line,
    suppress_autosuggestion,
    accept_autosuggestion,
    begin_selection,
    swap_selection_start_stop,
    end_selection,
    kill_selection,
    insert_line_under,
    insert_line_over,
    forward_jump,
    backward_jump,
    forward_jump_till,
    backward_jump_till,
    func_and,
    func_or,
    expand_abbr,
    delete_or_exit,
    exit,
    cancel_commandline,
    cancel,
    undo,
    redo,
    begin_undo_group,
    end_undo_group,
    repeat_jump,
    disable_mouse_tracking,
    // NOTE: This one has to be last.
    reverse_repeat_jump,
}

/// Represents an event on the character input stream.
#[derive(Debug, PartialEq, Eq, Clone, Copy)]
enum CharEventType {
    /// A character was entered.
    charc(wchar_t),

    /// A readline event.
    readline(ReadlineCmd),

    /// end-of-file was reached.
    eof,

    /// An event was handled internally, or an interrupt was received. Check to see if the reader
    /// loop should exit.
    check_exit,
}

/// Hackish: the input style, which describes how char events (only) are applied to the command
/// line. Note this is set only after applying bindings; it is not set from readb().
#[repr(u8)]
#[derive(Debug, Clone, Copy)]
enum CharInputStyle {
    // Insert characters normally.
    normal,

    // Insert characters only if the cursor is not at the beginning. Otherwise, discard them.
    notfirst,
}

#[derive(Debug, Clone)]
struct CharEvent {
    /// The type of event.
    event_type: CharEventType,

    /// The style to use when inserting characters into the command line.
    input_style: CharInputStyle,

    /// The sequence of characters in the input mapping which generated this event.
    /// Note that the generic self-insert case does not have any characters, so this would be empty.
    seq: WString,
}

impl CharEvent {
    fn is_char(&self) -> bool {
        matches!(self.event_type, CharEventType::charc(_))
    }

    fn is_eof(&self) -> bool {
        self.event_type == CharEventType::eof
    }

    fn is_check_exit(&self) -> bool {
        self.event_type == CharEventType::check_exit
    }

    fn is_readline(&self) -> bool {
        matches!(self.event_type, CharEventType::readline(_))
    }

    fn get_char(&self) -> wchar_t {
        match self.event_type {
            CharEventType::charc(c) => c,
            _ => panic!("Not a char type"),
        }
    }

    fn maybe_char(&self) -> Option<wchar_t> {
        match self.event_type {
            CharEventType::charc(c) => Some(c),
            _ => None,
        }
    }

    fn get_readline(&self) -> ReadlineCmd {
        match self.event_type {
            CharEventType::readline(c) => c,
            _ => panic!("Not a readline type"),
        }
    }

    fn new(event_type: CharEventType) -> Self {
        Self {
            event_type,
            input_style: CharInputStyle::normal,
            seq: Default::default(),
        }
    }
    fn new_with_seq(event_type: CharEventType, seq: WString) -> Self {
        Self {
            event_type,
            input_style: CharInputStyle::normal,
            seq,
        }
    }
}

/// Adjust the escape timeout.
/// Update the wait_on_escape_ms value in response to the fish_escape_delay_ms
/// user variable being set.
fn update_wait_on_escape_ms_ffi(escape_time_ms: &CxxWString) {
    let escape_time_ms = escape_time_ms.from_ffi();

    if escape_time_ms.is_empty() {
        unsafe {
            wait_on_escape_ms = WAIT_ON_ESCAPE_DEFAULT;
        }
        return;
    }

    let tmp = fish_wcstoi(escape_time_ms.chars());
    match tmp {
        Ok(n) if n >= 10 || n < 5000 => unsafe { wait_on_escape_ms = n },
        _ => {
            eprint!("{}", wgettext_fmt!("ignoring fish_escape_delay_ms: value '%ls' is not an integer or is < 10 or >= 5000 ms\n", escape_time_ms))
        }
    }
}

/// A class which knows how to produce a stream of input events.
/// This is a base class; you may subclass it for its override points.
struct InputEventQueue {
    input: RawFd,
    queue: VecDeque<CharEvent>,
    hooks: Box<dyn InputEventQueueHooks + 'static>,
}

impl InputEventQueue {
    /// Construct from a file descriptor \p in, and an interrupt handler \p handler.
    pub fn new_stdin() -> Self {
        Self {
            input: 0,
            queue: Default::default(),
            hooks: DefaultInputEventQueueHooks::new(),
        }
    }

    /// Function used by input_readch to read bytes from stdin until enough bytes have been read to
    /// convert them to a wchar_t. Conversion is done using mbrtowc. If a character has previously
    /// been read and then 'unread' using \c input_common_unreadch, that character is returned.
    pub fn readch(&mut self) -> CharEvent {
        moveit! {
            let mut state = ffi::read_mbtowc_t_create();
        }

        loop {
            // Do we have something enqueued already?
            // Note this may be initially true, or it may become true through calls to
            // iothread_service_main() or env_universal_barrier() below.
            if let Some(mevt) = self.try_pop() {
                return mevt;
            }

            // We are going to block; but first allow any override to inject events.
            self.hooks.prepare_to_select();
            if let Some(mevt) = self.try_pop() {
                return mevt;
            }

            let rr = readb(self.input);
            match rr {
                readb_result::readb_eof => return CharEvent::new(CharEventType::eof),

                // FIXME: here signals may break multibyte sequences.
                readb_result::readb_interrupted => self.hooks.select_interrupted(),

                readb_result::readb_uvar_notified => {
                    self.hooks.uvar_change_notified();
                }

                readb_result::readb_ioport_notified => ffi::iothread_service_main(),

                _ => {
                    let read_byte: u8 = rr
                        .try_into()
                        .expect("Read byte out of bounds - missing error case?");

                    if ffi::is_single_byte_locale() {
                        // single-byte locale, all values are legal
                        return CharEvent::new(CharEventType::charc(read_byte as wchar_t));
                    }

                    let sz = state.as_mut().read_mbtowc(read_byte as i8);
                    match sz.0 {
                        -1 => {
                            FLOG!(reader, "Illegal input");
                            return CharEvent::new(CharEventType::check_exit);
                        }
                        // Sequence not yet complete.
                        -2 => {}
                        // Actual nul char.
                        0 => return CharEvent::new(CharEventType::charc(0)),

                        // Sequence complete.
                        _ => return CharEvent::new(CharEventType::charc(state.get_char())),
                    }
                }
            }
        }
    }

    /// Like readch(), except it will wait at most WAIT_ON_ESCAPE milliseconds for a
    /// character to be available for reading.
    /// \return none on timeout, the event on success.
    fn readch_timed(&mut self) -> Option<CharEvent> {
        if let Some(mevt) = self.try_pop() {
            return Some(mevt);
        }
        // We are not prepared to handle a signal immediately; we only want to know if we get input on
        // our fd before the timeout. Use pselect to block all signals; we will handle signals
        // before the next call to readch().
        let mut sigs = sigset_t::default();
        unsafe { libc::sigfillset(&mut sigs) };

        // pselect expects timeouts in nanoseconds.
        let nsec_per_msec: i64 = 1000 * 1000;
        let nsec_per_sec = nsec_per_msec * 1000;
        let wait_nsec = unsafe { wait_on_escape_ms } * nsec_per_msec;
        let timeout = libc::timespec {
            tv_sec: (wait_nsec) / nsec_per_sec,
            tv_nsec: (wait_nsec) % nsec_per_sec,
        };
        unsafe {
            let mut fdset: libc::fd_set = std::mem::zeroed();
            // We have one fd of interest.

            libc::FD_ZERO(&mut fdset);
            libc::FD_SET(self.input, &mut fdset);

            let res = libc::pselect(
                self.input + 1,
                &mut fdset,
                null_mut(),
                null_mut(),
                &timeout,
                &sigs,
            );

            // Prevent signal starvation on WSL causing the `torn_escapes.py` test to fail
            if ffi::is_windows_subsystem_for_linux() {
                // Merely querying the current thread's sigmask is sufficient to deliver a pending signal
                libc::pthread_sigmask(0, null(), &mut sigs);
            }

            if res > 0 {
                return Some(self.readch());
            }
            return None;
        }
    }

    /// Enqueue a character or a readline function to the queue of unread characters that
    /// readch will return before actually reading from fd 0.
    fn push_back(&mut self, ch: CharEvent) {
        self.queue.push_back(ch);
    }

    /// Add a character or a readline function to the front of the queue of unread characters.  This
    /// will be the next character returned by readch.
    fn push_front(&mut self, ch: CharEvent) {
        self.queue.push_front(ch);
    }

    /// Find the first sequence of non-char events, and promote them to the front.
    fn promote_interruptions_to_front(&mut self) {
        // Find the first sequence of non-char events.
        // EOF is considered a char: we don't want to pull EOF in front of real chars.
        let is_char = |ch: &CharEvent| ch.is_char() || ch.is_eof();

        let first = self.queue.iter().position(|ch| !is_char(ch));
        let first = match first {
            Some(i) if i > 0 => i,
            _ => return,
        };

        let last = self
            .queue
            .iter()
            .skip(first)
            .position(is_char)
            .map(|i| i + first)
            .unwrap_or(self.queue.len());

        println!("{}, {}", first, last);
        for _ in first..last {
            if let Some(e) = self.queue.remove(last - 1) {
                self.queue.push_front(e);
            }
        }
    }

    /// Add multiple characters or readline events to the front of the queue of unread characters.
    /// The order of the provided events is not changed, i.e. they are not inserted in reverse
    /// order.
    fn insert_front(&mut self, i: impl Iterator<Item = CharEvent>) {
        for c in i {
            self.queue.push_front(c);
        }
    }

    /// \return the next event in the queue, or none if the queue is empty.
    fn try_pop(&mut self) -> Option<CharEvent> {
        self.queue.pop_front()
    }
}

pub trait InputEventQueueHooks {
    /// Override point for when we are about to (potentially) block in select().
    /// The default does nothing.
    fn prepare_to_select(&self) {}

    /// Override point for when when select() is interrupted by a signal. The default does nothing.
    fn select_interrupted(&self) {}

    /// Override point for when when select() is interrupted by the universal variable notifier.
    /// The default does nothing.
    fn uvar_change_notified(&self) {}
}

struct DefaultInputEventQueueHooks;
impl DefaultInputEventQueueHooks {
    fn new() -> Box<dyn InputEventQueueHooks> {
        Box::new(DefaultInputEventQueueHooks)
    }
}

impl InputEventQueueHooks for DefaultInputEventQueueHooks {}
