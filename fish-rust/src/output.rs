// Generic output functions.
use crate::color::RgbColor;
use crate::common::{self, assert_is_locked, wcs2string_appending};
use crate::curses::{self, tparm1, Term};
use crate::env::EnvVar;
use crate::wchar::prelude::*;
use bitflags::bitflags;
use std::cell::RefCell;
use std::ffi::{CStr, CString};
use std::io::{Result, Write};
use std::os::fd::RawFd;
use std::sync::atomic::{AtomicU8, Ordering};
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

/// FFI bits.
#[no_mangle]
extern "C" fn output_get_color_support() -> u8 {
    COLOR_SUPPORT.load(Ordering::Relaxed)
}

#[no_mangle]
extern "C" fn output_set_color_support(val: u8) {
    COLOR_SUPPORT.store(val, Ordering::Relaxed);
}

/// Returns true if we think tparm can handle outputting a color index.
fn term_supports_color_natively(term: &Term, c: i32) -> bool {
    #[allow(clippy::int_plus_one)]
    if let Some(max_colors) = term.max_colors {
        max_colors >= c + 1
    } else {
        false
    }
}

pub fn get_color_support() -> ColorSupport {
    let val = COLOR_SUPPORT.load(Ordering::Relaxed);
    ColorSupport::from_bits_truncate(val)
}

pub fn set_color_support(val: ColorSupport) {
    COLOR_SUPPORT.store(val.bits(), Ordering::Relaxed);
}

fn index_for_color(c: RgbColor) -> u8 {
    if c.is_named() || !(get_color_support().contains(ColorSupport::TERM_256COLOR)) {
        return c.to_name_index();
    }
    c.to_term256_index()
}

fn write_color_escape(outp: &mut Outputter, term: &Term, todo: &CStr, mut idx: u8, is_fg: bool) {
    if term_supports_color_natively(term, idx.into()) {
        // Use tparm to emit color escape.
        outp.tputs_if_some(&tparm1(todo, idx.into()));
    } else {
        // We are attempting to bypass the term here. Generate the ANSI escape sequence ourself.
        if idx < 16 {
            // this allows the non-bright color to happen instead of no color working at all when a
            // bright is attempted when only colors 0-7 are supported.
            //
            // TODO: enter bold mode in builtin_set_color in the same circumstance- doing that combined
            // with what we do here, will make the brights actually work for virtual consoles/ancient
            // emulators.
            if term.max_colors == Some(8) && idx > 8 {
                idx -= 8;
            }
            write!(
                outp,
                "\x1B[{}m",
                (if idx > 7 { 82 } else { 30 }) + i32::from(idx) + ((i32::from(!is_fg)) * 10)
            )
            .expect("Writing to in-memory buffer should never fail");
        } else {
            write!(outp, "\x1B[{};5;{}m", if is_fg { 38 } else { 48 }, idx).unwrap();
        }
    }
}

fn write_foreground_color(outp: &mut Outputter, idx: u8, term: &Term) -> bool {
    if let Some(cap) = &term.set_a_foreground {
        write_color_escape(outp, term, cap, idx, true);
        true
    } else if let Some(cap) = &term.set_foreground {
        write_color_escape(outp, term, cap, idx, true);
        true
    } else {
        false
    }
}

