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

use super::prelude::*;
use crate::locale::{get_numeric_locale, Locale};
use crate::wchar::encode_byte_to_char;
use crate::wutil::{
    errors::Error,
    wcstod::wcstod,
    wcstoi::{wcstoi_partial, Options as WcstoiOpts},
    wstr_offset_in,
};
use fish_printf::{sprintf_locale, Arg, FormatString, ToArg};

/// Return true if `c` is an octal digit.
fn is_octal_digit(c: char) -> bool {
    ('0'..='7').contains(&c)
}

/// Return true if `c` is a decimal digit.
fn iswdigit(c: char) -> bool {
    c.is_ascii_digit()
}

/// Return true if `c` is a hexadecimal digit.
fn iswxdigit(c: char) -> bool {
    c.is_ascii_hexdigit()
}

struct builtin_printf_state_t<'a, 'b> {
    // Out and err streams. Note this is a captured reference!
    streams: &'a mut IoStreams<'b>,

    // The status of the operation.
    exit_code: BuiltinResult,

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

/// Convert to a scalar type. Return the result of conversion, and the end of the converted string.
/// On conversion failure, `end` is not modified.
trait RawStringToScalarType: Copy + std::convert::From<u32> {
    /// Convert from a string to our self type.
    /// Return the result of conversion, and the remainder of the string.
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
        mval.unwrap_or(T::from(0))
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
                    self.nonfatal_error(wgettext!(
                        "Hint: a leading '0' without an 'x' indicates an octal number"
                    ));
                }
            }
        }
    }

    fn handle_sprintf_error(&mut self, err: fish_printf::Error) {
        match err {
            fish_printf::Error::Overflow => self.fatal_error(wgettext!("Number out of range")),
            _ => panic!("unhandled error: {err:?}"),
        }
    }

    /// Determines whether explicit argument positions are used in the format string.
    /// Detects some format string errors which are marked via `self.fatal_error`,
    /// so check `self.early_exit` before trusting the return value and exit if `self.early_exit`
    /// is true.
    fn uses_explicit_positions(&mut self, format: &wstr) -> bool {
        let mut format_chars = format;
        // suffix of the format string starting at the character following the % of the first directive.
        format_chars.skip_literal_prefix();
        // If explicit arguments are used, the directive_str must start with [0-9]+\$
        let num_digits = format_chars
            .chars()
            .take_while(|&c| c.is_ascii_digit())
            .count();
        if num_digits == 0 {
            return false;
        }
        match format_chars.try_char_at(num_digits) {
            Some('$') => {
                return true;
            }
            Some(_) => {
                return false;
            }
            None => {
                self.fatal_error(wgettext!(
                    "Unclosed directive: Directives cannot end on a number."
                ));
                return false;
            }
        }
    }

    /// Tries to parse the argument according to the conversion char.
    /// On failure, `self.fatal_error` is called and `None` is returned.
    fn parse_arg<'c>(&mut self, conversion: char, argument: &'c wstr) -> Option<Arg<'c>> {
        match conversion {
            'd' | 'i' => Some(string_to_scalar_type::<i64>(argument, self).to_arg()),
            'o' | 'u' | 'x' | 'X' => Some(string_to_scalar_type::<u64>(argument, self).to_arg()),
            'a' | 'A' | 'e' | 'E' | 'f' | 'F' | 'g' | 'G' => {
                Some(string_to_scalar_type::<f64>(argument, self).to_arg())
            }
            'c' => {
                if argument.len() != 1 {
                    self.fatal_error(wgettext_fmt!("Specified conversion 'c', but supplied argument '%s' is not a character (single codepoint).", argument));
                    return None;
                }
                Some(argument.to_arg())
            }
            's' => Some(argument.to_arg()),
            c => {
                self.fatal_error(wgettext_fmt!(
                    "Invalid conversion character in format string: '%c'",
                    c
                ));
                return None;
            }
        }
    }

    /// Evaluate a printf conversion specification.
    /// `spec` is the start of the directive, and `conversion` specifies the type of conversion.
    /// `spec` does not include any length modifier or the conversion specifier itself.
    /// `field_width` and `precision` are the field width and precision for '*' values, if any.
    /// `argument` is the argument to be formatted.
    #[allow(clippy::collapsible_else_if, clippy::too_many_arguments)]
    fn print_directive(
        &mut self,
        spec: &wstr,
        conversion: char,
        field_width: Option<i64>,
        precision: Option<i64>,
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
                        &mut [$($arg.to_arg()),*]
                    ).err().map(|err| self.handle_sprintf_error(err));
                }
            }
        }

        // Start with everything except the conversion specifier.
        let mut fmt = spec.to_owned();

        // Append the conversion itself.
        fmt.push(conversion);

        // Rebind as a ref.
        let fmt: &wstr = &fmt;
        match conversion {
            'd' | 'i' => {
                let arg: i64 = string_to_scalar_type(argument, self);
                match field_width {
                    Some(field_width) => match precision {
                        Some(precision) => {
                            append_output_fmt!(fmt, field_width, precision, arg);
                        }
                        None => {
                            append_output_fmt!(fmt, field_width, arg);
                        }
                    },
                    None => match precision {
                        Some(precision) => {
                            append_output_fmt!(fmt, precision, arg);
                        }
                        None => {
                            append_output_fmt!(fmt, arg);
                        }
                    },
                }
            }
            'o' | 'u' | 'x' | 'X' => {
                let arg: u64 = string_to_scalar_type(argument, self);
                match field_width {
                    Some(field_width) => match precision {
                        Some(precision) => {
                            append_output_fmt!(fmt, field_width, precision, arg);
                        }
                        None => {
                            append_output_fmt!(fmt, field_width, arg);
                        }
                    },
                    None => match precision {
                        Some(precision) => {
                            append_output_fmt!(fmt, precision, arg);
                        }
                        None => {
                            append_output_fmt!(fmt, arg);
                        }
                    },
                }
            }

            'a' | 'A' | 'e' | 'E' | 'f' | 'F' | 'g' | 'G' => {
                let arg: f64 = string_to_scalar_type(argument, self);
                match field_width {
                    Some(field_width) => match precision {
                        Some(precision) => {
                            append_output_fmt!(fmt, field_width, precision, arg);
                        }
                        None => {
                            append_output_fmt!(fmt, field_width, arg);
                        }
                    },
                    None => match precision {
                        Some(precision) => {
                            append_output_fmt!(fmt, precision, arg);
                        }
                        None => {
                            append_output_fmt!(fmt, arg);
                        }
                    },
                }
            }

            'c' => match field_width {
                Some(field_width) => {
                    append_output_fmt!(fmt, field_width, argument.char_at(0));
                }
                None => {
                    append_output_fmt!(fmt, argument.char_at(0));
                }
            },

            's' => match field_width {
                Some(field_width) => match precision {
                    Some(precision) => {
                        append_output_fmt!(fmt, field_width, precision, argument);
                    }
                    None => {
                        append_output_fmt!(fmt, field_width, argument);
                    }
                },
                None => match precision {
                    Some(precision) => {
                        append_output_fmt!(fmt, precision, argument);
                    }
                    None => {
                        append_output_fmt!(fmt, argument);
                    }
                },
            },

            _ => {
                panic!("unexpected opt: {}", conversion);
            }
        }
    }

    /// Print the text in `format`, using `argv` for arguments to any `%' directives.
    /// Return the number of elements of `argv` used.
    /// This version does not support explicit argument positions.
    /// Use it for format strings which only contain implicit argument positions,
    /// which can be checked with the `uses_explicit_positions` function.
    /// The advantage of this function is that it allows repeated application of the format string,
    /// which is a custom feature of fish's printf.
    fn print_formatted_implicit_positions(&mut self, format: &wstr, mut argv: &[&wstr]) -> usize {
        let mut argc = argv.len();
        let save_argc = argc; /* Preserve original value.  */
        let mut f: &wstr; /* Pointer into `format'.  */
        let mut directive_start: &wstr; /* Start of % directive.  */
        let mut directive_length: usize; /* Length of % directive.  */
        let mut field_width: Option<i64>; /* Arg to first '*'.  */
        let mut precision: Option<i64>; /* Arg to second '*'.  */
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
                    directive_start = f;
                    f = &f[1..];
                    directive_length = 1;
                    field_width = None;
                    precision = None;
                    if f.char_at(0) == '%' {
                        self.append_output('%');
                        continue;
                    }
                    if f.char_at(0) == 'b' {
                        // FIXME: Field width and precision are not supported for %b, even though POSIX
                        // requires it.
                        if argc > 0 {
                            self.print_esc_string(argv[0]);
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
                            directive_length += 1;
                        }
                    }

                    if f.char_at(0) == '*' {
                        f = &f[1..];
                        directive_length += 1;
                        if argc > 0 {
                            let width: i64 = string_to_scalar_type(argv[0], self);
                            if (c_int::MIN as i64) <= width && width <= (c_int::MAX as i64) {
                                field_width = Some(width);
                            } else {
                                self.fatal_error(wgettext_fmt!(
                                    "invalid field width: %ls",
                                    argv[0]
                                ));
                            }
                            argv = &argv[1..];
                            argc -= 1;
                        } else {
                            field_width = Some(0);
                        }
                    } else {
                        while iswdigit(f.char_at(0)) {
                            f = &f[1..];
                            directive_length += 1;
                        }
                    }

                    if f.char_at(0) == '.' {
                        f = &f[1..];
                        directive_length += 1;
                        modify_allowed_format_specifiers(&mut ok, "c", false);
                        if f.char_at(0) == '*' {
                            f = &f[1..];
                            directive_length += 1;
                            if argc > 0 {
                                let prec: i64 = string_to_scalar_type(argv[0], self);
                                if prec < 0 {
                                    // A negative precision is taken as if the precision were omitted,
                                    // so -1 is safe here even if prec < INT_MIN.
                                    precision = Some(-1);
                                } else if (c_int::MAX as i64) < prec {
                                    self.fatal_error(wgettext_fmt!(
                                        "invalid precision: %ls",
                                        argv[0]
                                    ));
                                } else {
                                    precision = Some(prec);
                                }
                                argv = &argv[1..];
                                argc -= 1;
                            } else {
                                precision = Some(0);
                            }
                        } else {
                            while iswdigit(f.char_at(0)) {
                                f = &f[1..];
                                directive_length += 1;
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
                            wstr_offset_in(f, directive_start) + 1,
                            directive_start
                        ));
                        return 0;
                    }

                    let mut argument = L!("");
                    if argc > 0 {
                        argument = argv[0];
                        argv = &argv[1..];
                        argc -= 1;
                    }
                    self.print_directive(
                        &directive_start[..directive_length],
                        f.char_at(0),
                        field_width,
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

    /// Print the text in `format`, using `args` for arguments to any `%' directives.
    /// This version only supports explicit argument positions, but does not support repeated
    /// application of the format string, a custom feature of fish's printf that only works when no
    /// explicit argument positions are used.
    fn print_formatted_explicit_positions(&mut self, format: &wstr, args: &[&wstr]) {
        let num_args = args.len();
        let mut parsed_args: Vec<Option<Arg>> = Vec::with_capacity(num_args);
        for _ in 0..num_args {
            parsed_args.push(None);
        }

        let mut remaining_format = format;
        remaining_format.skip_literal_prefix();
        while !remaining_format.is_empty() {
            // Syntax of directive (after %):
            // argument$[flags][width][.precision][length modifier]conversion
            //
            // argument: consists of one ore more ASCII digits and indicates the 1-based index of
            // the argument which should be used for the value to format.
            // If argument is given, the arg at that index must have the type indicated by
            // conversion.
            //
            // flags: [I'-+ #0]+
            // can be skipped over
            //
            // width: [1-9][0-9]* or `*m$`, where `m` is a decimal number indicating an
            // argument index.
            // In the latter case, the argument at index `m` must have type i64.
            //
            // precision: same as width, but can also be empty
            //
            // length modifier: [lLhjtz]*
            // Length modifiers are useless for our implementation, so we don't bother verifying
            // whether they match the conversion or are valid at all (technically, only single
            // letter length modifiers, plus hh and ll are allowed).
            //
            // conversion: [aAcdeEfFgGiosuxX]
            // This tells us the type which should be parsed from the argument string.

            // First, look for the argument index
            let next_value_index = match remaining_format.take_value_index(num_args) {
                Ok(position) => position,
                Err(e) => {
                    self.fatal_error(&e);
                    return;
                }
            };

            remaining_format.skip_flags();

            match remaining_format.take_width_index(num_args) {
                Ok(Some(width_index)) => match &parsed_args[width_index] {
                    Some(existing_arg) => {
                        if !matches!(existing_arg, Arg::SInt(_, _)) {
                            self.fatal_error(wgettext_fmt!(
                                "Argument at index %d has conflicting type constraints.",
                                width_index + 1
                            ));
                            return;
                        }
                    }
                    None => {
                        let arg = string_to_scalar_type::<i64>(args[width_index], self).to_arg();
                        parsed_args[width_index] = Some(arg);
                    }
                },
                Ok(None) => {}
                Err(e) => {
                    self.fatal_error(&e);
                    return;
                }
            }

            match remaining_format.take_precision_index(num_args) {
                Ok(Some(precision_index)) => match &parsed_args[precision_index] {
                    Some(existing_arg) => {
                        if !matches!(existing_arg, Arg::SInt(_, _)) {
                            self.fatal_error(wgettext_fmt!(
                                "Argument at index %d has conflicting type constraints.",
                                precision_index + 1
                            ));
                            return;
                        }
                    }
                    None => {
                        let arg =
                            string_to_scalar_type::<i64>(args[precision_index], self).to_arg();
                        parsed_args[precision_index] = Some(arg);
                    }
                },
                Ok(None) => {}
                Err(e) => {
                    self.fatal_error(&e);
                    return;
                }
            }

            remaining_format.skip_length_modifiers();

            match remaining_format.try_char_at(0) {
                Some(conversion) => match self.parse_arg(conversion, args[next_value_index]) {
                    Some(arg) => match &parsed_args[next_value_index] {
                        Some(existing_arg) => {
                            if std::mem::discriminant(&arg) != std::mem::discriminant(existing_arg)
                            {
                                self.fatal_error(wgettext_fmt!(
                                    "Argument at index %d has conflicting type constraints.",
                                    next_value_index + 1
                                ));
                            }
                        }
                        None => {
                            parsed_args[next_value_index] = Some(arg);
                        }
                    },
                    None => {
                        return;
                    }
                },
                None => {
                    self.fatal_error(wgettext!(
                        "Format string ends on directive without conversion specifier."
                    ));
                    return;
                }
            }
            remaining_format.advance_by(1);

            remaining_format.skip_literal_prefix();
        }
        let unused_args = parsed_args
            .iter()
            .enumerate()
            .filter_map(|(index, arg)| if arg.is_none() { Some(index + 1) } else { None })
            .collect::<Vec<usize>>();
        if !unused_args.is_empty() {
            self.fatal_error(wgettext_fmt!(
                "Unused argument indices: %s",
                format!("{unused_args:?}")
            ));
            return;
        }

        let mut parsed_args = parsed_args.into_iter().flatten().collect::<Vec<Arg>>();

        sprintf_locale(&mut self.buff, format, &self.locale, &mut parsed_args)
            .err()
            .map(|err| self.handle_sprintf_error(err));
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
        self.exit_code = Err(STATUS_CMD_ERROR);
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

        self.exit_code = Err(STATUS_CMD_ERROR);
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
                let consumed_minus_1 = self.print_esc(str, true);
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

trait FormatStringHelper {
    /// Strips the prefix of literal characters and returns the remaining suffix.
    /// If a formatting directive (starting with a %) is contained, the % of that directive is also
    /// stripped.
    /// Note that a format string with a trailing % is accepted by this function.
    fn skip_literal_prefix(&mut self);

    /// Expects the remaining format string to start with a positive number of ASCII digits,
    /// followed by a $.
    /// If this format is found, the number is parsed to a `usize`.
    /// A bounds check is performed, ensuring that the number is in 1..=num_args.
    /// If all checks succeed, the number is decremented to match Rust's 0-based indexing and
    /// returned in the `Ok` variant.
    /// On errors, a suitable message is returned.
    fn take_value_index(&mut self, num_args: usize) -> Result<usize, WString>;

    /// Advances past the formatting flags, if any.
    fn skip_flags(&mut self);

    /// Checks if the width is specified via an index.
    /// If so, that index is bounds-checked, decremented and returned.
    /// If no width is specified, or it is specified inline, `Ok(None)` is returned.
    fn take_width_index(&mut self, num_args: usize) -> Result<Option<usize>, WString>;

    /// Checks if the precision is specified via an index.
    /// If so, that index is bounds-checked, decremented and returned.
    /// If no precision is specified, or it is specified inline, `Ok(None)` is returned.
    fn take_precision_index(&mut self, num_args: usize) -> Result<Option<usize>, WString>;

    /// Advances past the formatting flags, if any.
    fn skip_length_modifiers(&mut self);
}

impl FormatStringHelper for &wstr {
    fn skip_literal_prefix(&mut self) {
        let mut directive_start_index = None;
        let mut i = 0;
        while i < self.len() {
            if self.char_at(i) == '%' {
                match self.try_char_at(i + 1) {
                    Some('%') => {
                        i += 2;
                    }
                    Some(_) => {
                        directive_start_index = Some(i + 1);
                        break;
                    }
                    None => {
                        // Invalid string. Trailing %.
                        *self = L!("");
                        return;
                    }
                }
            } else {
                i += 1;
            }
        }
        match directive_start_index {
            Some(directive_start_index) => {
                *self = &self[directive_start_index..];
            }
            None => {
                *self = L!("");
            }
        }
    }

    fn take_value_index(&mut self, num_args: usize) -> Result<usize, WString> {
        let num_digits = self.chars().take_while(|&c| c.is_ascii_digit()).count();
        if num_digits == 0 {
            return Err(wgettext_fmt!(
                "Missing argument position in format string.\nExpected decimal number but found:\n%s",
                self
            ));
        }
        if self.try_char_at(num_digits) != Some('$') {
            return Err(wgettext_fmt!(
                "Argument position in format string not terminated by a $:\n%s",
                self
            ));
        }
        let digits = self[..num_digits].to_string();
        let Ok(arg_index) = digits.parse::<usize>() else {
            return Err(wgettext_fmt!(
                "Failed to parse argument position in format string. Location in format string:\n%s\nExtracted digits: %s", self, digits
            ));
        };
        if arg_index == 0 {
            return Err(wgettext_fmt!(
                "Argument position 0 in format string is invalid. The first valid position is 1. Location in format string:\n%s", self
            ));
        }
        if arg_index > num_args {
            return Err(wgettext_fmt!(
                "Argument position %d in format string exceeds number of arguments (%d).",
                arg_index,
                num_args
            ));
        }
        *self = &self[(num_digits + 1)..];
        Ok(arg_index - 1)
    }

    fn skip_flags(&mut self) {
        let num_flag_chars = self
            .chars()
            .take_while(|&c| matches!(c, 'I' | '\'' | '-' | '+' | ' ' | '#' | '0'))
            .count();
        *self = &self[num_flag_chars..]
    }

    fn take_width_index(&mut self, num_args: usize) -> Result<Option<usize>, WString> {
        match self.try_char_at(0) {
            Some('*') => {
                // width is taken from arguments.
                self.advance_by(1);
                self.take_value_index(num_args).map(Some)
            }
            Some(c) if c.is_ascii_digit() => {
                // width is specified inline
                let num_digits = self.chars().take_while(|&c| c.is_ascii_digit()).count();
                *self = &self[num_digits..];
                Ok(None)
            }
            Some(_) | None => Ok(None),
        }
    }

    fn take_precision_index(&mut self, num_args: usize) -> Result<Option<usize>, WString> {
        if self.try_char_at(0) != Some('.') {
            return Ok(None);
        }
        self.advance_by(1);
        match self.try_char_at(0) {
            Some('*') => {
                // precision is taken from arguments.
                self.advance_by(1);
                self.take_value_index(num_args).map(Some)
            }
            Some(c) if c.is_ascii_digit() => {
                // precision is specified inline
                let num_digits = self.chars().take_while(|&c| c.is_ascii_digit()).count();
                *self = &self[num_digits..];
                Ok(None)
            }
            Some(_) | None => Ok(None),
        }
    }

    fn skip_length_modifiers(&mut self) {
        let num_length_modifier_chars = self
            .chars()
            .take_while(|&c| matches!(c, 'l' | 'L' | 'h' | 'j' | 't' | 'z'))
            .count();
        *self = &self[num_length_modifier_chars..];
    }
}

/// The printf builtin.
pub fn printf(_parser: &Parser, streams: &mut IoStreams, argv: &mut [&wstr]) -> BuiltinResult {
    let mut argc = argv.len();

    // Rebind argv as immutable slice (can't rearrange its elements), skipping the command name.
    let mut argv: &[&wstr] = &argv[1..];
    argc -= 1;
    if argc < 1 {
        return Err(STATUS_INVALID_ARGS);
    }

    let mut state = builtin_printf_state_t {
        streams,
        exit_code: Ok(SUCCESS),
        early_exit: false,
        buff: WString::new(),
        locale: get_numeric_locale(),
    };
    let format = argv[0];
    argc -= 1;
    argv = &argv[1..];
    let explicit_positions = state.uses_explicit_positions(format);
    if state.early_exit {
        return state.exit_code;
    }
    if explicit_positions {
        state.print_formatted_explicit_positions(format, argv);
        if !state.buff.is_empty() {
            state.streams.out.append(&state.buff);
        }
    } else {
        // This supports repeated application of the format string.
        loop {
            let args_used = state.print_formatted_implicit_positions(format, argv);
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
    }
    return state.exit_code;
}
