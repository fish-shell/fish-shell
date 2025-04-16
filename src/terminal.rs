// Generic output functions.
use crate::color::{Color, Color24};
use crate::common::ToCString;
use crate::common::{self, escape_string, wcs2string, wcs2string_appending, EscapeStringStyle};
use crate::future_feature_flags::{self, FeatureFlag};
use crate::global_safety::RelaxedAtomicBool;
use crate::screen::{is_dumb, only_grayscale};
use crate::text_face::{TextFace, TextStyling};
use crate::threads::MainThread;
use crate::wchar::prelude::*;
use crate::FLOGF;
use bitflags::bitflags;
use std::cell::{RefCell, RefMut};
use std::env;
use std::ffi::{CStr, CString};
use std::ops::{Deref, DerefMut};
use std::os::fd::RawFd;
use std::os::unix::ffi::OsStrExt;
use std::path::PathBuf;
use std::sync::atomic::{AtomicU8, Ordering};
use std::sync::Arc;
use std::sync::Mutex;

bitflags! {
    #[derive(Copy, Clone, Default)]
    pub struct ColorSupport: u8 {
        const TERM_256COLOR = 1<<0;
        const TERM_24BIT = 1<<1;
    }
}

/// Whether term256 and term24bit are supported.
static COLOR_SUPPORT: AtomicU8 = AtomicU8::new(0);

pub fn get_color_support() -> ColorSupport {
    let val = COLOR_SUPPORT.load(Ordering::Relaxed);
    ColorSupport::from_bits_truncate(val)
}

pub fn set_color_support(val: ColorSupport) {
    COLOR_SUPPORT.store(val.bits(), Ordering::Relaxed);
}

