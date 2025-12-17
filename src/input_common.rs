use crate::common::{
    WSL, bytes2wcstring, fish_reserved_codepoint, is_windows_subsystem_for_linux, read_blocked,
    shell_modes,
};
use crate::env::{EnvStack, Environment};
use crate::fd_readable_set::{FdReadableSet, Timeout};
use crate::flog::{FloggableDebug, FloggableDisplay, flog};
use crate::future_feature_flags::{FeatureFlag, test as feature_test};
use crate::key::{
    self, Key, Modifiers, ViewportPosition, alt, canonicalize_control_char,
    canonicalize_keyed_control_char, char_to_symbol, function_key, shift,
};
use crate::reader::reader_test_and_clear_interrupted;
use crate::tty_handoff::{
    SCROLL_CONTENT_UP_TERMINFO_CODE, TERMINAL_OS_NAME, XTGETTCAP_QUERY_OS_NAME, XTVERSION,
    maybe_set_kitty_keyboard_capability, maybe_set_scroll_content_up_capability,
};
use crate::universal_notifier::default_notifier;
use crate::wchar::prelude::*;
use crate::wutil::{fish_is_pua, fish_wcstol};
use fish_wchar::encode_byte_to_char;
use std::cell::{RefCell, RefMut};
use std::collections::VecDeque;
use std::mem::MaybeUninit;
use std::os::fd::RawFd;
use std::sync::atomic::{AtomicUsize, Ordering};
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
    ForwardPathComponent,
    ForwardToken,
    BackwardPathComponent,
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
    KillPathComponent,
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
    UpcaseSelection,
    DowncaseSelection,
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

#[derive(Clone, Copy, Debug)]
pub struct KeyEvent {
    pub key: Key,
    pub shifted_codepoint: char,
    pub base_layout_codepoint: char,
}

impl KeyEvent {
    pub(crate) fn new(modifiers: Modifiers, codepoint: char) -> Self {
        Self::from(Key::new(modifiers, codepoint))
    }
    pub(crate) fn new_with(
        modifiers: Modifiers,
        codepoint: char,
        shifted_key: Option<char>,
        base_layout_key: Option<char>,
    ) -> Self {
        Self {
            key: Key::new(modifiers, codepoint),
            shifted_codepoint: shifted_key.unwrap_or_default(),
            base_layout_codepoint: base_layout_key.unwrap_or_default(),
        }
    }
    pub(crate) fn from_raw(codepoint: char) -> Self {
        Self::from(Key::from_raw(codepoint))
    }
    pub fn from_single_byte(c: u8) -> Self {
        Self::from(Key::from_single_byte(c))
    }

    pub(crate) fn codepoint_text(&self) -> Option<char> {
        let mut modifiers = self.modifiers;
        let mut c = self.codepoint;
        if self.shifted_codepoint != '\0' && modifiers.shift {
            modifiers.shift = false;
            c = self.shifted_codepoint;
        }
        if modifiers.is_some() {
            return None;
        }
        if c == key::Space {
            return Some(' ');
        }
        if c == key::Enter {
            return Some('\n');
        }
        if c == key::Tab {
            return Some('\t');
        }
        if fish_is_pua(c) || u32::from(c) <= 27 {
            return None;
        }
        Some(c)
    }
}

impl From<Key> for KeyEvent {
    fn from(key: Key) -> Self {
        Self::new_with(key.modifiers, key.codepoint, None, None)
    }
}

impl std::ops::Deref for KeyEvent {
    type Target = Key;
    fn deref(&self) -> &Self::Target {
        &self.key
    }
}

impl std::ops::DerefMut for KeyEvent {
    fn deref_mut(&mut self) -> &mut Self::Target {
        &mut self.key
    }
}

fn apply_shift(mut key: Key, do_ascii: bool, shifted_codepoint: char) -> Option<Key> {
    if !key.modifiers.shift {
        return Some(key);
    }
    if shifted_codepoint != '\0' {
        key.codepoint = shifted_codepoint;
    } else if do_ascii && key.codepoint.is_ascii_lowercase() {
        // For backwards compatibility, we convert the "bind shift-a" notation to "bind A".
        // This enables us to match "A" events which are the legacy encoding for keys that
        // generate text -- until we request kitty's "Report all keys as escape codes".
        // We do not currently convert non-ASCII key notation such as "bind shift-ä".
        key.codepoint = key.codepoint.to_ascii_uppercase();
    } else {
        return None;
    };
    key.modifiers.shift = false;
    Some(key)
}

#[derive(Clone, Copy, Debug, PartialEq, Eq, PartialOrd, Ord)]
pub(crate) enum KeyMatchQuality {
    BaseLayoutModuloShift,
    BaseLayout,
    ModuloShift,
    Exact,
}

impl FloggableDebug for KeyMatchQuality {}

pub(crate) fn match_key_event_to_key(event: &KeyEvent, key: &Key) -> Option<KeyMatchQuality> {
    if &event.key == key {
        return Some(KeyMatchQuality::Exact);
    }

    let shifted_evt = apply_shift(event.key, false, event.shifted_codepoint);
    let shifted_key = apply_shift(*key, true, '\0');
    if shifted_evt.is_some() && shifted_evt == shifted_key {
        return Some(KeyMatchQuality::ModuloShift);
    }

    if event.base_layout_codepoint != '\0' {
        let mut base_layout_key = event.key;
        base_layout_key.codepoint = event.base_layout_codepoint;
        if base_layout_key == *key {
            return Some(KeyMatchQuality::BaseLayout);
        }
        let shifted_base_layout_key = apply_shift(base_layout_key, true, '\0');
        if shifted_base_layout_key.is_some() && shifted_base_layout_key == shifted_key {
            return Some(KeyMatchQuality::BaseLayoutModuloShift);
        }
    }

    None
}

