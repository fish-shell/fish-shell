//! Implementation of the echo builtin.

use super::prelude::*;
use crate::wchar::encode_byte_to_char;

#[derive(Debug, Clone, Copy)]
struct Options {
    print_newline: bool,
    print_spaces: bool,
    interpret_special_chars: bool,
}

impl Default for Options {
    fn default() -> Self {
        Self {
            print_newline: true,
            print_spaces: true,
            interpret_special_chars: false,
        }
    }
}

fn parse_options(
    args: &mut [WString],
    parser: &Parser,
    streams: &mut IoStreams<'_>,
) -> Result<(Options, usize), Option<c_int>> {
    const SHORT_OPTS: &wstr = L!("+:Eens");
    const LONG_OPTS: &[woption] = &[];

    let mut opts = Options::default();

    let mut oldopts = opts;
    let mut oldoptind = 0;

    let mut w = wgetopter_t::new(SHORT_OPTS, LONG_OPTS, args);
    while let Some(c) = w.wgetopt_long() {
        match c {
            'n' => opts.print_newline = false,
            'e' => opts.interpret_special_chars = true,
            's' => opts.print_spaces = false,
            'E' => opts.interpret_special_chars = false,
            ':' => {
                builtin_missing_argument(parser, streams, w.cmd(), &w.argv()[w.woptind - 1], true);
                return Err(STATUS_INVALID_ARGS);
            }
            '?' => {
                return Ok((oldopts, w.woptind - 1));
            }
            _ => {
                panic!("unexpected retval from wgetopter::wgetopt_long()");
            }
        }

        // Super cheesy: We keep an old copy of the option state around,
        // so we can revert it in case we get an argument like
        // "-n foo".
        // We need to keep it one out-of-date so we can ignore the *last* option.
        // (this might be an issue in wgetopt, but that's a whole other can of worms
        //  and really only occurs with our weird "put it back" option parsing)
        if w.woptind == oldoptind + 2 {
            oldopts = opts;
            oldoptind = w.woptind;
        }
    }

    Ok((opts, w.woptind))
}

/// Parse a numeric escape sequence in `s`, returning the number of characters consumed and the
/// resulting value. Supported escape sequences:
///
/// - `0nnn`: octal value, zero to three digits
/// - `nnn`: octal value, one to three digits
/// - `xhh`: hex value, one to two digits
fn parse_numeric_sequence<I>(chars: I) -> Option<(usize, u8)>
where
    I: IntoIterator<Item = char>,
{
    let mut chars = chars.into_iter().peekable();

    // the first character of the numeric part of the sequence
    let mut start = 0;

    let mut base: u8 = 0;
    let mut max_digits = 0;

    let first = *chars.peek()?;
    if first.is_digit(8) {
        // Octal escape
        base = 8;

        // If the first digit is a 0, we allow four digits (including that zero); otherwise, we
        // allow 3.
        max_digits = if first == '0' { 4 } else { 3 };
    } else if first == 'x' {
        // Hex escape
        base = 16;
        max_digits = 2;

        // Skip the x
        start = 1;
    };

    if base == 0 {
        return None;
    }

    let mut val = 0;
    let mut consumed = start;
    for digit in chars
        .skip(start)
        .take(max_digits)
        .map_while(|c| c.to_digit(base.into()))
    {
        // base is either 8 or 16, so digit can never be >255
        let digit = u8::try_from(digit).unwrap();

        val = val * base + digit;

        consumed += 1;
    }

    // We succeeded if we consumed at least one digit.
    if consumed > 0 {
        Some((consumed, val))
    } else {
        None
    }
}

/// The echo builtin.
///
/// Bash only respects `-n` if it's the first argument. We'll do the same. We also support a new,
/// fish specific, option `-s` to mean "no spaces".
pub fn echo(parser: &Parser, streams: &mut IoStreams<'_>, args: &mut [WString]) -> Option<c_int> {
    let (opts, optind) = match parse_options(args, parser, streams) {
        Ok((opts, optind)) => (opts, optind),
        Err(err @ Some(_)) if err != STATUS_CMD_OK => return err,
        Err(err) => panic!("Illogical exit code from parse_options(): {err:?}"),
    };

    // The special character \c can be used to indicate no more output.
    let mut output_stopped = false;

    // We buffer output so we can write in one go,
    // this matters when writing to an fd.
    let mut out = WString::new();
    let args_to_echo = &args[optind..];
    'outer: for (idx, arg) in args_to_echo.iter().enumerate() {
        if opts.print_spaces && idx > 0 {
            out.push(' ');
        }

        let mut chars = arg.chars().peekable();
        while let Some(c) = chars.next() {
            if !opts.interpret_special_chars || c != '\\' {
                // Not an escape.
                out.push(c);
                continue;
            }

            let Some(next_char) = chars.peek() else {
                // Incomplete escape sequence is echoed verbatim
                out.push('\\');
                break;
            };

            // Most escapes consume one character in addition to the backslash; the numeric
            // sequences may consume more, while an unrecognized escape sequence consumes none.
            let mut consumed = 1;

            let escaped = match next_char {
                'a' => '\x07',
                'b' => '\x08',
                'e' => '\x1B',
                'f' => '\x0C',
                'n' => '\n',
                'r' => '\r',
                't' => '\t',
                'v' => '\x0B',
                '\\' => '\\',
                'c' => {
                    output_stopped = true;
                    break 'outer;
                }
                _ => {
                    // Octal and hex escape sequences.
                    if let Some((digits_consumed, narrow_val)) =
                        parse_numeric_sequence(chars.clone())
                    {
                        consumed = digits_consumed;
                        // The narrow_val is a literal byte that we want to output (#1894).
                        encode_byte_to_char(narrow_val)
                    } else {
                        consumed = 0;
                        '\\'
                    }
                }
            };

            // Skip over characters that were part of this escape sequence (after the backslash
            // that was consumed by the `while` loop).
            // TODO: `Iterator::advance_by()`: https://github.com/rust-lang/rust/issues/77404
            for _ in 0..consumed {
                let _ = chars.next();
            }

            out.push(escaped);
        }
    }

    if opts.print_newline && !output_stopped {
        out.push('\n');
    }

    if !out.is_empty() {
        streams.out.append(&out);
    }

    STATUS_CMD_OK
}
