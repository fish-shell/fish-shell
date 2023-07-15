// printf - format and print data
// Copyright (C) 1990-2007 Free Software Foundation, Inc.
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2, or (at your option)
// any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software Foundation,
// Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.  */
// Usage: printf format [argument...]
//
// A front end to the printf function that lets it be used from the shell.
//
// Backslash escapes:
//
// \" = double quote
// \\ = backslash
// \a = alert (bell)
// \b = backspace
// \c = produce no further output
// \e = escape
// \f = form feed
// \n = new line
// \r = carriage return
// \t = horizontal tab
// \v = vertical tab
// \ooo = octal number (ooo is 1 to 3 digits)
// \xhh = hexadecimal number (hhh is 1 to 2 digits)
// \uhhhh = 16-bit Unicode character (hhhh is 4 digits)
// \Uhhhhhhhh = 32-bit Unicode character (hhhhhhhh is 8 digits)
//
// Additional directive:
//
// %b = print an argument string, interpreting backslash escapes,
//   except that octal escapes are of the form \0 or \0ooo.
//
// The `format' argument is re-used as many times as necessary
// to convert all of the given arguments.
//
// David MacKenzie <djm@gnu.ai.mit.edu>

// This file has been imported from source code of printf command in GNU Coreutils version 6.9.

use num_traits;

use super::prelude::*;
use crate::locale::{get_numeric_locale, Locale};
use crate::wchar::encode_byte_to_char;
use crate::wutil::{
    errors::Error,
    wcstod::wcstod,
    wcstoi::{wcstoi_partial, Options as WcstoiOpts},
    wstr_offset_in,
};
use printf_compat::args::ToArg;
use printf_compat::printf::sprintf_locale;

/// \return true if \p c is an octal digit.
fn is_octal_digit(c: char) -> bool {
    ('0'..='7').contains(&c)
}

/// \return true if \p c is a decimal digit.
fn iswdigit(c: char) -> bool {
    c.is_ascii_digit()
}

/// \return true if \p c is a hexadecimal digit.
fn iswxdigit(c: char) -> bool {
    c.is_ascii_hexdigit()
}

struct builtin_printf_state_t<'a, 'b> {
    // Out and err streams. Note this is a captured reference!
    streams: &'a mut IoStreams<'b>,

    // The status of the operation.
    exit_code: c_int,

    // Whether we should stop outputting. This gets set in the case of an error, and also with the
    // \c escape.
    early_exit: bool,

    // Our output buffer, so we don't write() constantly.
    // Our strategy is simple:
    // We print once per argument, and we flush the buffer before the error.
    buff: WString,

    // The locale, which affects printf output and also parsing of floats due to decimal separators.
    locale: Locale,
}

/// Convert to a scalar type. \return the result of conversion, and the end of the converted string.
/// On conversion failure, \p end is not modified.
trait RawStringToScalarType: Copy + num_traits::Zero + std::convert::From<u32> {
    /// Convert from a string to our self type.
    /// \return the result of conversion, and the remainder of the string.
    fn raw_string_to_scalar_type<'a>(
        s: &'a wstr,
        locale: &Locale,
        end: &mut &'a wstr,
    ) -> Result<Self, Error>;

    /// Convert from a Unicode code point to this type.
    /// This supports printf's ability to convert from char to scalar via a leading quote.
    /// Try it:
    ///     > printf "%f" "'a"
    ///     97.000000
    /// Wild stuff.
    fn from_ord(c: char) -> Self {
        let as_u32: u32 = c.into();
        as_u32.into()
    }
}

impl RawStringToScalarType for i64 {
    fn raw_string_to_scalar_type<'a>(
        s: &'a wstr,
        _locale: &Locale,
        end: &mut &'a wstr,
    ) -> Result<Self, Error> {
        let mut consumed = 0;
        let res = wcstoi_partial(s, WcstoiOpts::default(), &mut consumed);
        *end = s.slice_from(consumed);
        res
    }
}