/// Represents an event on the character input stream.
#[derive(Debug, Clone)]
pub enum CharEventType {
    /// A character was entered.
    Char(KeyInputEvent),

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
pub struct KeyInputEvent {
    // The key.
    pub key: KeyEvent,
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
    /// Mouse left click.
    MouseLeft(ViewportPosition),
    /// Terminal color theme change (light/dark mode).
    NewColorTheme,
    /// Window height changed.
    NewWindowHeight,
}

#[derive(Debug, Clone)]
pub enum QueryResponse {
    PrimaryDeviceAttribute,
    BackgroundColor(xterm_color::Color),
    CursorPosition(ViewportPosition),
}

#[derive(Debug, Clone)]
pub enum QueryResultEvent {
    Response(QueryResponse),
    Timeout,
    /// Canceled with ctrl-c.
    Interrupted,
}

#[derive(Debug, Clone)]
pub enum CharEvent {
    /// A character was entered.
    Key(KeyInputEvent),

    /// A readline event.
    Readline(ReadlineCmdEvent),

    /// A shell command.
    Command(WString),

    /// Any event that has no user-visible representation.
    Implicit(ImplicitEvent),

    QueryResult(QueryResultEvent),
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

    pub fn get_key(&self) -> Option<&KeyInputEvent> {
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
        Self::from_key(KeyEvent::from_raw(c))
    }

    pub fn from_key(key: KeyEvent) -> CharEvent {
        Self::from_key_seq(key, WString::new())
    }

