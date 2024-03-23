use libc::{STDOUT_FILENO, VERASE};

use crate::common::{
    fish_reserved_codepoint, is_windows_subsystem_for_linux, read_blocked, ScopeGuard,
    ScopeGuarding,
};
use crate::env::{EnvStack, Environment};
use crate::fd_readable_set::FdReadableSet;
use crate::flog::FLOG;
use crate::key::{self, alt, control, function_key, shift, Chord, Modifiers};
use crate::reader::{reader_current_data, TERMINAL_MODE_ON_STARTUP};
use crate::threads::{iothread_port, iothread_service_main};
use crate::universal_notifier::default_notifier;
use crate::wchar::{encode_byte_to_char, prelude::*};
use crate::wutil::encoding::{mbrtowc, mbstate_t, zero_mbstate};
use crate::wutil::{fish_wcstol, write_to_fd};
use std::collections::VecDeque;
use std::os::fd::RawFd;
use std::ptr;
use std::sync::atomic::{AtomicUsize, Ordering};

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
    ForwardSingleChar,
    ForwardWord,
    BackwardWord,
    ForwardBigword,
    BackwardBigword,
    NextdOrForwardWord,
    PrevdOrBackwardWord,
    HistorySearchBackward,
    HistorySearchForward,
    HistoryPrefixSearchBackward,
    HistoryPrefixSearchForward,
    HistoryPager,
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
    BackwardKillWord,
    BackwardKillPathComponent,
    BackwardKillBigword,
    HistoryTokenSearchBackward,
    HistoryTokenSearchForward,
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
    FuncAnd,
    FuncOr,
    ExpandAbbr,
    DeleteOrExit,
    Exit,
    CancelCommandline,
    Cancel,
    Undo,
    Redo,
    BeginUndoGroup,
    EndUndoGroup,
    RepeatJump,
    DisableMouseTracking,
    // ncurses uses the obvious name
    ClearScreenAndRepaint,
    // NOTE: This one has to be last.
    ReverseRepeatJump,
}

/// Represents an event on the character input stream.
#[derive(Debug, Clone)]
pub enum CharEventType {
    /// A character was entered.
    Char(Chord),

    /// A readline event.
    Readline(ReadlineCmd),

    /// A shell command.
    Command(WString),

    /// A request to change the input mapping mode.
    SetMode(WString),

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
    /// This is also empty for invalid Unicode code points, which produce multiple characters.
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

    pub fn is_readline_or_command(&self) -> bool {
        matches!(
            self.evt,
            CharEventType::Readline(_) | CharEventType::Command(_) | CharEventType::SetMode(_)
        )
    }

    pub fn get_char(&self) -> char {
        let CharEventType::Char(c) = self.evt else {
            panic!("Not a char type");
        };
        if c.codepoint == key::Space {
            ' '
        } else {
            c.codepoint
        }
    }

    pub fn get_chord(&self) -> Option<Chord> {
        match &self.evt {
            CharEventType::Char(c) => Some(*c),
            _ => None,
        }
    }

