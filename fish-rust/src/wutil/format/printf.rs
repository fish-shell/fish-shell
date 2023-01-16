// Adapted from https://github.com/tjol/sprintf-rs
// License follows:
//
// Copyright (c) 2021 Thomas Jollans
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is furnished
// to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
// FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS
// OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
// WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF
// OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

pub use super::format::Printf;
use super::parser::{parse_format_string, ConversionType, FormatElement, NumericParam};
use crate::wchar::{wstr, WString};

/// Error type
#[derive(Debug, Clone, Copy)]
pub enum PrintfError {
    /// Error parsing the format string
    ParseError,
    /// Incorrect type passed as an argument
    WrongType,
    /// Too many arguments passed
    TooManyArgs,
    /// Too few arguments passed
    NotEnoughArgs,
    /// Other error (should never happen)
    Unknown,
}

pub type Result<T> = std::result::Result<T, PrintfError>;

/// Format a string. (Roughly equivalent to `vsnprintf` or `vasprintf` in C)
///
/// Takes a printf-style format string `format` and a slice of dynamically
/// typed arguments, `args`.
///
///     use sprintf::{vsprintf, Printf};
///     let n = 16;
///     let args: Vec<&dyn Printf> = vec![&n];
///     let s = vsprintf("%#06x", &args).unwrap();
///     assert_eq!(s, "0x0010");
///
/// See also: [sprintf]
pub fn vsprintf(format: &wstr, args: &[&dyn Printf]) -> Result<WString> {
    vsprintfp(&parse_format_string(format)?, args)
}

fn vsprintfp(format: &[FormatElement], args: &[&dyn Printf]) -> Result<WString> {
    let mut res = WString::new();

    let mut args = args;
    let mut pop_arg = || {
        if args.is_empty() {
            Err(PrintfError::NotEnoughArgs)
        } else {
            let a = args[0];
            args = &args[1..];
            Ok(a)
        }
    };

    for elem in format {
        match elem {
            FormatElement::Verbatim(s) => {
                res.push_utfstr(s);
            }
            FormatElement::Format(spec) => {
                if spec.conversion_type == ConversionType::PercentSign {
                    res.push('%');
                } else {
                    let mut completed_spec = *spec;
                    if spec.width == NumericParam::FromArgument {
                        completed_spec.width = NumericParam::Literal(
                            pop_arg()?.as_int().ok_or(PrintfError::WrongType)?,
                        )
                    }
                    if spec.precision == NumericParam::FromArgument {
                        completed_spec.precision = NumericParam::Literal(
                            pop_arg()?.as_int().ok_or(PrintfError::WrongType)?,
                        )
                    }
                    res.push_utfstr(&pop_arg()?.format(&completed_spec)?);
                }
            }
        }
    }

    if args.is_empty() {
        Ok(res)
    } else {
        Err(PrintfError::TooManyArgs)
    }
}

/// Format a string. (Roughly equivalent to `snprintf` or `asprintf` in C)
///
/// Takes a printf-style format string `format` and a variable number of
/// additional arguments.
///
///     use sprintf::sprintf;
///     let s = sprintf!("%s = %*d", "forty-two", 4, 42);
///     assert_eq!(s, "forty-two =   42");
///
/// Wrapper around [vsprintf].
macro_rules! sprintf {
    (
        $fmt:expr, // format string
        $($arg:expr),* // arguments
        $(,)? // optional trailing comma
    ) => {
        crate::wutil::format::printf::vsprintf($fmt, &[$( &($arg) as &dyn crate::wutil::format::printf::Printf),* ][..]).expect("Invalid format string and/or arguments")
    };
}
pub(crate) use sprintf;
