// Generic output functions.
use crate::color::{self, Color24, RgbColor};
use crate::common::{self, wcs2string_appending};
use crate::env::EnvVar;
use crate::screen::{is_dumb, only_grayscale};
use crate::terminal_command::{
    palette_color, rgb_color, ENTER_BOLD_MODE, ENTER_DIM_MODE, ENTER_ITALICS_MODE,
    ENTER_REVERSE_MODE, ENTER_UNDERLINE_MODE, EXIT_ATTRIBUTE_MODE, EXIT_ITALICS_MODE,
    EXIT_UNDERLINE_MODE,
};
use crate::threads::MainThread;
use crate::wchar::prelude::*;
use bitflags::bitflags;
use std::cell::{RefCell, RefMut};
use std::io::{Result, Write};
use std::ops::{Deref, DerefMut};
use std::os::fd::RawFd;
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

pub trait InfallibleWrite: std::io::Write + Sized {
    fn infallible_write(&mut self, buf: &[u8]) {
        let n = (self as &mut dyn std::io::Write).write(buf).unwrap();
        assert!(n == buf.len());
    }
}

impl InfallibleWrite for Vec<u8> {}

fn index_for_color(c: RgbColor) -> u8 {
    if c.is_named() || !(get_color_support().contains(ColorSupport::TERM_256COLOR)) {
        return c.to_name_index();
    }
    c.to_term256_index()
}

fn write_color(outp: &mut Outputter, foreground: bool, idx: u8) -> bool {
    if is_dumb() {
        return false;
    }
    if only_grayscale()
        && !(RgbColor {
            typ: color::Type::Named { idx },
            flags: color::Flags::DEFAULT,
        })
        .is_grayscale()
    {
        return false;
    }
    palette_color(outp, foreground, idx);
    true
}

fn write_foreground_color(outp: &mut Outputter, idx: u8) -> bool {
    write_color(outp, true, idx)
}

fn write_background_color(outp: &mut Outputter, idx: u8) -> bool {
    write_color(outp, false, idx)
}

pub struct Outputter {
    /// Storage for buffered contents.
    contents: Vec<u8>,

    /// Count of how many outstanding begin_buffering() calls there are.
    buffer_count: u32,

    /// fd to output to, or -1 for none.
    fd: RawFd,

    /// Foreground.
    last_color: RgbColor,

    /// Background.
    last_color2: RgbColor,

    was_bold: bool,
    was_underline: bool,
    was_italics: bool,
    was_dim: bool,
    was_reverse: bool,
}

impl Outputter {
    /// Construct an outputter which outputs to a given fd.
    /// If the fd is negative, the outputter will buffer its output.
    const fn new_from_fd(fd: RawFd) -> Self {
        Self {
            contents: Vec::new(),
            buffer_count: 0,
            fd,
            last_color: RgbColor::NORMAL,
            last_color2: RgbColor::NORMAL,
            was_bold: false,
            was_underline: false,
            was_italics: false,
            was_dim: false,
            was_reverse: false,
        }
    }

    /// Construct an outputter which outputs to its string buffer.
    pub fn new_buffering() -> Self {
        Self::new_from_fd(-1)
    }

    fn reset_modes(&mut self) {
        self.was_bold = false;
        self.was_underline = false;
        self.was_italics = false;
        self.was_dim = false;
        self.was_reverse = false;
    }

    fn can_flush(&self) -> bool {
        self.fd >= 0 && self.buffer_count == 0
    }

    fn maybe_flush(&mut self) {
        if self.can_flush() {
            self.flush_to(self.fd);
        }
    }

    /// Add to the buffer. Assumes we are either in a buffering state, or not backed by a file
    /// at all.
    pub fn extend(&mut self, str: &[u8]) {
        assert!(!self.can_flush());
        self.infallible_write(str);
    }