impl RawStringToScalarType for u64 {
    fn raw_string_to_scalar_type<'a>(
        s: &'a wstr,
        _locale: &Locale,
        end: &mut &'a wstr,
    ) -> Result<Self, Error> {
        let mut consumed = 0;
        let res = wcstoi_partial(
            s,
            WcstoiOpts {
                wrap_negatives: true,
                ..Default::default()
            },
            &mut consumed,
        );
        *end = s.slice_from(consumed);
        res
    }
}

impl RawStringToScalarType for f64 {
    fn raw_string_to_scalar_type<'a>(
        s: &'a wstr,
        locale: &Locale,
        end: &mut &'a wstr,
    ) -> Result<Self, Error> {
        let mut consumed: usize = 0;
        let mut result = wcstod(s, locale.decimal_point, &mut consumed);
        if result.is_ok() && consumed == s.chars().count() {
            *end = s.slice_from(consumed);
            return result;
        }
        // The conversion using the user's locale failed. That may be due to the string not being a
        // valid floating point value. It could also be due to the locale using different separator
        // characters than the normal english convention. So try again by forcing the use of a locale
        // that employs the english convention for writing floating point numbers.
        consumed = 0;
        result = wcstod(s, '.', &mut consumed);
        if result.is_ok() {
            *end = s.slice_from(consumed);
        }
        return result;
    }
}

/// Convert a string to a scalar type.
/// Use state.verify_numeric to report any errors.
fn string_to_scalar_type<T: RawStringToScalarType>(
    s: &wstr,
    state: &mut builtin_printf_state_t,
) -> T {
    if s.char_at(0) == '"' || s.char_at(0) == '\'' {
        // Note that if the string is really just a leading quote,
        // we really do want to convert the "trailing nul".
        T::from_ord(s.char_at(1))
    } else {
        let mut end = s;
        let mval = T::raw_string_to_scalar_type(s, &state.locale, &mut end);
        state.verify_numeric(s, end, mval.err());
        mval.unwrap_or(T::zero())
    }
}

/// For each character in str, set the corresponding boolean in the array to the given flag.
fn modify_allowed_format_specifiers(ok: &mut [bool; 256], str: &str, flag: bool) {
    for c in str.chars() {
        ok[c as usize] = flag;
    }
}

impl<'a, 'b> builtin_printf_state_t<'a, 'b> {
    #[allow(clippy::partialeq_to_none)]
    fn verify_numeric(&mut self, s: &wstr, end: &wstr, errcode: Option<Error>) {
        // This check matches the historic `errcode != EINVAL` check from C++.
        // Note that empty or missing values will be silently treated as 0.
        if errcode != None && errcode != Some(Error::InvalidChar) && errcode != Some(Error::Empty) {
            match errcode.unwrap() {
                Error::Overflow => {
                    self.fatal_error(sprintf!("%ls: %ls", s, wgettext!("Number out of range")));
                }
                Error::Empty => {
                    self.fatal_error(sprintf!("%ls: %ls", s, wgettext!("Number was empty")));
                }
                Error::InvalidChar => {
                    panic!("Unreachable");
                }
            }
        } else if !end.is_empty() {
            if s.as_ptr() == end.as_ptr() {
                self.fatal_error(wgettext_fmt!("%ls: expected a numeric value", s));
            } else {
                // This isn't entirely fatal - the value should still be printed.
                self.nonfatal_error(wgettext_fmt!(
                    "%ls: value not completely converted (can't convert '%ls')",
                    s,
                    end
                ));
                // Warn about octal numbers as they can be confusing.
                // Do it if the unconverted digit is a valid hex digit,
                // because it could also be an "0x" -> "0" typo.
                if s.char_at(0) == '0' && iswxdigit(end.char_at(0)) {
                    self.nonfatal_error(wgettext_fmt!(
                        "Hint: a leading '0' without an 'x' indicates an octal number"
                    ));
                }
            }
        }
    }

