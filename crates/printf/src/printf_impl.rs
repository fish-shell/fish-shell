/** Rust printf implementation, based on musl. */
use super::arg::Arg;
use super::fmt_fp::format_float;
use super::locale::Locale;
use std::fmt::{self, Write};
use std::mem;
use std::result::Result;
use unicode_segmentation::UnicodeSegmentation;
use unicode_width::UnicodeWidthStr;

#[cfg(feature = "widestring")]
use widestring::Utf32Str as wstr;

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
        }
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

// A helper type with convenience functions for format strings.
pub trait FormatString {
    // Return true if we are empty.
    fn is_empty(&self) -> bool;

    // Return the character at a given index, or None if out of bounds.
    // Note the index is a count of characters, not bytes.
    fn at(&self, index: usize) -> Option<char>;

    // Advance by the given number of characters.
    fn advance_by(&mut self, n: usize);

    // Read a sequence of characters to be output literally, advancing the cursor.
    // The characters may optionally be stored in the given buffer.
    // This handles a tail of %%.
    fn take_literal<'a: 'b, 'b>(&'a mut self, buffer: &'b mut String) -> &'b str;
}

impl FormatString for &str {
    fn is_empty(&self) -> bool {
        (*self).is_empty()
    }

    fn at(&self, index: usize) -> Option<char> {
        self.chars().nth(index)
    }

    fn advance_by(&mut self, n: usize) {
        let mut chars = self.chars();
        for _ in 0..n {
            let c = chars.next();
            assert!(c.is_some(), "FormatString::advance(): index out of bounds");
        }
        *self = chars.as_str();
    }

    fn take_literal<'a: 'b, 'b>(&'a mut self, _buffer: &'b mut String) -> &'b str {
        // Count length of non-percent characters.
        let non_percents: usize = self
            .chars()
            .take_while(|&c| c != '%')
            .map(|c| c.len_utf8())
            .sum();
        // Take only an even number of percents. Note we know these have byte length 1.
        let percent_pairs = self[non_percents..]
            .chars()
            .take_while(|&c| c == '%')
            .count()
            / 2;
        let (prefix, rest) = self.split_at(non_percents + percent_pairs * 2);
        *self = rest;
        // Trim half of the trailing percent characters from the prefix.
        &prefix[..prefix.len() - percent_pairs]
    }
}

#[cfg(feature = "widestring")]
impl FormatString for &wstr {
    fn is_empty(&self) -> bool {
        (*self).is_empty()
    }

    fn at(&self, index: usize) -> Option<char> {
        self.as_char_slice().get(index).copied()
    }

    fn advance_by(&mut self, n: usize) {
        *self = &self[n..];
    }

    fn take_literal<'a: 'b, 'b>(&'a mut self, buffer: &'b mut String) -> &'b str {
        let s = self.as_char_slice();
        let non_percents = s.iter().take_while(|&&c| c != '%').count();
        // Take only an even number of percents.
        let percent_pairs: usize = s[non_percents..].iter().take_while(|&&c| c == '%').count() / 2;
        *self = &self[non_percents + percent_pairs * 2..];
        buffer.clear();
        buffer.extend(s[..non_percents + percent_pairs].iter());
        buffer.as_str()
    }
}

// Read an int from a format string, stopping at the first non-digit.
// Negative values are not supported.
// If there are no digits, return 0.
// Adjust the format string to point to the char after the int.
fn get_int(fmt: &mut impl FormatString) -> Result<usize, Error> {
    use Error::Overflow;
    let mut i: usize = 0;
    while let Some(digit) = fmt.at(0).and_then(|c| c.to_digit(10)) {
        i = i.checked_mul(10).ok_or(Overflow)?;
        i = i.checked_add(digit as usize).ok_or(Overflow)?;
        fmt.advance_by(1);
    }
    Ok(i)
}

