/** Rust printf implementation, based on musl. */
use super::arg::Arg;
use super::fmt_fp::format_float;
use super::locale::Locale;
use std::fmt::{self, Write};
use std::mem;
use std::ops::{AddAssign, Index};
use std::result::Result;

/// Possible errors from printf.
#[derive(Debug, PartialEq, Eq)]
pub enum Error {
    /// Invalid format string.
    BadFormatString,
    /// Too few arguments.
    MissingArg,
    /// Too many arguments.
    ExtraArg,
    /// Argument type doesn't match format specifier.
    BadArgType,
    /// Precision is too large to represent.
    Overflow,
    /// Error emitted by the output stream.
    Fmt(fmt::Error),
}

// Convenience conversion from fmt::Error.
impl From<fmt::Error> for Error {
    fn from(err: fmt::Error) -> Error {
        Error::Fmt(err)
    }
}

#[derive(Debug, Copy, Clone, Default)]
pub(super) struct ModifierFlags {
    pub alt_form: bool, // #
    pub zero_pad: bool, // 0
    pub left_adj: bool, // negative field width
    pub pad_pos: bool,  // space: blank before positive numbers
    pub mark_pos: bool, // +: sign before positive numbers
    pub grouped: bool,  // ': group indicator
}

impl ModifierFlags {
    // If c is a modifier character, set the flag and return true.
    // Otherwise return false. Note we allow repeated modifier flags.
    fn try_set(&mut self, c: char) -> bool {
        match c {
            '#' => self.alt_form = true,
            '0' => self.zero_pad = true,
            '-' => self.left_adj = true,
            ' ' => self.pad_pos = true,
            '+' => self.mark_pos = true,
            '\'' => self.grouped = true,
            _ => return false,
        };
        true
    }
}

// The set of prefixes of conversion specifiers.
// Note that we mostly ignore prefixes - we take sizes of values from the arguments themselves.
#[derive(Debug, Copy, Clone, PartialEq, Eq)]
#[allow(non_camel_case_types)]
enum ConversionPrefix {
    Empty,
    hh,
    h,
    l,
    ll,
    j,
    t,
    z,
    L,
}

#[derive(Debug, Copy, Clone, PartialEq, Eq)]
#[allow(non_camel_case_types)]
#[rustfmt::skip]
pub(super) enum ConversionSpec {
    // Integers, with prefixes "hh", "h", "l", "ll", "j", "t", "z"
    // Note that we treat '%i' as '%d'.
    d, o, u, x, X,

    // USizeRef receiver, with same prefixes as ints
    n,

    // Float, with prefixes "l" and "L"
    a, A, e, E, f, F, g, G,

    // Pointer, no prefixes
    p,

    // Character or String, with supported prefixes "l"
    // Note that we treat '%C' as '%c' and '%S' as '%s'.
    c, s,
}

impl ConversionSpec {
    // Returns true if the given prefix is supported by this conversion specifier.
    fn supports_prefix(self, prefix: ConversionPrefix) -> bool {
        use ConversionPrefix::*;
        use ConversionSpec::*;
        if matches!(prefix, Empty) {
            // No prefix is always supported.
            return true;
        }
        match self {
            d | o | u | x | X | n => matches!(prefix, hh | h | l | ll | j | t | z),
            a | A | e | E | f | F | g | G => matches!(prefix, l | L),
            p => false,
            c | s => matches!(prefix, l),
        }
    }

    // Returns true if the conversion specifier is lowercase,
    // which affects certain rendering.
    #[inline]
    pub(super) fn is_lower(self) -> bool {
        use ConversionSpec::*;
        match self {
            d | o | u | x | n | a | e | f | g | p | c | s => true,
            X | A | E | F | G => false,
        }
    }

    // Returns a ConversionSpec from a character, or None if none.
    fn from_char(cc: char) -> Option<Self> {
        use ConversionSpec::*;
        let res = match cc {
            'd' | 'i' => d,
            'o' => o,
            'u' => u,
            'x' => x,
            'X' => X,
            'n' => n,
            'a' => a,
            'A' => A,
            'e' => e,
            'E' => E,
            'f' => f,
            'F' => F,
            'g' => g,
            'G' => G,
            'p' => p,
            'c' | 'C' => c,
            's' | 'S' => s,
            _ => return None,
        };
        Some(res)
    }
}