    pub fn maybe_char(&self) -> Option<char> {
        if let CharEventType::Char(c) = self.evt {
            Some(c.codepoint)
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

    pub fn get_command(&self) -> Option<&wstr> {
        match &self.evt {
            CharEventType::Command(c) => Some(c),
            _ => None,
        }
    }

    pub fn get_mode(&self) -> Option<&wstr> {
        match &self.evt {
            CharEventType::SetMode(m) => Some(m),
            _ => None,
        }
    }

    pub fn from_char(c: char) -> CharEvent {
        CharEvent {
            evt: CharEventType::Char(Chord {
                modifiers: Modifiers::default(),
                codepoint: c,
            }),
            input_style: CharInputStyle::Normal,
            seq: WString::new(),
        }
    }

    pub fn from_chord(c: Chord) -> CharEvent {
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

    pub fn from_command(cmd: WString) -> CharEvent {
        CharEvent {
            evt: CharEventType::Command(cmd),
            input_style: CharInputStyle::Normal,
            seq: WString::new(),
        }
    }

    pub fn from_set_mode(mode: WString) -> CharEvent {
        CharEvent {
            evt: CharEventType::SetMode(mode),
            input_style: CharInputStyle::Normal,
            seq: WString::new(),
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
            FdReadableSet::kNoTimeout
        } else {
            0
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
            // The common path is to return a u8.
            return ReadbResult::Byte(arr[0]);
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

pub fn restore_terminal() {
    let _ = write_to_fd(
        concat!(
            "\x1b[?2004l",
            // "\x1b[?1004l",
            "\x1b[>4;0m",
            "\x1b[<u",
            "\x1b>",
        )
        .as_bytes(),
        STDOUT_FILENO,
    );
}

pub fn setup_terminal() -> impl ScopeGuarding {
    let _ = write_to_fd(
        concat!(
            "\x1b[?2004h",
            // "\x1b[?1004h", // TODO
            "\x1b[>4;1m",
            "\x1b[>5u",
            "\x1b=",
        )
        .as_bytes(),
        STDOUT_FILENO,
    );
    ScopeGuard::new((), |()| restore_terminal())
}

fn control_char(c: char) -> char {
    char::from_u32(u32::from(c) & 0o37).unwrap()
}

fn convert(c: char) -> char {
    if c == control_char('m') || c == control_char('j') {
        return key::Return;
    }
    if c == control_char('i') {
        return key::Tab;
    }
    if c == ' ' {
        return key::Space;
    }
    if c == char::from(TERMINAL_MODE_ON_STARTUP.lock().unwrap().c_cc[VERASE]) {
        return key::Backspace;
    }
    if c == 127 as char {
        // when it's not backspace
        return key::Delete;
    }
    if c == '\x1b' {
        return key::Escape;
    }
    c
}

fn parse_mask(mask: u32) -> Modifiers {
    Modifiers {
        control: (mask & 4) != 0,
        alt: (mask & 2) != 0,
        shift: (mask & 1) != 0,
    }
}

pub fn parse_key(c: u8) -> Option<Chord> {
    let codepoint = convert(char::from(c));
    if u32::from(codepoint) > 255 {
        return Some(Chord {
            modifiers: Modifiers::default(),
            codepoint,
        });
    }
    // Special case: you can type NUL with Ctrl-2 or Ctrl-Shift-2 or
    // Ctrl-Backtick, but the most straightforward way is Ctrl-Space.
    if c == 0 {
        return Some(control(key::Space));
    }
    // Represent Ctrl-letter combinations in lower-case, to be clear
    // that Shift is not involved.
    if c < 27 {
        return Some(control(char::from(c - 1 + b'a')));
    }
    // Represent Ctrl-symbol combinations in "upper-case", as they are
    // traditionally-rendered.
    // Note that Escape is handled elsewhere.
    if c < 32 {
        return Some(control(char::from(c - 1 + b'A')));
    }

    None
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

            let rr = readb(self.get_in_fd(), /*blocking=*/ true);
            match rr {
                ReadbResult::Eof => {
                    return CharEvent::from_eof();
                }

                ReadbResult::Interrupted => {
                    self.select_interrupted();
                }

                ReadbResult::UvarNotified => {
                    self.uvar_change_notified();
                }

                ReadbResult::IOPortNotified => {
                    iothread_service_main(reader_current_data().unwrap());
                }

                ReadbResult::Byte(read_byte) => {
                    let mut have_escape_prefix = false;
                    let mut buffer = vec![read_byte];
                    let chord_with_escape = if read_byte == 0x1b {
                        self.parse_escape_sequence(&mut buffer, &mut have_escape_prefix)
                    } else {
                        parse_key(read_byte)
                    };
                    if self.paste_is_buffering() {
                        if read_byte != 0x1b {
                            self.paste_push_char(read_byte);
                        }
                        continue;
                    }
                    let mut seq = WString::new();
                    let mut chord_event = chord_with_escape.map(CharEvent::from_chord);
                    let mut consumed = 0;
                    for i in 0..buffer.len() {
                        self.parse_codepoint(
                            &mut state,
                            &mut chord_event,
                            &mut seq,
                            &buffer,
                            i,
                            &mut consumed,
                            &mut have_escape_prefix,
                        );
                    }
                    return if let Some(mut chord_event) = chord_event {
                        chord_event.seq = seq;
                        chord_event
                    } else {
                        self.insert_front(seq.chars().skip(1).map(CharEvent::from_char));
                        let Some(c) = seq.chars().next() else {
                            continue;
                        };
                        let mut event = CharEvent::from_char(c);
                        event.seq = seq;
                        event
                    };
                }
                ReadbResult::NothingToRead => unreachable!(),
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
    ) -> Option<Chord> {
        let Some(next) = self.try_readb(buffer) else {
            if !self.paste_is_buffering() {
                return Some(Chord {
                    modifiers: Modifiers::default(),
                    codepoint: key::Escape,
                });
            }
            return None;
        };
        if next == b'[' {
            // potential CSI
            return Some(self.parse_csi(buffer).unwrap_or(alt('[')));
        }
        if next == b'O' {
            // potential SS3
            return Some(self.parse_ss3(buffer).unwrap_or(alt('O')));
        }
        match parse_key(next) {
            Some(mut chord) => {
                chord.modifiers.alt = true;
                Some(chord)
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
        out_chord_event: &mut Option<CharEvent>,
        out_seq: &mut WString,
        buffer: &[u8],
        i: usize,
        consumed: &mut usize,
        have_escape_prefix: &mut bool,
    ) {
        let mut res: char = '\0';
        let read_byte = buffer[i];
        if crate::libc::MB_CUR_MAX() == 1 {
            // single-byte locale, all values are legal
            // FIXME: this looks wrong, this falsely assumes that
            // the single-byte locale is compatible with Unicode upper-ASCII.
            res = read_byte.into();
            out_seq.push(res);
            return;
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
                return;
            }
            -2 => {
                // Sequence not yet complete.
                return;
            }
            0 => {
                // Actual nul char.
                *consumed += 1;
                out_seq.push('\0');
                return;
            }
            _ => (),
        }
        if let Some(res) = char::from_u32(codepoint) {
            // Sequence complete.
            if !fish_reserved_codepoint(res) {
                if *have_escape_prefix && i != 0 {
                    *have_escape_prefix = false;
                    *out_chord_event = Some(CharEvent::from_chord(alt(res)));
                }
                *consumed += 1;
                out_seq.push(res);
                return;
            }
        }
        for &b in &buffer[*consumed..i] {
            out_seq.push(encode_byte_to_char(b));
            *consumed += 1;
        }
    }

    fn parse_csi(&mut self, buffer: &mut Vec<u8>) -> Option<Chord> {
        let mut next_char = |zelf: &mut Self| zelf.try_readb(buffer).unwrap_or(0xff);
        let mut params = [[0_u32; 16]; 4];
        let mut c = next_char(self);
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
                params[count][subcount] = params[count][subcount] * 10 + u32::from(c - b'0');
            } else if c == b':' && subcount < 3 {
                subcount += 1;
            } else if c == b';' {
                count += 1;
                subcount = 0;
            } else {
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
            Chord {
                modifiers,
                codepoint,
            }
        };

        let chord = match c {
            b'$' => match params[0][0] {
                23 | 24 => shift(
                    char::from_u32(u32::from(function_key(11)) + params[0][0] - 23).unwrap(), // rxvt style
                ),
                _ => return None,
            },
            b'A' => masked_key(key::Up, None),
            b'B' => masked_key(key::Down, None),
            b'C' => masked_key(key::Right, None),
            b'D' => masked_key(key::Left, None),
            b'E' => masked_key('5', None),       // Numeric keypad
            b'F' => masked_key(key::End, None),  // PC/xterm style
            b'H' => masked_key(key::Home, None), // PC/xterm style
            b'M' | b'm' => {
                self.disable_mouse_tracking();
                let sgr = private_mode == Some(b'<');
                if !sgr && c == b'm' {
                    return None;
                }
                // Extended (SGR/1006) mouse reporting mode, with semicolon-separated parameters
                // for button code, Px, and Py, ending with 'M' for button press or 'm' for
                // button release.
                if sgr {
                    return None;
                }
                // Generic X10 or modified VT200 sequence. It doesn't matter which, they're both 6
                // chars (although in mode 1005, the characters may be unicode and not necessarily
                // just one byte long) reporting the button that was clicked and its location.
                let _ = next_char(self);
                let _ = next_char(self);
                let _ = next_char(self);
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
                for _ in 0..7 {
                    let _ = next_char(self);
                }
                return None;
            }
            b'P' => masked_key(function_key(1), None),
            b'Q' => masked_key(function_key(2), None),
            b'R' => masked_key(function_key(3), None),
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
                    self.push_front(CharEvent::from_readline(ReadlineCmd::BeginUndoGroup));
                    return Some(Chord::from(key::Invalid));
                }
                201 => {
                    self.push_front(CharEvent::from_readline(ReadlineCmd::EndUndoGroup));
                    self.paste_commit();
                    return Some(Chord::from(key::Invalid));
                }
                _ => return None,
            },
            b'u' => {
                // Treat numpad keys the same as their non-numpad counterparts. Could add a numpad modifier here.
                let key = match params[0][0] {
                    57414 => key::Return,
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
                    cp => convert(char::from_u32(cp).unwrap()),
                };
                masked_key(key, Some(convert(char::from_u32(params[0][1]).unwrap())))
            }
            b'Z' => shift(key::Tab),
            b'I' => shift(key::FocusIn),
            b'O' => shift(key::FocusOut),
            _ => return None,
        };
        Some(chord)
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
        self.push_front(CharEvent::from_readline(ReadlineCmd::DisableMouseTracking));
    }

    fn parse_ss3(&mut self, buffer: &mut Vec<u8>) -> Option<Chord> {
        let mut raw_mask = 0;
        let mut code = b'0';
        loop {
            raw_mask = raw_mask * 10 + u32::from(code - b'0');
            code = self.try_readb(buffer).unwrap_or(0xff);
            if !(b'0'..=b'9').contains(&code) {
                break;
            }
        }
        let modifiers = parse_mask(raw_mask.saturating_sub(1));
        #[rustfmt::skip]
        let chord = match code {
            b' ' => Chord{modifiers, codepoint: key::Space},
            b'A' => Chord{modifiers, codepoint: key::Up},
            b'B' => Chord{modifiers, codepoint: key::Down},
            b'C' => Chord{modifiers, codepoint: key::Right},
            b'D' => Chord{modifiers, codepoint: key::Left},
            b'F' => Chord{modifiers, codepoint: key::End},
            b'H' => Chord{modifiers, codepoint: key::Home},
            b'I' => Chord{modifiers, codepoint: key::Tab},
            b'M' => Chord{modifiers, codepoint: key::Return},
            b'P' => Chord{modifiers, codepoint: function_key(1)},
            b'Q' => Chord{modifiers, codepoint: function_key(2)},
            b'R' => Chord{modifiers, codepoint: function_key(3)},
            b'S' => Chord{modifiers, codepoint: function_key(4)},
            b'X' => Chord{modifiers, codepoint: '='},
            b'j' => Chord{modifiers, codepoint: '*'},
            b'k' => Chord{modifiers, codepoint: '+'},
            b'l' => Chord{modifiers, codepoint: ','},
            b'm' => Chord{modifiers, codepoint: '-'},
            b'n' => Chord{modifiers, codepoint: '.'},
            b'o' => Chord{modifiers, codepoint: '/'},
            b'p' => Chord{modifiers, codepoint: '0'},
            b'q' => Chord{modifiers, codepoint: '1'},
            b'r' => Chord{modifiers, codepoint: '2'},
            b's' => Chord{modifiers, codepoint: '3'},
            b't' => Chord{modifiers, codepoint: '4'},
            b'u' => Chord{modifiers, codepoint: '5'},
            b'v' => Chord{modifiers, codepoint: '6'},
            b'w' => Chord{modifiers, codepoint: '7'},
            b'x' => Chord{modifiers, codepoint: '8'},
            b'y' => Chord{modifiers, codepoint: '9'},
            _ => return None,
        };
        Some(chord)
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

    // Support for "bracketed paste"
    // The way it works is that we acknowledge our support by printing
    // \e\[?2004h
    // then the terminal will "bracket" every paste in
    // \e\[200~ and \e\[201~
    // Every character in between those two will be part of the paste and should not cause a binding to execute (like \n executing commands).
    //
    // We enable it after every command and disable it before (in __fish_config_interactive.fish)
    //
    // Support for this seems to be ubiquitous - emacs enables it unconditionally (!) since 25.1
    // (though it only supports it since then, it seems to be the last term to gain support).
    //
    // NOTE: This is more of a "security" measure than a proper feature.
    // The better way to paste remains the `fish_clipboard_paste` function (bound to \cv by default).
    // We don't disable highlighting here, so it will be redone after every character (which can be slow),
    // and it doesn't handle "paste-stop" sequences in the paste (which the terminal needs to strip).
    //
    // See http://thejh.net/misc/website-terminal-copy-paste.
    fn paste_is_buffering(&self) -> bool;
    fn paste_start_buffering(&mut self);
    fn paste_push_char(&mut self, _b: u8) {}
    fn paste_commit(&mut self);

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
            if evt.is_readline_or_command() {
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
    is_in_bracketed_paste: bool,
}

impl InputEventQueue {
    pub fn new(in_fd: RawFd) -> InputEventQueue {
        InputEventQueue {
            queue: VecDeque::new(),
            in_fd,
            is_in_bracketed_paste: false,
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

    fn paste_is_buffering(&self) -> bool {
        self.is_in_bracketed_paste
    }
    fn paste_start_buffering(&mut self) {
        self.is_in_bracketed_paste = true;
    }
    fn paste_commit(&mut self) {
        self.is_in_bracketed_paste = false;
    }
}