#[derive(Clone)]
pub(crate) enum TerminalCommand<'a> {
    // Text attributes
    ExitAttributeMode,
    EnterBoldMode,
    EnterDimMode,
    EnterItalicsMode,
    EnterUnderlineMode,
    EnterReverseMode,
    EnterStandoutMode,
    ExitItalicsMode,
    ExitUnderlineMode,

    // Screen clearing
    ClearScreen,
    ClearToEndOfLine,
    ClearToEndOfScreen,

    // Colors
    SelectPaletteColor(/*is_foreground=*/ bool, u8),
    SelectRgbColor(/*is_foreground=*/ bool, Color24),

    // Cursor Movement
    CursorUp,
    CursorDown,
    CursorLeft,
    CursorRight,
    CursorMove(CardinalDirection, usize),

    // Commands related to querying (used for backwards-incompatible features).
    QueryPrimaryDeviceAttribute,
    QueryXtversion,
    QueryXtgettcap(&'static str),

    DecsetAlternateScreenBuffer,
    DecrstAlternateScreenBuffer,
    DecsetSynchronizedUpdate,
    DecrstSynchronizedUpdate,

    QuerySynchronizedOutput,

    // Keyboard protocols
    KittyKeyboardProgressiveEnhancementsEnable,
    KittyKeyboardProgressiveEnhancementsDisable,
    QueryKittyKeyboardProgressiveEnhancements,

    ModifyOtherKeysEnable,
    ModifyOtherKeysDisable,

    ApplicationKeypadModeEnable,
    ApplicationKeypadModeDisable,

    // OSC sequences
    //
    // Note that OSC 7 and OSC 52 are written from fish script, and OSC 8 is written in our
    // man pages (via "man_show_urls").
    Osc0WindowTitle(&'a [WString]),
    Osc133CommandStart(&'a wstr),
    Osc133PromptStart,
    Osc133CommandFinished(libc::c_int),

    // Other terminal features
    QueryCursorPosition,
    ScrollForward(usize),

    DecsetShowCursor,
    DecrstMouseTracking,
    DecsetFocusReporting,
    DecrstFocusReporting,
    DecsetBracketedPaste,
    DecrstBracketedPaste,
}

pub(crate) trait Output {
    fn write_bytes(&mut self, buf: &[u8]);

    fn by_ref(&mut self) -> &mut Self
    where
        Self: Sized,
    {
        self
    }

    fn write_command(&mut self, cmd: TerminalCommand<'_>) -> bool
    where
        Self: Sized,
    {
        use TerminalCommand::*;
        if is_dumb() {
            assert!(!matches!(cmd, CursorDown));
            return false;
        }
        let ti = maybe_terminfo;
        fn write(out: &mut impl Output, sequence: &'static [u8]) -> bool {
            out.write_bytes(sequence);
            true
        }
        match cmd {
            ExitAttributeMode => ti(self, b"\x1b[m", |t| &t.exit_attribute_mode),
            EnterBoldMode => ti(self, b"\x1b[1m", |t| &t.enter_bold_mode),
            EnterDimMode => ti(self, b"\x1b[2m", |t| &t.enter_dim_mode),
            EnterItalicsMode => ti(self, b"\x1b[3m", |t| &t.enter_italics_mode),
            EnterUnderlineMode => ti(self, b"\x1b[4m", |t| &t.enter_underline_mode),
            EnterReverseMode => ti(self, b"\x1b[7m", |t| &t.enter_reverse_mode),
            EnterStandoutMode => ti(self, b"\x1b[7m", |t| &t.enter_standout_mode),
            ExitItalicsMode => ti(self, b"\x1b[23m", |t| &t.exit_italics_mode),
            ExitUnderlineMode => ti(self, b"\x1b[24m", |t| &t.exit_underline_mode),
            ClearScreen => ti(self, b"\x1b[H\x1b[2J", |term| &term.clear_screen),
            ClearToEndOfLine => ti(self, b"\x1b[K", |term| &term.clr_eol),
            ClearToEndOfScreen => ti(self, b"\x1b[J", |term| &term.clr_eos),
            SelectPaletteColor(is_foreground, idx) => palette_color(self, is_foreground, idx),
            SelectRgbColor(is_foreground, rgb) => rgb_color(self, is_foreground, rgb),
            CursorUp => ti(self, b"\x1b[A", |term| &term.cursor_up),
            CursorDown => ti(self, b"\n", |term| &term.cursor_down),
            CursorLeft => ti(self, b"\x08", |term| &term.cursor_left),
            CursorRight => ti(self, b"\x1b[C", |term| &term.cursor_right),
            CursorMove(direction, steps) => cursor_move(self, direction, steps),
            QueryPrimaryDeviceAttribute => write(self, b"\x1b[0c"),
            QueryXtversion => write(self, b"\x1b[>0q"),
            QueryXtgettcap(cap) => query_xtgettcap(self, cap),
            DecsetAlternateScreenBuffer => write(self, b"\x1b[?1049h"),
            DecrstAlternateScreenBuffer => write(self, b"\x1b[?1049l"),
            DecsetSynchronizedUpdate => write(self, b"\x1b[?2026h"),
            DecrstSynchronizedUpdate => write(self, b"\x1b[?2026l"),
            QuerySynchronizedOutput => write(self, b"\x1b[?2026$p"),
            KittyKeyboardProgressiveEnhancementsEnable => write(self, b"\x1b[=5u"),
            KittyKeyboardProgressiveEnhancementsDisable => write(self, b"\x1b[=0u"),
            QueryKittyKeyboardProgressiveEnhancements => query_kitty_progressive_enhancements(self),
            ModifyOtherKeysEnable => write(self, b"\x1b[>4;1m"),
            ModifyOtherKeysDisable => write(self, b"\x1b[>4;0m"),
            ApplicationKeypadModeEnable => write(self, b"\x1b="),
            ApplicationKeypadModeDisable => write(self, b"\x1b>"),
            Osc0WindowTitle(title) => osc_0_window_title(self, title),
            Osc133PromptStart => write(self, OSC_133_PROMPT_START),
            Osc133CommandStart(command) => osc_133_command_start(self, command),
            Osc133CommandFinished(s) => osc_133_command_finished(self, s),
            QueryCursorPosition => write(self, b"\x1b[6n"),
            ScrollForward(lines) => scroll_forward(self, lines),
            DecsetShowCursor => write(self, b"\x1b[?25h"),
            DecrstMouseTracking => write(self, b"\x1b[?1000l"),
            DecsetFocusReporting => write(self, b"\x1b[?1004h"),
            DecrstFocusReporting => write(self, b"\x1b[?1004l"),
            DecsetBracketedPaste => write(self, b"\x1b[?2004h"),
            DecrstBracketedPaste => write(self, b"\x1b[?2004l"),
        }
    }
}

impl Output for Vec<u8> {
    fn write_bytes(&mut self, buf: &[u8]) {
        self.extend_from_slice(buf);
    }
}

fn maybe_terminfo(
    out: &mut impl Output,
    sequence: &'static [u8],
    terminfo: fn(&Term) -> &Option<CString>,
) -> bool {
    if use_terminfo() {
        let term = crate::terminal::term();
        let Some(sequence) = (terminfo)(&term) else {
            return false;
        };
        out.write_bytes(sequence.to_bytes());
    } else {
        out.write_bytes(sequence);
    }
    true
}

#[repr(u8)]
pub(crate) enum Capability {
    Unknown,
    Supported,
    NotSupported,
}

pub(crate) static KITTY_KEYBOARD_SUPPORTED: AtomicU8 = AtomicU8::new(Capability::Unknown as _);

pub(crate) static SCROLL_FORWARD_SUPPORTED: RelaxedAtomicBool = RelaxedAtomicBool::new(false);
pub(crate) static SCROLL_FORWARD_TERMINFO_CODE: &str = "indn";

pub(crate) static SYNCHRONIZED_OUTPUT_SUPPORTED: RelaxedAtomicBool = RelaxedAtomicBool::new(false);

pub(crate) fn use_terminfo() -> bool {
    !future_feature_flags::test(FeatureFlag::ignore_terminfo) && TERM.lock().unwrap().is_some()
}

fn palette_color(out: &mut impl Output, foreground: bool, mut idx: u8) -> bool {
    if only_grayscale() && !(Color::Named { idx }).is_grayscale() {
        return false;
    }
    if use_terminfo() {
        let term = crate::terminal::term();
        let Some(command) = (if foreground {
            term.set_a_foreground
                .as_ref()
                .or(term.set_foreground.as_ref())
        } else {
            term.set_a_background
                .as_ref()
                .or(term.set_background.as_ref())
        }) else {
            return false;
        };
        if term_supports_color_natively(&term, idx) {
            let Some(sequence) = tparm1(command, idx.into()) else {
                return false;
            };
            out.write_bytes(sequence.as_bytes());
            return true;
        }
        if term.max_colors == Some(8) && idx > 8 {
            idx -= 8;
        }
    }
    let bg = if foreground { 0 } else { 10 };
    match idx {
        0..=7 => write_to_output!(out, "\x1b[{}m", 30 + bg + idx),
        8..=15 => write_to_output!(out, "\x1b[{}m", 90 + bg + (idx - 8)),
        _ => write_to_output!(out, "\x1b[{};5;{}m", 38 + bg, idx),
    };
    true
}

/// Returns true if we think tparm can handle outputting a color index.
fn term_supports_color_natively(term: &Term, c: u8) -> bool {
    #[allow(clippy::int_plus_one)]
    if let Some(max_colors) = term.max_colors {
        max_colors >= usize::from(c) + 1
    } else {
        false
    }
}

fn rgb_color(out: &mut impl Output, foreground: bool, rgb: Color24) -> bool {
    // Foreground: ^[38;2;<r>;<g>;<b>m
    // Background: ^[48;2;<r>;<g>;<b>m
    write_to_output!(
        out,
        "\x1b[{};2;{};{};{}m",
        if foreground { 38 } else { 48 },
        rgb.r,
        rgb.g,
        rgb.b
    );
    true
}

#[derive(Clone)]
pub(crate) enum CardinalDirection {
    Up,
    Left,
    Right,
}

fn cursor_move(out: &mut impl Output, direction: CardinalDirection, steps: usize) -> bool {
    if use_terminfo() {
        let term = crate::terminal::term();
        if let Some(command) = match direction {
            CardinalDirection::Up => None, // Historical
            CardinalDirection::Left => term.parm_left_cursor.as_ref(),
            CardinalDirection::Right => term.parm_right_cursor.as_ref(),
        } {
            if let Some(sequence) = tparm1(command, i32::try_from(steps).unwrap()) {
                out.write_bytes(sequence.as_bytes());
                return true;
            }
        } else if let Some(command) = match direction {
            CardinalDirection::Up => term.cursor_up.as_ref(),
            CardinalDirection::Left => term.cursor_left.as_ref(),
            CardinalDirection::Right => term.cursor_right.as_ref(),
        } {
            for _i in 0..steps {
                out.write_bytes(command.as_bytes());
            }
            return true;
        }
        return false;
    }
    write_to_output!(
        out,
        "\x1b[{steps}{}",
        match direction {
            CardinalDirection::Up => 'A',
            CardinalDirection::Left => 'D',
            CardinalDirection::Right => 'C',
        }
    );
    true
}

fn query_xtgettcap(out: &mut impl Output, cap: &str) -> bool {
    write_to_output!(out, "\x1bP+q{}\x1b\\", DisplayAsHex(cap));
    true
}

struct DisplayAsHex<'a>(&'a str);

impl<'a> std::fmt::Display for DisplayAsHex<'a> {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        for byte in self.0.bytes() {
            write!(f, "{:x}", byte)?;
        }
        Ok(())
    }
}

fn query_kitty_progressive_enhancements(out: &mut impl Output) -> bool {
    if std::env::var_os("TERM").is_some_and(|term| term.as_os_str().as_bytes() == b"st-256color") {
        return false;
    }
    out.write_bytes(b"\x1b[?u");
    true
}

fn osc_0_window_title(out: &mut impl Output, title: &[WString]) -> bool {
    out.write_bytes(b"\x1b]0;");
    for title_line in title {
        out.write_bytes(&wcs2string(title_line));
    }
    out.write_bytes(b"\x07"); // BEL
    true
}

const OSC_133_PROMPT_START: &[u8] = b"\x1b]133;A;click_events=1\x07";

fn osc_133_command_start(out: &mut impl Output, command: &wstr) -> bool {
    write_to_output!(
        out,
        "\x1b]133;C;cmdline_url={}\x07",
        escape_string(command, EscapeStringStyle::Url),
    );
    true
}

fn osc_133_command_finished(out: &mut impl Output, exit_status: libc::c_int) -> bool {
    write_to_output!(out, "\x1b]133;D;{}\x07", exit_status);
    true
}

fn scroll_forward(out: &mut impl Output, lines: usize) -> bool {
    assert!(SCROLL_FORWARD_SUPPORTED.load());
    write_to_output!(out, "\x1b[{}S", lines);
    true
}

fn index_for_color(c: Color) -> u8 {
    if c.is_named() || !(get_color_support().contains(ColorSupport::TERM_256COLOR)) {
        return c.to_name_index();
    }
    c.to_term256_index()
}

fn write_foreground_color(outp: &mut Outputter, idx: u8) -> bool {
    outp.write_command(TerminalCommand::SelectPaletteColor(true, idx))
}

fn write_background_color(outp: &mut Outputter, idx: u8) -> bool {
    outp.write_command(TerminalCommand::SelectPaletteColor(false, idx))
}

pub struct Outputter {
    /// Storage for buffered contents.
    contents: Vec<u8>,

    /// Count of how many outstanding begin_buffering() calls there are.
    buffer_count: u32,

    /// fd to output to, or -1 for none.
    fd: RawFd,

    /// Assume this corresponds to the current state of the terminal.
    /// This assumption holds if we start from the default state. Currently, builtin set_color
    /// should be the only exception.
    last: TextFace,
}

impl Outputter {
    /// Construct an outputter which outputs to a given fd.
    /// If the fd is negative, the outputter will buffer its output.
    const fn new_from_fd(fd: RawFd) -> Self {
        Self {
            contents: Vec::new(),
            buffer_count: 0,
            fd,
            last: TextFace::default(),
        }
    }

    /// Construct an outputter which outputs to its string buffer.
    pub fn new_buffering() -> Self {
        Self::new_from_fd(-1)
    }

    fn maybe_flush(&mut self) {
        if self.fd >= 0 && self.buffer_count == 0 {
            self.flush_to(self.fd);
        }
    }

    /// Unconditionally write the color string to the output.
    /// Exported for builtin_set_color's usage only.
    pub fn write_color(&mut self, color: Color, is_fg: bool) -> bool {
        let supports_term24bit = get_color_support().contains(ColorSupport::TERM_24BIT);
        if !supports_term24bit || !color.is_rgb() {
            // Indexed or non-24 bit color.
            let idx = index_for_color(color);
            if is_fg {
                return write_foreground_color(self, idx);
            } else {
                return write_background_color(self, idx);
            };
        }

        if only_grayscale() && color.is_grayscale() {
            return false;
        }

        // 24 bit!
        self.write_command(TerminalCommand::SelectRgbColor(is_fg, color.to_color24()))
    }

    /// Unconditionally resets colors and text style.
    pub(crate) fn reset_text_face(&mut self, weird_workaround: bool) {
        if weird_workaround {
            // If we exit attribute mode, we must first set a color, or previously colored text might
            // lose its color. Terminals are weird...
            write_foreground_color(self, 0);
        }
        use TerminalCommand::ExitAttributeMode;
        self.write_command(ExitAttributeMode);
        self.last = TextFace::default();
    }

    /// Sets the fg and bg color. May be called as often as you like, since if the new color is the same
    /// as the previous, nothing will be written. Negative values for set_color will also be ignored.
    /// Since the command this function emits can potentially cause the screen to flicker, the
    /// function takes care to write as little as possible.
    ///
    /// Possible values for colors are RgbColor colors or special values like RgbColor::NORMAL
    ///
    /// In order to set the color to normal, three commands may have to be written.
    ///
    /// - First a command to set the color, such as set_a_foreground. This is needed because otherwise
    /// the previous strings colors might be removed as well.
    ///
    /// - After that we write the exit_attribute_mode command to reset all color attributes.
    ///
    /// - Lastly we may need to write set_a_background or set_a_foreground to set the other half of the
    /// color pair to what it should be.
    #[allow(clippy::if_same_then_else)]
    pub(crate) fn set_text_face(&mut self, face: TextFace) {
        let mut fg = face.fg;
        let bg = face.bg;
        let style = face.style;
        let mut bg_set = false;
        let mut last_bg_set = false;
        let is_bold = face.is_bold();
        let is_underline = face.is_underline();
        let is_italics = face.is_italics();
        let is_dim = face.is_dim();
        let is_reverse = face.is_reverse();

        if fg.is_reset() || bg.is_reset() {
            self.reset_text_face(true);
            return;
        }
        use TerminalCommand::{
            EnterBoldMode, EnterDimMode, EnterItalicsMode, EnterReverseMode, EnterStandoutMode,
            EnterUnderlineMode, ExitAttributeMode, ExitItalicsMode, ExitUnderlineMode,
        };
        let non_resettable = |style| style & !(TextStyling::ITALICS | TextStyling::UNDERLINE);
        let non_resettable_attributes_to_unset =
            non_resettable(self.last.style).difference(non_resettable(style));
        if !non_resettable_attributes_to_unset.is_empty() {
            // Only way to exit non-resettable ones is a reset of all attributes.
            self.reset_text_face(false);
        }
        if !self.last.bg.is_special() {
            // Background was set.
            // "Special" here refers to the special "normal", "reset" and "none" colors,
            // that really just disable the background.
            last_bg_set = true;
        }
        if !bg.is_special() {
            // Background is set.
            bg_set = true;
            if fg == bg {
                fg = if bg == Color::WHITE {
                    Color::BLACK
                } else {
                    Color::WHITE
                };
            }
        }

        if bg_set && !last_bg_set {
            // Background color changed and is set, so we enter bold mode to make reading easier.
            // This means bold mode is _always_ on when the background color is set.
            self.write_command(EnterBoldMode);
        }
        if !bg_set && last_bg_set {
            // Background color changed and is no longer set, so we exit bold mode.
            self.reset_text_face(false);
        }

        if !fg.is_none() && self.last.fg != fg {
            if fg.is_normal() {
                write_foreground_color(self, 0);
                self.write_command(ExitAttributeMode);

                self.last.bg = Color::Normal;
                self.last.style = TextStyling::empty();
            } else {
                assert!(!fg.is_special());
                self.write_color(fg, true /* foreground */);
            }
            self.last.fg = fg;
        }

        if !bg.is_none() && self.last.bg != bg {
            if bg.is_normal() {
                write_background_color(self, 0);

                self.write_command(ExitAttributeMode);
                if !self.last.fg.is_normal() {
                    self.write_color(self.last.fg, true /* foreground */);
                }
                self.last.style = TextStyling::empty();
            } else {
                assert!(!bg.is_special());
                self.write_color(bg, false /* not foreground */);
            }
            self.last.bg = bg;
        }

        // Lastly, we set bold, underline, italics, dim, and reverse modes correctly.
        if is_bold && !self.last.is_bold() && !bg_set && self.write_command(EnterBoldMode) {
            self.last.style.set(TextStyling::BOLD, is_bold);
        }

        if !self.last.is_underline() && is_underline && self.write_command(EnterUnderlineMode) {
            self.last.style.set(TextStyling::UNDERLINE, is_underline);
        } else if self.last.is_underline() && !is_underline && self.write_command(ExitUnderlineMode)
        {
            self.last.style.set(TextStyling::UNDERLINE, is_underline);
        }

        if self.last.is_italics() && !is_italics && self.write_command(ExitItalicsMode) {
            self.last.style.set(TextStyling::ITALICS, is_italics);
        } else if !self.last.is_italics() && is_italics && self.write_command(EnterItalicsMode) {
            self.last.style.set(TextStyling::ITALICS, is_italics);
        }

        if is_dim && !self.last.is_dim() && self.write_command(EnterDimMode) {
            self.last.style.set(TextStyling::DIM, is_dim);
        }

        if is_reverse && !self.last.is_reverse() {
            if self.write_command(EnterReverseMode) || self.write_command(EnterStandoutMode) {
                self.last.style.set(TextStyling::REVERSE, is_reverse);
            }
        }
    }

    /// Write a wide character to the receiver.
    pub fn writech(&mut self, ch: char) {
        self.write_wstr(wstr::from_char_slice(&[ch]));
    }

    /// Write a narrow character to the receiver.
    pub fn push(&mut self, ch: u8) {
        self.contents.push(ch);
        self.maybe_flush();
    }

    /// Write a wide string.
    pub fn write_wstr(&mut self, str: &wstr) {
        wcs2string_appending(&mut self.contents, str);
        self.maybe_flush();
    }

    /// Return the "output" contents.
    pub fn contents(&self) -> &[u8] {
        &self.contents
    }

    /// Output any buffered data to the given `fd`.
    fn flush_to(&mut self, fd: RawFd) {
        if fd >= 0 && !self.contents.is_empty() {
            let _ = common::write_loop(&fd, &self.contents);
            self.contents.clear();
        }
    }

    /// Begins buffering. Output will not be automatically flushed until a corresponding
    /// end_buffering() call.
    pub fn begin_buffering(&mut self) {
        self.buffer_count += 1;
        assert!(self.buffer_count > 0, "buffer_count overflow");
    }

    /// Balance a begin_buffering() call.
    pub fn end_buffering(&mut self) {
        assert!(self.buffer_count > 0, "buffer_count underflow");
        self.buffer_count -= 1;
        self.maybe_flush();
    }
}

impl Output for Outputter {
    fn write_bytes(&mut self, buf: &[u8]) {
        self.contents.extend_from_slice(buf);
        self.maybe_flush();
    }
}

impl Outputter {
    /// Access the outputter for stdout.
    /// This should only be used from the main thread.
    pub fn stdoutput() -> &'static RefCell<Outputter> {
        static STDOUTPUT: MainThread<RefCell<Outputter>> =
            MainThread::new(RefCell::new(Outputter::new_from_fd(libc::STDOUT_FILENO)));
        STDOUTPUT.get()
    }
}

