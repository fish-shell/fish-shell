pub use super::errors::Error;
use crate::wchar::IntoCharIter;
use num_traits::{NumCast, PrimInt};
use std::default::Default;
use std::iter::{Fuse, Peekable};
use std::result::Result;

struct ParseResult {
    result: u64,
    negative: bool,
    consumed_all: bool,
    consumed: usize,
}

#[derive(Copy, Clone, Debug, Default)]
pub struct Options {
    /// If set, and the requested type is unsigned, then negative values are wrapped
    /// to positive, as strtoul does.
    /// For example, strtoul("-2") returns ULONG_MAX - 1.
    pub wrap_negatives: bool,

    /// If set, it is an error to have unconsumed characters.
    pub consume_all: bool,

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
        assert!((2..=36).contains(&r), "fish_parse_radix: invalid radix {r}");
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
            consumed_all: chars.peek().is_none(),
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
    let consumed_all = chars.peek().is_none();
    Ok(ParseResult {
        result,
        negative,
        consumed_all,
        consumed,
    })
}

/// Parse some iterator over Chars into some Integer type, optionally with a radix.
fn fish_wcstoi_impl<Int, Chars>(
    src: Chars,
    options: Options,
    out_consumed: &mut usize,
) -> Result<Int, Error>
where
    Chars: Iterator<Item = char>,
    Int: PrimInt,
{
    let bits = Int::zero().count_zeros();
    assert!(bits <= 64, "fish_wcstoi: Int must be <= 64 bits");
    let signed = Int::min_value() < Int::zero();

    let Options {
        wrap_negatives,
        consume_all,
        mradix,
    } = options;

    let ParseResult {
        result,
        negative,
        consumed_all,
        consumed,
    } = parse_radix(src, mradix, !signed && !wrap_negatives)?;
    *out_consumed = consumed;

    assert!(!negative || result > 0, "Should never get negative zero");

    if consume_all && !consumed_all {
        Err(Error::CharsLeft)
    } else if !negative {
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
pub fn fish_wcstoi<Int, Chars>(src: Chars) -> Result<Int, Error>
where
    Chars: IntoCharIter,
    Int: PrimInt,
{
    fish_wcstoi_impl(src.chars(), Default::default(), &mut 0)
}

/// Convert the given wide string to an integer using the given radix.
/// Leading whitespace is skipped.
pub fn fish_wcstoi_opts<Int, Chars>(src: Chars, options: Options) -> Result<Int, Error>
where
    Chars: IntoCharIter,
    Int: PrimInt,
{
    fish_wcstoi_impl(src.chars(), options, &mut 0)
}

/// Convert the given wide string to an integer.
/// The semantics here match wcstol():
///  - Leading whitespace is skipped.
///  - 0 means octal, 0x means hex
///  - Leading + is supported.
/// The number of consumed characters is returned in out_consumed.
pub fn fish_wcstoi_partial<Int, Chars>(
    src: Chars,
    options: Options,
    out_consumed: &mut usize,
) -> Result<Int, Error>
where
    Chars: IntoCharIter,
    Int: PrimInt,
{
    fish_wcstoi_impl(src.chars(), options, out_consumed)
}

#[cfg(test)]
mod tests {
    use super::*;

    fn test_min_max<Int: PrimInt + std::fmt::Display + std::fmt::Debug>(min: Int, max: Int) {
        assert_eq!(fish_wcstoi(min.to_string().chars()), Ok(min));
        assert_eq!(fish_wcstoi(max.to_string().chars()), Ok(max));
    }

    #[test]
    fn test_signed() {
        let run1 = |s: &str| -> Result<i32, Error> { fish_wcstoi(s.chars()) };
        let run1_rad = |s: &str, radix: u32| -> Result<i32, Error> {
            fish_wcstoi_opts(
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
            fish_wcstoi_opts(
                s.chars(),
                Options {
                    wrap_negatives: true,
                    ..Default::default()
                },
            )
        };
        let run1_rad = |s: &str, radix: u32| -> Result<u64, Error> {
            fish_wcstoi_opts(
                s.chars(),
                Options {
                    wrap_negatives: true,
                    mradix: Some(radix),
                    ..Default::default()
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

        let run1 = |s: &str, opts: Options| -> Result<u64, Error> { fish_wcstoi_opts(s, opts) };
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
            let res = fish_wcstoi_partial(s, Default::default(), &mut consumed)
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
}