fn write_background_color(outp: &mut Outputter, idx: u8, term: &Term) -> bool {
    if let Some(cap) = &term.set_a_background {
        write_color_escape(outp, term, cap, idx, false);
        true
    } else if let Some(cap) = &term.set_background {
        write_color_escape(outp, term, cap, idx, false);
        true
    } else {
        false
    }
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

    fn maybe_flush(&mut self) {
        if self.fd >= 0 && self.buffer_count == 0 {
            self.flush_to(self.fd);
        }
    }

    /// Unconditionally write the color string to the output.
    /// Exported for builtin_set_color's usage only.
    pub fn write_color(&mut self, color: RgbColor, is_fg: bool) -> bool {
        let Some(term) = curses::term() else {
            return false;
        };
        let term: &Term = &term;
        let supports_term24bit = get_color_support().contains(ColorSupport::TERM_24BIT);
        if !supports_term24bit || !color.is_rgb() {
            // Indexed or non-24 bit color.
            let idx = index_for_color(color);
            if is_fg {
                return write_foreground_color(self, idx, term);
            } else {
                return write_background_color(self, idx, term);
            };
        }

        // 24 bit! No tparm here, just ANSI escape sequences.
        // Foreground: ^[38;2;<r>;<g>;<b>m
        // Background: ^[48;2;<r>;<g>;<b>m
        let rgb = color.to_color24();
        write!(
            self,
            "\x1B[{};2;{};{};{}m",
            if is_fg { 38 } else { 48 },
            rgb.r,
            rgb.g,
            rgb.b
        )
        .expect("Outputter::write should never fail");
        true
    }

    /// Sets the fg and bg color. May be called as often as you like, since if the new color is the same
    /// as the previous, nothing will be written. Negative values for set_color will also be ignored.
    /// Since the terminfo string this function emits can potentially cause the screen to flicker, the
    /// function takes care to write as little as possible.
    ///
    /// Possible values for colors are RgbColor colors or special values like RgbColor::NORMAL
    ///
    /// In order to set the color to normal, three terminfo strings may have to be written.
    ///
    /// - First a string to set the color, such as set_a_foreground. This is needed because otherwise
    /// the previous strings colors might be removed as well.
    ///
    /// - After that we write the exit_attribute_mode string to reset all color attributes.
    ///
    /// - Lastly we may need to write set_a_background or set_a_foreground to set the other half of the
    /// color pair to what it should be.
    #[allow(clippy::if_same_then_else)]
    pub fn set_color(&mut self, mut fg: RgbColor, mut bg: RgbColor) {
        // Test if we have at least basic support for setting fonts, colors and related bits - otherwise
        // just give up...
        let Some(term) = curses::term() else {
            return;
        };
        let term: &Term = &term;
        let Term {
            enter_bold_mode,
            enter_underline_mode,
            exit_underline_mode,
            enter_italics_mode,
            exit_italics_mode,
            enter_dim_mode,
            enter_reverse_mode,
            enter_standout_mode,
            exit_attribute_mode,
            ..
        } = term;
        let Some(exit_attribute_mode) = exit_attribute_mode else {
            return;
        };

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
            write_foreground_color(self, 0, term);
            self.tputs(exit_attribute_mode);
            return;
        }
        if (self.was_bold && !is_bold)
            || (self.was_dim && !is_dim)
            || (self.was_reverse && !is_reverse)
        {
            // Only way to exit bold/dim/reverse mode is a reset of all attributes.
            self.tputs(exit_attribute_mode);
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

        if let Some(enter_bold_mode) = &enter_bold_mode {
            if bg_set && !last_bg_set {
                // Background color changed and is set, so we enter bold mode to make reading easier.
                // This means bold mode is _always_ on when the background color is set.
                self.tputs(enter_bold_mode);
            }
            if !bg_set && last_bg_set {
                // Background color changed and is no longer set, so we exit bold mode.
                self.tputs(exit_attribute_mode);
                self.reset_modes();
                // We don't know if exit_attribute_mode resets colors, so we set it to something known.
                if write_foreground_color(self, 0, term) {
                    self.last_color = RgbColor::BLACK;
                }
            }
        }

        if self.last_color != fg {
            if fg.is_normal() {
                write_foreground_color(self, 0, term);
                self.tputs(exit_attribute_mode);

                self.last_color2 = RgbColor::NORMAL;
                self.reset_modes();
            } else if !fg.is_special() {
                self.write_color(fg, true /* foreground */);
            }
        }
        self.last_color = fg;

        if self.last_color2 != bg {
            if bg.is_normal() {
                write_background_color(self, 0, term);

                self.tputs(exit_attribute_mode);
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
        if is_bold && !self.was_bold && !bg_set && self.tputs_if_some(enter_bold_mode) {
            self.was_bold = is_bold;
        }

        if !self.was_underline && is_underline && self.tputs_if_some(enter_underline_mode) {
            self.was_underline = is_underline;
        } else if self.was_underline && !is_underline && self.tputs_if_some(exit_underline_mode) {
            self.was_underline = is_underline;
        }

        if self.was_italics && !is_italics && self.tputs_if_some(exit_italics_mode) {
            self.was_italics = is_italics;
        } else if !self.was_italics && is_italics && self.tputs_if_some(enter_italics_mode) {
            self.was_italics = is_italics;
        }

        if is_dim && !self.was_dim && self.tputs_if_some(enter_dim_mode) {
            self.was_dim = is_dim;
        }
        // N.B. there is no exit_dim_mode in curses, it's handled by exit_attribute_mode above.

        if is_reverse && !self.was_reverse {
            // Some terms do not have a reverse mode set, so standout mode is a fallback.
            if self.tputs_if_some(enter_reverse_mode) {
                self.was_reverse = is_reverse;
            } else if self.tputs_if_some(enter_standout_mode) {
                self.was_reverse = is_reverse;
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

    /// \return the "output" contents.
    pub fn contents(&self) -> &[u8] {
        &self.contents
    }

    /// Output any buffered data to the given \p fd.
    fn flush_to(&mut self, fd: RawFd) {
        if fd >= 0 && !self.contents.is_empty() {
            let _ = common::write_loop(&fd, &self.contents);
            self.contents.clear();
        }
    }

    /// Begins buffering. Output will not be automatically flushed until a corresponding
    /// end_buffering() call.
    fn begin_buffering(&mut self) {
        self.buffer_count += 1;
        assert!(self.buffer_count > 0, "buffer_count overflow");
    }

    /// Balance a begin_buffering() call.
    fn end_buffering(&mut self) {
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

// tputs accepts a function pointer that receives an int only.
// Use the following lock to redirect it to the proper outputter.
// Note we can't use an owning Mutex because the tputs_writer must access it and Mutex is not
// recursive.
static TPUTS_RECEIVER_LOCK: Mutex<()> = Mutex::new(());
static mut TPUTS_RECEIVER: *mut Outputter = std::ptr::null_mut();

extern "C" fn tputs_writer(b: curses::TputsArg) -> libc::c_int {
    // Safety: we hold the lock.
    assert_is_locked!(&TPUTS_RECEIVER_LOCK);
    let receiver = unsafe { TPUTS_RECEIVER.as_mut().expect("null TPUTS_RECEIVER") };
    receiver.push(b as u8);
    0
}

impl Outputter {
    /// Emit a terminfo string, like tputs.
    /// affcnt (number of lines affected) is assumed to be 1, i.e. not applicable.
    pub fn tputs(&mut self, str: &CStr) {
        let affcnt = 1;
        // Acquire the lock, set the receiver, and call tputs.
        let _guard = TPUTS_RECEIVER_LOCK.lock().unwrap();
        // Safety: we hold the lock.
        let saved_recv = unsafe { TPUTS_RECEIVER };
        unsafe { TPUTS_RECEIVER = self as *mut Outputter };
        self.begin_buffering();
        let _ = curses::tputs(str, affcnt, tputs_writer);
        self.end_buffering();
        unsafe { TPUTS_RECEIVER = saved_recv };
    }

    /// Convenience cover over tputs, in recognition of the fact that our Term has Optional fields.
    /// If `str` is Some, write it with tputs and return true. Otherwise, return false.
    pub fn tputs_if_some(&mut self, str: &Option<CString>) -> bool {
        if let Some(str) = str {
            self.tputs(str);
            true
        } else {
            false
        }
    }

    /// Access the outputter for stdout.
    /// This should only be used from the main thread.
    pub fn stdoutput() -> &'static mut RefCell<Outputter> {
        crate::threads::assert_is_main_thread();
        static mut STDOUTPUT: RefCell<Outputter> =
            RefCell::new(Outputter::new_from_fd(libc::STDOUT_FILENO));
        // Safety: this is only called from the main thread.
        unsafe { &mut STDOUTPUT }
    }
}

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
#[allow(clippy::collapsible_else_if)]
pub fn parse_color(var: &EnvVar, is_background: bool) -> RgbColor {
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
    if result.is_none() {
        result = RgbColor::NORMAL;
    }
    result.set_bold(is_bold);
    result.set_underline(is_underline);
    result.set_italics(is_italics);
    result.set_dim(is_dim);
    result.set_reverse(is_reverse);
    result
}

/// FFI junk.
fn stdoutput_ffi() -> &'static mut Outputter {
    // TODO: this is bogus because it avoids RefCell's check, but is temporary for FFI purposes.
    unsafe { &mut *Outputter::stdoutput().as_ptr() }
}

/// Make an outputter which outputs to its string.
fn make_buffering_outputter_ffi() -> Box<Outputter> {
    Box::new(Outputter::new_buffering())
}

pub type RgbColorFFI = crate::ffi::rgb_color_t;
use crate::wchar_ffi::AsWstr;
impl Outputter {
    fn set_color_ffi(&mut self, fg: &RgbColorFFI, bg: &RgbColorFFI) {
        self.set_color(fg.from_ffi(), bg.from_ffi());
    }

    fn writech_ffi(&mut self, ch: crate::ffi::wchar_t) {
        self.writech(char::from_u32(ch).expect("Invalid wchar"));
    }

    // Write a nul-terminated string.
    // We accept CxxString because it prevents needing to do typecasts at the call site,
    // as it's unclear what Cxx type corresponds to const char *.
    // We are unconcerned with interior nul-bytes: none of the termcap sequences contain them
    // for obvious reasons.
    fn writembs_ffi(&mut self, mbs: &cxx::CxxString) {
        let mbs = unsafe { CStr::from_ptr(mbs.as_ptr() as *const std::ffi::c_char) };
        self.tputs(mbs);
    }

    fn writestr_ffi(&mut self, str: crate::ffi::wcharz_t) {
        self.write_wstr(str.as_wstr());
    }

    fn write_color_ffi(&mut self, color: &RgbColorFFI, is_fg: bool) -> bool {
        self.write_color(color.from_ffi(), is_fg)
    }
}

#[cxx::bridge]
mod ffi {
    extern "C++" {
        include!("color.h");
        include!("wutil.h");

        #[cxx_name = "rgb_color_t"]
        type RgbColorFFI = super::RgbColorFFI;

        type wcharz_t = crate::ffi::wcharz_t;
    }

    extern "Rust" {
        #[cxx_name = "outputter_t"]
        type Outputter;

        #[cxx_name = "make_buffering_outputter"]
        fn make_buffering_outputter_ffi() -> Box<Outputter>;

        #[cxx_name = "stdoutput"]
        fn stdoutput_ffi() -> &'static mut Outputter;

        #[cxx_name = "set_color"]
        fn set_color_ffi(&mut self, fg: &RgbColorFFI, bg: &RgbColorFFI);

        #[cxx_name = "writech"]
        fn writech_ffi(&mut self, ch: wchar_t);

        #[cxx_name = "writestr"]
        fn writestr_ffi(&mut self, str: wcharz_t);

        #[cxx_name = "writembs"]
        fn writembs_ffi(&mut self, mbs: &CxxString);

        #[cxx_name = "write_color"]
        fn write_color_ffi(&mut self, color: &RgbColorFFI, is_fg: bool) -> bool;

        // These do not need separate FFI variants.
        fn contents(&self) -> &[u8];
        fn begin_buffering(&mut self);
        fn end_buffering(&mut self);

        #[cxx_name = "push_back"]
        fn push(&mut self, ch: u8);
    }
}
