use libc::STDOUT_FILENO;

use crate::common::{
    fish_reserved_codepoint, is_windows_subsystem_for_linux, read_blocked, shell_modes,
    str2wcstring, write_loop, ScopeGuard, WSL,
};
use crate::env::{EnvStack, Environment};
use crate::fd_readable_set::{FdReadableSet, Timeout};
use crate::flog::{FloggableDebug, FLOG};
use crate::fork_exec::flog_safe::FLOG_SAFE;
use crate::global_safety::RelaxedAtomicBool;
use crate::key::{
    self, alt, canonicalize_control_char, canonicalize_keyed_control_char, char_to_symbol, ctrl,
    function_key, shift, Key, Modifiers, ViewportPosition,
};
use crate::reader::{reader_current_data, reader_test_and_clear_interrupted};
use crate::threads::{iothread_port, is_main_thread};
use crate::universal_notifier::default_notifier;
use crate::wchar::{encode_byte_to_char, prelude::*};
use crate::wutil::encoding::{mbrtowc, mbstate_t, zero_mbstate};
use crate::wutil::fish_wcstol;
use std::collections::VecDeque;
use std::ops::ControlFlow;
use std::os::fd::RawFd;
use std::os::unix::ffi::OsStrExt;
use std::ptr;
use std::sync::atomic::{AtomicBool, AtomicU8, AtomicUsize, Ordering};
use std::sync::{Mutex, MutexGuard};
use std::time::Duration;

// The range of key codes for inputrc-style keyboard functions.
pub const R_END_INPUT_FUNCTIONS: usize = (ReadlineCmd::ReverseRepeatJump as usize) + 1;

/// Hackish: the input style, which describes how char events (only) are applied to the command
/// line. Note this is set only after applying bindings; it is not set from readb().
#[derive(Copy, Clone, Debug, Eq, PartialEq)]
pub enum CharInputStyle {
    // Insert characters normally.
    Normal,

    // Insert characters only if the cursor is not at the beginning. Otherwise, discard them.
    NotFirst,
}

#[derive(Debug, Copy, Clone, PartialEq, Eq)]
#[repr(u8)]
pub enum ReadlineCmd {
    BeginningOfLine,
    EndOfLine,
    ForwardChar,
    BackwardChar,
    BackwardCharPassive,
    ForwardSingleChar,
    ForwardCharPassive,
    ForwardWord,
    BackwardWord,
    ForwardBigword,
    BackwardBigword,
    ForwardToken,
    BackwardToken,
    NextdOrForwardWord,
    PrevdOrBackwardWord,
    HistoryDelete,
    HistorySearchBackward,
    HistorySearchForward,
    HistoryPrefixSearchBackward,
    HistoryPrefixSearchForward,
    HistoryPager,
    #[deprecated]
    HistoryPagerDelete,
    DeleteChar,
    BackwardDeleteChar,
    KillLine,
    Yank,
    YankPop,
    Complete,
    CompleteAndSearch,
    PagerToggleSearch,
    BeginningOfHistory,
    EndOfHistory,
    BackwardKillLine,
    KillWholeLine,
    KillInnerLine,
    KillWord,
    KillBigword,
    KillToken,
    BackwardKillWord,
    BackwardKillPathComponent,
    BackwardKillBigword,
    BackwardKillToken,
    HistoryTokenSearchBackward,
    HistoryTokenSearchForward,
    HistoryLastTokenSearchBackward,
    HistoryLastTokenSearchForward,
    SelfInsert,
    SelfInsertNotFirst,
    TransposeChars,
    TransposeWords,
    UpcaseWord,
    DowncaseWord,
    CapitalizeWord,
    TogglecaseChar,
    TogglecaseSelection,
    Execute,
    BeginningOfBuffer,
    EndOfBuffer,
    RepaintMode,
    Repaint,
    ForceRepaint,
    UpLine,
    DownLine,
    SuppressAutosuggestion,
    AcceptAutosuggestion,
    BeginSelection,
    SwapSelectionStartStop,
    EndSelection,
    KillSelection,
    InsertLineUnder,
    InsertLineOver,
    ForwardJump,
    BackwardJump,
    ForwardJumpTill,
    BackwardJumpTill,
    JumpToMatchingBracket,
    JumpTillMatchingBracket,
    FuncAnd,
    FuncOr,
    ExpandAbbr,
    DeleteOrExit,
    Exit,
    ClearCommandline,
    CancelCommandline,
    Cancel,
    Undo,
    Redo,
    BeginUndoGroup,
    EndUndoGroup,
    RepeatJump,
    ClearScreenAndRepaint,
    ScrollbackPush,
    // NOTE: This one has to be last.
    ReverseRepeatJump,
}

/// Represents an event on the character input stream.
#[derive(Debug, Clone)]
pub enum CharEventType {
    /// A character was entered.
    Char(Key),

    /// A readline event.
    Readline(ReadlineCmd),

    /// A shell command.
    Command(WString),

    /// end-of-file was reached.
    Eof,

    /// An event was handled internally, or an interrupt was received. Check to see if the reader
    /// loop should exit.
    CheckExit,
}

#[derive(Debug, Clone)]
pub struct ReadlineCmdEvent {
    pub cmd: ReadlineCmd,
    /// The sequence of characters in the input mapping which generated this event.
    /// Note that the generic self-insert case does not have any characters, so this would be empty.
    /// This is also empty for invalid Unicode code points, which produce multiple characters.
    pub seq: WString,
}

#[derive(Debug, Clone)]
pub struct KeyEvent {
    // The key.
    pub key: Key,
    // The style to use when inserting characters into the command line.
    pub input_style: CharInputStyle,
    /// The sequence of characters in the input mapping which generated this event.
    /// Note that the generic self-insert case does not have any characters, so this would be empty.
    /// This is also empty for invalid Unicode code points, which produce multiple characters.
    pub seq: WString,
}

#[derive(Debug, Clone)]
pub enum ImplicitEvent {
    /// end-of-file was reached.
    Eof,
    /// An event was handled internally, or an interrupt was received. Check to see if the reader
    /// loop should exit.
    CheckExit,
    /// Our terminal window gained focus.
    FocusIn,
    /// Our terminal window lost focus.
    FocusOut,
    /// Request to disable mouse tracking.
    DisableMouseTracking,
    /// Primary DA response.
    PrimaryDeviceAttribute,
    /// Handle mouse left click.
    MouseLeftClickContinuation(ViewportPosition, ViewportPosition),
    /// Push prompt to top.
    ScrollbackPushContinuation(usize),
}