    /// Evaluate a printf conversion specification.  SPEC is the start of the directive, and CONVERSION
    /// specifies the type of conversion.  SPEC does not include any length modifier or the
    /// conversion specifier itself.  FIELD_WIDTH and PRECISION are the field width and
    /// precision for '*' values, if HAVE_FIELD_WIDTH and HAVE_PRECISION are true, respectively.
    /// ARGUMENT is the argument to be formatted.
    #[allow(clippy::collapsible_else_if, clippy::too_many_arguments)]
    fn print_direc(
        &mut self,
        spec: &wstr,
        conversion: char,
        have_field_width: bool,
        field_width: i32,
        have_precision: bool,
        precision: i32,
        argument: &wstr,
    ) {
        /// Printf macro helper which provides our locale.
        macro_rules! append_output_fmt {
            (
            $fmt:expr, // format string of type &wstr
            $($arg:expr),* // arguments
            ) => {
                // Don't output if we're done.
                if !self.early_exit {
                    sprintf_locale(
                        &mut self.buff,
                        $fmt,
                        &self.locale,
                        &[$($arg.to_arg()),*]
                    )
                }
            }
        }

        // Start with everything except the conversion specifier.
        let mut fmt = spec.to_owned();

        // Create a copy of the % directive, with a width modifier substituted for any
        // existing integer length modifier.
        match conversion {
            'x' | 'X' | 'd' | 'i' | 'o' | 'u' => {
                fmt.push_str("ll");
            }
            'a' | 'e' | 'f' | 'g' | 'A' | 'E' | 'F' | 'G' => {
                fmt.push_str("L");
            }
            's' | 'c' => {
                fmt.push_str("l");
            }
            _ => {}
        }

        // Append the conversion itself.
        fmt.push(conversion);

        // Rebind as a ref.
        let fmt: &wstr = &fmt;
        match conversion {
            'd' | 'i' => {
                let arg: i64 = string_to_scalar_type(argument, self);
                if !have_field_width {
                    if !have_precision {
                        append_output_fmt!(fmt, arg);
                    } else {
                        append_output_fmt!(fmt, precision, arg);
                    }
                } else {
                    if !have_precision {
                        append_output_fmt!(fmt, field_width, arg);
                    } else {
                        append_output_fmt!(fmt, field_width, precision, arg);
                    }
                }
            }
            'o' | 'u' | 'x' | 'X' => {
                let arg: u64 = string_to_scalar_type(argument, self);
                if !have_field_width {
                    if !have_precision {
                        append_output_fmt!(fmt, arg);
                    } else {
                        append_output_fmt!(fmt, precision, arg);
                    }
                } else {
                    if !have_precision {
                        append_output_fmt!(fmt, field_width, arg);
                    } else {
                        append_output_fmt!(fmt, field_width, precision, arg);
                    }
                }
            }

            'a' | 'A' | 'e' | 'E' | 'f' | 'F' | 'g' | 'G' => {
                let arg: f64 = string_to_scalar_type(argument, self);
                if !have_field_width {
                    if !have_precision {
                        append_output_fmt!(fmt, arg);
                    } else {
                        append_output_fmt!(fmt, precision, arg);
                    }
                } else {
                    if !have_precision {
                        append_output_fmt!(fmt, field_width, arg);
                    } else {
                        append_output_fmt!(fmt, field_width, precision, arg);
                    }
                }
            }

            'c' => {
                if !have_field_width {
                    append_output_fmt!(fmt, argument.char_at(0));
                } else {
                    append_output_fmt!(fmt, field_width, argument.char_at(0));
                }
            }

            's' => {
                if !have_field_width {
                    if !have_precision {
                        append_output_fmt!(fmt, argument);
                    } else {
                        append_output_fmt!(fmt, precision, argument);
                    }
                } else {
                    if !have_precision {
                        append_output_fmt!(fmt, field_width, argument);
                    } else {
                        append_output_fmt!(fmt, field_width, precision, argument);
                    }
                }
            }

            _ => {
                panic!("unexpected opt: {}", conversion);
            }
        }
    }

