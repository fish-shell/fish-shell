// Generic output functions.
use crate::common::{self, EscapeStringStyle, escape_string, wcs2bytes, wcs2bytes_appending};
use crate::future_feature_flags::{self, FeatureFlag};
use crate::prelude::*;
use crate::screen::{is_dumb, only_grayscale};
use crate::text_face::{TextFace, TextStyling, UnderlineStyle};
use crate::threads::MainThread;
use bitflags::bitflags;
use fish_color::{Color, Color24};
use std::cell::{RefCell, RefMut};
use std::ops::{Deref, DerefMut};
use std::os::fd::RawFd;
use std::os::unix::ffi::OsStrExt;
use std::sync::OnceLock;
use std::sync::atomic::{AtomicU8, Ordering};

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

#[derive(Debug, Clone, Copy)]
pub(crate) enum Paintable {
    Foreground,
    Background,
    Underline,
}

#[derive(Debug, Clone)]
pub(crate) enum SgrTerminalCommand {
    // Text attributes
    ExitAttributeMode,
    EnterBoldMode,
    EnterDimMode,
    EnterItalicsMode,
    EnterUnderlineMode(UnderlineStyle),
    EnterReverseMode,
    EnterStrikethroughMode,
    EnterStandoutMode,
    ExitItalicsMode,
    ExitUnderlineMode,
    ExitStrikethroughMode,

    // Colors
    SelectPaletteColor(Paintable, u8),
    SelectRgbColor(Paintable, Color24),
    DefaultBackgroundColor,
    DefaultUnderlineColor,
}

#[derive(Debug, Clone)]
pub(crate) enum TerminalCommand<'a> {
    // Screen clearing
    ClearScreen,
    ClearToEndOfLine,
    ClearToEndOfScreen,

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
    Osc133CommandFinished { exit_status: libc::c_int },

    // Other terminal features
    QueryCursorPosition,
    QueryBackgroundColor,
    ScrollContentUp { lines: usize },

    DecsetShowCursor,
    DecsetFocusReporting,
    DecrstFocusReporting,
    DecsetBracketedPaste,
    DecrstBracketedPaste,
    DecsetColorThemeReporting,
    DecrstColorThemeReporting,
}

#[derive(Debug, Clone)]
pub(crate) enum CardinalDirection {
    Up,
    Left,
    Right,
}

