// Generic output functions.
use crate::common::ToCString;
use crate::common::{self, EscapeStringStyle, escape_string, wcs2bytes, wcs2bytes_appending};
use crate::flogf;
use crate::future_feature_flags::{self, FeatureFlag};
use crate::prelude::*;
use crate::screen::{is_dumb, only_grayscale};
use crate::text_face::{TextFace, TextStyling, UnderlineStyle};
use crate::threads::MainThread;
use bitflags::bitflags;
use fish_color::{Color, Color24};
use std::cell::{RefCell, RefMut};
use std::env;
use std::ffi::{CStr, CString};
use std::ops::{Deref, DerefMut};
use std::os::fd::RawFd;
use std::os::unix::ffi::OsStrExt;
use std::path::PathBuf;
use std::sync::atomic::{AtomicU8, Ordering};
use std::sync::{Arc, Mutex, OnceLock};

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

#[derive(Clone, Copy)]
pub(crate) enum Paintable {
    Foreground,
    Background,
    Underline,
}

#[derive(Clone)]
pub(crate) enum TerminalCommand<'a> {
    // Text attributes
    ExitAttributeMode,
    EnterBoldMode,
    EnterDimMode,
    EnterItalicsMode,
    EnterUnderlineMode(UnderlineStyle),
    EnterReverseMode,
    EnterStandoutMode,
    ExitItalicsMode,
    ExitUnderlineMode,

    // Screen clearing
    ClearScreen,
    ClearToEndOfLine,
    ClearToEndOfScreen,

    // Colors
    SelectPaletteColor(Paintable, u8),
    SelectRgbColor(Paintable, Color24),
    DefaultBackgroundColor,
    DefaultUnderlineColor,

    // Cursor Movement
    CursorUp,
    CursorDown,
    CursorLeft,
    CursorRight,
    CursorMove(CardinalDirection, usize),

    // Commands related to querying (used mainly for backwards-incompatible features).
    QueryPrimaryDeviceAttribute,
    QueryXtversion,
    QueryXtgettcap(&'static str),

    DecsetAlternateScreenBuffer,
    DecrstAlternateScreenBuffer,

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
    Osc1TabTitle(&'a [WString]),
    Osc133PromptStart,
    Osc133PromptEnd,
    Osc133CommandStart(&'a wstr),
    Osc133CommandFinished(libc::c_int),

    // Other terminal features
    QueryCursorPosition,
    QueryBackgroundColor,
    ScrollContentUp(usize),

    DecsetShowCursor,
    DecsetFocusReporting,
    DecrstFocusReporting,
    DecsetBracketedPaste,
    DecrstBracketedPaste,
    DecsetColorThemeReporting,
    DecrstColorThemeReporting,
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
            EnterUnderlineMode(style) => underline_mode(self, style),
            EnterReverseMode => ti(self, b"\x1b[7m", |t| &t.enter_reverse_mode),
            EnterStandoutMode => ti(self, b"\x1b[7m", |t| &t.enter_standout_mode),
            ExitItalicsMode => ti(self, b"\x1b[23m", |t| &t.exit_italics_mode),
            ExitUnderlineMode => ti(self, b"\x1b[24m", |t| &t.exit_underline_mode),
            ClearScreen => ti(self, b"\x1b[H\x1b[2J", |term| &term.clear_screen),
            ClearToEndOfLine => ti(self, b"\x1b[K", |term| &term.clr_eol),
            ClearToEndOfScreen => ti(self, b"\x1b[J", |term| &term.clr_eos),
            SelectPaletteColor(paintable, idx) => palette_color(self, paintable, idx),
            SelectRgbColor(paintable, rgb) => rgb_color(self, paintable, rgb),
            DefaultBackgroundColor => write(self, b"\x1b[49m"),
            DefaultUnderlineColor => write(self, b"\x1b[59m"),
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
            KittyKeyboardProgressiveEnhancementsEnable => write(self, b"\x1b[=5u"),
            KittyKeyboardProgressiveEnhancementsDisable => write(self, b"\x1b[=0u"),
            QueryKittyKeyboardProgressiveEnhancements => query_kitty_progressive_enhancements(self),
            ModifyOtherKeysEnable => write(self, b"\x1b[>4;1m"),
            ModifyOtherKeysDisable => write(self, b"\x1b[>4;0m"),
            ApplicationKeypadModeEnable => write(self, b"\x1b="),
            ApplicationKeypadModeDisable => write(self, b"\x1b>"),
            Osc0WindowTitle(title) => osc_0_or_1_terminal_title(self, false, title),
            Osc1TabTitle(title) => osc_0_or_1_terminal_title(self, true, title),
            Osc133PromptStart => osc_133_prompt_start(self),
            Osc133PromptEnd => osc_133_prompt_end(self),
            Osc133CommandStart(command) => osc_133_command_start(self, command),
            Osc133CommandFinished(s) => osc_133_command_finished(self, s),
            QueryCursorPosition => write(self, b"\x1b[6n"),
            QueryBackgroundColor => write(self, b"\x1b]11;?\x1b\\"),
            ScrollContentUp(lines) => scroll_content_up(self, lines),
            DecsetShowCursor => write(self, b"\x1b[?25h"),
            DecsetFocusReporting => write(self, b"\x1b[?1004h"),
            DecrstFocusReporting => write(self, b"\x1b[?1004l"),
            DecsetBracketedPaste => write(self, b"\x1b[?2004h"),
            DecrstBracketedPaste => write(self, b"\x1b[?2004l"),
            DecsetColorThemeReporting => write(self, b"\x1b[?2031h"),
            DecrstColorThemeReporting => write(self, b"\x1b[?2031l"),
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

pub(crate) fn use_terminfo() -> bool {
    !future_feature_flags::test(FeatureFlag::IgnoreTerminfo) && TERM.lock().unwrap().is_some()
}

fn underline_mode(out: &mut impl Output, style: UnderlineStyle) -> bool {
    use UnderlineStyle as UL;
    let style = match style {
        UL::Single => return maybe_terminfo(out, b"\x1b[4m", |t| &t.enter_underline_mode),
        UL::Double => 2,
        UL::Curly => 3,
        UL::Dotted => 4,
        UL::Dashed => 5,
    };
    write_to_output!(out, "\x1b[4:{}m", style);
    true
}

fn palette_color(out: &mut impl Output, paintable: Paintable, mut idx: u8) -> bool {
    if only_grayscale() && !(Color::Named { idx }).is_grayscale() {
        return false;
    }
    if use_terminfo() {
        let term = crate::terminal::term();
        let Some(command) = (match paintable {
            Paintable::Foreground => term
                .set_a_foreground
                .as_ref()
                .or(term.set_foreground.as_ref()),
            Paintable::Background => term
                .set_a_background
                .as_ref()
                .or(term.set_background.as_ref()),
            Paintable::Underline => None,
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
    let bg = match paintable {
        Paintable::Foreground => 0,
        Paintable::Background => 10,
        Paintable::Underline => {
            write_to_output!(out, "\x1b[58:5:{}m", idx);
            return true;
        }
    };
    match idx {
        0..=7 => write_to_output!(out, "\x1b[{}m", 30 + bg + idx),
        8..=15 => write_to_output!(out, "\x1b[{}m", 90 + bg + (idx - 8)),
        _ => write_to_output!(out, "\x1b[{};5;{}m", 38 + bg, idx),
    }
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

fn rgb_color(out: &mut impl Output, paintable: Paintable, rgb: Color24) -> bool {
    // Foreground: ^[38;2;<r>;<g>;<b>m
    // Background: ^[48;2;<r>;<g>;<b>m
    // Underline: ^[58:2::<r>:<g>:<b>m
    let code = match paintable {
        Paintable::Foreground => 38,
        Paintable::Background => 48,
        Paintable::Underline => {
            write_to_output!(out, "\x1b[58:2::{}:{}:{}m", rgb.r, rgb.g, rgb.b);
            return true;
        }
    };
    write_to_output!(out, "\x1b[{code};2;{};{};{}m", rgb.r, rgb.g, rgb.b);
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
    #[allow(unused_parens)]
    if (
        // TODO(term-workaround)
        std::env::var_os("TERM").is_some_and(|term| term.as_os_str().as_bytes() == b"st-256color")
    ) {
        return false;
    }
    out.write_bytes(b"\x1b[?u");
    true
}

fn osc_0_or_1_terminal_title(out: &mut impl Output, is_1: bool, title: &[WString]) -> bool {
    out.write_bytes(if is_1 { b"\x1b]1;" } else { b"\x1b]0;" });
    for title_line in title {
        out.write_bytes(&wcs2bytes(title_line));
    }
    out.write_bytes(b"\x07"); // BEL
    true
}

fn osc_133_prompt_start(out: &mut impl Output) -> bool {
    if !future_feature_flags::test(FeatureFlag::MarkPrompt) {
        return false;
    }
    static TEST_BALLOON: OnceLock<()> = OnceLock::new();
    if TEST_BALLOON.set(()).is_ok() {
        write_to_output!(out, "\x1b]133;A;click_events=1\x1b\\");
    } else {
        write_to_output!(out, "\x1b]133;A;click_events=1\x07");
    }
    true
}

fn osc_133_prompt_end(out: &mut impl Output) -> bool {
    if !future_feature_flags::test(FeatureFlag::MarkPrompt) {
        return false;
    }
    write_to_output!(out, "\x1b]133;B\x07");
    true
}

fn osc_133_command_start(out: &mut impl Output, command: &wstr) -> bool {
    if !future_feature_flags::test(FeatureFlag::MarkPrompt) {
        return false;
    }
    write_to_output!(
        out,
        "\x1b]133;C;cmdline_url={}\x07",
        escape_string(command, EscapeStringStyle::Url),
    );
    true
}

fn osc_133_command_finished(out: &mut impl Output, exit_status: libc::c_int) -> bool {
    if !future_feature_flags::test(FeatureFlag::MarkPrompt) {
        return false;
    }
    write_to_output!(out, "\x1b]133;D;{}\x07", exit_status);
    true
}

fn scroll_content_up(out: &mut impl Output, lines: usize) -> bool {
    write_to_output!(out, "\x1b[{}S", lines);
    true
}

fn index_for_color(c: Color) -> u8 {
    if c.is_named() || !(get_color_support().contains(ColorSupport::TERM_256COLOR)) {
        return c.to_name_index();
    }
    c.to_term256_index()
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

    pub fn new_buffering_no_assume_normal() -> Self {
        let mut zelf = Self::new_buffering();
        zelf.last.fg = Color::None;
        zelf.last.bg = Color::None;
        assert_eq!(zelf.last.underline_color, Color::None);
        zelf
    }

    fn maybe_flush(&mut self) {
        if self.fd >= 0 && self.buffer_count == 0 {
            self.flush_to(self.fd);
        }
    }

    /// Unconditionally write the color string to the output.
    /// Exported for builtin_set_color's usage only.
    pub(crate) fn write_color(&mut self, paintable: Paintable, color: Color) -> bool {
        let supports_term24bit = get_color_support().contains(ColorSupport::TERM_24BIT);
        if !supports_term24bit || !color.is_rgb() {
            // Indexed or non-24 bit color.
            let idx = index_for_color(color);
            return self.write_command(TerminalCommand::SelectPaletteColor(paintable, idx));
        }

        if only_grayscale() && color.is_grayscale() {
            return false;
        }

        // 24 bit!
        self.write_command(TerminalCommand::SelectRgbColor(
            paintable,
            color.to_color24(),
        ))
    }

    /// Unconditionally resets colors and text style.
    pub(crate) fn reset_text_face(&mut self) {
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
    ///   the previous strings colors might be removed as well.
    ///
    /// - After that we write the exit_attribute_mode command to reset all color attributes.
    ///
    /// - Lastly we may need to write set_a_background or set_a_foreground to set the other half of the
    ///   color pair to what it should be.
    pub(crate) fn set_text_face(&mut self, face: TextFace) {
        self.set_text_face_internal(face, true)
    }
    pub(crate) fn set_text_face_no_magic(&mut self, face: TextFace) {
        self.set_text_face_internal(face, false)
    }
    fn set_text_face_internal(&mut self, face: TextFace, salvage_unreadable: bool) {
        let mut fg = face.fg;
        let bg = face.bg;
        let underline_color = face.underline_color;
        let style = face.style;

        use TerminalCommand::{
            DefaultBackgroundColor, DefaultUnderlineColor, EnterBoldMode, EnterDimMode,
            EnterItalicsMode, EnterReverseMode, EnterStandoutMode, EnterUnderlineMode,
            ExitAttributeMode, ExitItalicsMode, ExitUnderlineMode,
        };

        // Removes all styles that are individually resettable.
        let non_resettable = |mut style: TextStyling| {
            style.italics = false;
            style.underline_style = None;
            style
        };
        let non_resettable_attributes_to_unset =
            non_resettable(self.last.style).difference_prefer_empty(non_resettable(style));
        if !non_resettable_attributes_to_unset.is_empty() {
            // Only way to exit non-resettable ones is a reset of all attributes.
            self.reset_text_face();
        }
        if salvage_unreadable && !bg.is_special() && fg == bg {
            fg = if bg == Color::WHITE {
                Color::BLACK
            } else {
                Color::WHITE
            };
        }

        if !fg.is_none() && fg != self.last.fg {
            if fg.is_normal() {
                self.write_command(ExitAttributeMode);

                self.last.bg = Color::Normal;
                self.last.underline_color = Color::Normal;
                self.last.style = TextStyling::default();
            } else {
                assert!(!fg.is_special());
                self.write_color(Paintable::Foreground, fg);
            }
            self.last.fg = fg;
        }

        if !bg.is_none() && bg != self.last.bg {
            if bg.is_normal() {
                self.write_command(DefaultBackgroundColor);
            } else {
                assert!(!bg.is_special());
                self.write_color(Paintable::Background, bg);
            }
            self.last.bg = bg;
        }

        if !underline_color.is_none() && underline_color != self.last.underline_color {
            if underline_color.is_normal() {
                self.write_command(DefaultUnderlineColor);
            } else {
                self.write_color(Paintable::Underline, underline_color);
            }
            self.last.underline_color = underline_color;
        }

        // Lastly, we set bold, underline, italics, dim, and reverse modes correctly.
        if style.is_bold() && !self.last.style.is_bold() && self.write_command(EnterBoldMode) {
            self.last.style.bold = true;
        }

        if style.underline_style != self.last.style.underline_style {
            match style.underline_style {
                None => {
                    if self.write_command(ExitUnderlineMode) {
                        self.last.style.underline_style = None;
                    }
                }
                Some(underline_style) => {
                    if self.write_command(EnterUnderlineMode(underline_style)) {
                        self.last.style.underline_style = Some(underline_style);
                    }
                }
            }
        }

        let was_italics = self.last.style.is_italics();
        if !style.is_italics() && was_italics && self.write_command(ExitItalicsMode) {
            self.last.style.italics = false;
        } else if style.is_italics() && !was_italics && self.write_command(EnterItalicsMode) {
            self.last.style.italics = true;
        }

        if style.is_dim() && !self.last.style.is_dim() && self.write_command(EnterDimMode) {
            self.last.style.dim = true;
        }

        if style.is_reverse()
            && !self.last.style.is_reverse()
            && (self.write_command(EnterReverseMode) || self.write_command(EnterStandoutMode))
        {
            self.last.style.reverse = true;
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
        wcs2bytes_appending(&mut self.contents, str);
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
            flogf!(term_support, "Trying path '%s'", path.to_str().unwrap());
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
        *global_term = Some(term);
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
        .is_some_and(|cap| matches!(cap, terminfo::Value::True))
}

/// Covers over tparm() with one parameter.
pub fn tparm1(cap: &CStr, param1: i32) -> Option<CString> {
    assert!(!cap.to_bytes().is_empty());
    let cap = cap.to_bytes();
    terminfo::expand!(cap; param1).ok().map(|x| x.to_cstring())
}