pub struct BufferedOutputter<'a>(RefMut<'a, Outputter>);

impl<'a> BufferedOutputter<'a> {
    pub fn new(outputter: &'a RefCell<Outputter>) -> Self {
        let mut outputter = outputter.borrow_mut();
        outputter.begin_buffering();
        Self(outputter)
    }
}

impl<'a> Drop for BufferedOutputter<'a> {
    fn drop(&mut self) {
        self.0.end_buffering();
    }
}

impl Deref for BufferedOutputter<'_> {
    type Target = Outputter;

    fn deref(&self) -> &Self::Target {
        &self.0
    }
}

impl DerefMut for BufferedOutputter<'_> {
    fn deref_mut(&mut self) -> &mut Self::Target {
        &mut self.0
    }
}

impl<'a> Output for BufferedOutputter<'a> {
    fn write_bytes(&mut self, buf: &[u8]) {
        self.0.write_bytes(buf);
    }
}

/// Given a list of RgbColor, pick the "best" one, as determined by the color support. Returns
/// RgbColor::NONE if empty.
pub fn best_color(candidates: impl Iterator<Item = Color>, support: ColorSupport) -> Option<Color> {
    let mut first = None;
    let mut first_rgb = None;
    let mut first_named = None;
    for color in candidates {
        if first.is_none() {
            first = Some(color);
        }
        if first_rgb.is_none() && color.is_rgb() {
            first_rgb = Some(color);
        }
        if first_named.is_none() && color.is_named() {
            first_named = Some(color);
        }
    }

    // If we have both RGB and named colors, then prefer rgb if term256 is supported.
    let has_term256 = support.contains(ColorSupport::TERM_256COLOR);
    (if (first_rgb.is_some() && has_term256) || first_named.is_none() {
        first_rgb
    } else {
        first_named
    })
    .or(first)
}