    /// Print the text in FORMAT, using ARGV for arguments to any `%' directives.
    /// Return the number of elements of ARGV used.
    fn print_formatted(&mut self, format: &wstr, mut argv: &[WString]) -> usize {
        let mut argc = argv.len();
        let save_argc = argc; /* Preserve original value.  */
        let mut f: &wstr; /* Pointer into `format'.  */
        let mut direc_start: &wstr; /* Start of % directive.  */
        let mut direc_length: usize; /* Length of % directive.  */
        let mut have_field_width: bool; /* True if FIELD_WIDTH is valid.  */
        let mut field_width: c_int = 0; /* Arg to first '*'.  */
        let mut have_precision: bool; /* True if PRECISION is valid.  */
        let mut precision = 0; /* Arg to second '*'.  */
        let mut ok = [false; 256]; /* ok['x'] is true if %x is allowed.  */

        // N.B. this was originally written as a loop like so:
        //    for (f = format; *f != L'\0'; ++f) {
        // so we emulate that.
        f = format;
        let mut first = true;
        loop {
            if !first {
                f = &f[1..];
            }
            first = false;
            if f.is_empty() {
                break;
            }

            match f.char_at(0) {
                '%' => {
                    direc_start = f;
                    f = &f[1..];
                    direc_length = 1;
                    have_field_width = false;
                    have_precision = false;
                    if f.char_at(0) == '%' {
                        self.append_output('%');
                        continue;
                    }
                    if f.char_at(0) == 'b' {
                        // FIXME: Field width and precision are not supported for %b, even though POSIX
                        // requires it.
                        if argc > 0 {
                            self.print_esc_string(&argv[0]);
                            argv = &argv[1..];
                            argc -= 1;
                        }
                        continue;
                    }

                    modify_allowed_format_specifiers(&mut ok, "aAcdeEfFgGiosuxX", true);
                    let mut continue_looking_for_flags = true;
                    while continue_looking_for_flags {
                        match f.char_at(0) {
                            'I' | '\'' => {
                                modify_allowed_format_specifiers(&mut ok, "aAceEosxX", false);
                            }

                            '-' | '+' | ' ' => {
                                // pass
                            }

                            '#' => {
                                modify_allowed_format_specifiers(&mut ok, "cdisu", false);
                            }

                            '0' => {
                                modify_allowed_format_specifiers(&mut ok, "cs", false);
                            }

                            _ => {
                                continue_looking_for_flags = false;
                            }
                        }
                        if continue_looking_for_flags {
                            f = &f[1..];
                            direc_length += 1;
                        }
                    }

                    if f.char_at(0) == '*' {
                        f = &f[1..];
                        direc_length += 1;
                        if argc > 0 {
                            let width: i64 = string_to_scalar_type(&argv[0], self);
                            if (c_int::MIN as i64) <= width && width <= (c_int::MAX as i64) {
                                field_width = width as c_int;
                            } else {
                                self.fatal_error(wgettext_fmt!(
                                    "invalid field width: %ls",
                                    argv[0]
                                ));
                            }
                            argv = &argv[1..];
                            argc -= 1;
                        } else {
                            field_width = 0;
                        }
                        have_field_width = true;
                    } else {
                        while iswdigit(f.char_at(0)) {
                            f = &f[1..];
                            direc_length += 1;
                        }
                    }

                    if f.char_at(0) == '.' {
                        f = &f[1..];
                        direc_length += 1;
                        modify_allowed_format_specifiers(&mut ok, "c", false);
                        if f.char_at(0) == '*' {
                            f = &f[1..];
                            direc_length += 1;
                            if argc > 0 {
                                let prec: i64 = string_to_scalar_type(&argv[0], self);
                                if prec < 0 {
                                    // A negative precision is taken as if the precision were omitted,
                                    // so -1 is safe here even if prec < INT_MIN.
                                    precision = -1;
                                } else if (c_int::MAX as i64) < prec {
                                    self.fatal_error(wgettext_fmt!(
                                        "invalid precision: %ls",
                                        argv[0]
                                    ));
                                } else {
                                    precision = prec as c_int;
                                }
                                argv = &argv[1..];
                                argc -= 1;
                            } else {
                                precision = 0;
                            }
                            have_precision = true;
                        } else {
                            while iswdigit(f.char_at(0)) {
                                f = &f[1..];
                                direc_length += 1;
                            }
                        }
                    }

                    while matches!(f.char_at(0), 'l' | 'L' | 'h' | 'j' | 't' | 'z') {
                        f = &f[1..];
                    }

                    let conversion = f.char_at(0);
                    if (conversion as usize) > 0xFF || !ok[conversion as usize] {
                        self.fatal_error(wgettext_fmt!(
                            "%.*ls: invalid conversion specification",
                            wstr_offset_in(f, direc_start) + 1,
                            direc_start
                        ));
                        return 0;
                    }

                    let mut argument = L!("");
                    if argc > 0 {
                        argument = &argv[0];
                        argv = &argv[1..];
                        argc -= 1;
                    }
                    self.print_direc(
                        &direc_start[..direc_length],
                        f.char_at(0),
                        have_field_width,
                        field_width,
                        have_precision,
                        precision,
                        argument,
                    );
                }
                '\\' => {
                    let consumed_minus_1 = self.print_esc(f, false);
                    f = &f[consumed_minus_1..]; // Loop increment will add 1.
                }

                c => {
                    self.append_output(c);
                }
            }
        }
        save_argc - argc
    }