// A helper type that holds a format string slice and points into it.
// As a convenience, this returns '\0' for one-past-the-end.
#[derive(Debug)]
struct FormatString<'a>(&'a [char]);

impl<'a> FormatString<'a> {
    // Return the underlying slice.
    fn as_slice(&self) -> &'a [char] {
        self.0
    }

    // Return true if we are empty.
    fn is_empty(&self) -> bool {
        self.0.is_empty()
    }

    // Read an int from our cursor, stopping at the first non-digit.
    // Negative values are not supported.
    // If there are no digits, return 0.
    // Adjust the cursor to point to the char after the int.
    fn get_int(&mut self) -> Result<usize, Error> {
        use Error::Overflow;
        let mut i: usize = 0;
        while let Some(digit) = self[0].to_digit(10) {
            i = i.checked_mul(10).ok_or(Overflow)?;
            i = i.checked_add(digit as usize).ok_or(Overflow)?;
            *self += 1;
        }
        Ok(i)
    }

    // Read a conversion prefix from our cursor, advancing it.
    fn get_prefix(&mut self) -> ConversionPrefix {
        use ConversionPrefix as CP;
        let prefix = match self[0] {
            'h' if self[1] == 'h' => CP::hh,
            'h' => CP::h,
            'l' if self[1] == 'l' => CP::ll,
            'l' => CP::l,
            'j' => CP::j,
            't' => CP::t,
            'z' => CP::z,
            'L' => CP::L,
            _ => CP::Empty,
        };
        *self += match prefix {
            CP::Empty => 0,
            CP::hh | CP::ll => 2,
            _ => 1,
        };
        prefix
    }

    // Read an (optionally prefixed) format specifier, such as d, Lf, etc.
    // Adjust the cursor to point to the char after the specifier.
    fn get_specifier(&mut self) -> Result<ConversionSpec, Error> {
        let prefix = self.get_prefix();
        // Awkwardly placed hack to disallow %lC and %lS, since we otherwise treat
        // them as the same.
        if prefix != ConversionPrefix::Empty && matches!(self[0], 'C' | 'S') {
            return Err(Error::BadFormatString);
        }
        let spec = ConversionSpec::from_char(self[0]).ok_or(Error::BadFormatString)?;
        if !spec.supports_prefix(prefix) {
            return Err(Error::BadFormatString);
        }
        *self += 1;
        Ok(spec)
    }

    // Read a sequence of characters to be output literally, advancing the cursor.
    // This handles a tail of %%.
    fn get_lit(&mut self) -> &'a [char] {
        let s = self.0;
        let non_percents = s.iter().take_while(|&&c| c != '%').count();
        // Take only an even number of percents.
        let percent_pairs: usize = s[non_percents..].iter().take_while(|&&c| c == '%').count() / 2;
        *self += non_percents + percent_pairs * 2;
        &s[..non_percents + percent_pairs]
    }
}

// Advance this format string by a number of chars.
impl AddAssign<usize> for FormatString<'_> {
    fn add_assign(&mut self, rhs: usize) {
        self.0 = &self.0[rhs..];
    }
}

// Index into FormatString, returning \0 for one-past-the-end.
impl Index<usize> for FormatString<'_> {
    type Output = char;

    fn index(&self, idx: usize) -> &char {
        let s = self.as_slice();
        if idx == s.len() {
            &'\0'
        } else {
            &s[idx]
        }
    }
}

// Pad output by emitting `c` until `min_width` is reached.
pub(super) fn pad(
    f: &mut impl Write,
    c: char,
    min_width: usize,
    current_width: usize,
) -> fmt::Result {
    assert!(c == '0' || c == ' ');
    if current_width >= min_width {
        return Ok(());
    }
    const ZEROS: &str = "0000000000000000";
    const SPACES: &str = "                ";
    let buff = if c == '0' { ZEROS } else { SPACES };
    let mut remaining = min_width - current_width;
    while remaining > 0 {
        let n = remaining.min(buff.len());
        f.write_str(&buff[..n])?;
        remaining -= n;
    }
    Ok(())
}