fn cursor_move(out: &mut Outputter, direction: CardinalDirection, steps: usize) -> bool {
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

fn query_xtgettcap(out: &mut Outputter, cap: &str) -> bool {
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

fn query_kitty_progressive_enhancements(out: &mut Outputter) -> bool {
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

fn osc_0_or_1_terminal_title(out: &mut Outputter, is_1: bool, title: &[WString]) -> bool {
    out.write_bytes(if is_1 { b"\x1b]1;" } else { b"\x1b]0;" });
    for title_line in title {
        out.write_bytes(&wcs2bytes(title_line));
    }
    out.write_bytes(b"\x07"); // BEL
    true
}

fn osc_133_prompt_start(out: &mut Outputter) -> bool {
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

fn osc_133_prompt_end(out: &mut Outputter) -> bool {
    if !future_feature_flags::test(FeatureFlag::MarkPrompt) {
        return false;
    }
    write_to_output!(out, "\x1b]133;B\x07");
    true
}

fn osc_133_command_start(out: &mut Outputter, command: &wstr) -> bool {
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

fn osc_133_command_finished(out: &mut Outputter, exit_status: libc::c_int) -> bool {
    if !future_feature_flags::test(FeatureFlag::MarkPrompt) {
        return false;
    }
    write_to_output!(out, "\x1b]133;D;{}\x07", exit_status);
    true
}

fn scroll_content_up(out: &mut Outputter, lines: usize) -> bool {
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

    pub(crate) fn write_command(&mut self, cmd: TerminalCommand<'_>) -> bool
    where
        Self: Sized,
    {
        use TerminalCommand::*;
        if is_dumb() {
            assert!(!matches!(cmd, CursorDown));
            return false;
        }
        fn write(out: &mut Outputter, sequence: &'static [u8]) -> bool {
            out.write_bytes(sequence);
            true
        }
        match cmd {
            ClearScreen => write(self, b"\x1b[H\x1b[2J"),
            ClearToEndOfLine => write(self, b"\x1b[K"),
            ClearToEndOfScreen => write(self, b"\x1b[J"),
            CursorUp => write(self, b"\x1b[A"),
            CursorDown => write(self, b"\n"),
            CursorLeft => write(self, b"\x08"),
            CursorRight => write(self, b"\x1b[C"),
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
            Osc133CommandFinished { exit_status } => osc_133_command_finished(self, exit_status),
            QueryCursorPosition => write(self, b"\x1b[6n"),
            QueryBackgroundColor => write(self, b"\x1b]11;?\x1b\\"),
            ScrollContentUp { lines } => scroll_content_up(self, lines),
            DecsetShowCursor => write(self, b"\x1b[?25h"),
            DecsetFocusReporting => write(self, b"\x1b[?1004h"),
            DecrstFocusReporting => write(self, b"\x1b[?1004l"),
            DecsetBracketedPaste => write(self, b"\x1b[?2004h"),
            DecrstBracketedPaste => write(self, b"\x1b[?2004l"),
            DecsetColorThemeReporting => write(self, b"\x1b[?2031h"),
            DecrstColorThemeReporting => write(self, b"\x1b[?2031l"),
        }
    }

    fn maybe_flush(&mut self) {
        if self.fd >= 0 && self.buffer_count == 0 {
            self.flush_to(self.fd);
        }
    }

    pub fn style_writer(&mut self) -> OutputterStyleWriter<'_> {
        OutputterStyleWriter::new(self)
    }

    /// Unconditionally resets colors and text style.
    /// If you're going to immediately change other TextFace attributes
    /// prefer using `StyleWriter::reset_text_face()`
    pub(crate) fn reset_text_face(&mut self) {
        self.style_writer().reset_text_face();
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
        self.set_text_face_internal(face, true, false);
    }
    pub(crate) fn set_text_face_no_magic(&mut self, face: TextFace, with_reset: bool) {
        self.set_text_face_internal(face, false, with_reset);
    }
    fn set_text_face_internal(
        &mut self,
        face: TextFace,
        salvage_unreadable: bool,
        with_reset: bool,
    ) {
        let mut fg = face.fg;
        let bg = face.bg;
        let underline_color = face.underline_color;
        let style = face.style;

        use SgrTerminalCommand::{
            DefaultBackgroundColor, DefaultUnderlineColor, EnterBoldMode, EnterDimMode,
            EnterItalicsMode, EnterReverseMode, EnterStandoutMode, EnterStrikethroughMode,
            EnterUnderlineMode, ExitAttributeMode, ExitItalicsMode, ExitStrikethroughMode,
            ExitUnderlineMode,
        };

        let mut style_writer = self.style_writer();

        if with_reset {
            style_writer.reset_text_face();
        }

        // Removes all styles that are individually resettable.
        let non_resettable = |mut style: TextStyling| {
            style.italics = false;
            style.underline_style = None;
            style
        };
        let non_resettable_attributes_to_unset = non_resettable(style_writer.last().style)
            .difference_prefer_empty(non_resettable(style));
        if !non_resettable_attributes_to_unset.is_empty() {
            // Only way to exit non-resettable ones is a reset of all attributes.
            style_writer.reset_text_face();
        }
        if salvage_unreadable && !bg.is_special() && fg == bg {
            fg = if bg == Color::WHITE {
                Color::BLACK
            } else {
                Color::WHITE
            };
        }

        if !fg.is_none() && fg != style_writer.last().fg {
            if fg.is_normal() {
                style_writer.write_command(ExitAttributeMode);

                style_writer.last().bg = Color::Normal;
                style_writer.last().underline_color = Color::Normal;
                style_writer.last().style = TextStyling::default();
            } else {
                assert!(!fg.is_special());
                style_writer.write_color(Paintable::Foreground, fg);
            }
            style_writer.last().fg = fg;
        }

        if !bg.is_none() && bg != style_writer.last().bg {
            if bg.is_normal() {
                style_writer.write_command(DefaultBackgroundColor);
            } else {
                assert!(!bg.is_special());
                style_writer.write_color(Paintable::Background, bg);
            }
            style_writer.last().bg = bg;
        }

        if !underline_color.is_none() && underline_color != style_writer.last().underline_color {
            if underline_color.is_normal() {
                style_writer.write_command(DefaultUnderlineColor);
            } else {
                style_writer.write_color(Paintable::Underline, underline_color);
            }
            style_writer.last().underline_color = underline_color;
        }

        // Lastly, we set bold, underline, italics, dim, reverse, and strikethrough modes correctly.
        if style.is_bold()
            && !style_writer.last().style.is_bold()
            && style_writer.write_command(EnterBoldMode)
        {
            style_writer.last().style.bold = true;
        }

        if style.underline_style != style_writer.last().style.underline_style {
            match style.underline_style {
                None => {
                    if style_writer.write_command(ExitUnderlineMode) {
                        style_writer.last().style.underline_style = None;
                    }
                }
                Some(underline_style) => {
                    if style_writer.write_command(EnterUnderlineMode(underline_style)) {
                        style_writer.last().style.underline_style = Some(underline_style);
                    }
                }
            }
        }

        let was_italics = style_writer.last().style.is_italics();
        if !style.is_italics() && was_italics && style_writer.write_command(ExitItalicsMode) {
            style_writer.last().style.italics = false;
        } else if style.is_italics() && !was_italics && style_writer.write_command(EnterItalicsMode)
        {
            style_writer.last().style.italics = true;
        }

        if style.is_dim()
            && !style_writer.last().style.is_dim()
            && style_writer.write_command(EnterDimMode)
        {
            style_writer.last().style.dim = true;
        }

        let was_strikethrough = style_writer.last().style.is_strikethrough();
        if !style.is_strikethrough()
            && was_strikethrough
            && style_writer.write_command(ExitStrikethroughMode)
        {
            style_writer.last().style.strikethrough = false;
        } else if style.is_strikethrough()
            && !was_strikethrough
            && style_writer.write_command(EnterStrikethroughMode)
        {
            style_writer.last().style.strikethrough = true;
        }

        if style.is_reverse()
            && !style_writer.last().style.is_reverse()
            && (style_writer.write_command(EnterReverseMode)
                || style_writer.write_command(EnterStandoutMode))
        {
            style_writer.last().style.reverse = true;
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
    pub fn contents_mut(&mut self) -> &mut Vec<u8> {
        &mut self.contents
    }
    pub fn take_contents(self) -> Vec<u8> {
        self.contents
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

    pub fn write_bytes(&mut self, buf: &[u8]) {
        self.contents.extend_from_slice(buf);
        self.maybe_flush();
    }

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

pub struct OutputterStyleWriter<'a> {
    out: &'a mut Outputter,
    param_count: u32,
}

impl<'a> OutputterStyleWriter<'a> {
    const MAX_PARAMS: u32 = 16;

    fn new(out: &'a mut Outputter) -> Self {
        Self {
            out,
            param_count: 0,
        }
    }

    fn last(&mut self) -> &mut TextFace {
        &mut self.out.last
    }

    // Unconditionally resets colors and text style.
    pub(crate) fn reset_text_face(&mut self) {
        use SgrTerminalCommand::ExitAttributeMode;
        self.write_command(ExitAttributeMode);
        self.out.last = TextFace::default();
    }

    pub(crate) fn write_command(&mut self, cmd: SgrTerminalCommand) -> bool {
        use SgrTerminalCommand::*;
        if is_dumb() {
            return false;
        }

        match cmd {
            ExitAttributeMode => self.write_param_str(1, b""),
            EnterBoldMode => self.write_param_str(1, b"1"),
            EnterDimMode => self.write_param_str(1, b"2"),
            EnterItalicsMode => self.write_param_str(1, b"3"),
            EnterUnderlineMode(style) => self.write_underline_mode(style),
            EnterReverseMode => self.write_param_str(1, b"7"),
            EnterStandoutMode => self.write_param_str(1, b"7"),
            EnterStrikethroughMode => self.write_param_str(1, b"9"),
            ExitStrikethroughMode => self.write_param_str(1, b"29"),
            ExitItalicsMode => self.write_param_str(1, b"23"),
            ExitUnderlineMode => self.write_param_str(1, b"24"),
            SelectPaletteColor(paintable, idx) => self.write_palette_color(paintable, idx),
            SelectRgbColor(paintable, rgb) => self.write_rgb_color(paintable, rgb),
            DefaultBackgroundColor => self.write_param_str(1, b"49"),
            DefaultUnderlineColor => self.write_param_str(1, b"59"),
        }
    }

    fn prepare_for_params(&mut self, count: u32) {
        assert!(count <= Self::MAX_PARAMS);
        if self.param_count + count > Self::MAX_PARAMS {
            self.out.write_bytes(b"m");
            self.param_count = 0;
        }
        if self.param_count == 0 {
            self.out.write_bytes(b"\x1b[");
        } else {
            self.out.write_bytes(b";");
        }
        self.param_count += count;
    }

    fn write_param_str(&mut self, count: u32, str: &[u8]) -> bool {
        self.prepare_for_params(count);
        self.out.write_bytes(str);
        true
    }

    fn write_param_fmt(&mut self, count: u32, args: std::fmt::Arguments<'_>) -> bool {
        self.prepare_for_params(count);
        crate::common::do_write_to_output(self.out, args);
        true
    }

    fn is_valid_color_idx(idx: u8) -> bool {
        !only_grayscale() || (Color::Named { idx }).is_grayscale()
    }

    fn write_palette_color(&mut self, paintable: Paintable, idx: u8) -> bool {
        if !Self::is_valid_color_idx(idx) {
            return false;
        }
        let bg = match paintable {
            Paintable::Foreground => 0,
            Paintable::Background => 10,
            Paintable::Underline => {
                return self.write_param_fmt(1, format_args!("58:5:{}", idx));
            }
        };
        match idx {
            0..=7 => self.write_param_fmt(1, format_args!("{}", 30 + bg + idx)),
            8..=15 => self.write_param_fmt(1, format_args!("{}", 90 + bg + (idx - 8))),
            _ => self.write_param_fmt(3, format_args!("{};5;{}", 38 + bg, idx)),
        }
    }

    fn write_color(&mut self, paintable: Paintable, color: Color) -> bool {
        let supports_term24bit = get_color_support().contains(ColorSupport::TERM_24BIT);
        if !supports_term24bit || !color.is_rgb() {
            // Indexed or non-24 bit color.
            let idx = index_for_color(color);
            return self.write_command(SgrTerminalCommand::SelectPaletteColor(paintable, idx));
        }

        if only_grayscale() && color.is_grayscale() {
            return false;
        }

        // 24 bit!
        self.write_command(SgrTerminalCommand::SelectRgbColor(
            paintable,
            color.to_color24(),
        ))
    }

    fn write_rgb_color(&mut self, paintable: Paintable, rgb: Color24) -> bool {
        // Foreground: ^[38;2;<r>;<g>;<b>m
        // Background: ^[48;2;<r>;<g>;<b>m
        // Underline: ^[58:2::<r>:<g>:<b>m
        let code = match paintable {
            Paintable::Foreground => 38,
            Paintable::Background => 48,
            Paintable::Underline => {
                return self
                    .write_param_fmt(1, format_args!("58:2::{}:{}:{}", rgb.r, rgb.g, rgb.b));
            }
        };
        self.write_param_fmt(5, format_args!("{};2;{};{};{}", code, rgb.r, rgb.g, rgb.b))
    }

    fn write_underline_mode(&mut self, style: UnderlineStyle) -> bool {
        use UnderlineStyle as UL;
        let style = match style {
            UL::Single => return self.write_param_str(1, b"4"),
            UL::Double => 2,
            UL::Curly => 3,
            UL::Dotted => 4,
            UL::Dashed => 5,
        };
        self.write_param_fmt(1, format_args!("4:{}", style))
    }
}

impl<'a> Drop for OutputterStyleWriter<'a> {
    fn drop(&mut self) {
        if self.param_count != 0 {
            self.out.write_bytes(b"m");
        }
    }
}

#[cfg(test)]
mod tests {
    use fish_color::Color24;

    use super::{
        Outputter,
        Paintable::{Background, Foreground, Underline},
        SgrTerminalCommand::ExitAttributeMode,
    };

    #[test]
    fn sgr_combining() {
        // No style, no content
        let mut outp = Outputter::new_buffering_no_assume_normal();
        {
            let mut _style_writer = outp.style_writer();
        }
        assert!(outp.contents().is_empty());

        // 0 parameter
        let mut outp = Outputter::new_buffering_no_assume_normal();
        {
            let mut style_writer = outp.style_writer();
            style_writer.write_command(ExitAttributeMode);
        }
        assert_eq!(String::from_utf8_lossy(outp.contents()), "\u{1b}[m");

        // 1 parameter
        let mut outp = Outputter::new_buffering_no_assume_normal();
        {
            let mut style_writer = outp.style_writer();
            style_writer.write_palette_color(Foreground, 0);
        }
        assert_eq!(String::from_utf8_lossy(outp.contents()), "\u{1b}[30m");

        // 2 parameters
        let mut outp = Outputter::new_buffering_no_assume_normal();
        {
            let mut style_writer = outp.style_writer();
            style_writer.write_palette_color(Foreground, 0);
            style_writer.write_palette_color(Background, 0);
        }
        assert_eq!(String::from_utf8_lossy(outp.contents()), "\u{1b}[30;40m");
    }

    #[test]
    fn sgr_max_length() {
        // Cut at max length
        let mut outp = Outputter::new_buffering_no_assume_normal();
        {
            let mut style_writer = outp.style_writer();
            for idx in 0..33 {
                style_writer.write_palette_color(Foreground, idx % 8);
            }
        }
        assert_eq!(
            String::from_utf8_lossy(outp.contents()),
            concat!(
                "\u{1b}[30;31;32;33;34;35;36;37;30;31;32;33;34;35;36;37m",
                "\u{1b}[30;31;32;33;34;35;36;37;30;31;32;33;34;35;36;37m",
                "\u{1b}[30m",
            )
        );

        // Cut early if a complex attribute would overshoot the max length
        outp = Outputter::new_buffering_no_assume_normal();
        {
            let mut style_writer = outp.style_writer();
            for idx in 0..13 {
                style_writer.write_palette_color(Foreground, idx % 8);
            }
            style_writer.write_rgb_color(Foreground, Color24 { r: 0, g: 0, b: 0 });
        }
        assert_eq!(
            String::from_utf8_lossy(outp.contents()),
            concat!(
                "\u{1b}[30;31;32;33;34;35;36;37;30;31;32;33;34m",
                "\u{1b}[38;2;0;0;0m",
            )
        );

        // "Fake" parameters don't count toward the limit
        outp = Outputter::new_buffering_no_assume_normal();
        {
            let mut style_writer = outp.style_writer();
            for idx in 0..13 {
                style_writer.write_palette_color(Foreground, idx % 8);
            }
            style_writer.write_rgb_color(Underline, Color24 { r: 0, g: 0, b: 0 });
        }
        assert_eq!(
            String::from_utf8_lossy(outp.contents()),
            concat!(
                "\u{1b}[30;31;32;33;34;35;36;37;30;31;32;33;34;",
                "58:2::0:0:0m",
            )
        );
    }
}