    fn nonfatal_error<Str: AsRef<wstr>>(&mut self, errstr: Str) {
        let errstr = errstr.as_ref();
        // Don't error twice.
        if self.early_exit {
            return;
        }

        // If we have output, write it so it appears first.
        if !self.buff.is_empty() {
            self.streams.out.append(&self.buff);
            self.buff.clear();
        }

        self.streams.err.append(errstr);
        if !errstr.ends_with('\n') {
            self.streams.err.push('\n');
        }

        // We set the exit code to error, because one occurred,
        // but we don't do an early exit so we still print what we can.
        self.exit_code = STATUS_CMD_ERROR.unwrap();
    }

    fn fatal_error<Str: AsRef<wstr>>(&mut self, errstr: Str) {
        let errstr = errstr.as_ref();

        // Don't error twice.
        if self.early_exit {
            return;
        }

        // If we have output, write it so it appears first.
        if !self.buff.is_empty() {
            self.streams.out.append(&self.buff);
            self.buff.clear();
        }

        self.streams.err.append(errstr);
        if !errstr.ends_with('\n') {
            self.streams.err.push('\n');
        }

        self.exit_code = STATUS_CMD_ERROR.unwrap();
        self.early_exit = true;
    }

    /// Print a \ escape sequence starting at ESCSTART.
    /// Return the number of characters in the string, *besides the backslash*.
    /// That is this is ONE LESS than the number of characters consumed.
    /// If octal_0 is nonzero, octal escapes are of the form \0ooo, where o
    /// is an octal digit; otherwise they are of the form \ooo.
    fn print_esc(&mut self, escstart: &wstr, octal_0: bool) -> usize {
        assert!(escstart.char_at(0) == '\\');
        let mut p = &escstart[1..];
        let mut esc_value = 0; /* Value of \nnn escape. */
        let mut esc_length; /* Length of \nnn escape. */
        if p.char_at(0) == 'x' {
            // A hexadecimal \xhh escape sequence must have 1 or 2 hex. digits.
            p = &p[1..];
            esc_length = 0;
            while esc_length < 2 && iswxdigit(p.char_at(0)) {
                esc_value = esc_value * 16 + p.char_at(0).to_digit(16).unwrap();
                esc_length += 1;
                p = &p[1..];
            }
            if esc_length == 0 {
                self.fatal_error(wgettext!("missing hexadecimal number in escape"));
            }
            self.append_output(encode_byte_to_char((esc_value % 256) as u8));
        } else if is_octal_digit(p.char_at(0)) {
            // Parse \0ooo (if octal_0 && *p == L'0') or \ooo (otherwise). Allow \ooo if octal_0 && *p
            // != L'0'; this is an undocumented extension to POSIX that is compatible with Bash 2.05b.
            // Wrap mod 256, which matches historic behavior.
            esc_length = 0;
            if octal_0 && p.char_at(0) == '0' {
                p = &p[1..];
            }
            while esc_length < 3 && is_octal_digit(p.char_at(0)) {
                esc_value = esc_value * 8 + p.char_at(0).to_digit(8).unwrap();
                esc_length += 1;
                p = &p[1..];
            }
            self.append_output(encode_byte_to_char((esc_value % 256) as u8));
        } else if "\"\\abcefnrtv".contains(p.char_at(0)) {
            self.print_esc_char(p.char_at(0));
            p = &p[1..];
        } else if p.char_at(0) == 'u' || p.char_at(0) == 'U' {
            let esc_char: char = p.char_at(0);
            p = &p[1..];
            let mut uni_value = 0;
            let exp_esc_length = if esc_char == 'u' { 4 } else { 8 };
            for esc_length in 0..exp_esc_length {
                if !iswxdigit(p.char_at(0)) {
                    // Escape sequence must be done. Complain if we didn't get anything.
                    if esc_length == 0 {
                        self.fatal_error(wgettext!("Missing hexadecimal number in Unicode escape"));
                    }
                    break;
                }
                uni_value = uni_value * 16 + p.char_at(0).to_digit(16).unwrap();
                p = &p[1..];
            }
            // N.B. we assume __STDC_ISO_10646__.
            if uni_value > 0x10FFFF {
                self.fatal_error(wgettext_fmt!(
                    "Unicode character out of range: \\%c%0*x",
                    esc_char,
                    exp_esc_length,
                    uni_value
                ));
            } else {
                // TODO-RUST: if uni_value is a surrogate, we need to encode it using our PUA scheme.
                if let Some(c) = char::from_u32(uni_value) {
                    self.append_output(c);
                } else {
                    self.fatal_error(wgettext!("Invalid code points not yet supported by printf"));
                }
            }
        } else {
            self.append_output('\\');
            if !p.is_empty() {
                self.append_output(p.char_at(0));
                p = &p[1..];
            }
        }
        return wstr_offset_in(p, escstart) - 1;
    }

