pub use super::errors::Error;
use crate::wchar::prelude::*;
use num_traits::{NumCast, PrimInt};
use std::default::Default;
use std::iter::{Fuse, Peekable};
use std::result::Result;

struct ParseResult {
    result: u64,
    negative: bool,
    consumed: usize,
}

#[derive(Copy, Clone, Debug, Default)]
pub struct Options {
    /// If set, and the requested type is unsigned, then negative values are wrapped
    /// to positive, as strtoul does.
    /// For example, strtoul("-2") returns ULONG_MAX - 1.
    pub wrap_negatives: bool,

    /// The radix, or None to infer it.
    pub mradix: Option<u32>,
}

struct CharsIterator<Iter: Iterator<Item = char>> {
    chars: Peekable<Fuse<Iter>>,
    consumed: usize,
}

impl<Iter: Iterator<Item = char>> CharsIterator<Iter> {
    /// Get the current char, or \0.
    fn current(&mut self) -> char {
        self.peek().unwrap_or('\0')
    }

    /// Get the current char, or None.
    fn peek(&mut self) -> Option<char> {
        self.chars.peek().copied()
    }

    /// Get the next char, incrementing self.consumed.
    fn next(&mut self) -> Option<char> {
        let res = self.chars.next();
        if res.is_some() {
            self.consumed += 1;
        }
        res
    }
}

/// Parse the given \p src as an integer.
/// If mradix is not None, it is used as the radix; otherwise the radix is inferred:
///   - Leading 0x or 0X means 16.
///   - Leading 0 means 8.
///   - Otherwise 10.
/// The parse result contains the number as a u64, and whether it was negative.
fn parse_radix<Iter: Iterator<Item = char>>(
    iter: Iter,
    mradix: Option<u32>,
    error_if_negative: bool,
) -> Result<ParseResult, Error> {
    if let Some(r) = mradix {
        assert!((2..=36).contains(&r), "parse_radix: invalid radix {r}");
    }

    // Construct a CharsIterator to keep track of how many we consume.
    let mut chars = CharsIterator {
        chars: iter.fuse().peekable(),
        consumed: 0,
    };

    // Skip leading whitespace.
    while chars.current().is_whitespace() {
        chars.next();
    }

    if chars.peek().is_none() {
        return Err(Error::Empty);
    }

    // Consume leading +/-.
    let mut negative;
    match chars.current() {
        '-' | '+' => {
            negative = chars.current() == '-';
            chars.next();
        }
        _ => negative = false,
    }

    if negative && error_if_negative {
        return Err(Error::InvalidChar);
    }

    // We eagerly attempt to parse "0" as octal and "0x" as hex, but
    // we may backtrack to just returning 0.
    let mut leading_zero_result: Option<ParseResult> = None;

    // Determine the radix.
    let radix = if let Some(radix) = mradix {
        radix
    } else if chars.current() == '0' {
        chars.next();
        leading_zero_result = Some(ParseResult {
            result: 0,
            negative: false,
            consumed: chars.consumed,
        });
        match chars.current() {
            'x' | 'X' => {
                chars.next();
                16
            }
            c if ('0'..='9').contains(&c) => 8,
            _ => {
                // Just a 0.
                return Ok(leading_zero_result.unwrap());
            }
        }
    } else {
        10
    };

    // Compute as u64.
    let start_consumed = chars.consumed;
    let mut result: u64 = 0;
    while let Some(digit) = chars.current().to_digit(radix) {
        result = result
            .checked_mul(radix as u64)
            .and_then(|r| r.checked_add(digit as u64))
            .ok_or(Error::Overflow)?;
        chars.next();
    }

    // Did we consume at least one char after the prefix?
    // If not, but we also had a leading 0 (say 08 or 0x), then we just parsed a zero.
    let consumed = chars.consumed;
    if consumed == start_consumed {
        if let Some(leading_zero_result) = leading_zero_result {
            return Ok(leading_zero_result);
        }
        return Err(Error::InvalidChar);
    }

    // Do not return -0.
    if result == 0 {
        negative = false;
    }
    Ok(ParseResult {
        result,
        negative,
        consumed,
    })
}