#[derive(Debug, Clone)]
pub enum CharEvent {
    /// A character was entered.
    Key(KeyEvent),

    /// A readline event.
    Readline(ReadlineCmdEvent),

    /// A shell command.
    Command(WString),

    /// Any event that has no user-visible representation.
    Implicit(ImplicitEvent),
}
impl FloggableDebug for CharEvent {}

impl CharEvent {
    pub fn is_char(&self) -> bool {
        matches!(self, CharEvent::Key(_))
    }

    pub fn is_readline(&self) -> bool {
        matches!(self, CharEvent::Readline(_))
    }

    pub fn is_readline_or_command(&self) -> bool {
        matches!(self, CharEvent::Readline(_) | CharEvent::Command(_))
    }

    pub fn get_char(&self) -> char {
        let CharEvent::Key(kevt) = self else {
            panic!("Not a char type");
        };
        kevt.key.codepoint
    }

    pub fn get_key(&self) -> Option<&KeyEvent> {
        match self {
            CharEvent::Key(kevt) => Some(kevt),
            _ => None,
        }
    }

    pub fn get_readline(&self) -> ReadlineCmd {
        let CharEvent::Readline(c) = self else {
            panic!("Not a readline type");
        };
        c.cmd
    }

    pub fn get_command(&self) -> Option<&wstr> {
        match self {
            CharEvent::Command(c) => Some(c),
            _ => None,
        }
    }

    pub fn from_char(c: char) -> CharEvent {
        Self::from_key(Key::from_raw(c))
    }

    pub fn from_key(key: Key) -> CharEvent {
        Self::from_key_seq(key, WString::new())
    }

    pub fn from_key_seq(key: Key, seq: WString) -> CharEvent {
        CharEvent::Key(KeyEvent {
            key,
            input_style: CharInputStyle::Normal,
            seq,
        })
    }

    pub fn from_readline(cmd: ReadlineCmd) -> CharEvent {
        Self::from_readline_seq(cmd, WString::new())
    }

    pub fn from_readline_seq(cmd: ReadlineCmd, seq: WString) -> CharEvent {
        CharEvent::Readline(ReadlineCmdEvent { cmd, seq })
    }