    /// Print string str, evaluating \ escapes.
    fn print_esc_string(&mut self, mut str: &wstr) {
        // Emulating the following loop: for (; *str; str++)
        while !str.is_empty() {
            let c = str.char_at(0);
            if c == '\\' {
                let consumed_minus_1 = self.print_esc(str, false);
                str = &str[consumed_minus_1..];
            } else {
                self.append_output(c);
            }
            str = &str[1..];
        }
    }

    /// Output a single-character \ escape.
    fn print_esc_char(&mut self, c: char) {
        match c {
            'a' => {
                // alert
                self.append_output('\x07'); // \a
            }
            'b' => {
                // backspace
                self.append_output('\x08'); // \b
            }
            'c' => {
                // cancel the rest of the output
                self.early_exit = true;
            }
            'e' => {
                // escape
                self.append_output('\x1B');
            }
            'f' => {
                // form feed
                self.append_output('\x0C'); // \f
            }
            'n' => {
                // new line
                self.append_output('\n');
            }
            'r' => {
                // carriage return
                self.append_output('\r');
            }
            't' => {
                // horizontal tab
                self.append_output('\t');
            }
            'v' => {
                // vertical tab
                self.append_output('\x0B'); // \v
            }
            _ => {
                self.append_output(c);
            }
        }
    }

    fn append_output(&mut self, c: char) {
        // Don't output if we're done.
        if self.early_exit {
            return;
        }

        self.buff.push(c);
    }
}

/// The printf builtin.
pub fn printf(
    _parser: &Parser,
    streams: &mut IoStreams<'_>,
    argv: &mut [WString],
) -> Option<c_int> {
    let mut argc = argv.len();

    // Rebind argv as immutable slice (can't rearrange its elements), skipping the command name.
    let mut argv: &[WString] = &argv[1..];
    argc -= 1;
    if argc < 1 {
        return STATUS_INVALID_ARGS;
    }

    let mut state = builtin_printf_state_t {
        streams,
        exit_code: STATUS_CMD_OK.unwrap(),
        early_exit: false,
        buff: WString::new(),
        locale: get_numeric_locale(),
    };
    let format = &argv[0];
    argc -= 1;
    argv = &argv[1..];
    loop {
        let args_used = state.print_formatted(format, argv);
        argc -= args_used;
        argv = &argv[args_used..];
        if !state.buff.is_empty() {
            state.streams.out.append(&state.buff);
            state.buff.clear();
        }
        if !(args_used > 0 && argc > 0 && !state.early_exit) {
            break;
        }
    }
    return Some(state.exit_code);
}