/// The [`Term`] singleton. Initialized via a call to [`setup()`] and surfaced to the outside world via [`term()`].
///
/// We can't just use an [`AtomicPtr<Arc<Term>>`](std::sync::atomic::AtomicPtr) here because there's a race condition when the old Arc
/// gets dropped - we would obtain the current (non-null) value of `TERM` in [`term()`] but there's
/// no guarantee that a simultaneous call to [`setup()`] won't result in this refcount being
/// decremented to zero and the memory being reclaimed before we can clone it, since we can only
/// atomically *read* the value of the pointer, not clone the `Arc` it points to.
pub static TERM: Mutex<Option<Arc<Term>>> = Mutex::new(None);

/// Returns a reference to the global [`Term`] singleton or `None` if not preceded by a successful
/// call to [`terminal::setup()`](setup).
pub fn term() -> Arc<Term> {
    Arc::clone(TERM.lock().expect("Mutex poisoned!").as_ref().unwrap())
}

/// The safe wrapper around terminfo functionality, initialized by a successful call to [`setup()`]
/// and obtained thereafter by calls to [`term()`].
#[derive(Default)]
pub struct Term {
    // String capabilities. Any Some value is confirmed non-empty.
    pub enter_bold_mode: Option<CString>,
    pub enter_italics_mode: Option<CString>,
    pub exit_italics_mode: Option<CString>,
    pub enter_dim_mode: Option<CString>,
    pub enter_underline_mode: Option<CString>,
    pub exit_underline_mode: Option<CString>,
    pub enter_reverse_mode: Option<CString>,
    pub enter_standout_mode: Option<CString>,
    pub set_a_foreground: Option<CString>,
    pub set_foreground: Option<CString>,
    pub set_a_background: Option<CString>,
    pub set_background: Option<CString>,
    pub exit_attribute_mode: Option<CString>,
    pub clear_screen: Option<CString>,
    pub cursor_up: Option<CString>,
    pub cursor_down: Option<CString>,
    pub cursor_left: Option<CString>,
    pub cursor_right: Option<CString>,
    pub parm_left_cursor: Option<CString>,
    pub parm_right_cursor: Option<CString>,
    pub clr_eol: Option<CString>,
    pub clr_eos: Option<CString>,