    /// Unconditionally write the color string to the output.
    /// Exported for builtin_set_color's usage only.
    pub fn write_color(&mut self, color: RgbColor, is_fg: bool) -> bool {
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

        if is_dumb() {
            return false;
        }
        if only_grayscale() && color.is_grayscale() {
            return false;
        }

        // 24 bit!
        let Color24 { r, g, b } = color.to_color24();
        rgb_color(self, is_fg, r, g, b);
        true
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
    pub fn set_color(&mut self, mut fg: RgbColor, mut bg: RgbColor) {
        // Test if we have at least basic support for setting fonts, colors and related bits - otherwise
        // just give up...
        if is_dumb() {
            return;
        }

        const normal: RgbColor = RgbColor::NORMAL;
        let mut bg_set = false;
        let mut last_bg_set = false;
        let is_bold = fg.is_bold() || bg.is_bold();
        let is_underline = fg.is_underline() || bg.is_underline();
        let is_italics = fg.is_italics() || bg.is_italics();
        let is_dim = fg.is_dim() || bg.is_dim();
        let is_reverse = fg.is_reverse() || bg.is_reverse();

        if fg.is_reset() || bg.is_reset() {
            #[allow(unused_assignments)]
            {
                fg = normal;
                bg = normal;
            }
            self.reset_modes();
            // If we exit attribute mode, we must first set a color, or previously colored text might
            // lose its color. Terminals are weird...
            write_foreground_color(self, 0);
            self.infallible_write(EXIT_ATTRIBUTE_MODE);
            return;
        }
        if (self.was_bold && !is_bold)
            || (self.was_dim && !is_dim)
            || (self.was_reverse && !is_reverse)
        {
            // Only way to exit bold/dim/reverse mode is a reset of all attributes.
            self.infallible_write(EXIT_ATTRIBUTE_MODE);
            self.last_color = normal;
            self.last_color2 = normal;
            self.reset_modes();
        }
        if !self.last_color2.is_special() {
            // Background was set.
            // "Special" here refers to the special "normal", "reset" and "none" colors,
            // that really just disable the background.
            last_bg_set = true;
        }
        if !bg.is_special() {
            // Background is set.
            bg_set = true;
            if fg == bg {
                fg = if bg == RgbColor::WHITE {
                    RgbColor::BLACK
                } else {
                    RgbColor::WHITE
                };
            }
        }

        if bg_set && !last_bg_set {
            // Background color changed and is set, so we enter bold mode to make reading easier.
            // This means bold mode is _always_ on when the background color is set.
            self.infallible_write(ENTER_BOLD_MODE);
        }
        if !bg_set && last_bg_set {
            // Background color changed and is no longer set, so we exit bold mode.
            self.infallible_write(EXIT_ATTRIBUTE_MODE);
            self.reset_modes();
            // We don't know if exit_attribute_mode resets colors, so we set it to something known.
            if write_foreground_color(self, 0) {
                self.last_color = RgbColor::BLACK;
            }
        }

        if self.last_color != fg {
            if fg.is_normal() {
                write_foreground_color(self, 0);
                self.infallible_write(EXIT_ATTRIBUTE_MODE);

                self.last_color2 = RgbColor::NORMAL;
                self.reset_modes();
            } else if !fg.is_special() {
                self.write_color(fg, true /* foreground */);
            }
        }
        self.last_color = fg;

        if self.last_color2 != bg {
            if bg.is_normal() {
                write_background_color(self, 0);

                self.infallible_write(EXIT_ATTRIBUTE_MODE);
                if !self.last_color.is_normal() {
                    self.write_color(self.last_color, true /* foreground */);
                }
                self.reset_modes();
                self.last_color2 = bg;
            } else if !bg.is_special() {
                self.write_color(bg, false /* not foreground */);
                self.last_color2 = bg;
            }
        }

        // Lastly, we set bold, underline, italics, dim, and reverse modes correctly.
        if is_bold && !self.was_bold && !bg_set {
            self.infallible_write(ENTER_BOLD_MODE);
            self.was_bold = is_bold;
        }

        if !self.was_underline && is_underline {
            self.infallible_write(ENTER_UNDERLINE_MODE);
            self.was_underline = is_underline;
        } else if self.was_underline && !is_underline {
            self.infallible_write(EXIT_UNDERLINE_MODE);
            self.was_underline = is_underline;
        }

        if self.was_italics && !is_italics {
            self.infallible_write(EXIT_ITALICS_MODE);
            self.was_italics = is_italics;
        } else if !self.was_italics && is_italics {
            self.infallible_write(ENTER_ITALICS_MODE);
            self.was_italics = is_italics;
        }

        if is_dim && !self.was_dim {
            self.infallible_write(ENTER_DIM_MODE);
            self.was_dim = is_dim;
        }
        // N.B. there is no exit_dim_mode, it's handled by exit_attribute_mode above.

        if is_reverse && !self.was_reverse {
            // Some terms do not have a reverse mode set, so standout mode is a fallback.
            self.infallible_write(ENTER_REVERSE_MODE);
            self.was_reverse = is_reverse;
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

/// Outputter implements Write, so it may be used as the receiver of the write! macro.
/// Only ASCII data should be written this way: do NOT assume that the receiver is UTF-8.
impl Write for Outputter {
    fn write(&mut self, buf: &[u8]) -> Result<usize> {
        self.contents.extend_from_slice(buf);
        self.maybe_flush();
        Ok(buf.len())
    }

    fn flush(&mut self) -> Result<()> {
        self.flush_to(self.fd);
        Ok(())
    }
}

impl InfallibleWrite for Outputter {}

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

impl<'a> Write for BufferedOutputter<'a> {
    fn write(&mut self, buf: &[u8]) -> Result<usize> {
        self.0.infallible_write(buf);
        Ok(buf.len())
    }

    fn flush(&mut self) -> Result<()> {
        self.0.flush().unwrap();
        Ok(())
    }
}
impl<'a> InfallibleWrite for BufferedOutputter<'a> {}

/// Given a list of RgbColor, pick the "best" one, as determined by the color support. Returns
/// RgbColor::NONE if empty.
pub fn best_color(candidates: &[RgbColor], support: ColorSupport) -> RgbColor {
    if candidates.is_empty() {
        return RgbColor::NONE;
    }

    let mut first_rgb = RgbColor::NONE;
    let mut first_named = RgbColor::NONE;
    for color in candidates {
        if first_rgb.is_none() && color.is_rgb() {
            first_rgb = *color;
        }
        if first_named.is_none() && color.is_named() {
            first_named = *color;
        }
    }

    // If we have both RGB and named colors, then prefer rgb if term256 is supported.
    let mut result;
    let has_term256 = support.contains(ColorSupport::TERM_256COLOR);
    if (!first_rgb.is_none() && has_term256) || first_named.is_none() {
        result = first_rgb;
    } else {
        result = first_named;
    }
    if result.is_none() {
        result = candidates[0];
    }
    result
}

/// Return the internal color code representing the specified color.
/// TODO: This code should be refactored to enable sharing with builtin_set_color.
///       In particular, the argument parsing still isn't fully capable.
pub fn parse_color(var: &EnvVar, is_background: bool) -> RgbColor {
    let mut result = parse_color_maybe_none(var, is_background);
    if result.is_none() {
        result.typ = color::Type::Normal;
    }
    result
}

pub fn parse_color_maybe_none(var: &EnvVar, is_background: bool) -> RgbColor {
    let mut is_bold = false;
    let mut is_underline = false;
    let mut is_italics = false;
    let mut is_dim = false;
    let mut is_reverse = false;

    let mut candidates: Vec<RgbColor> = Vec::new();

    let prefix = L!("--background=");

    let mut next_is_background = false;
    let mut color_name = WString::new();
    for next in var.as_list() {
        color_name.clear();
        #[allow(clippy::collapsible_else_if)]
        if is_background {
            if color_name.is_empty() && next_is_background {
                color_name = next.to_owned();
                next_is_background = false;
            } else if next.starts_with(prefix) {
                // Look for something like "--background=red".
                color_name = next.slice_from(prefix.char_count()).to_owned();
            } else if next == "--background" || next == "-b" {
                // Without argument attached the next token is the color
                // - if it's another option it's an error.
                next_is_background = true;
            } else if next == "--reverse" || next == "-r" {
                // Reverse should be meaningful in either context
                is_reverse = true;
            } else if next.starts_with("-b") {
                // Look for something like "-bred".
                // Yes, that length is hardcoded.
                color_name = next.slice_from(2).to_owned();
            }
        } else {
            if next == "--bold" || next == "-o" {
                is_bold = true;
            } else if next == "--underline" || next == "-u" {
                is_underline = true;
            } else if next == "--italics" || next == "-i" {
                is_italics = true;
            } else if next == "--dim" || next == "-d" {
                is_dim = true;
            } else if next == "--reverse" || next == "-r" {
                is_reverse = true;
            } else {
                color_name = next.clone();
            }
        }

        if !color_name.is_empty() {
            let color: Option<RgbColor> = RgbColor::from_wstr(&color_name);
            if let Some(color) = color {
                candidates.push(color);
            }
        }
    }

    let mut result = best_color(&candidates, get_color_support());
    result.set_bold(is_bold);
    result.set_underline(is_underline);
    result.set_italics(is_italics);
    result.set_dim(is_dim);
    result.set_reverse(is_reverse);
    result
}