/// Formats a string using the provided format specifiers, arguments, and locale,
/// and writes the output to the given `Write` implementation.
///
/// # Parameters
/// - `f`: The receiver of formatted output.
/// - `fmt`: The format string being parsed.
/// - `locale`: The locale to use for number formatting.
/// - `args`: Iterator over the arguments to format.
///
/// # Returns
/// A `Result` which is `Ok` containing the number of bytes written on success, or an `Error`.
pub fn sprintf_locale(
    f: &mut impl Write,
    fmt: &[char],
    locale: &Locale,
    args: &mut [Arg],
) -> Result<usize, Error> {
    use ConversionSpec as CS;
    let mut s = FormatString(fmt);
    let mut args = args.iter_mut();
    let mut out_len: usize = 0;

    // Shared storage for the output of the conversion specifier.
    let buf = &mut String::new();
    'main: while !s.is_empty() {
        buf.clear();

        // Handle literal text and %% format specifiers.
        let lit = s.get_lit();
        if !lit.is_empty() {
            buf.extend(lit.iter());
            f.write_str(buf)?;
            out_len = out_len.checked_add(lit.len()).ok_or(Error::Overflow)?;
            continue 'main;
        }

        // Consume the % at the start of the format specifier.
        debug_assert!(s[0] == '%');
        s += 1;

        // Read modifier flags. '-' and '0' flags are mutually exclusive.
        let mut flags = ModifierFlags::default();
        while flags.try_set(s[0]) {
            s += 1;
        }
        if flags.left_adj {
            flags.zero_pad = false;
        }

        // Read field width. We do not support $.
        let width = if s[0] == '*' {
            let arg_width = args.next().ok_or(Error::MissingArg)?.as_sint()?;
            s += 1;
            if arg_width < 0 {
                flags.left_adj = true;
            }
            arg_width
                .unsigned_abs()
                .try_into()
                .map_err(|_| Error::Overflow)?
        } else {
            s.get_int()?
        };

        // Optionally read precision. We do not support $.
        let mut prec: Option<usize> = if s[0] == '.' && s[1] == '*' {
            // "A negative precision is treated as though it were missing."
            // Here we assume the precision is always signed.
            s += 2;
            let p = args.next().ok_or(Error::MissingArg)?.as_sint()?;
            p.try_into().ok()
        } else if s[0] == '.' {
            s += 1;
            Some(s.get_int()?)
        } else {
            None
        };
        // Disallow precisions larger than i32::MAX, in keeping with C.
        if prec.unwrap_or(0) > i32::MAX as usize {
            return Err(Error::Overflow);
        }

        // Read out the format specifier and arg.
        let conv_spec = s.get_specifier()?;
        let arg = args.next().ok_or(Error::MissingArg)?;
        let mut prefix = "";

        // Thousands grouping only works for d,u,i,f,F.
        // 'i' is mapped to 'd'.
        if flags.grouped && !matches!(conv_spec, CS::d | CS::u | CS::f | CS::F) {
            return Err(Error::BadFormatString);
        }

        // Disable zero-pad if we have an explicit precision.
        // "If a precision is given with a numeric conversion (d, i, o, u, i, x, and X),
        // the 0 flag is ignored." p is included here.
        let spec_is_numeric = matches!(conv_spec, CS::d | CS::u | CS::o | CS::p | CS::x | CS::X);
        if spec_is_numeric && prec.is_some() {
            flags.zero_pad = false;
        }

        // Apply the formatting. Some cases continue the main loop.
        // Note that numeric conversions must leave 'body' empty if the value is 0.
        let body: &str = match conv_spec {
            CS::n => {
                arg.set_count(out_len)?;
                continue 'main;
            }
            CS::e | CS::f | CS::g | CS::a | CS::E | CS::F | CS::G | CS::A => {
                // Floating point types handle output on their own.
                let float = arg.as_float()?;
                let len = format_float(f, float, width, prec, flags, locale, conv_spec, buf)?;
                out_len = out_len.checked_add(len).ok_or(Error::Overflow)?;
                continue 'main;
            }
            CS::p => {
                const PTR_HEX_DIGITS: usize = 2 * mem::size_of::<*const u8>();
                prec = prec.map(|p| p.max(PTR_HEX_DIGITS));
                let uint = arg.as_uint()?;
                if uint != 0 {
                    prefix = "0x";
                    write!(buf, "{:x}", uint)?;
                }
                buf
            }
            CS::x | CS::X => {
                // If someone passes us a negative value, format it with the width
                // we were given.
                let lower = conv_spec.is_lower();
                let (_, uint) = arg.as_wrapping_sint()?;
                if uint != 0 {
                    if flags.alt_form {
                        prefix = if lower { "0x" } else { "0X" };
                    }
                    if lower {
                        write!(buf, "{:x}", uint)?;
                    } else {
                        write!(buf, "{:X}", uint)?;
                    }
                }
                buf
            }
            CS::o => {
                let uint = arg.as_uint()?;
                if uint != 0 {
                    write!(buf, "{:o}", uint)?;
                }
                if flags.alt_form && prec.unwrap_or(0) <= buf.len() + 1 {
                    prec = Some(buf.len() + 1);
                }
                buf
            }
            CS::u => {
                let uint = arg.as_uint()?;
                if uint != 0 {
                    write!(buf, "{}", uint)?;
                }
                buf
            }
            CS::d => {
                let arg_i = arg.as_sint()?;
                if arg_i < 0 {
                    prefix = "-";
                } else if flags.mark_pos {
                    prefix = "+";
                } else if flags.pad_pos {
                    prefix = " ";
                }
                if arg_i != 0 {
                    write!(buf, "{}", arg_i.unsigned_abs())?;
                }
                buf
            }
            CS::c => {
                // also 'C'
                flags.zero_pad = false;
                buf.push(arg.as_char()?);
                buf
            }
            CS::s => {
                // also 'S'
                let s = arg.as_str(buf)?;
                let p = prec.unwrap_or(s.len()).min(s.len());
                prec = Some(p);
                flags.zero_pad = false;
                &s[..p]
            }
        };
        // Numeric output should be empty iff the value is 0.
        if spec_is_numeric && body.is_empty() {
            debug_assert!(arg.as_uint().unwrap() == 0);
        }

        // Decide if we want to apply thousands grouping to the body, and compute its size.
        // Note we have already errored out if grouped is set and this is non-numeric.
        let wants_grouping = flags.grouped && locale.thousands_sep.is_some();
        let body_len = match wants_grouping {
            true => body.len() + locale.separator_count(body.len()),
            false => body.len(),
        };

        // Resolve the precision.
        // In the case of a non-numeric conversion, update the precision to at least the
        // length of the string.
        let prec = if !spec_is_numeric {
            prec.unwrap_or(body_len)
        } else {
            prec.unwrap_or(1).max(body_len)
        };

        let prefix_len = prefix.len();
        let unpadded_width = prefix_len.checked_add(prec).ok_or(Error::Overflow)?;
        let width = width.max(unpadded_width);

        // Pad on the left with spaces to the desired width?
        if !flags.left_adj && !flags.zero_pad {
            pad(f, ' ', width, unpadded_width)?;
        }

        // Output any prefix.
        f.write_str(prefix)?;

        // Pad after the prefix with zeros to the desired width?
        if !flags.left_adj && flags.zero_pad {
            pad(f, '0', width, unpadded_width)?;
        }

        // Pad on the left to the given precision?
        pad(f, '0', prec, body_len)?;

        // Output the actual value, perhaps with grouping.
        if wants_grouping {
            f.write_str(&locale.apply_grouping(body))?;
        } else {
            f.write_str(body)?;
        }

        // Pad on the right with spaces if we are left adjusted?
        if flags.left_adj {
            pad(f, ' ', width, unpadded_width)?;
        }

        out_len = out_len.checked_add(width).ok_or(Error::Overflow)?;
    }

    // Too many args?
    if args.next().is_some() {
        return Err(Error::ExtraArg);
    }
    Ok(out_len)
}
