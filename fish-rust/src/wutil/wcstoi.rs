use num_traits::{NumCast, PrimInt};
use std::iter::Peekable;

#[derive(Debug, Copy, Clone, PartialEq, Eq)]
pub enum Error {
    Overflow,
    Empty,
    InvalidDigit,
    CharsLeft,
}

struct ParseResult {
    result: u64,
    negative: bool,
    consumed_all: bool,
    radix: u32,
    num_digits: u32,
}

/// Helper to get the current char, or \0.
fn current<Chars>(chars: &mut Peekable<Chars>) -> char
where
    Chars: Iterator<Item = char>,
{
    match chars.peek() {
        Some(c) => *c,
        None => '\0',
    }
}

struct RecognizedRadices {
    /// Octal numbers prefixed with `0`
    octal: bool,
    /// Decimal numbers, no prefix
    decimal: bool,
    /// Hexadecimal numbers prefixed with `0x`
    hexadecimal: bool,
}

impl RecognizedRadices {
    const ALL: Self = RecognizedRadices {
        octal: true,
        decimal: true,
        hexadecimal: true,
    };

    pub fn any(&self) -> bool {
        self.octal || self.decimal || self.hexadecimal
    }
}

enum Radix {
    /// Input is interpreted purely as digits of the specified radix
    Plain(u32),
    /// Input may be prefixed to indicate the radix
    Prefixed(RecognizedRadices),
}

struct ParseOptions {
    sign_prefix: bool,
    leading_whitespace: bool,
}

/// Parse the given \p src as an integer.
/// If mradix is not None, it is used as the radix; otherwise the radix is inferred:
///   - Leading 0x or 0X means 16.
///   - Leading 0 means 8.
///   - Otherwise 10.
/// The parse result contains the number as a u64, and whether it was negative.
fn fish_parse_radix<Chars>(
    ichars: Chars,
    mradix: Radix,
    options: ParseOptions,
) -> Result<ParseResult, Error>
where
    Chars: Iterator<Item = char>,
{
    if let Radix::Plain(r) = mradix {
        assert!((2..=36).contains(&r), "fish_parse_radix: invalid radix {r}");
    }
    let chars = &mut ichars.peekable();

    // Skip leading whitespace.
    if options.leading_whitespace {
        while current(chars).is_whitespace() {
            chars.next();
        }
    }

    if chars.peek().is_none() {
        return Err(Error::Empty);
    }

    // Consume leading +/-.
    let mut negative = false;
    if options.sign_prefix {
        if current(chars) == '+' {
            chars.next();
        } else if current(chars) == '-' {
            negative = true;
            chars.next();
        }
    }

    let mut num_digits = 0;

    // Determine the radix.
    let radix = match mradix {
        Radix::Plain(radix) => radix,
        Radix::Prefixed(recognize) if current(chars) == '0' => {
            // Leading `0` - either an octal prefix, part of a hex prefix, or a leading zero in a
            // decimal number if octal numbers aren't allowed.

            // Skip the `0`
            chars.next();

            if recognize.hexadecimal && matches!(current(chars), 'x' | 'X') {
                // Skip the `x`
                chars.next();
                16
            } else if recognize.octal {
                // If no more numbers follow, that's fine, then it's just 0.
                num_digits += 1;
                8
            } else if recognize.decimal {
                // Octal numbers are not allowed, so this must simply be a leading zero of a decimal
                // number.
                num_digits += 1;
                10
            } else {
                // no prefix, and no (un-prefixed) decimal numbers allowed.
                return Err(Error::InvalidDigit);
            }
        }
        Radix::Prefixed(recognize) if recognize.decimal => 10,
        _ => {
            // No valid prefix, and no decimal numbers allowed.
            return Err(Error::InvalidDigit);
        }
    };

    // Compute as u64.
    let mut result: u64 = 0;
    while let Some(digit) = current(chars).to_digit(radix) {
        result = result
            .checked_mul(radix as u64)
            .and_then(|r| r.checked_add(digit as u64))
            .ok_or(Error::Overflow)?;
        chars.next();
        num_digits += 1;
    }

    // Did we consume at least one char?
    if num_digits == 0 {
        return Err(Error::InvalidDigit);
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
        radix,
        num_digits,
    })
}

/// Parse some iterator over Chars into some Integer type, optionally with a radix.
fn fish_wcstoi_impl<Int, Chars>(src: Chars, mradix: Radix, consume_all: bool) -> Result<Int, Error>
where
    Chars: Iterator<Item = char>,
    Int: PrimInt,
{
    let bits = Int::zero().count_zeros();
    assert!(bits <= 64, "fish_wcstoi: Int must be <= 64 bits");
    let signed = Int::min_value() < Int::zero();

    let ParseResult {
        result,
        negative,
        consumed_all,
        ..
    } = fish_parse_radix(
        src,
        mradix,
        ParseOptions {
            sign_prefix: true,
            leading_whitespace: true,
        },
    )?;

    if !signed && negative {
        Err(Error::InvalidDigit)
    } else if consume_all && !consumed_all {
        Err(Error::CharsLeft)
    } else if !signed || !negative {
        match Int::from(result) {
            Some(r) => Ok(r),
            None => Err(Error::Overflow),
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
    Chars: Iterator<Item = char>,
    Int: PrimInt,
{
    fish_wcstoi_impl(src, Radix::Prefixed(RecognizedRadices::ALL), false)
}

/// Convert the given wide string to an integer using the given radix.
/// Leading whitespace is skipped.
pub fn fish_wcstoi_radix<Int, Chars>(src: Chars, radix: u32) -> Result<Int, Error>
where
    Chars: Iterator<Item = char>,
    Int: PrimInt,
{
    fish_wcstoi_impl(src, Radix::Plain(radix), false)
}

pub fn fish_wcstoi_radix_all<Int, Chars>(
    src: Chars,
    radix: Option<u32>,
    consume_all: bool,
) -> Result<Int, Error>
where
    Chars: Iterator<Item = char>,
    Int: PrimInt,
{
    let radix = if let Some(radix) = radix {
        Radix::Plain(radix)
    } else {
        Radix::Prefixed(RecognizedRadices::ALL)
    };
    fish_wcstoi_impl(src, radix, consume_all)
}

#[cfg(test)]
mod tests {
    use super::*;

    fn test_min_max<Int: PrimInt + std::fmt::Display + std::fmt::Debug>(min: Int, max: Int) {
        assert_eq!(fish_wcstoi(min.to_string().chars()), Ok(min));
        assert_eq!(fish_wcstoi(max.to_string().chars()), Ok(max));
    }

    #[test]
    fn tests() {
        let run1 = |s: &str| -> Result<i32, Error> { fish_wcstoi(s.chars()) };
        let run1_rad =
            |s: &str, radix: u32| -> Result<i32, Error> { fish_wcstoi_radix(s.chars(), radix) };
        assert_eq!(run1(""), Err(Error::Empty));
        assert_eq!(run1("   \n   "), Err(Error::Empty));
        assert_eq!(run1("0"), Ok(0));
        assert_eq!(run1("-0"), Ok(0));
        assert_eq!(run1("+0"), Ok(0));
        assert_eq!(run1("+-0"), Err(Error::InvalidDigit));
        assert_eq!(run1("-+0"), Err(Error::InvalidDigit));
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
        assert_eq!(run1("  x345"), Err(Error::InvalidDigit));
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
}