    pub fn from_key_seq(key: KeyEvent, seq: WString) -> CharEvent {
        CharEvent::Key(KeyInputEvent {
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
enum InputEventTrigger {
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

    // No file descriptor was ready within the query timeout.
    TimeoutElapsed,
}

fn readb(in_fd: RawFd) -> Option<u8> {
    assert!(in_fd >= 0, "Invalid in fd");
    let mut arr: [u8; 1] = [0];
    if read_blocked(in_fd, &mut arr) != Ok(1) {
        // The terminal has been closed.
        return None;
    }
    let c = arr[0];
    flog!(reader, "Read byte", char_to_symbol(char::from(c), true));
    // The common path is to return a u8.
    Some(c)
}

fn next_input_event(in_fd: RawFd, ioport_fd: RawFd, timeout: Timeout) -> InputEventTrigger {
    let mut fdset = FdReadableSet::new();
    loop {
        fdset.clear();
        fdset.add(in_fd);

        // Add the completion ioport (possibly -1 - a no-op).
        fdset.add(ioport_fd);

        // Get the uvar notifier fd (possibly none).
        let notifier = default_notifier();
        let notifier_fd = notifier.notification_fd();
        if let Some(notifier_fd) = notifier_fd {
            fdset.add(notifier_fd);
        }

        // Here's where we call select().
        let select_res = fdset.check_readable(timeout);
        if select_res < 0 {
            let err = errno::errno().0;
            if err == libc::EINTR || err == libc::EAGAIN {
                // A signal.
                return InputEventTrigger::Interrupted;
            } else {
                // Some fd was invalid, so probably the tty has been closed.
                return InputEventTrigger::Eof;
            }
        }
        if select_res == 0 {
            assert!(!matches!(timeout, Timeout::Forever));
            return InputEventTrigger::TimeoutElapsed;
        }

        // select() did not return an error, so we may have a readable fd.
        // The priority order is: uvars, stdin, ioport.
        // Check to see if we want a universal variable barrier.
        if let Some(notifier_fd) = notifier_fd {
            if fdset.test(notifier_fd) && notifier.notification_fd_became_readable(notifier_fd) {
                return InputEventTrigger::UvarNotified;
            }
        }

        // Check stdin.
        if fdset.test(in_fd) {
            return readb(in_fd).map_or(InputEventTrigger::Eof, InputEventTrigger::Byte);
        }

        // Check for iothread completions only if there is no data to be read from the stdin.
        // This gives priority to the foreground.
        if fdset.test(ioport_fd) {
            return InputEventTrigger::IOPortNotified;
        }
    }
}

pub fn check_fd_readable(in_fd: RawFd, timeout: Duration) -> bool {
    use std::ptr;
    // We are not prepared to handle a signal immediately; we only want to know if we get input on
    // our fd before the timeout. Use pselect to block all signals; we will handle signals
    // before the next call to readch().
    let mut sigs = MaybeUninit::uninit();
    let mut sigs = unsafe {
        libc::sigfillset(sigs.as_mut_ptr());
        sigs.assume_init()
    };

    // pselect expects timeouts in nanoseconds.
    const NSEC_PER_MSEC: u64 = 1000 * 1000;
    const NSEC_PER_SEC: u64 = NSEC_PER_MSEC * 1000;
    let wait_nsec: u64 = (timeout.as_millis() as u64) * NSEC_PER_MSEC;
    let timeout = libc::timespec {
        tv_sec: (wait_nsec / NSEC_PER_SEC).try_into().unwrap(),
        tv_nsec: (wait_nsec % NSEC_PER_SEC).try_into().unwrap(),
    };

    // We have one fd of interest.
    let mut fdset = MaybeUninit::uninit();
    let mut fdset = unsafe {
        libc::FD_ZERO(fdset.as_mut_ptr());
        fdset.assume_init()
    };
    unsafe {
        libc::FD_SET(in_fd, &mut fdset);
    }

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
    res > 0
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
            eprintf!(
                concat!(
                    "ignoring fish_escape_delay_ms: value '%s' ",
                    "is not an integer or is < 10 or >= 5000 ms\n"
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
            eprintf!(
                concat!(
                    "ignoring fish_sequence_key_delay_ms: value '%s' ",
                    "is not an integer or is < 10 or >= 5000 ms\n"
                ),
                sequence_key_time_ms
            )
        }
    }
}

fn parse_mask(mask: u32) -> (Modifiers, bool) {
    let modifiers = Modifiers {
        ctrl: (mask & 4) != 0,
        alt: (mask & 2) != 0,
        shift: (mask & 1) != 0,
        sup: (mask & 8) != 0,
    };
    let caps_lock = (mask & 64) != 0;
    (modifiers, caps_lock)
}

// A data type used by the input machinery.
#[derive(Default)]
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

    // How long to wait for responses for TTY queries.
    pub blocking_query_timeout: Option<Duration>,

    // If set, events will be buffered until the query finishes.
    pub blocking_query: RefCell<Option<TerminalQuery>>,
}

impl InputData {
    /// Construct from the fd from which to read.
    pub fn new(in_fd: RawFd, blocking_query_timeout: Option<Duration>) -> Self {
        Self {
            in_fd,
            queue: VecDeque::new(),
            paste_buffer: None,
            input_function_args: Vec::new(),
            function_status: false,
            event_storage: Vec::new(),
            blocking_query_timeout,
            blocking_query: RefCell::new(None),
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

#[derive(Clone, Default, Eq, PartialEq)]
pub struct BackgroundColorQuery {
    pub result: Option<xterm_color::Color>,
}

#[derive(Clone, Eq, PartialEq)]
pub enum CursorPositionQueryReason {
    NewPrompt,
    WindowHeightChange,
}

#[derive(Clone, Eq, PartialEq)]
pub struct CursorPositionQuery {
    pub reason: CursorPositionQueryReason,
    pub result: Option<ViewportPosition>,
}

impl CursorPositionQuery {
    pub fn new(reason: CursorPositionQueryReason) -> Self {
        Self {
            reason,
            result: None,
        }
    }
}

#[derive(Clone, Default, Eq, PartialEq)]
pub struct RecurrentQuery {
    pub background_color: Option<BackgroundColorQuery>,
    pub cursor_position: Option<CursorPositionQuery>,
}

#[derive(Clone, Eq, PartialEq)]
pub enum TerminalQuery {
    Initial,
    Recurrent(RecurrentQuery),
}

pub const LONG_READ_TIMEOUT: Duration = Duration::from_secs(2);

/// A trait which knows how to produce a stream of input events.
/// Note this is conceptually a "base class" with override points.
pub trait InputEventQueuer {
    /// Return the next event in the queue, or none if the queue is empty.
    fn try_pop(&mut self) -> Option<CharEvent> {
        if self.is_blocked_querying() {
            use ImplicitEvent::*;
            match self.get_input_data().queue.front()? {
                CharEvent::QueryResult(_) | CharEvent::Implicit(CheckExit | Eof) => {}
                CharEvent::Key(_)
                | CharEvent::Readline(_)
                | CharEvent::Command(_)
                | CharEvent::Implicit(_) => {
                    return None; // No code execution while blocked.
                }
            }
        }
        self.get_input_data_mut().queue.pop_front()
    }

    /// Read the next event, such as a UTF-8-encoded codepoint.
    fn readch(&mut self) -> CharEvent {
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

            match next_input_event(
                self.get_in_fd(),
                self.get_ioport_fd(),
                if self.is_blocked_querying() {
                    Timeout::Duration(self.get_input_data().blocking_query_timeout.unwrap())
                } else {
                    Timeout::Forever
                },
            ) {
                InputEventTrigger::Eof => {
                    return CharEvent::Implicit(ImplicitEvent::Eof);
                }

                InputEventTrigger::Interrupted => {
                    self.select_interrupted();
                }

                InputEventTrigger::UvarNotified => {
                    self.uvar_change_notified();
                }

                InputEventTrigger::IOPortNotified => {
                    self.ioport_notified();
                }

                InputEventTrigger::Byte(read_byte) => {
                    let mut have_escape_prefix = false;
                    let mut buffer = vec![read_byte];
                    let mut key = if read_byte == 0x1b {
                        self.parse_escape_sequence(&mut buffer, &mut have_escape_prefix)
                    } else {
                        canonicalize_control_char(read_byte).map(KeyEvent::from)
                    };
                    if self.paste_is_buffering() {
                        if read_byte != 0x1b {
                            self.paste_push_char(read_byte);
                        }
                        continue;
                    }
                    let mut seq = WString::new();
                    if key.is_some_and(|key| key.key == Key::from_raw(key::Invalid)) {
                        continue;
                    }
                    assert!(key.is_none_or(|key| key.codepoint != key::Invalid));
                    // At this point, the bytes in `buffer` should be parsed as a UTF-8 sequence,
                    // or, if they are not valid UTF-8, ignored. On incomplete sequences, another
                    // byte is read and decoding is tried again in the next iteration.
                    let ok = loop {
                        match decode_one_codepoint_utf8(&mut seq, InvalidPolicy::Error, &buffer) {
                            DecodeState::Incomplete => {
                                buffer.push(
                                    match next_input_event(
                                        self.get_in_fd(),
                                        self.get_ioport_fd(),
                                        Timeout::Forever,
                                    ) {
                                        InputEventTrigger::Byte(b) => b,
                                        _ => 0,
                                    },
                                );
                            }
                            DecodeState::Complete => {
                                if have_escape_prefix {
                                    let c = seq.as_char_slice().last().unwrap();
                                    key = Some(KeyEvent::from(alt(*c)));
                                }
                                break true;
                            }
                            DecodeState::Error => {
                                self.push_front(CharEvent::from_check_exit());
                                break false;
                            }
                        }
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
                            CharEvent::from_key_seq(KeyEvent::from_raw(c), seq.clone()),
                            Some(seq.chars().skip(1).map(CharEvent::from_char)),
                        )
                    };
                    if self.is_blocked_querying() {
                        flog!(
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
                        if vintr != 0
                            && key.is_some_and(|key| {
                                match_key_event_to_key(&key, &Key::from_single_byte(vintr))
                                    .is_some()
                            })
                        {
                            flog!(
                                reader,
                                "Received interrupt key, giving up waiting for response from terminal"
                            );
                            let ok = stop_query(self.blocking_query());
                            assert!(ok);
                            self.get_input_data_mut().queue.clear();
                            self.push_front(CharEvent::QueryResult(QueryResultEvent::Interrupted));
                        }
                        continue;
                    }
                    extra.map(|extra| self.insert_front(extra));
                    return key_evt;
                }
                InputEventTrigger::TimeoutElapsed => {
                    return CharEvent::QueryResult(QueryResultEvent::Timeout);
                }
            }
        }
    }

    fn read_sequence_byte(&mut self, buffer: &mut Vec<u8>) -> Option<u8> {
        let fd = self.get_in_fd();
        let strict = feature_test(FeatureFlag::OmitTermWorkarounds);
        let historical_millis = |ms| {
            if strict {
                LONG_READ_TIMEOUT
            } else {
                Duration::from_millis(ms)
            }
        };
        if !check_fd_readable(
            fd,
            if self.paste_is_buffering() || self.is_blocked_querying() {
                historical_millis(300)
            } else if buffer == b"\x1b" {
                Duration::from_millis(1) // distinguish legacy escape
            } else {
                historical_millis(30)
            },
        ) {
            flog!(
                reader,
                format!("Incomplete escape sequence: {}", DisplayBytes(buffer))
            );
            if buffer != b"\x1b" && strict {
                flog!(
                    error,
                    format!(
                        "Incomplete escape sequence seen (logging because omit-term-workarounds is on): {}",
                        DisplayBytes(buffer)
                    )
                );
            }
            return None;
        }
        let next = readb(fd)?;
        buffer.push(next);
        Some(next)
    }

    fn parse_escape_sequence(
        &mut self,
        buffer: &mut Vec<u8>,
        have_escape_prefix: &mut bool,
    ) -> Option<KeyEvent> {
        assert!(buffer.len() <= 2);
        let recursive_invocation = buffer.len() == 2;
        let Some(next) = self.read_sequence_byte(buffer) else {
            return Some(KeyEvent::from_raw(key::Escape));
        };
        let invalid = KeyEvent::from_raw(key::Invalid);
        if recursive_invocation && next == b'\x1b' {
            return Some(
                match self.parse_escape_sequence(buffer, have_escape_prefix) {
                    Some(mut nested_sequence) => {
                        if nested_sequence.key == invalid.key {
                            return Some(KeyEvent::from_raw(key::Escape));
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
        if !recursive_invocation {
            if next == b']' {
                // OSC
                self.parse_osc(buffer);
                return Some(invalid);
            }
            if next == b'P' {
                // potential DCS
                return Some(self.parse_dcs(buffer).unwrap_or(invalid));
            }
        }
        match canonicalize_control_char(next) {
            Some(mut key) => {
                key.modifiers.alt = true;
                Some(KeyEvent::from(key))
            }
            None => {
                *have_escape_prefix = true;
                None
            }
        }
    }

    fn parse_csi(&mut self, buffer: &mut Vec<u8>) -> Option<KeyEvent> {
        // The maximum number of CSI parameters is defined by NPAR, nominally 16.
        let mut params = [[0_u32; 4]; 16];
        let Some(mut c) = self.read_sequence_byte(buffer) else {
            return Some(KeyEvent::from(alt('[')));
        };
        let mut next_char = |zelf: &mut Self| zelf.read_sequence_byte(buffer).unwrap_or(0xff);
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
        while count < 16 && (0x30..=0x3f).contains(&c) {
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

        let kitty_key = |key: char, shifted_key: Option<char>, base_layout_key: Option<char>| {
            let mask = params[1][0].saturating_sub(1);
            let (mut modifiers, caps_lock) = parse_mask(mask);

            // An event like "capslock-shift-=" should have a shifted codepoint ("+") to enable
            // fish to match "bind +".
            //
            // With letters that are affected by capslock, capslock and shift cancel each
            // other out ("capslock-shift-ä"), unless there is another modifier to imply that
            // capslock should be ignored.
            //
            // So if shift is the only modifier, we should consume it, but not if the event is
            // something like "capslock-shift-delete" because delete is not affected by capslock.
            //
            // Normally, we could consume shift by translating to the shifted key.
            // While capslock is on however, we don't get a shifted key, see
            // https://github.com/kovidgoyal/kitty/issues/8493.
            //
            // Do it by trying to find out ourselves whether the key is affected by capslock.
            //
            // Alternatively, we could relax our exact matching semantics, and make "bind ä"
            // match the "shift-ä" event, as suggested in the kitty issue.
            if caps_lock && modifiers == Modifiers::SHIFT && !key.to_uppercase().eq(Some(key)) {
                modifiers.shift = false;
            }
            KeyEvent::new_with(modifiers, key, shifted_key, base_layout_key)
        };
        let masked_key = |key: char| kitty_key(key, None, None);

        let key = match c {
            b'$' => {
                if next_char(self) == b'y' {
                    // DECRPM/DECRQM
                    return None;
                }
                match params[0][0] {
                    23 | 24 => KeyEvent::from(shift(
                        char::from_u32(u32::from(function_key(11)) + params[0][0] - 23).unwrap(), // rxvt style
                    )),
                    _ => return None,
                }
            }
            b'A' => masked_key(key::Up),
            b'B' => masked_key(key::Down),
            b'C' => masked_key(key::Right),
            b'D' => masked_key(key::Left),
            b'E' => masked_key('5'),       // Numeric keypad
            b'F' => masked_key(key::End),  // PC/xterm style
            b'H' => masked_key(key::Home), // PC/xterm style
            b'M' | b'm' => {
                flog!(reader, "mouse event");
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
                let (modifiers, _caps_lock) = parse_mask((button >> 2) & 0x07);
                let code = button & 0x43;
                if code != 0 || c != b'M' || modifiers.is_some() {
                    return None;
                }
                self.push_front(CharEvent::Implicit(ImplicitEvent::MouseLeft(position)));
                return None;
            }
            b't' => {
                flog!(reader, "mouse event");
                // VT200 button released in mouse highlighting mode at valid text location. 5 chars.
                let _ = next_char(self);
                let _ = next_char(self);
                return None;
            }
            b'T' => {
                flog!(reader, "mouse event");
                // VT200 button released in mouse highlighting mode past end-of-line. 9 characters.
                for _ in 0..6 {
                    let _ = next_char(self);
                }
                return None;
            }
            b'P' => masked_key(function_key(1)),
            b'Q' => masked_key(function_key(2)),
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
                flog!(reader, "Received cursor position report y:", y, "x:", x);
                let cursor_pos = ViewportPosition { x, y };
                self.push_query_response(QueryResponse::CursorPosition(cursor_pos));
                return None;
            }
            b'S' => masked_key(function_key(4)),
            b'~' => match params[0][0] {
                1 => masked_key(key::Home), // VT220/tmux style
                2 => masked_key(key::Insert),
                3 => masked_key(key::Delete),
                4 => masked_key(key::End), // VT220/tmux style
                5 => masked_key(key::PageUp),
                6 => masked_key(key::PageDown),
                7 => masked_key(key::Home), // rxvt style
                8 => masked_key(key::End),  // rxvt style
                11..=15 => masked_key(
                    char::from_u32(u32::from(function_key(1)) + params[0][0] - 11).unwrap(),
                ),
                17..=21 => masked_key(
                    char::from_u32(u32::from(function_key(6)) + params[0][0] - 17).unwrap(),
                ),
                23 | 24 => masked_key(
                    char::from_u32(u32::from(function_key(11)) + params[0][0] - 23).unwrap(),
                ),
                25 | 26 => KeyEvent::from(shift(
                    char::from_u32(u32::from(function_key(3)) + params[0][0] - 25).unwrap(),
                )), // rxvt style
                27 => {
                    let Some(key) = char::from_u32(params[2][0]) else {
                        return invalid_sequence(buffer);
                    };
                    masked_key(canonicalize_keyed_control_char(key))
                }
                28 | 29 => KeyEvent::from(shift(
                    char::from_u32(u32::from(function_key(5)) + params[0][0] - 28).unwrap(),
                )), // rxvt style
                31 | 32 => KeyEvent::from(shift(
                    char::from_u32(u32::from(function_key(7)) + params[0][0] - 31).unwrap(),
                )), // rxvt style
                33 | 34 => KeyEvent::from(shift(
                    char::from_u32(u32::from(function_key(9)) + params[0][0] - 33).unwrap(),
                )), // rxvt style
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
                flog!(reader, "Received Primary Device Attribute response");
                self.push_query_response(QueryResponse::PrimaryDeviceAttribute);
                return None;
            }
            b'n' if private_mode == Some(b'?') && params[0] == [997, 0, 0, 0] => {
                match params[1] {
                    [1, 0, 0, 0] | [2, 0, 0, 0] => (),
                    _ => return None,
                };
                flog!(reader, "Received color theme change");
                self.push_front(CharEvent::Implicit(ImplicitEvent::NewColorTheme));
                return None;
            }
            b'u' => {
                if private_mode == Some(b'?') {
                    maybe_set_kitty_keyboard_capability();
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
                    cp => {
                        let Some(key) = char::from_u32(cp) else {
                            return invalid_sequence(buffer);
                        };
                        canonicalize_keyed_control_char(key)
                    }
                };
                let Some(shifted_key) = char::from_u32(params[0][1]) else {
                    return invalid_sequence(buffer);
                };
                let Some(base_layout_key) = char::from_u32(params[0][2]) else {
                    return invalid_sequence(buffer);
                };
                kitty_key(
                    key,
                    Some(canonicalize_keyed_control_char(shifted_key)),
                    Some(base_layout_key),
                )
            }
            b'Z' => KeyEvent::from(shift(key::Tab)),
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

    fn parse_ss3(&mut self, buffer: &mut Vec<u8>) -> Option<KeyEvent> {
        let mut raw_mask = 0;
        let Some(mut code) = self.read_sequence_byte(buffer) else {
            return Some(KeyEvent::from(alt('O')));
        };
        while code.is_ascii_digit() {
            raw_mask = raw_mask * 10 + u32::from(code - b'0');
            code = self.read_sequence_byte(buffer).unwrap_or(0xff);
        }
        let (modifiers, _caps_lock) = parse_mask(raw_mask.saturating_sub(1));
        #[rustfmt::skip]
        let key = match code {
            b' ' => KeyEvent::new(modifiers, key::Space),
            b'A' => KeyEvent::new(modifiers, key::Up),
            b'B' => KeyEvent::new(modifiers, key::Down),
            b'C' => KeyEvent::new(modifiers, key::Right),
            b'D' => KeyEvent::new(modifiers, key::Left),
            b'F' => KeyEvent::new(modifiers, key::End),
            b'H' => KeyEvent::new(modifiers, key::Home),
            b'I' => KeyEvent::new(modifiers, key::Tab),
            b'M' => KeyEvent::new(modifiers, key::Enter),
            b'P' => KeyEvent::new(modifiers, function_key(1)),
            b'Q' => KeyEvent::new(modifiers, function_key(2)),
            b'R' => KeyEvent::new(modifiers, function_key(3)),
            b'S' => KeyEvent::new(modifiers, function_key(4)),
            b'X' => KeyEvent::new(modifiers, '='),
            b'j' => KeyEvent::new(modifiers, '*'),
            b'k' => KeyEvent::new(modifiers, '+'),
            b'l' => KeyEvent::new(modifiers, ','),
            b'm' => KeyEvent::new(modifiers, '-'),
            b'n' => KeyEvent::new(modifiers, '.'),
            b'o' => KeyEvent::new(modifiers, '/'),
            b'p' => KeyEvent::new(modifiers, '0'),
            b'q' => KeyEvent::new(modifiers, '1'),
            b'r' => KeyEvent::new(modifiers, '2'),
            b's' => KeyEvent::new(modifiers, '3'),
            b't' => KeyEvent::new(modifiers, '4'),
            b'u' => KeyEvent::new(modifiers, '5'),
            b'v' => KeyEvent::new(modifiers, '6'),
            b'w' => KeyEvent::new(modifiers, '7'),
            b'x' => KeyEvent::new(modifiers, '8'),
            b'y' => KeyEvent::new(modifiers, '9'),
            _ => return None,
        };
        Some(key)
    }

    fn read_until_sequence_terminator(
        &mut self,
        buffer: &mut Vec<u8>,
        allow_bel: bool,
    ) -> Option<()> {
        let mut escape = false;
        loop {
            let b = self.read_sequence_byte(buffer)?;
            if allow_bel && b == b'\x07' {
                buffer.pop();
                return Some(());
            }
            if escape && b == b'\\' {
                buffer.pop();
                buffer.pop();
                return Some(());
            }
            escape = b == b'\x1b';
        }
    }

    fn parse_xtversion(&mut self, buffer: &mut Vec<u8>) -> Option<()> {
        assert_eq!(buffer, b"\x1bP>");
        self.read_until_sequence_terminator(buffer, false)?;
        if buffer.get(3)? != &b'|' {
            return None;
        }
        XTVERSION.get_or_init(|| {
            let xtversion = bytes2wcstring(&buffer[4..buffer.len()]);
            flog!(
                reader,
                format!("Received XTVERSION response: {}", xtversion)
            );
            xtversion
        });
        None
    }

    fn parse_osc(&mut self, buffer: &mut Vec<u8>) -> Option<()> {
        let osc_prefix = b"\x1b]";
        assert!(buffer == osc_prefix);
        self.read_until_sequence_terminator(buffer, /*allow_bel=*/ true)?;
        let buffer = &buffer[osc_prefix.len()..];
        let buffer = buffer.strip_prefix(b"11;")?;
        let c = xterm_color::Color::parse(buffer).ok()?;
        flog!(reader, format!("Received background color {c:?}"));
        self.push_query_response(QueryResponse::BackgroundColor(c));
        None
    }

    fn parse_dcs(&mut self, buffer: &mut Vec<u8>) -> Option<KeyEvent> {
        assert!(buffer == b"\x1bP");
        let Some(success) = self.read_sequence_byte(buffer) else {
            return Some(KeyEvent::from(alt('P')));
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
        if self.read_sequence_byte(buffer)? != b'+' {
            return None;
        }
        if self.read_sequence_byte(buffer)? != b'r' {
            return None;
        }
        self.read_until_sequence_terminator(buffer, false)?;
        // \e P 1 r + Pn ST
        // \e P 0 r + msg ST
        let buffer = &buffer[5..];
        if !success {
            flog!(
                reader,
                format!(
                    "Received XTGETTCAP failure response: {}",
                    bytes2wcstring(&parse_hex(buffer)?),
                )
            );
            return None;
        }
        let mut buffer = buffer.splitn(2, |&c| c == b'=');
        let key = buffer.next().unwrap();
        let key = parse_hex(key)?;
        let value = if let Some(value) = buffer.next() {
            let value = parse_hex(value)?;
            flog!(
                reader,
                format!(
                    "Received XTGETTCAP response: {}={:?}",
                    bytes2wcstring(&key),
                    bytes2wcstring(&value)
                )
            );
            Some(value)
        } else {
            flog!(
                reader,
                format!("Received XTGETTCAP response: {}", bytes2wcstring(&key))
            );
            None
        };
        if key == SCROLL_CONTENT_UP_TERMINFO_CODE.as_bytes() {
            maybe_set_scroll_content_up_capability();
        } else if key == XTGETTCAP_QUERY_OS_NAME.as_bytes() {
            if let Some(value) = value {
                TERMINAL_OS_NAME.get_or_init(|| Some(bytes2wcstring(&value)));
            }
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

        check_fd_readable(
            self.get_in_fd(),
            Duration::from_millis(u64::try_from(wait_time_ms).unwrap()),
        )
        .then(|| self.readch())
    }

    /// Return the fd from which to read.
    fn get_in_fd(&self) -> RawFd {
        self.get_input_data().in_fd
    }

    /// Return the fd of the IO port, or -1 if none.
    fn get_ioport_fd(&self) -> RawFd {
        -1
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

    fn push_query_response(&mut self, response: QueryResponse) {
        self.push_front(CharEvent::QueryResult(QueryResultEvent::Response(response)));
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

    fn blocking_query(&self) -> RefMut<'_, Option<TerminalQuery>> {
        self.get_input_data().blocking_query.borrow_mut()
    }
    fn is_blocked_querying(&self) -> bool {
        self.blocking_query().is_some()
    }

    /// Override point for when we are about to (potentially) block in select(). The default does
    /// nothing.
    fn prepare_to_select(&mut self) {}

    /// Called when select() is interrupted by a signal.
    fn select_interrupted(&mut self) {}

    fn enqueue_interrupt_key(&mut self) {
        let vintr = shell_modes().c_cc[libc::VINTR];
        if vintr != 0 {
            let interrupt_evt = CharEvent::from_key(KeyEvent::from_single_byte(vintr));
            if stop_query(self.blocking_query()) {
                flog!(
                    reader,
                    "Received interrupt, giving up on waiting for terminal response"
                );
                self.get_input_data_mut().queue.clear();
                self.push_front(CharEvent::QueryResult(QueryResultEvent::Interrupted));
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

pub(crate) enum DecodeState {
    Incomplete,
    Complete,
    Error,
}

#[derive(Eq, PartialEq)]
pub(crate) enum InvalidPolicy {
    Error,
    Passthrough,
}

pub(crate) fn decode_one_codepoint_utf8(
    out_seq: &mut WString,
    invalid_policy: InvalidPolicy,
    buffer: &[u8],
) -> DecodeState {
    use DecodeState::*;
    match std::str::from_utf8(buffer) {
        Ok(parsed_str) => {
            for c in parsed_str.chars() {
                if !fish_reserved_codepoint(c) {
                    out_seq.push(c);
                }
            }
            Complete
        }
        Err(e) => match e.error_len() {
            Some(_) => match invalid_policy {
                InvalidPolicy::Error => {
                    flog!(reader, "Illegal input encoding");
                    Error
                }
                InvalidPolicy::Passthrough => {
                    for &b in buffer {
                        out_seq.push(encode_byte_to_char(b));
                    }
                    Complete
                }
            },
            None => Incomplete,
        },
    }
}

pub(crate) fn stop_query(mut query: RefMut<'_, Option<TerminalQuery>>) -> bool {
    query.take().is_some()
}

fn invalid_sequence(buffer: &[u8]) -> Option<KeyEvent> {
    flog!(
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
            write!(f, "{}", char_to_symbol(char::from(c), i == 0))?;
        }
        Ok(())
    }
}
impl<'a> FloggableDisplay for DisplayBytes<'a> {}

/// A simple, concrete implementation of InputEventQueuer.
pub struct InputEventQueue {
    data: InputData,
}

impl InputEventQueue {
    pub fn new(in_fd: RawFd, blocking_query_timeout: Option<Duration>) -> Self {
        Self {
            data: InputData::new(in_fd, blocking_query_timeout),
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
    parse_hex_into(&mut result, hex)?;
    Some(result)
}
fn parse_hex_into(out: &mut [u8], hex: &[u8]) -> Option<()> {
    assert!(out.len() * 2 == hex.len());
    let mut i = 0;
    while i < hex.len() {
        let d1 = char::from(hex[i]).to_digit(16)?;
        let d2 = char::from(hex[i + 1]).to_digit(16)?;
        let decoded = u8::try_from(16 * d1 + d2).unwrap();
        out[i / 2] = decoded;
        i += 2;
    }
    Some(())
}

#[cfg(test)]
mod tests {
    use super::{
        CharEvent, InputEventQueue, InputEventQueuer, KeyEvent, KeyMatchQuality, ReadlineCmd,
        match_key_event_to_key, parse_hex,
    };
    use crate::key::{Key, Modifiers};

    #[test]
    fn test_match_key_event_to_key() {
        macro_rules! validate {
            ($evt:expr, $key:expr, $expected:expr) => {
                assert_eq!(match_key_event_to_key(&$evt, &$key), $expected);
            };
        }

        let none = Modifiers::default();
        let shift = Modifiers::SHIFT;
        let ctrl = Modifiers::CTRL;
        let ctrl_shift = Modifiers {
            ctrl: true,
            shift: true,
            ..Default::default()
        };

        let exact = KeyMatchQuality::Exact;
        let modulo_shift = KeyMatchQuality::ModuloShift;
        let base_layout = KeyMatchQuality::BaseLayout;
        let base_layout_modulo_shift = KeyMatchQuality::BaseLayoutModuloShift;

        validate!(KeyEvent::new(none, 'a'), Key::new(none, 'a'), Some(exact));
        validate!(KeyEvent::new(none, 'a'), Key::new(none, 'A'), None);
        validate!(KeyEvent::new(shift, 'a'), Key::new(shift, 'a'), Some(exact));
        validate!(KeyEvent::new(shift, 'a'), Key::new(none, 'A'), None);
        validate!(KeyEvent::new(shift, 'ä'), Key::new(none, 'Ä'), None);
        // For historical reasons we canonicalize notation for ASCII keys like "shift-a" to "A",
        // but not "shift-a" events - those should send a shifted key.
        validate!(
            KeyEvent::new(none, 'A'),
            Key::new(shift, 'a'),
            Some(modulo_shift)
        );
        validate!(KeyEvent::new(none, 'A'), Key::new(shift, 'A'), None);
        validate!(KeyEvent::new(none, 'Ä'), Key::new(none, 'Ä'), Some(exact));
        validate!(KeyEvent::new(none, 'Ä'), Key::new(shift, 'ä'), None);

        // FYI: for codepoints that are not letters with uppercase/lowercase versions, we use
        // the shifted key in the canonical notation, because the unshifted one may depend on the
        // keyboard layout.
        let ctrl_shift_equals = KeyEvent::new_with(ctrl_shift, '=', Some('+'), None);
        validate!(ctrl_shift_equals, Key::new(ctrl_shift, '='), Some(exact));
        validate!(ctrl_shift_equals, Key::new(ctrl, '+'), Some(modulo_shift)); // canonical notation
        validate!(ctrl_shift_equals, Key::new(ctrl_shift, '+'), None);
        validate!(ctrl_shift_equals, Key::new(ctrl, '='), None);

        // A event like capslock-shift-ä may or may not include a shifted codepoint.
        //
        // Without a shifted codepoint, we cannot easily match ctrl-Ä.
        let caps_ctrl_shift_ä = KeyEvent::new(ctrl_shift, 'ä');
        validate!(caps_ctrl_shift_ä, Key::new(ctrl_shift, 'ä'), Some(exact)); // canonical notation
        validate!(caps_ctrl_shift_ä, Key::new(ctrl, 'ä'), None);
        validate!(caps_ctrl_shift_ä, Key::new(ctrl, 'Ä'), None); // can't match without shifted key
        validate!(caps_ctrl_shift_ä, Key::new(ctrl_shift, 'Ä'), None);
        // With a shifted codepoint, we can match the alternative notation too.
        let caps_ctrl_shift_ä = KeyEvent::new_with(ctrl_shift, 'ä', Some('Ä'), None);
        validate!(caps_ctrl_shift_ä, Key::new(ctrl_shift, 'ä'), Some(exact)); // canonical notation
        validate!(caps_ctrl_shift_ä, Key::new(ctrl, 'ä'), None);
        validate!(caps_ctrl_shift_ä, Key::new(ctrl, 'Ä'), Some(modulo_shift)); // matched via shifted key
        validate!(caps_ctrl_shift_ä, Key::new(ctrl_shift, 'Ä'), None);

        let ctrl_ц = KeyEvent::new_with(ctrl, 'ц', None, Some('w'));
        let ctrl_shift_ц = KeyEvent::new_with(ctrl_shift, 'ц', Some('Ц'), Some('w'));
        validate!(ctrl_ц, Key::new(ctrl, 'ц'), Some(exact));
        validate!(ctrl_ц, Key::new(ctrl, 'w'), Some(base_layout));
        validate!(ctrl_ц, Key::new(ctrl_shift, 'ц'), None);
        validate!(ctrl_ц, Key::new(ctrl_shift, 'w'), None);
        validate!(
            ctrl_shift_ц,
            Key::new(ctrl, 'W'),
            Some(base_layout_modulo_shift)
        );
        validate!(ctrl_shift_ц, Key::new(ctrl, 'w'), None);

        // Note that "bind ctrl-Ц" will win over "bind ctrl-shift-w".
        // This is because we consider shift transformation to be less magic than base-key
        // transformation.
        validate!(ctrl_shift_ц, Key::new(ctrl, 'Ц'), Some(modulo_shift));
        validate!(ctrl_shift_ц, Key::new(ctrl_shift, 'w'), Some(base_layout));
    }

    #[test]
    fn test_push_front_back() {
        let mut queue = InputEventQueue::new(0, None);
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
        let mut queue = InputEventQueue::new(0, None);
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
        let mut queue = InputEventQueue::new(0, None);
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

    #[test]
    fn test_parse_hex() {
        assert_eq!(parse_hex(b"3d"), Some(vec![61]));
    }
}