    // Number capabilities
    pub max_colors: Option<usize>,
    pub init_tabs: Option<usize>,

    // Flag/boolean capabilities
    pub eat_newline_glitch: bool,
    pub auto_right_margin: bool,
}

impl Term {
    /// Initialize a new `Term` instance, prepopulating the values of all the terminfo string
    /// capabilities we care about in the process.
    fn new(db: terminfo::Database) -> Self {
        Term {
            // String capabilities
            enter_bold_mode: get_str_cap(&db, "md"),
            enter_italics_mode: get_str_cap(&db, "ZH"),
            exit_italics_mode: get_str_cap(&db, "ZR"),
            enter_dim_mode: get_str_cap(&db, "mh"),
            enter_underline_mode: get_str_cap(&db, "us"),
            exit_underline_mode: get_str_cap(&db, "ue"),
            enter_reverse_mode: get_str_cap(&db, "mr"),
            enter_standout_mode: get_str_cap(&db, "so"),
            set_a_foreground: get_str_cap(&db, "AF"),
            set_foreground: get_str_cap(&db, "Sf"),
            set_a_background: get_str_cap(&db, "AB"),
            set_background: get_str_cap(&db, "Sb"),
            exit_attribute_mode: get_str_cap(&db, "me"),
            clear_screen: get_str_cap(&db, "cl"),
            cursor_up: get_str_cap(&db, "up"),
            cursor_down: get_str_cap(&db, "do"),
            cursor_left: get_str_cap(&db, "le"),
            cursor_right: get_str_cap(&db, "nd"),
            parm_left_cursor: get_str_cap(&db, "LE"),
            parm_right_cursor: get_str_cap(&db, "RI"),
            clr_eol: get_str_cap(&db, "ce"),
            clr_eos: get_str_cap(&db, "cd"),

            // Number capabilities
            max_colors: get_num_cap(&db, "Co"),
            init_tabs: get_num_cap(&db, "it"),

            // Flag/boolean capabilities
            eat_newline_glitch: get_flag_cap(&db, "xn"),
            auto_right_margin: get_flag_cap(&db, "am"),
        }
    }
}