    pub fn from_check_exit() -> CharEvent {
        CharEvent::Implicit(ImplicitEvent::CheckExit)
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

    NothingToRead,
}

fn readb(in_fd: RawFd, blocking: bool) -> ReadbResult {
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
        let select_res = fdset.check_readable(if blocking {
            Timeout::Forever
        } else {
            Timeout::Duration(Duration::from_millis(1))
        });
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

        if blocking {
            // select() did not return an error, so we may have a readable fd.
            // The priority order is: uvars, stdin, ioport.
            // Check to see if we want a universal variable barrier.
            if let Some(notifier_fd) = notifier_fd {
                if fdset.test(notifier_fd) && notifier.notification_fd_became_readable(notifier_fd)
                {
                    return ReadbResult::UvarNotified;
                }
            }
        }

        // Check stdin.
        if fdset.test(in_fd) {
            let mut arr: [u8; 1] = [0];
            if read_blocked(in_fd, &mut arr) != Ok(1) {
                // The terminal has been closed.
                return ReadbResult::Eof;
            }
            let c = arr[0];
            FLOG!(reader, "Read byte", char_to_symbol(char::from(c)));
            // The common path is to return a u8.
            return ReadbResult::Byte(c);
        }
        if !blocking {
            return ReadbResult::NothingToRead;
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

static TERMINAL_PROTOCOLS: AtomicBool = AtomicBool::new(false);
static BRACKETED_PASTE: AtomicBool = AtomicBool::new(false);

pub(crate) static SCROLL_FORWARD_SUPPORTED: RelaxedAtomicBool = RelaxedAtomicBool::new(false);
pub(crate) static CURSOR_UP_SUPPORTED: RelaxedAtomicBool = RelaxedAtomicBool::new(false);

#[repr(u8)]
pub(crate) enum Capability {
    Unknown,
    Supported,
    NotSupported,
}
pub(crate) static KITTY_KEYBOARD_SUPPORTED: AtomicU8 = AtomicU8::new(Capability::Unknown as _);

pub(crate) static SYNCHRONIZED_OUTPUT_SUPPORTED: RelaxedAtomicBool = RelaxedAtomicBool::new(false);

pub(crate) static CURSOR_POSITION_REPORTING_SUPPORTED: RelaxedAtomicBool =
    RelaxedAtomicBool::new(false);

pub fn kitty_progressive_enhancements_query() -> &'static [u8] {
    if std::env::var_os("TERM").is_some_and(|term| term.as_os_str().as_bytes() == b"st-256color") {
        return b"";
    }
    b"\x1b[?u"
}

static IS_TMUX: RelaxedAtomicBool = RelaxedAtomicBool::new(false);

pub(crate) static IN_MIDNIGHT_COMMANDER: RelaxedAtomicBool = RelaxedAtomicBool::new(false);
pub(crate) static IN_DVTM: RelaxedAtomicBool = RelaxedAtomicBool::new(false);
static ITERM_NO_KITTY_KEYBOARD: RelaxedAtomicBool = RelaxedAtomicBool::new(false);

pub fn terminal_protocol_hacks() {
    use std::env::var_os;
    IN_MIDNIGHT_COMMANDER.store(var_os("MC_TMPDIR").is_some());
    IN_DVTM
        .store(var_os("TERM").is_some_and(|term| term.as_os_str().as_bytes() == b"dvtm-256color"));
    IS_TMUX.store(var_os("TMUX").is_some());
    ITERM_NO_KITTY_KEYBOARD.store(
        var_os("LC_TERMINAL").is_some_and(|term| term.as_os_str().as_bytes() == b"iTerm2")
            && var_os("LC_TERMINAL_VERSION").is_some_and(|version| {
                let Some(version) = parse_version(&str2wcstring(version.as_os_str().as_bytes()))
                else {
                    return false;
                };
                version < (3, 5, 12)
            }),
    );
}

fn parse_version(version: &wstr) -> Option<(i64, i64, i64)> {
    let mut numbers = version.split('.');
    let major = fish_wcstol(numbers.next()?).ok()?;
    let minor = fish_wcstol(numbers.next()?).ok()?;
    let patch = numbers.next()?;
    let patch = &patch[..patch
        .chars()
        .position(|c| !c.is_ascii_digit())
        .unwrap_or(patch.len())];
    let patch = fish_wcstol(patch).ok()?;
    Some((major, minor, patch))
}

#[test]
fn test_parse_version() {
    assert_eq!(parse_version(L!("3.5.2")), Some((3, 5, 2)));
    assert_eq!(parse_version(L!("3.5.3beta")), Some((3, 5, 3)));
}

pub fn terminal_protocols_enable_ifn() {
    let did_write = RelaxedAtomicBool::new(false);
    let _save_screen_state = ScopeGuard::new((), |()| {
        if did_write.load() {
            reader_current_data().map(|data| data.save_screen_state());
        }
    });
    if !BRACKETED_PASTE.load(Ordering::Relaxed) {
        BRACKETED_PASTE.store(true, Ordering::Release);
        let _ = write_loop(&STDOUT_FILENO, b"\x1b[?2004h");
        if IS_TMUX.load() {
            let _ = write_loop(&STDOUT_FILENO, "\x1b[?1004h".as_bytes()); // focus reporting
        }
        did_write.store(true);
    }
    let kitty_keyboard_supported = KITTY_KEYBOARD_SUPPORTED.load(Ordering::Relaxed);
    if kitty_keyboard_supported == Capability::Unknown as _ {
        return;
    }
    if TERMINAL_PROTOCOLS.load(Ordering::Relaxed) {
        return;
    }
    TERMINAL_PROTOCOLS.store(true, Ordering::Release);
    FLOG!(term_protocols, "Enabling extended keys");
    if kitty_keyboard_supported == Capability::NotSupported as _ || ITERM_NO_KITTY_KEYBOARD.load() {
        let _ = write_loop(&STDOUT_FILENO, b"\x1b[>4;1m"); // XTerm's modifyOtherKeys
        let _ = write_loop(&STDOUT_FILENO, b"\x1b="); // set application keypad mode, so the keypad keys send unique codes
    } else {
        let _ = write_loop(&STDOUT_FILENO, b"\x1b[=5u"); // kitty progressive enhancements
    }
    did_write.store(true);
}

pub(crate) fn terminal_protocols_disable_ifn() {
    let did_write = RelaxedAtomicBool::new(false);
    let _save_screen_state = is_main_thread().then(|| {
        ScopeGuard::new((), |()| {
            if did_write.load() {
                reader_current_data().map(|data| data.save_screen_state());
            }
        })
    });
    if BRACKETED_PASTE.load(Ordering::Acquire) {
        let _ = write_loop(&STDOUT_FILENO, b"\x1b[?2004l");
        if IS_TMUX.load() {
            let _ = write_loop(&STDOUT_FILENO, "\x1b[?1004l".as_bytes());
        }
        BRACKETED_PASTE.store(false, Ordering::Release);
        did_write.store(true);
    }
    if !TERMINAL_PROTOCOLS.load(Ordering::Acquire) {
        return;
    }
    FLOG_SAFE!(term_protocols, "Disabling extended keys");
    let kitty_keyboard_supported = KITTY_KEYBOARD_SUPPORTED.load(Ordering::Acquire);
    assert_ne!(kitty_keyboard_supported, Capability::Unknown as _);
    if kitty_keyboard_supported == Capability::NotSupported as _ || ITERM_NO_KITTY_KEYBOARD.load() {
        let _ = write_loop(&STDOUT_FILENO, b"\x1b[>4;0m"); // XTerm's modifyOtherKeys
        let _ = write_loop(&STDOUT_FILENO, b"\x1b>"); // application keypad mode
    } else {
        let _ = write_loop(&STDOUT_FILENO, b"\x1b[=0u"); // kitty progressive enhancements
    }
    TERMINAL_PROTOCOLS.store(false, Ordering::Release);
    did_write.store(true);
}

fn parse_mask(mask: u32) -> Modifiers {
    Modifiers {
        sup: (mask & 8) != 0,
        ctrl: (mask & 4) != 0,
        alt: (mask & 2) != 0,
        shift: (mask & 1) != 0,
    }
}

// A data type used by the input machinery.
pub struct InputData {
    // The file descriptor from which we read input, often stdin.
    pub in_fd: RawFd,

    // Queue of unread characters.
    pub queue: VecDeque<CharEvent>,

    // The current paste buffer, if any.
    pub paste_buffer: Option<Vec<u8>>,

    // The arguments to the most recently invoked input function.
    pub input_function_args: Vec<char>,

    // The return status of the most recently invoked input function.
    pub function_status: bool,

    // Transient storage to avoid repeated allocations.
    pub event_storage: Vec<CharEvent>,
}

impl InputData {
    /// Construct from the fd from which to read.
    pub fn new(in_fd: RawFd) -> Self {
        Self {
            in_fd,
            queue: VecDeque::new(),
            paste_buffer: None,
            input_function_args: Vec::new(),
            function_status: false,
            event_storage: Vec::new(),
        }
    }

    /// Enqueue a char event to the queue of unread characters that input_readch will return before
    /// actually reading from fd 0.
    pub fn queue_char(&mut self, ch: CharEvent) {
        self.queue.push_back(ch);
    }

    /// Sets the return status of the most recently executed input function.
    pub fn function_set_status(&mut self, status: bool) {
        self.function_status = status;
    }
}

#[derive(Eq, PartialEq)]
pub enum CursorPositionWait {
    MouseLeft(ViewportPosition),
    ScrollbackPush,
}

#[derive(Eq, PartialEq)]
pub enum Queried {
    NotYet,
    Once,
    Twice,
}

#[derive(Eq, PartialEq)]
pub enum BlockingWait {
    Startup(Queried),
    CursorPosition(CursorPositionWait),
}

/// A trait which knows how to produce a stream of input events.
/// Note this is conceptually a "base class" with override points.
pub trait InputEventQueuer {
    /// Return the next event in the queue, or none if the queue is empty.
    fn try_pop(&mut self) -> Option<CharEvent> {
        if self.is_blocked() {
            match self.get_input_data().queue.front()? {
                CharEvent::Key(_) | CharEvent::Readline(_) | CharEvent::Command(_) => {
                    return None; // No code execution while blocked.
                }
                CharEvent::Implicit(_) => (),
            }
        }
        self.get_input_data_mut().queue.pop_front()
    }

    /// An "infallible" version of [`try_readch`](Self::try_readch) to be used when the input pipe
    /// fd is expected to outlive the input reader. Will panic upon EOF.
    #[inline(always)]
    fn readch(&mut self) -> CharEvent {
        match self.try_readch(/*blocking*/ true) {
            Some(c) => c,
            None => unreachable!(),
        }
    }

    /// Function used by [`readch`](Self::readch) to read bytes from stdin until enough bytes have been read to
    /// convert them to a wchar_t. Conversion is done using `mbrtowc`. If a character has previously
    /// been read and then 'unread', that character is returned.
    ///
    /// This is guaranteed to keep returning `Some(CharEvent)` so long as the input stream remains
    /// open; `None` is only returned upon EOF as the main loop within blocks until input becomes
    /// available.
    ///
    /// This method is used directly by the fuzzing harness to avoid a panic on bounded inputs.
    fn try_readch(&mut self, blocking: bool) -> Option<CharEvent> {
        loop {
            // Do we have something enqueued already?
            // Note this may be initially true, or it may become true through calls to
            // iothread_service_main() or env_universal_barrier() below.
            if let Some(mevt) = self.try_pop() {
                return Some(mevt);
            }

            // We are going to block; but first allow any override to inject events.
            self.prepare_to_select();
            if let Some(mevt) = self.try_pop() {
                return Some(mevt);
            }

            let rr = readb(self.get_in_fd(), blocking);
            match rr {
                ReadbResult::Eof => {
                    return Some(CharEvent::Implicit(ImplicitEvent::Eof));
                }

                ReadbResult::Interrupted => {
                    self.select_interrupted();
                }

                ReadbResult::UvarNotified => {
                    self.uvar_change_notified();
                }

                ReadbResult::IOPortNotified => {
                    self.ioport_notified();
                }

                ReadbResult::Byte(read_byte) => {
                    let mut have_escape_prefix = false;
                    let mut buffer = vec![read_byte];
                    let key_with_escape = if read_byte == 0x1b {
                        self.parse_escape_sequence(&mut buffer, &mut have_escape_prefix)
                    } else {
                        canonicalize_control_char(read_byte)
                    };
                    if self.paste_is_buffering() {
                        if read_byte != 0x1b {
                            self.paste_push_char(read_byte);
                        }
                        continue;
                    }
                    let mut seq = WString::new();
                    let mut key = key_with_escape;
                    if key == Some(Key::from_raw(key::Invalid)) {
                        continue;
                    }
                    assert!(key.map_or(true, |key| key.codepoint != key::Invalid));
                    let mut consumed = 0;
                    let mut state = zero_mbstate();
                    let mut i = 0;
                    let ok = loop {
                        if i == buffer.len() {
                            buffer.push(match readb(self.get_in_fd(), /*blocking=*/ true) {
                                ReadbResult::Byte(b) => b,
                                _ => 0,
                            });
                        }
                        match self.parse_codepoint(
                            &mut state,
                            &mut key,
                            &mut seq,
                            &buffer,
                            i,
                            &mut consumed,
                            &mut have_escape_prefix,
                        ) {
                            ControlFlow::Continue(codepoint_complete) => {
                                if codepoint_complete && i + 1 == buffer.len() {
                                    break true;
                                }
                            }
                            ControlFlow::Break(()) => {
                                break false;
                            }
                        }
                        i += 1;
                    };
                    if !ok {
                        continue;
                    }
                    let (key_evt, extra) = if let Some(key) = key {
                        (CharEvent::from_key_seq(key, seq), None)
                    } else {
                        let Some(c) = seq.chars().next() else {
                            continue;
                        };
                        (
                            CharEvent::from_key_seq(Key::from_raw(c), seq.clone()),
                            Some(seq.chars().skip(1).map(CharEvent::from_char)),
                        )
                    };
                    if self.is_blocked() {
                        FLOG!(
                            reader,
                            "Still blocked on response from terminal, deferring key event",
                            key_evt
                        );
                        self.push_back(key_evt);
                        extra.map(|extra| {
                            for evt in extra {
                                self.push_back(evt);
                            }
                        });
                        let vintr = shell_modes().c_cc[libc::VINTR];
                        if vintr != 0 && key == Some(Key::from_single_byte(vintr)) {
                            FLOG!(
                                reader,
                                "Received interrupt key, giving up waiting for response from terminal"
                            );
                            let ok = unblock_input(self.blocking_wait());
                            assert!(ok);
                        }
                        continue;
                    }
                    extra.map(|extra| self.insert_front(extra));
                    return Some(key_evt);
                }
                ReadbResult::NothingToRead => return None,
            }
        }
    }

    fn try_readb(&mut self, buffer: &mut Vec<u8>) -> Option<u8> {
        let ReadbResult::Byte(next) = readb(self.get_in_fd(), /*blocking=*/ false) else {
            return None;
        };
        buffer.push(next);
        Some(next)
    }

    fn parse_escape_sequence(
        &mut self,
        buffer: &mut Vec<u8>,
        have_escape_prefix: &mut bool,
    ) -> Option<Key> {
        assert!(buffer.len() <= 2);
        let recursive_invocation = buffer.len() == 2;
        let Some(next) = self.try_readb(buffer) else {
            if !self.paste_is_buffering() {
                return Some(Key::from_raw(key::Escape));
            }
            return None;
        };
        let invalid = Key::from_raw(key::Invalid);
        if recursive_invocation && next == b'\x1b' {
            return Some(
                match self.parse_escape_sequence(buffer, have_escape_prefix) {
                    Some(mut nested_sequence) => {
                        if nested_sequence == invalid {
                            return Some(Key::from_raw(key::Escape));
                        }
                        nested_sequence.modifiers.alt = true;
                        nested_sequence
                    }
                    _ => invalid,
                },
            );
        }
        if next == b'[' {
            // potential CSI
            return Some(self.parse_csi(buffer).unwrap_or(invalid));
        }
        if next == b'O' {
            // potential SS3
            return Some(self.parse_ss3(buffer).unwrap_or(invalid));
        }
        if !recursive_invocation && next == b'P' {
            // potential DCS
            return Some(self.parse_dcs(buffer).unwrap_or(invalid));
        }
        match canonicalize_control_char(next) {
            Some(mut key) => {
                key.modifiers.alt = true;
                Some(key)
            }
            None => {
                *have_escape_prefix = true;
                None
            }
        }
    }

    fn parse_codepoint(
        &mut self,
        state: &mut mbstate_t,
        out_key: &mut Option<Key>,
        out_seq: &mut WString,
        buffer: &[u8],
        i: usize,
        consumed: &mut usize,
        have_escape_prefix: &mut bool,
    ) -> ControlFlow<(), bool> {
        let mut res: char = '\0';
        let read_byte = buffer[i];
        if crate::libc::MB_CUR_MAX() == 1 {
            // single-byte locale, all values are legal
            // FIXME: this looks wrong, this falsely assumes that
            // the single-byte locale is compatible with Unicode upper-ASCII.
            res = read_byte.into();
            out_seq.push(res);
            return ControlFlow::Continue(true);
        }
        let mut codepoint = u32::from(res);
        let sz = unsafe {
            mbrtowc(
                std::ptr::addr_of_mut!(codepoint).cast(),
                std::ptr::addr_of!(read_byte).cast(),
                1,
                state,
            )
        } as isize;
        match sz {
            -1 => {
                FLOG!(reader, "Illegal input");
                *consumed += 1;
                self.push_front(CharEvent::from_check_exit());
                return ControlFlow::Break(());
            }
            -2 => {
                // Sequence not yet complete.
                return ControlFlow::Continue(false);
            }
            0 => {
                // Actual nul char.
                *consumed += 1;
                out_seq.push('\0');
                return ControlFlow::Continue(true);
            }
            _ => (),
        }
        if let Some(res) = char::from_u32(codepoint) {
            // Sequence complete.
            if !fish_reserved_codepoint(res) {
                if *have_escape_prefix && i != 0 {
                    *have_escape_prefix = false;
                    *out_key = Some(alt(res));
                }
                *consumed += 1;
                out_seq.push(res);
                return ControlFlow::Continue(true);
            }
        }
        for &b in &buffer[*consumed..i] {
            out_seq.push(encode_byte_to_char(b));
            *consumed += 1;
        }
        ControlFlow::Continue(true)
    }

    fn parse_csi(&mut self, buffer: &mut Vec<u8>) -> Option<Key> {
        // The maximum number of CSI parameters is defined by NPAR, nominally 16.
        let mut params = [[0_u32; 4]; 16];
        let Some(mut c) = self.try_readb(buffer) else {
            return Some(ctrl('['));
        };
        let mut next_char = |zelf: &mut Self| zelf.try_readb(buffer).unwrap_or(0xff);
        let private_mode;
        if matches!(c, b'?' | b'<' | b'=' | b'>') {
            // private mode
            private_mode = Some(c);
            c = next_char(self);
        } else {
            private_mode = None;
        }
        let mut count = 0;
        let mut subcount = 0;
        while count < 16 && c >= 0x30 && c <= 0x3f {
            if c.is_ascii_digit() {
                // Return None on invalid ascii numeric CSI parameter exceeding u32 bounds
                match params[count][subcount]
                    .checked_mul(10)
                    .and_then(|result| result.checked_add(u32::from(c - b'0')))
                {
                    Some(c) => params[count][subcount] = c,
                    None => return invalid_sequence(buffer),
                };
            } else if c == b':' && subcount < 3 {
                subcount += 1;
            } else if c == b';' {
                count += 1;
                subcount = 0;
            } else {
                // Unexpected character or unrecognized CSI
                return None;
            }
            c = next_char(self);
        }
        if c != b'$' && !(0x40..=0x7e).contains(&c) {
            return None;
        }

        let masked_key = |mut codepoint, shifted_codepoint| {
            let mask = params[1][0].saturating_sub(1);
            let mut modifiers = parse_mask(mask);
            if let Some(shifted_codepoint) = shifted_codepoint {
                if shifted_codepoint != '\0' && modifiers.shift {
                    modifiers.shift = false;
                    codepoint = shifted_codepoint;
                }
            }
            Key {
                modifiers,
                codepoint,
            }
        };

        let key = match c {
            b'$' => {
                if next_char(self) == b'y' {
                    if private_mode == Some(b'?') {
                        // DECRPM
                        if params[0][0] == 2026 && matches!(params[1][0], 1 | 2) {
                            FLOG!(reader, "Synchronized output is supported");
                            SYNCHRONIZED_OUTPUT_SUPPORTED.store(true);
                        }
                    }
                    // DECRQM
                    return None;
                }
                match params[0][0] {
                    23 | 24 => shift(
                        char::from_u32(u32::from(function_key(11)) + params[0][0] - 23).unwrap(), // rxvt style
                    ),
                    _ => return None,
                }
            }
            b'A' => masked_key(key::Up, None),
            b'B' => masked_key(key::Down, None),
            b'C' => masked_key(key::Right, None),
            b'D' => masked_key(key::Left, None),
            b'E' => masked_key('5', None),       // Numeric keypad
            b'F' => masked_key(key::End, None),  // PC/xterm style
            b'H' => masked_key(key::Home, None), // PC/xterm style
            b'M' | b'm' => {
                self.disable_mouse_tracking();
                // Generic X10 or modified VT200 sequence, or extended (SGR/1006) mouse
                // reporting mode, with semicolon-separated parameters for button code, Px,
                // and Py, ending with 'M' for button press or 'm' for button release.
                let sgr = private_mode == Some(b'<');
                if !sgr && c == b'm' {
                    return None;
                }
                let Some(button) = (if sgr {
                    Some(params[0][0])
                } else {
                    u32::from(next_char(self)).checked_sub(32)
                }) else {
                    return invalid_sequence(buffer);
                };
                let mut convert = |param| {
                    (if sgr {
                        Some(param)
                    } else {
                        u32::from(next_char(self)).checked_sub(32)
                    })
                    .and_then(|coord| coord.checked_sub(1))
                    .and_then(|coord| usize::try_from(coord).ok())
                };
                let Some(x) = convert(params[1][0]) else {
                    return invalid_sequence(buffer);
                };
                let Some(y) = convert(params[2][0]) else {
                    return invalid_sequence(buffer);
                };
                let position = ViewportPosition { x, y };
                let modifiers = parse_mask((button >> 2) & 0x07);
                let code = button & 0x43;
                if code != 0 || c != b'M' || modifiers.is_some() {
                    return None;
                }
                let wait_guard = self.blocking_wait();
                let Some(wait) = &*wait_guard else {
                    drop(wait_guard);
                    self.on_mouse_left_click(position);
                    return None;
                };
                match wait {
                    BlockingWait::Startup(_) => {}
                    BlockingWait::CursorPosition(_) => {
                        // TODO: re-queue it I guess.
                        FLOG!(
                                reader,
                                "Ignoring mouse left click received while still waiting for Cursor Position Report"
                            );
                    }
                }
                return None;
            }
            b't' => {
                self.disable_mouse_tracking();
                // VT200 button released in mouse highlighting mode at valid text location. 5 chars.
                let _ = next_char(self);
                let _ = next_char(self);
                return None;
            }
            b'T' => {
                self.disable_mouse_tracking();
                // VT200 button released in mouse highlighting mode past end-of-line. 9 characters.
                for _ in 0..6 {
                    let _ = next_char(self);
                }
                return None;
            }
            b'P' => masked_key(function_key(1), None),
            b'Q' => masked_key(function_key(2), None),
            b'R' => {
                let Some(y) = params[0][0]
                    .checked_sub(1)
                    .and_then(|y| usize::try_from(y).ok())
                else {
                    return invalid_sequence(buffer);
                };
                let Some(x) = params[1][0]
                    .checked_sub(1)
                    .and_then(|x| usize::try_from(x).ok())
                else {
                    return invalid_sequence(buffer);
                };
                FLOG!(reader, "Received cursor position report y:", y, "x:", x);
                let wait_guard = self.blocking_wait();
                let Some(BlockingWait::CursorPosition(wait)) = &*wait_guard else {
                    CURSOR_POSITION_REPORTING_SUPPORTED.store(true);
                    return None;
                };
                let continuation = match wait {
                    CursorPositionWait::MouseLeft(click_position) => {
                        ImplicitEvent::MouseLeftClickContinuation(
                            ViewportPosition { x, y },
                            *click_position,
                        )
                    }
                    CursorPositionWait::ScrollbackPush => {
                        ImplicitEvent::ScrollbackPushContinuation(y)
                    }
                };
                drop(wait_guard);
                self.push_front(CharEvent::Implicit(continuation));
                return None;
            }
            b'S' => masked_key(function_key(4), None),
            b'~' => match params[0][0] {
                1 => masked_key(key::Home, None), // VT220/tmux style
                2 => masked_key(key::Insert, None),
                3 => masked_key(key::Delete, None),
                4 => masked_key(key::End, None), // VT220/tmux style
                5 => masked_key(key::PageUp, None),
                6 => masked_key(key::PageDown, None),
                7 => masked_key(key::Home, None), // rxvt style
                8 => masked_key(key::End, None),  // rxvt style
                11..=15 => masked_key(
                    char::from_u32(u32::from(function_key(1)) + params[0][0] - 11).unwrap(),
                    None,
                ),
                17..=21 => masked_key(
                    char::from_u32(u32::from(function_key(6)) + params[0][0] - 17).unwrap(),
                    None,
                ),
                23 | 24 => masked_key(
                    char::from_u32(u32::from(function_key(11)) + params[0][0] - 23).unwrap(),
                    None,
                ),
                25 | 26 => {
                    shift(char::from_u32(u32::from(function_key(3)) + params[0][0] - 25).unwrap())
                } // rxvt style
                27 => {
                    let key =
                        canonicalize_keyed_control_char(char::from_u32(params[2][0]).unwrap());
                    masked_key(key, None)
                }
                28 | 29 => {
                    shift(char::from_u32(u32::from(function_key(5)) + params[0][0] - 28).unwrap())
                } // rxvt style
                31 | 32 => {
                    shift(char::from_u32(u32::from(function_key(7)) + params[0][0] - 31).unwrap())
                } // rxvt style
                33 | 34 => {
                    shift(char::from_u32(u32::from(function_key(9)) + params[0][0] - 33).unwrap())
                } // rxvt style
                200 => {
                    self.paste_start_buffering();
                    return None;
                }
                201 => {
                    self.paste_commit();
                    return None;
                }
                _ => return None,
            },
            b'c' if private_mode == Some(b'?') => {
                self.push_front(CharEvent::Implicit(ImplicitEvent::PrimaryDeviceAttribute));
                return None;
            }
            b'u' => {
                if private_mode == Some(b'?') {
                    FLOG!(
                        reader,
                        "Received kitty progressive enhancement flags, marking as supported"
                    );
                    KITTY_KEYBOARD_SUPPORTED.store(Capability::Supported as _, Ordering::Release);
                    return None;
                }

                // Treat numpad keys the same as their non-numpad counterparts. Could add a numpad modifier here.
                let key = match params[0][0] {
                    57361 => key::PrintScreen,
                    57363 => key::Menu,
                    57399 => '0',
                    57400 => '1',
                    57401 => '2',
                    57402 => '3',
                    57403 => '4',
                    57404 => '5',
                    57405 => '6',
                    57406 => '7',
                    57407 => '8',
                    57408 => '9',
                    57409 => '.',
                    57410 => '/',
                    57411 => '*',
                    57412 => '-',
                    57413 => '+',
                    57414 => key::Enter,
                    57415 => '=',
                    57417 => key::Left,
                    57418 => key::Right,
                    57419 => key::Up,
                    57420 => key::Down,
                    57421 => key::PageUp,
                    57422 => key::PageDown,
                    57423 => key::Home,
                    57424 => key::End,
                    57425 => key::Insert,
                    57426 => key::Delete,
                    cp => canonicalize_keyed_control_char(char::from_u32(cp).unwrap()),
                };
                masked_key(
                    key,
                    Some(canonicalize_keyed_control_char(
                        char::from_u32(params[0][1]).unwrap(),
                    )),
                )
            }
            b'Z' => shift(key::Tab),
            b'I' => {
                self.push_front(CharEvent::Implicit(ImplicitEvent::FocusIn));
                return None;
            }
            b'O' => {
                self.push_front(CharEvent::Implicit(ImplicitEvent::FocusOut));
                return None;
            }
            _ => return None,
        };
        Some(key)
    }

    fn disable_mouse_tracking(&mut self) {
        // fish recognizes but does not actually support mouse reporting. We never turn it on, and
        // it's only ever enabled if a program we spawned enabled it and crashed or forgot to turn
        // it off before exiting. We turn it off here to avoid wasting resources.
        //
        // Since this is only called when we detect an incoming mouse reporting payload, we know the
        // terminal emulator supports mouse reporting, so no terminfo checks.
        FLOG!(reader, "Disabling mouse tracking");

        // We shouldn't directly manipulate stdout from here, so we ask the reader to do it.
        // writembs(outputter_t::stdoutput(), "\x1B[?1000l");
        self.push_front(CharEvent::Implicit(ImplicitEvent::DisableMouseTracking));
    }

    fn parse_ss3(&mut self, buffer: &mut Vec<u8>) -> Option<Key> {
        let mut raw_mask = 0;
        let Some(mut code) = self.try_readb(buffer) else {
            return Some(alt('O'));
        };
        while (b'0'..=b'9').contains(&code) {
            raw_mask = raw_mask * 10 + u32::from(code - b'0');
            code = self.try_readb(buffer).unwrap_or(0xff);
        }
        let modifiers = parse_mask(raw_mask.saturating_sub(1));
        #[rustfmt::skip]
        let key = match code {
            b' ' => Key{modifiers, codepoint: key::Space},
            b'A' => Key{modifiers, codepoint: key::Up},
            b'B' => Key{modifiers, codepoint: key::Down},
            b'C' => Key{modifiers, codepoint: key::Right},
            b'D' => Key{modifiers, codepoint: key::Left},
            b'F' => Key{modifiers, codepoint: key::End},
            b'H' => Key{modifiers, codepoint: key::Home},
            b'I' => Key{modifiers, codepoint: key::Tab},
            b'M' => Key{modifiers, codepoint: key::Enter},
            b'P' => Key{modifiers, codepoint: function_key(1)},
            b'Q' => Key{modifiers, codepoint: function_key(2)},
            b'R' => Key{modifiers, codepoint: function_key(3)},
            b'S' => Key{modifiers, codepoint: function_key(4)},
            b'X' => Key{modifiers, codepoint: '='},
            b'j' => Key{modifiers, codepoint: '*'},
            b'k' => Key{modifiers, codepoint: '+'},
            b'l' => Key{modifiers, codepoint: ','},
            b'm' => Key{modifiers, codepoint: '-'},
            b'n' => Key{modifiers, codepoint: '.'},
            b'o' => Key{modifiers, codepoint: '/'},
            b'p' => Key{modifiers, codepoint: '0'},
            b'q' => Key{modifiers, codepoint: '1'},
            b'r' => Key{modifiers, codepoint: '2'},
            b's' => Key{modifiers, codepoint: '3'},
            b't' => Key{modifiers, codepoint: '4'},
            b'u' => Key{modifiers, codepoint: '5'},
            b'v' => Key{modifiers, codepoint: '6'},
            b'w' => Key{modifiers, codepoint: '7'},
            b'x' => Key{modifiers, codepoint: '8'},
            b'y' => Key{modifiers, codepoint: '9'},
            _ => return None,
        };
        Some(key)
    }

    fn parse_xtversion(&mut self, buffer: &mut Vec<u8>) {
        assert!(buffer.len() == 3);
        loop {
            match self.try_readb(buffer) {
                None => return,
                Some(b'\x1b') => break,
                Some(_) => continue,
            }
        }
        if self.try_readb(buffer) != Some(b'\\') {
            return;
        }
        if buffer[3] != b'|' {
            return;
        }
        FLOG!(
            reader,
            format!(
                "Received XTVERSION response: {}",
                str2wcstring(&buffer[4..buffer.len() - 2]),
            )
        );
    }

    fn parse_dcs(&mut self, buffer: &mut Vec<u8>) -> Option<Key> {
        assert!(buffer.len() == 2);
        let Some(success) = self.try_readb(buffer) else {
            return Some(alt('P'));
        };
        let success = match success {
            b'0' => false,
            b'1' => true,
            b'>' => {
                self.parse_xtversion(buffer);
                return None;
            }
            _ => return None,
        };
        if self.try_readb(buffer)? != b'+' {
            return None;
        }
        if self.try_readb(buffer)? != b'r' {
            return None;
        }
        while self.try_readb(buffer)? != b'\x1b' {}
        if self.try_readb(buffer)? != b'\\' {
            return None;
        }
        buffer.pop();
        buffer.pop();
        // \e P 1 r + Pn ST
        // \e P 0 r + msg ST
        let buffer = &buffer[5..];
        if !success {
            FLOG!(
                reader,
                format!(
                    "Received XTGETTCAP failure response: {}",
                    str2wcstring(&parse_hex(buffer)?),
                )
            );
            return None;
        }
        let mut buffer = buffer.splitn(2, |&c| c == b'=');
        let key = buffer.next().unwrap();
        let value = buffer.next()?;
        let key = parse_hex(key)?;
        let value = parse_hex(value)?;
        FLOG!(
            reader,
            format!(
                "Received XTGETTCAP response: {}={:?}",
                str2wcstring(&key),
                str2wcstring(&value)
            )
        );
        if key == b"indn" && matches!(&value[..], b"\x1b[%p1%dS" | b"\\E[%p1%dS") {
            SCROLL_FORWARD_SUPPORTED.store(true);
            FLOG!(reader, "Scroll forward is supported");
        }
        if key == b"cuu" && matches!(&value[..], b"\x1b[%p1%dA" | b"\\E[%p1%dA") {
            CURSOR_UP_SUPPORTED.store(true);
            FLOG!(reader, "Cursor up is supported");
        }
        return None;
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
    /// Return None on timeout, the event on success.
    fn readch_timed(&mut self, wait_time_ms: usize) -> Option<CharEvent> {
        if let Some(evt) = self.try_pop() {
            return Some(evt);
        }
        terminal_protocols_enable_ifn();

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
        if is_windows_subsystem_for_linux(WSL::V1) {
            // Merely querying the current thread's sigmask is sufficient to deliver a pending signal
            let _ = unsafe { libc::pthread_sigmask(0, ptr::null(), &mut sigs) };
        }
        if res > 0 {
            return Some(self.readch());
        }
        None
    }

    /// Return the fd from which to read.
    fn get_in_fd(&self) -> RawFd {
        self.get_input_data().in_fd
    }

    /// Return the input data. This is to be implemented by the concrete type.
    fn get_input_data(&self) -> &InputData;
    fn get_input_data_mut(&mut self) -> &mut InputData;

    // Support for "bracketed paste"
    // The way it works is that we acknowledge our support by printing
    // \e\[?2004h
    // then the terminal will "bracket" every paste in
    // \e\[200~ and \e\[201~
    // Every character in between those two will be part of the paste and should not cause a binding to execute (like \n executing commands).
    //
    // We enable it after every command and disable it before, see the terminal protocols logic.
    //
    // Support for this seems to be ubiquitous - emacs enables it unconditionally (!) since 25.1
    // (though it only supports it since then, it seems to be the last term to gain support).
    //
    // See http://thejh.net/misc/website-terminal-copy-paste.

    fn paste_start_buffering(&mut self) {
        self.get_input_data_mut().paste_buffer = Some(Vec::new());
    }

    fn paste_is_buffering(&self) -> bool {
        self.get_input_data().paste_buffer.is_some()
    }

    fn paste_push_char(&mut self, b: u8) {
        self.get_input_data_mut()
            .paste_buffer
            .as_mut()
            .unwrap()
            .push(b)
    }

    fn paste_commit(&mut self) {
        self.get_input_data_mut().paste_buffer = None;
    }

    /// Enqueue a character or a readline function to the queue of unread characters that
    /// readch will return before actually reading from fd 0.
    fn push_back(&mut self, ch: CharEvent) {
        self.get_input_data_mut().queue.push_back(ch);
    }

    /// Add a character or a readline function to the front of the queue of unread characters.  This
    /// will be the next character returned by readch.
    fn push_front(&mut self, ch: CharEvent) {
        self.get_input_data_mut().queue.push_front(ch);
    }

    /// Find the first sequence of non-char events, and promote them to the front.
    fn promote_interruptions_to_front(&mut self) {
        // Find the first sequence of non-char events.
        // EOF is considered a char: we don't want to pull EOF in front of real chars.
        let queue = &mut self.get_input_data_mut().queue;
        let is_char = |evt: &CharEvent| {
            evt.is_char() || matches!(evt, CharEvent::Implicit(ImplicitEvent::Eof))
        };
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
        let queue = &mut self.get_input_data_mut().queue;
        let iter = evts.into_iter().rev();
        queue.reserve(iter.size_hint().0);
        for evt in iter {
            queue.push_front(evt);
        }
    }

    /// Forget all enqueued readline events in the front of the queue.
    fn drop_leading_readline_events(&mut self) {
        let queue = &mut self.get_input_data_mut().queue;
        while let Some(evt) = queue.front() {
            if evt.is_readline_or_command() {
                queue.pop_front();
            } else {
                break;
            }
        }
    }

    fn blocking_wait(&self) -> MutexGuard<Option<BlockingWait>> {
        static NO_WAIT: Mutex<Option<BlockingWait>> = Mutex::new(None);
        NO_WAIT.lock().unwrap()
    }
    fn is_blocked(&self) -> bool {
        false
    }

    fn on_mouse_left_click(&mut self, _position: ViewportPosition) {}

    /// Override point for when we are about to (potentially) block in select(). The default does
    /// nothing.
    fn prepare_to_select(&mut self) {}

    /// Called when select() is interrupted by a signal.
    fn select_interrupted(&mut self) {}

    fn enqueue_interrupt_key(&mut self) {
        let vintr = shell_modes().c_cc[libc::VINTR];
        if vintr != 0 {
            let interrupt_evt = CharEvent::from_key(Key::from_single_byte(vintr));
            if unblock_input(self.blocking_wait()) {
                FLOG!(
                    reader,
                    "Received interrupt, giving up on waiting for terminal response"
                );
                self.push_back(interrupt_evt);
            } else {
                self.push_front(interrupt_evt);
            }
        }
    }

    /// Override point for when when select() is interrupted by the universal variable notifier.
    /// The default does nothing.
    fn uvar_change_notified(&mut self) {}

    /// Override point for when the ioport is ready.
    /// The default does nothing.
    fn ioport_notified(&mut self) {}

    /// Reset the function status.
    fn get_function_status(&self) -> bool {
        self.get_input_data().function_status
    }

    /// Return if we have any lookahead.
    fn has_lookahead(&self) -> bool {
        !self.get_input_data().queue.is_empty()
    }
}

pub(crate) fn unblock_input(mut wait_guard: MutexGuard<Option<BlockingWait>>) -> bool {
    if wait_guard.is_none() {
        return false;
    }
    *wait_guard = None;
    true
}

fn invalid_sequence(buffer: &[u8]) -> Option<Key> {
    FLOG!(
        reader,
        "Error: invalid escape sequence: ",
        DisplayBytes(buffer)
    );
    None
}

struct DisplayBytes<'a>(&'a [u8]);

impl<'a> std::fmt::Display for DisplayBytes<'a> {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        for (i, &c) in self.0.iter().enumerate() {
            if i != 0 {
                write!(f, " ")?;
            }
            write!(f, "{}", char_to_symbol(char::from(c)))?;
        }
        Ok(())
    }
}

/// A simple, concrete implementation of InputEventQueuer.
pub struct InputEventQueue {
    data: InputData,
}

impl InputEventQueue {
    pub fn new(in_fd: RawFd) -> InputEventQueue {
        InputEventQueue {
            data: InputData::new(in_fd),
        }
    }
}

impl InputEventQueuer for InputEventQueue {
    fn get_input_data(&self) -> &InputData {
        &self.data
    }

    fn get_input_data_mut(&mut self) -> &mut InputData {
        &mut self.data
    }

    fn select_interrupted(&mut self) {
        if reader_test_and_clear_interrupted() != 0 {
            self.enqueue_interrupt_key();
        }
    }
}

fn parse_hex(hex: &[u8]) -> Option<Vec<u8>> {
    if hex.len() % 2 != 0 {
        return None;
    }
    let mut result = vec![0; hex.len() / 2];
    let mut i = 0;
    while i < hex.len() {
        let d1 = char::from(hex[i]).to_digit(16)?;
        let d2 = char::from(hex[i + 1]).to_digit(16)?;
        let decoded = u8::try_from(16 * d1 + d2).unwrap();
        result[i / 2] = decoded;
        i += 2;
    }
    Some(result)
}

#[test]
fn test_parse_hex() {
    assert_eq!(parse_hex(b"3d"), Some(vec![61]));
}