/// Parse some iterator over Chars into some Integer type, optionally with a radix.
fn wcstoi_impl<Int, Chars>(
    src: Chars,
    options: Options,
    out_consumed: &mut usize,
) -> Result<Int, Error>
where
    Chars: Iterator<Item = char>,
    Int: PrimInt,
{
    let bits = Int::zero().count_zeros();
    assert!(bits <= 64, "wcstoi: Int must be <= 64 bits");
    let signed = Int::min_value() < Int::zero();

    let Options {
        wrap_negatives,
        mradix,
    } = options;

    let ParseResult {
        result,
        negative,
        consumed,
    } = parse_radix(src, mradix, !signed && !wrap_negatives)?;
    *out_consumed = consumed;

    assert!(!negative || result > 0, "Should never get negative zero");

    if !negative {
        match Int::from(result) {
            Some(r) => Ok(r),
            None => Err(Error::Overflow),
        }
    } else if !signed {
        // strtoul documents "if there was a leading minus sign, the negation of the result of the conversion".
        // However in practice it's modulo the base. For example strtoul("-1") returns ULONG_MAX.
        // We wish to check if `val + base` is in the range [0, base), where val is negative.
        // Rewrite this as `base - -val`; this will be in range iff val is at least 1 and less than base.
        assert!(result > 0, "Should never get negative zero");
        let max_val = Int::max_value().to_u64().unwrap();
        if result > max_val {
            Err(Error::Overflow)
        } else {
            Ok(Int::from(max_val - result + 1).expect("value should be in range"))
        }
    } else {
        assert!(signed && negative);
        // Signed type, so convert to s64.
        // Careful of the most negative value.
        if bits == 64 && result == 1 << 63 {
            return Ok(Int::min_value());
        }
        <i64 as NumCast>::from(result)
            .and_then(|r| r.checked_neg())
            .and_then(|r| Int::from(r))
            .ok_or(Error::Overflow)
    }
}

/// Convert the given wide string to an integer.
/// The semantics here match wcstol():
///  - Leading whitespace is skipped.
///  - 0 means octal, 0x means hex
///  - Leading + is supported.
pub fn wcstoi<Int, Chars>(src: Chars) -> Result<Int, Error>
where
    Chars: IntoCharIter,
    Int: PrimInt,
{
    wcstoi_impl(src.chars(), Default::default(), &mut 0)
}

/// Convert the given wide string to an integer using the given radix.
/// Leading whitespace is skipped.
pub fn wcstoi_opts<Int, Chars>(src: Chars, options: Options) -> Result<Int, Error>
where
    Chars: IntoCharIter,
    Int: PrimInt,
{
    wcstoi_impl(src.chars(), options, &mut 0)
}

/// Convert the given wide string to an integer.
/// The semantics here match wcstol():
///  - Leading whitespace is skipped.
///  - 0 means octal, 0x means hex
///  - Leading + is supported.
/// The number of consumed characters is returned in out_consumed.
pub fn wcstoi_partial<Int, Chars>(
    src: Chars,
    options: Options,
    out_consumed: &mut usize,
) -> Result<Int, Error>
where
    Chars: IntoCharIter,
    Int: PrimInt,
{
    wcstoi_impl(src.chars(), options, out_consumed)
}

/// A historic "enhanced" version of wcstol.
/// Leading whitespace is ignored (per wcstol).
/// Trailing whitespace is also ignored.
/// Trailing characters other than whitespace are errors.
pub fn fish_wcstol_radix(mut src: &wstr, radix: u32) -> Result<i64, Error> {
    // Unlike wcstol, we do not infer the radix.
    assert!(radix > 0, "radix cannot be 0");
    let options: Options = Options {
        mradix: Some(radix),
        ..Default::default()
    };
    let mut consumed = 0;
    let result = wcstoi_partial(src, options, &mut consumed)?;
    // Skip trailing whitespace, erroring if we encounter a non-whitespace character.
    src = src.slice_from(consumed);
    while !src.is_empty() && src.char_at(0).is_whitespace() {
        src = src.slice_from(1);
    }
    if !src.is_empty() {
        return Err(Error::InvalidChar);
    }
    Ok(result)
}