/// Initializes our database from $TERM.
/// Returns a reference to the newly initialized [`Term`] singleton on success or `None` if this failed.
///
/// Any existing references from `terminal::term()` will be invalidated by this call!
pub fn setup() {
    let mut global_term = TERM.lock().expect("Mutex poisoned!");

    let res = terminfo::Database::from_env().or_else(|x| {
        // Try some more paths
        let t = if let Ok(name) = env::var("TERM") {
            name
        } else {
            return Err(x);
        };
        let first_char = t.chars().next().unwrap().to_string();
        for dir in [
            "/run/current-system/sw/share/terminfo", // Nix
            "/usr/pkg/share/terminfo",               // NetBSD
        ] {
            let mut path = PathBuf::from(dir);
            path.push(first_char.clone());
            path.push(t.clone());
            FLOGF!(term_support, "Trying path '%ls'", path.to_str().unwrap());
            if let Ok(db) = terminfo::Database::from_path(path) {
                return Ok(db);
            }
        }
        Err(x)
    });

    // Safely store the new Term instance or replace the old one. We have the lock so it's safe to
    // drop the old TERM value and have its refcount decremented - no one will be cloning it.
    if let Ok(result) = res {
        // Create a new `Term` instance, prepopulate the capabilities we care about.
        let term = Arc::new(Term::new(result));
        *global_term = Some(term.clone());
    } else {
        *global_term = None;
    }
}

