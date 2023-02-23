use num_traits::{NumCast, PrimInt};

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

/// A peekable iterator adapter that counts the number of characters consumed.
struct CountedPeekable<I: Iterator> {
    iter: I,
    current: Option<char>,
    consumed: usize,
}

impl<I: Iterator<Item = char>> CountedPeekable<I> {
    pub fn new(mut iter: I) -> Self {
        Self {
            current: iter.next(),
            iter,
            consumed: 0,
        }
    }

    pub fn is_exhausted(&self) -> bool {
        self.current.is_none()
    }

    /// Helper to get the current char, or \0.
    pub fn current(&self) -> char {
        self.current.unwrap_or('\0')
    }

    /// Consume the current character, advancing the iterator.
    pub fn next(&mut self) {
        if self.current.is_some() {
            self.consumed += 1;
        }

        self.current = self.iter.next();
    }
}

impl ParseResult {
    fn signed_result(&self) -> Result<i64, Error> {
        let result = i64::try_from(self.result).map_err(|_| Error::Overflow)?;

        if self.negative {
            Ok(-result)
        } else {
            Ok(result)
        }
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
    allow_underscores: bool,
}

enum Sign {
    Positive,
    Negative,
}

/// Tries to consume a `+` or `-` sign, or returns `None` if no sign prefix is present.
fn parse_sign<Chars>(chars: &mut CountedPeekable<Chars>) -> Option<Sign>
where
    Chars: Iterator<Item = char>,
{
    if chars.current() == '+' {
        chars.next();
        Some(Sign::Positive)
    } else if chars.current() == '-' {
        chars.next();
        Some(Sign::Negative)
    } else {
        None
    }
}

/// Tries to parse a fixed string, case-insensitively.
fn parse_string<Chars>(chars: &mut CountedPeekable<Chars>, s: &str) -> Result<(), Error>
where
    Chars: Iterator<Item = char>,
{
    for c in s.chars() {
        if chars.current().to_ascii_lowercase() == c.to_ascii_lowercase() {
            chars.next();
        } else {
            return Err(Error::InvalidDigit);
        }
    }
    Ok(())
}

/// Parse the given \p src as an integer.
/// If mradix is not None, it is used as the radix; otherwise the radix is inferred:
///   - Leading 0x or 0X means 16.
///   - Leading 0 means 8.
///   - Otherwise 10.
/// The parse result contains the number as a u64, and whether it was negative.
fn fish_parse_radix<Chars>(
    chars: &mut CountedPeekable<Chars>,
    mradix: Radix,
    options: ParseOptions,
) -> Result<ParseResult, Error>
where
    Chars: Iterator<Item = char>,
{
    if let Radix::Plain(r) = mradix {
        assert!((2..=36).contains(&r), "fish_parse_radix: invalid radix {r}");
    }

    let skip_underscores = |chars: &mut CountedPeekable<_>| {
        if options.allow_underscores {
            while chars.current() == '_' {
                chars.next();
            }
        }
    };

    // Skip leading whitespace and underscores.
    if options.leading_whitespace {
        while chars.current().is_whitespace() {
            chars.next();
        }
    }

    skip_underscores(chars);

    if chars.is_exhausted() {
        return Err(Error::Empty);
    }

    // Consume leading +/-.
    let mut negative = false;
    if options.sign_prefix {
        if let Some(Sign::Negative) = parse_sign(chars) {
            negative = true;
        }
    }

    skip_underscores(chars);

    let mut num_digits = 0;

    // Determine the radix.
    let radix = match mradix {
        Radix::Plain(radix) => radix,
        Radix::Prefixed(recognize) if chars.current() == '0' => {
            // Leading `0` - either an octal prefix, part of a hex prefix, or a leading zero in a
            // decimal number if octal numbers aren't allowed.

            // Skip the `0`
            chars.next();
            skip_underscores(chars);

            if recognize.hexadecimal && matches!(chars.current(), 'x' | 'X') {
                // Skip the `x`
                chars.next();
                skip_underscores(chars);
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
    while let Some(digit) = chars.current().to_digit(radix) {
        result = result
            .checked_mul(radix as u64)
            .and_then(|r| r.checked_add(digit as u64))
            .ok_or(Error::Overflow)?;
        chars.next();
        num_digits += 1;

        skip_underscores(chars);
    }

    // Did we consume at least one char?
    if num_digits == 0 {
        return Err(Error::InvalidDigit);
    }

    // Do not return -0.
    if result == 0 {
        negative = false;
    }
    let consumed_all = chars.is_exhausted();

    skip_underscores(chars);

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
    let src = &mut CountedPeekable::new(src);

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
            allow_underscores: false,
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

/// Parses the fractional part of a floating-point literal (`.` and following digits).
///
/// Returns `Ok(None)` when no fractional part is present.
fn parse_float_fractional<Chars>(
    src: &mut CountedPeekable<Chars>,
    significand_radix: u32,
    allow_underscores: bool,
) -> Result<Option<f64>, Error>
where
    Chars: Iterator<Item = char>,
{
    // FIXME: do this properly instead of parsing as an integer and dividing
    if src.current() == '.' {
        // skip `.`
        src.next();

        let frac = fish_parse_radix(
            src,
            Radix::Plain(significand_radix),
            ParseOptions {
                sign_prefix: false,
                leading_whitespace: false,
                allow_underscores,
            },
        )?;

        let num_digits = i32::try_from(frac.num_digits)
            .expect("float literal should have less than 2^31 fractional digits");

        let frac = frac.result as f64 * (significand_radix as f64).powi(-num_digits);

        Ok(Some(frac))
    } else {
        Ok(None)
    }
}

/// Parses the exponent part of a floating-point literal of the given radix. For decimal literals,
/// the exponent has a base of 10, for hexadecimal it has a base of 2.
///
/// Returns `Ok(None)` when no exponent is present.
fn parse_float_exponent<Chars>(
    src: &mut CountedPeekable<Chars>,
    significand_radix: u32,
    allow_underscores: bool,
) -> Result<Option<f64>, Error>
where
    Chars: Iterator<Item = char>,
{
    let (exponent_base, has_exponent) = match significand_radix {
        10 => (10.0_f64, matches!(src.current(), 'e' | 'E')),
        16 => (2.0_f64, matches!(src.current(), 'p' | 'P')),
        _ => unreachable!("float literal integer part can only be decimal or hexadecimal"),
    };

    if has_exponent {
        // skip `e`/`p`
        src.next();

        let exponent = fish_parse_radix(
            src,
            Radix::Plain(10),
            ParseOptions {
                sign_prefix: true,
                leading_whitespace: false,
                allow_underscores,
            },
        )?;
        let exponent = exponent
            .signed_result()?
            .try_into()
            .map_err(|_| Error::Overflow)?;

        Ok(Some(exponent_base.powi(exponent)))
    } else {
        Ok(None)
    }
}

fn fish_wcstod_impl<Chars>(src: Chars, allow_underscores: bool) -> Result<(f64, usize), Error>
where
    Chars: Iterator<Item = char>,
{
    let src = &mut CountedPeekable::new(src);

    // skip leading whitespace
    while src.current().is_whitespace() {
        src.next();
    }

    let mut leading_underscore = false;
    if allow_underscores {
        // skip leading underscores
        while src.current() == '_' {
            src.next();
            leading_underscore = true;
        }
    }

    let sig_sign = parse_sign(src);

    if !leading_underscore {
        // decode `infinity`/`nan`
        match src.current() {
            'i' | 'I' => {
                parse_string(src, "infinity")?;

                let inf = if let Some(Sign::Negative) = sig_sign {
                    f64::NEG_INFINITY
                } else {
                    f64::INFINITY
                };
                return Ok((inf, src.consumed));
            }
            'n' | 'N' => {
                parse_string(src, "nan")?;

                return Ok((f64::NAN, src.consumed));
            }
            _ => {}
        }
    }

    let mut sig_int = fish_parse_radix(
        src,
        Radix::Prefixed(RecognizedRadices {
            octal: false,
            decimal: true,
            hexadecimal: true,
        }),
        ParseOptions {
            // already done above
            sign_prefix: false,
            leading_whitespace: false,
            allow_underscores,
        },
    )?;
    sig_int.negative = matches!(sig_sign, Some(Sign::Negative));

    let sig_frac = parse_float_fractional(src, sig_int.radix, allow_underscores)?.unwrap_or(0.0);
    let exponent = parse_float_exponent(src, sig_int.radix, allow_underscores)?.unwrap_or(1.0);

    let mut significand = sig_int.result as f64 + sig_frac;
    if let Some(Sign::Negative) = sig_sign {
        significand = -significand;
    }

    Ok((significand * exponent, src.consumed))
}

/// Parses a floating-point literal and returns the number of characters consumed.
pub fn fish_wcstod<Chars>(src: Chars) -> Result<(f64, usize), Error>
where
    Chars: Iterator<Item = char>,
{
    fish_wcstod_impl(src, false)
}

/// Like [`fish_wcstod()`], but allows underscore separators. Leading, trailing, and multiple
/// underscores are allowed, as are underscores next to decimal (`.`), exponent (`E`/`e`/`P`/`p`),
/// and hexadecimal (`X`/`x`) delimiters. This consumes trailing underscores. Free-floating leading
/// underscores (`"_ 3"`) are not allowed and will result in a no-parse. Underscores are not allowed
/// before or inside of `"infinity"` or `"nan"` input. Trailing underscores after `"infinity"` or
/// `"nan"` are not consumed.
pub fn fish_wcstod_underscores<Chars>(src: Chars) -> Result<(f64, usize), Error>
where
    Chars: Iterator<Item = char>,
{
    fish_wcstod_impl(src, true)
}

#[cfg(test)]
mod tests {
    use super::*;

    fn test_min_max<Int: PrimInt + std::fmt::Display + std::fmt::Debug>(min: Int, max: Int) {
        assert_eq!(fish_wcstoi(min.to_string().chars()), Ok(min));
        assert_eq!(fish_wcstoi(max.to_string().chars()), Ok(max));
    }

    #[test]
    fn wcstoi() {
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

    #[test]
    fn wcstod() {
        let run = |s: &str| fish_wcstod(s.chars());
        let run_us = |s: &str| fish_wcstod_underscores(s.chars());

        assert_eq!(run("0"), Ok((0.0, 1)));
        assert_eq!(run("+42.5.3"), Ok((42.5, 5)));
        assert_eq!(run("   -123e-4x"), Ok((-0.0123, 10)));

        assert_eq!(run(""), Err(Error::Empty));
        assert_eq!(run("1.2"), Ok((1.2, 3)));
        assert_eq!(run("1.5"), Ok((1.5, 3)));
        assert_eq!(run("-1000"), Ok((-1000.0, 5)));
        assert_eq!(run("0.12345"), Ok((0.12345, 7)));
        assert_eq!(run("nope"), Err(Error::InvalidDigit));

        assert_eq!(run_us("123"), Ok((123.0, 3)));
        assert_eq!(run_us("1_2.3_4.5_6"), Ok((12.34, 7)));
        assert_eq!(run_us("_-__1_23e-2.5"), Ok((-1.23, 11)));
        assert_eq!(run_us("1_2"), Ok((12.0, 3)));
        assert_eq!(run_us("1_._2"), Ok((1.2, 5)));
        assert_eq!(run_us("1__2"), Ok((12.0, 4)));
        assert_eq!(run_us(" 1__2 3__4 "), Ok((12.0, 5)));
        assert_eq!(run_us("1_2 3_4"), Ok((12.0, 3)));
        assert_eq!(run_us(" 1"), Ok((1.0, 2)));
        assert_eq!(run_us(" 1_"), Ok((1.0, 3)));
        assert_eq!(run_us(" 1__"), Ok((1.0, 4)));
        assert_eq!(run_us(" 1___"), Ok((1.0, 5)));
        assert_eq!(run_us(" 1___ 2___"), Ok((1.0, 5)));
        assert_eq!(run_us(" _1"), Ok((1.0, 3)));
        assert_eq!(run_us("1 "), Ok((1.0, 1)));
        assert_eq!(run_us("infinity_"), Ok((f64::INFINITY, 8)));
        assert_eq!(run_us(" -INFINITY"), Ok((-f64::INFINITY, 10)));
        assert_eq!(run_us("_-INFINITY"), Err(Error::InvalidDigit));
        assert_eq!(run_us("_infinity"), Err(Error::InvalidDigit));
        //FIXME
        //assert_eq!(run_us("nan(0)"), Ok((nan, 6)));
        //assert_eq!(run_us("nan(0)_"), Ok((nan, 6)));
        assert_eq!(run_us("_nan(0)"), Err(Error::InvalidDigit));
        // We don't strip the underscores in this commented-out test case, and the behavior is
        // implementation-defined, so we don't actually know how many characters will get consumed. On
        // macOS the strtod man page only says what happens with an alphanumeric string passed to nan(),
        // but the strtod consumes all of the characters even if there are underscores.
        // test_case(L"nan(0_1_2)", Ok((???, 3)));
        assert_eq!(run_us(" _ 1"), Err(Error::InvalidDigit));
        assert_eq!(run_us("0x_dead_beef"), Ok((0xdeadbeef_u32 as f64, 12)));
        assert_eq!(run_us("None"), Err(Error::InvalidDigit));
        assert_eq!(run_us(" None"), Err(Error::InvalidDigit));
        assert_eq!(run_us("Also none"), Err(Error::InvalidDigit));
        assert_eq!(run_us(" Also none"), Err(Error::InvalidDigit));
    }
}