/// Variant of fish_wcstol_radix which assumes base 10.
pub fn fish_wcstol(src: &wstr) -> Result<i64, Error> {
    fish_wcstol_radix(src, 10)
}

/// Variant of fish_wcstol for ints, erroring if it does not fit.
pub fn fish_wcstoi(src: &wstr) -> Result<i32, Error> {
    let res = fish_wcstol(src)?;
    if let Ok(val) = res.try_into() {
        Ok(val)
    } else {
        Err(Error::Overflow)
    }
}

/// Historic "enhanced" version of wcstoul.
/// Leading minus is considered invalid.
/// Leading whitespace is ignored (per wcstoul).
/// Trailing whitespace is also ignored.
pub fn fish_wcstoul(mut src: &wstr) -> Result<u64, Error> {
    // Skip leading whitespace.
    while !src.is_empty() && src.char_at(0).is_whitespace() {
        src = src.slice_from(1);
    }
    // Disallow minus as the first character to avoid questionable wrap-around.
    if src.is_empty() || src.char_at(0) == '-' {
        return Err(Error::InvalidChar);
    }
    let options: Options = Options {
        mradix: Some(10),
        ..Default::default()
    };
    let mut consumed = 0;
    let result = wcstoi_partial(src, options, &mut consumed)?;
    // Skip trailing whitespace.
    src = src.slice_from(consumed);
    while !src.is_empty() && src.char_at(0).is_whitespace() {
        src = src.slice_from(1);
    }
    if !src.is_empty() {
        return Err(Error::InvalidChar);
    }
    Ok(result)
}

#[cfg(test)]
mod tests {
    use super::*;

    fn test_min_max<Int: PrimInt + std::fmt::Display + std::fmt::Debug>(min: Int, max: Int) {
        assert_eq!(wcstoi(min.to_string().chars()), Ok(min));
        assert_eq!(wcstoi(max.to_string().chars()), Ok(max));
    }

    #[test]
    fn test_signed() {
        let run1 = |s: &str| -> Result<i32, Error> { wcstoi(s.chars()) };
        let run1_rad = |s: &str, radix: u32| -> Result<i32, Error> {
            wcstoi_opts(
                s.chars(),
                Options {
                    mradix: Some(radix),
                    ..Default::default()
                },
            )
        };
        assert_eq!(run1(""), Err(Error::Empty));
        assert_eq!(run1("   \n   "), Err(Error::Empty));
        assert_eq!(run1("0"), Ok(0));
        assert_eq!(run1("-0"), Ok(0));
        assert_eq!(run1("+0"), Ok(0));
        assert_eq!(run1("+00"), Ok(0));
        assert_eq!(run1("-00"), Ok(0));
        assert_eq!(run1("+0x00"), Ok(0));
        assert_eq!(run1("-0x00"), Ok(0));
        assert_eq!(run1("+-0"), Err(Error::InvalidChar));
        assert_eq!(run1("-+0"), Err(Error::InvalidChar));
        assert_eq!(run1("5"), Ok(5));
        assert_eq!(run1("-5"), Ok(-5));
        assert_eq!(run1("123"), Ok(123));
        assert_eq!(run1("+123"), Ok(123));
        assert_eq!(run1("-123"), Ok(-123));
        assert_eq!(run1("123"), Ok(123));
        assert_eq!(run1("+0x123"), Ok(291));
        assert_eq!(run1("-0x123"), Ok(-291));
        assert_eq!(run1("+0X123"), Ok(291));
        assert_eq!(run1("-0X123"), Ok(-291));
        assert_eq!(run1("+0123"), Ok(83));
        assert_eq!(run1("-0123"), Ok(-83));
        assert_eq!(run1("  345  "), Ok(345));
        assert_eq!(run1("  -345  "), Ok(-345));
        assert_eq!(run1("  x345"), Err(Error::InvalidChar));
        assert_eq!(run1("456x"), Ok(456));
        assert_eq!(run1("456 x"), Ok(456));
        assert_eq!(run1("99999999999999999999999"), Err(Error::Overflow));
        assert_eq!(run1("-99999999999999999999999"), Err(Error::Overflow));
        // This is subtle. "567" in base 8 is "375" in base 10. The final "8" is not converted.
        assert_eq!(run1_rad("5678", 8), Ok(375));

        test_min_max(std::i8::MIN, std::i8::MAX);
        test_min_max(std::i16::MIN, std::i16::MAX);
        test_min_max(std::i32::MIN, std::i32::MAX);
        test_min_max(std::i64::MIN, std::i64::MAX);
        test_min_max(std::u8::MIN, std::u8::MAX);
        test_min_max(std::u16::MIN, std::u16::MAX);
        test_min_max(std::u32::MIN, std::u32::MAX);
        test_min_max(std::u64::MIN, std::u64::MAX);
    }