// Read a conversion prefix from a format string, advancing it.
fn get_prefix(fmt: &mut impl FormatString) -> ConversionPrefix {
    use ConversionPrefix as CP;
    let prefix = match fmt.at(0).unwrap_or('\0') {
        'h' if fmt.at(1) == Some('h') => CP::hh,
        'h' => CP::h,
        'l' if fmt.at(1) == Some('l') => CP::ll,
        'l' => CP::l,
        'j' => CP::j,
        't' => CP::t,
        'z' => CP::z,
        'L' => CP::L,
        _ => CP::Empty,
    };
    fmt.advance_by(match prefix {
        CP::Empty => 0,
        CP::hh | CP::ll => 2,
        _ => 1,
    });
    prefix
}

// Read an (optionally prefixed) format specifier, such as d, Lf, etc.
// Adjust the cursor to point to the char after the specifier.
fn get_specifier(fmt: &mut impl FormatString) -> Result<ConversionSpec, Error> {
    let prefix = get_prefix(fmt);
    // Awkwardly placed hack to disallow %lC and %lS, since we otherwise treat
    // them as the same.
    if prefix != ConversionPrefix::Empty && matches!(fmt.at(0), Some('C' | 'S')) {
        return Err(Error::BadFormatString);
    }
    let spec = fmt
        .at(0)
        .and_then(ConversionSpec::from_char)
        .ok_or(Error::BadFormatString)?;
    if !spec.supports_prefix(prefix) {
        return Err(Error::BadFormatString);
    }
    fmt.advance_by(1);
    Ok(spec)
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
///
/// # Example
///
/// ```
/// use fish_printf::{sprintf_locale, ToArg, FormatString, locale};
/// use std::fmt::Write;
///
/// let mut output = String::new();
/// let fmt: &str = "%'0.2f";
/// let mut args = [1234567.89.to_arg()];
///
/// let result = sprintf_locale(&mut output, fmt, &locale::EN_US_LOCALE, &mut args);
///
/// assert!(result == Ok(12));
/// assert_eq!(output, "1,234,567.89");
/// ```
pub fn sprintf_locale(
    f: &mut impl Write,
    fmt: impl FormatString,
    locale: &Locale,
    args: &mut [Arg],
) -> Result<usize, Error> {
    use ConversionSpec as CS;
    let mut s = fmt;
    let mut args = args.iter_mut();
    let mut out_len: usize = 0;

    // Shared storage for the output of the conversion specifier.
    let buf = &mut String::new();
    'main: while !s.is_empty() {
        buf.clear();

        // Handle literal text and %% format specifiers.
        let lit = s.take_literal(buf);
        if !lit.is_empty() {
            f.write_str(lit)?;
            out_len = out_len
                .checked_add(lit.chars().count())
                .ok_or(Error::Overflow)?;
            continue 'main;
        }

        // Consume the % at the start of the format specifier.
        debug_assert!(s.at(0) == Some('%'));
        s.advance_by(1);

        // Read modifier flags. '-' and '0' flags are mutually exclusive.
        let mut flags = ModifierFlags::default();
        while flags.try_set(s.at(0).unwrap_or('\0')) {
            s.advance_by(1);
        }
        if flags.left_adj {
            flags.zero_pad = false;
        }

        // Read field width. We do not support $.
        let desired_width = if s.at(0) == Some('*') {
            let arg_width = args.next().ok_or(Error::MissingArg)?.as_sint()?;
            s.advance_by(1);
            if arg_width < 0 {
                flags.left_adj = true;
            }
            arg_width
                .unsigned_abs()
                .try_into()
                .map_err(|_| Error::Overflow)?
        } else {
            get_int(&mut s)?
        };

        // Optionally read precision. We do not support $.
        let mut desired_precision: Option<usize> = if s.at(0) == Some('.') && s.at(1) == Some('*') {
            // "A negative precision is treated as though it were missing."
            // Here we assume the precision is always signed.
            s.advance_by(2);
            let p = args.next().ok_or(Error::MissingArg)?.as_sint()?;
            p.try_into().ok()
        } else if s.at(0) == Some('.') {
            s.advance_by(1);
            Some(get_int(&mut s)?)
        } else {
            None
        };
        // Disallow precisions larger than i32::MAX, in keeping with C.
        if desired_precision.unwrap_or(0) > i32::MAX as usize {
            return Err(Error::Overflow);
        }

        // Read out the format specifier and arg.
        let conv_spec = get_specifier(&mut s)?;
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
        if spec_is_numeric && desired_precision.is_some() {
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
                let len = format_float(
                    f,
                    float,
                    desired_width,
                    desired_precision,
                    flags,
                    locale,
                    conv_spec,
                    buf,
                )?;
                out_len = out_len.checked_add(len).ok_or(Error::Overflow)?;
                continue 'main;
            }
            CS::p => {
                const PTR_HEX_DIGITS: usize = 2 * mem::size_of::<*const u8>();
                desired_precision = desired_precision.map(|p| p.max(PTR_HEX_DIGITS));
                let uint = arg.as_uint()?;
                if uint != 0 {
                    prefix = "0x";
                    write!(buf, "{uint:x}")?;
                }
                buf
            }
            CS::x | CS::X => {
                // If someone passes us a negative value, format it with the width
                // we were given.
                let lower = conv_spec.is_lower();
                let uint = arg.as_wrapping_sint()?;
                if uint != 0 {
                    if flags.alt_form {
                        prefix = if lower { "0x" } else { "0X" };
                    }
                    if lower {
                        write!(buf, "{uint:x}")?;
                    } else {
                        write!(buf, "{uint:X}")?;
                    }
                }
                buf
            }
            CS::o => {
                let uint = arg.as_uint()?;
                if uint != 0 {
                    write!(buf, "{uint:o}")?;
                }
                if flags.alt_form && desired_precision.unwrap_or(0) <= buf.len() + 1 {
                    desired_precision = Some(buf.len() + 1);
                }
                buf
            }
            CS::u => {
                let uint = arg.as_uint()?;
                if uint != 0 {
                    write!(buf, "{uint}")?;
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
                flags.zero_pad = false;
                match desired_precision {
                    Some(precision) => {
                        // from man printf(3)
                        // "the maximum number of characters to be printed from a string"
                        // We interpret this to mean the maximum width when printed, as defined by
                        // Unicode grapheme cluster width.
                        let mut byte_len = 0;
                        let mut width = 0;
                        let mut graphemes = s.graphemes(true);
                        // Iteratively add single grapheme clusters as long as the fit within the
                        // width limited by precision.
                        while width < precision {
                            match graphemes.next() {
                                Some(grapheme) => {
                                    let grapheme_width = grapheme.width();
                                    if width + grapheme_width <= precision {
                                        byte_len += grapheme.len();
                                        width += grapheme_width;
                                    } else {
                                        break;
                                    }
                                }
                                None => break,
                            }
                        }
                        let p = precision.min(width);
                        desired_precision = Some(p);
                        &s[..byte_len]
                    }
                    None => s,
                }
            }
        };
        // Numeric output should be empty iff the value is 0.
        if spec_is_numeric && body.is_empty() {
            debug_assert!(arg.as_uint().unwrap() == 0);
        }

        // Decide if we want to apply thousands grouping to the body, and compute its size.
        // Note we have already errored out if grouped is set and this is non-numeric.
        let wants_grouping = flags.grouped && locale.thousands_sep.is_some();
        let body_width = match wants_grouping {
            // We assume that text representing numbers is ASCII, so len == width.
            true => body.len() + locale.separator_count(body.len()),
            false => body.width(),
        };

        // Resolve the precision.
        // In the case of a non-numeric conversion, update the precision to at least the
        // length of the string.
        let desired_precision = if !spec_is_numeric {
            desired_precision.unwrap_or(body_width)
        } else {
            desired_precision.unwrap_or(1).max(body_width)
        };

        let prefix_width = prefix.width();
        let unpadded_width = prefix_width
            .checked_add(desired_precision)
            .ok_or(Error::Overflow)?;
        let width = desired_width.max(unpadded_width);

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
        // TODO: why pad with 0 here?
        pad(f, '0', desired_precision, body_width)?;

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