/// Return a nonempty String capability from termcap, or None if missing or empty.
/// Panics if the given code string does not contain exactly two bytes.
fn get_str_cap(db: &terminfo::Database, code: &str) -> Option<CString> {
    db.raw(code).map(|cap| match cap {
        terminfo::Value::True => "1".to_string().as_bytes().to_cstring(),
        terminfo::Value::Number(n) => n.to_string().as_bytes().to_cstring(),
        terminfo::Value::String(s) => s.clone().to_cstring(),
    })
}

/// Return a number capability from termcap, or None if missing.
/// Panics if the given code string does not contain exactly two bytes.
fn get_num_cap(db: &terminfo::Database, code: &str) -> Option<usize> {
    match db.raw(code) {
        Some(terminfo::Value::Number(n)) if *n >= 0 => Some(*n as usize),
        _ => None,
    }
}

/// Return a flag capability from termcap, or false if missing.
/// Panics if the given code string does not contain exactly two bytes.
fn get_flag_cap(db: &terminfo::Database, code: &str) -> bool {
    db.raw(code)
        .map(|cap| matches!(cap, terminfo::Value::True))
        .unwrap_or(false)
}

/// Covers over tparm() with one parameter.
pub fn tparm1(cap: &CStr, param1: i32) -> Option<CString> {
    assert!(!cap.to_bytes().is_empty());
    let cap = cap.to_bytes();
    terminfo::expand!(cap; param1).ok().map(|x| x.to_cstring())
}