    #[test]
    fn test_unsigned() {
        fn negu(x: u64) -> u64 {
            std::u64::MAX - x + 1
        }

        let run1 = |s: &str| -> Result<u64, Error> {
            wcstoi_opts(
                s.chars(),
                Options {
                    wrap_negatives: true,
                    mradix: None,
                },
            )
        };
        let run1_rad = |s: &str, radix: u32| -> Result<u64, Error> {
            wcstoi_opts(
                s.chars(),
                Options {
                    wrap_negatives: true,
                    mradix: Some(radix),
                },
            )
        };
        assert_eq!(run1("-5"), Ok(negu(5)));
        assert_eq!(run1(""), Err(Error::Empty));
        assert_eq!(run1("   \n   "), Err(Error::Empty));
        assert_eq!(run1("0"), Ok(0));
        assert_eq!(run1("-0"), Ok(0));
        assert_eq!(run1("+0"), Ok(0));
        assert_eq!(run1("+00"), Ok(0));
        assert_eq!(run1("-00"), Ok(0));
        assert_eq!(run1("+0x00"), Ok(0));
        assert_eq!(run1("-0x00"), Ok(0));
        assert_eq!(run1("+-0"), Err(Error::InvalidChar));
        assert_eq!(run1("-+0"), Err(Error::InvalidChar));
        assert_eq!(run1("5"), Ok(5));
        assert_eq!(run1("-5"), Ok(negu(5)));
        assert_eq!(run1("123"), Ok(123));
        assert_eq!(run1("+123"), Ok(123));
        assert_eq!(run1("-123"), Ok(negu(123)));
        assert_eq!(run1("123"), Ok(123));
        assert_eq!(run1("+0x123"), Ok(291));
        assert_eq!(run1("-0x123"), Ok(negu(291)));
        assert_eq!(run1("+0X123"), Ok(291));
        assert_eq!(run1("-0X123"), Ok(negu(291)));
        assert_eq!(run1("+0123"), Ok(83));
        assert_eq!(run1("-0123"), Ok(negu(83)));
        assert_eq!(run1("  345  "), Ok(345));
        assert_eq!(run1("  -345  "), Ok(negu(345)));
        assert_eq!(run1("  x345"), Err(Error::InvalidChar));
        assert_eq!(run1("456x"), Ok(456));
        assert_eq!(run1("456 x"), Ok(456));
        assert_eq!(run1("99999999999999999999999"), Err(Error::Overflow));
        assert_eq!(run1("-99999999999999999999999"), Err(Error::Overflow));
        // This is subtle. "567" in base 8 is "375" in base 10. The final "8" is not converted.
        assert_eq!(run1_rad("5678", 8), Ok(375));
    }

    #[test]
    fn test_wrap_neg() {
        fn negu(x: u64) -> u64 {
            std::u64::MAX - x + 1
        }

        let run1 = |s: &str, opts: Options| -> Result<u64, Error> { wcstoi_opts(s, opts) };
        let mut opts = Options::default();
        assert_eq!(run1("-123", opts), Err(Error::InvalidChar));
        assert_eq!(run1("-0x123", opts), Err(Error::InvalidChar));
        assert_eq!(run1("-0", opts), Err(Error::InvalidChar));

        opts.wrap_negatives = true;
        assert_eq!(run1("-123", opts), Ok(negu(123)));
        assert_eq!(run1("-0x123", opts), Ok(negu(0x123)));
        assert_eq!(run1("-0", opts), Ok(0));
    }

    #[test]
    fn test_partial() {
        let run1 = |s: &str| -> (i32, usize) {
            let mut consumed = 0;
            let res = wcstoi_partial(s, Default::default(), &mut consumed)
                .expect("Should have parsed an int");
            (res, consumed)
        };

        assert_eq!(run1("0"), (0, 1));
        assert_eq!(run1("-0"), (0, 2));
        assert_eq!(run1(" -1  "), (-1, 3));
        assert_eq!(run1(" +1  "), (1, 3));
        assert_eq!(run1("  345  "), (345, 5));
        assert_eq!(run1(" -345  "), (-345, 5));
        assert_eq!(run1("  0345  "), (229, 6));
        assert_eq!(run1("  +0345  "), (229, 7));
        assert_eq!(run1("  -0345  "), (-229, 7));
        assert_eq!(run1(" 0x345  "), (0x345, 6));
        assert_eq!(run1(" -0x345  "), (-0x345, 7));
        assert_eq!(run1("08"), (0, 1));
        assert_eq!(run1("0x"), (0, 1));
        assert_eq!(run1("0xx"), (0, 1));
    }

    #[test]
    fn test_fish_wcstol() {
        assert_eq!(fish_wcstol(L!("0")), Ok(0));
        assert_eq!(fish_wcstol(L!("10")), Ok(10));
        assert_eq!(fish_wcstol(L!("  10")), Ok(10));
        assert_eq!(fish_wcstol(L!("  10  ")), Ok(10));
        assert_eq!(fish_wcstol(L!("-10")), Ok(-10));
        assert_eq!(fish_wcstol(L!("  +10  ")), Ok(10));
        assert_eq!(fish_wcstol(L!("10foo")), Err(Error::InvalidChar));
        assert_eq!(fish_wcstol(L!("10.5")), Err(Error::InvalidChar));
        assert_eq!(fish_wcstol(L!("10   x  ")), Err(Error::InvalidChar));
    }

    #[test]
    fn test_fish_wcstoi() {
        assert_eq!(fish_wcstoi(L!("0")), Ok(0));
        assert_eq!(fish_wcstoi(L!("10")), Ok(10));
        assert_eq!(fish_wcstoi(L!("  10")), Ok(10));
        assert_eq!(fish_wcstoi(L!("  10  ")), Ok(10));
        assert_eq!(fish_wcstoi(L!("-10")), Ok(-10));
        assert_eq!(fish_wcstoi(L!("  +10  ")), Ok(10));
        assert_eq!(fish_wcstoi(L!("  2147483647  ")), Ok(2147483647));
        assert_eq!(fish_wcstoi(L!("  2147483648  ")), Err(Error::Overflow));
        assert_eq!(fish_wcstoi(L!("  -2147483647  ")), Ok(-2147483647));
        assert_eq!(fish_wcstoi(L!("  -2147483648  ")), Ok(-2147483648));
        assert_eq!(fish_wcstoi(L!("  -2147483649  ")), Err(Error::Overflow));
        assert_eq!(fish_wcstoi(L!("10foo")), Err(Error::InvalidChar));
        assert_eq!(fish_wcstoi(L!("10.5")), Err(Error::InvalidChar));
    }

    #[test]
    fn test_fish_wcstoul() {
        assert_eq!(fish_wcstoul(L!("0")), Ok(0));
        assert_eq!(fish_wcstoul(L!("10")), Ok(10));
        assert_eq!(fish_wcstoul(L!(" +10")), Ok(10));
        assert_eq!(fish_wcstoul(L!("  -10")), Err(Error::InvalidChar));
        assert_eq!(fish_wcstoul(L!("  10  ")), Ok(10));
        assert_eq!(fish_wcstoul(L!("10foo")), Err(Error::InvalidChar));
        assert_eq!(fish_wcstoul(L!("10.5")), Err(Error::InvalidChar));
        assert_eq!(fish_wcstoul(L!("18446744073709551615")), Ok(u64::MAX));
    }
}
