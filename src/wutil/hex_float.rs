/*
 * Implementation of hex float parsing.
 * Floating suffixes (f/l/F/L) are not supported.
 *
 * Grammar:
 *
 * hexadecimal-floating-constant:
 *     hexadecimal-prefix hexadecimal-fractional-constant
 *         binary-exponent-part
 *
 *     hexadecimal-prefix hexadecimal-digit-sequence
 *         binary-exponent-part
 *
 *     hexadecimal-fractional-constant
 *         hexadecimal-digit-sequence_opt . hexadecimal-digit-sequence
 *         hexadecimal-digit-sequence .
 *
 *      binary-exponent-part:
 *            p sign_opt digit-sequence
 *            P sign_opt digit-sequence
 *
 *     hexadecimal-digit-sequence:
 *            hexadecimal-digit
 *            hexadecimal-digit-sequence hexadecimal-digit
 *
 * Note this omits an optional leading sign (+ or -).
 *
 * Hex digits may be lowercase or uppercase. The exponent is a power of 2.
 */

/// Error type for failing to parse a hexadecimal floating-point number.
/// Only syntax errors may occur.
#[derive(Debug, PartialEq, Eq, Clone, Copy)]
pub struct SyntaxError;

/// Parses a hexadecimal floating-point number from a character iterator.
///
/// # Arguments
///
/// * `chars` - An iterator over characters representing the hexadecimal float.
/// * `decimal_sep` - The character to indicate the decimal (probably a period).
///
/// # Returns
///
/// A `Result` containing either:
/// - A tuple of the parsed floating-point number (`f64`) and the number of characters consumed (`usize`), or
/// - A `SyntaxError` if the parsing fails.
pub fn parse_hex_float(
    chars: impl Iterator<Item = char>,
    decimal_sep: char,
) -> Result<(f64, usize), SyntaxError> {
    const F64_EXP_BIAS: i32 = 1023;
    let mut chars = chars.peekable();
    let mut consumed = 0;
    // Parse sign?
    let negative = match chars.peek().copied() {
        Some('+' | '-') => {
            consumed += 1;
            chars.next() == Some('-')
        }
        _ => false,
    };
    // Make a value of 1.0 or -1.0 for later.
    let sign = if negative { -1.0 } else { 1.0 };

    // Parse hex prefix.
    match (chars.next(), chars.next()) {
        (Some('0'), Some('x' | 'X')) => consumed += 2,
        _ => return Err(SyntaxError),
    }

    // We must have at least one digit.
    let mut seen_digits = false;

    // Skip leading 0s.
    while chars.next_if_eq(&'0').is_some() {
        consumed += 1;
        seen_digits = true;
    }

    // Start constructing the mantissa.
    // We move from most to least significant bits, and discard digits after we have fully
    // populated a u64. Note we don't distinguish between digits before and after the decimal
    // as that is handled by the exponent.
    let mut mantissa: u64 = 0;
    let mut shift = 64;
    let mut add_digit = |digit: u32| {
        debug_assert!(digit < 16);
        seen_digits = true;
        // Ignore digits if we would shift them out of range; that indicates excess precision.
        if shift > 0 {
            shift -= 4;
            mantissa |= u64::from(digit) << shift;
        }
    };

    // Parse digits before the decimal.
    // Record the number of digits before the decimal to inform the exponent.
    let consumed_before_decimal = consumed;
    while let Some(d) = chars.peek().and_then(|c| c.to_digit(16)) {
        consumed += 1;
        add_digit(d);
        chars.next();
    }

    // Record the number of digits before the decimal (if any), saturating.
    let decimal_point_pos: i32 = (consumed - consumed_before_decimal)
        .try_into()
        .unwrap_or(i32::MAX);

    // Optionally parse a decimal and another sequence.
    // If we have no decimal, pretend it's here anyways.
    if chars.next_if_eq(&decimal_sep).is_some() {
        consumed += 1;
        while let Some(d) = chars.peek().and_then(|c| c.to_digit(16)) {
            consumed += 1;
            add_digit(d);
            chars.next();
        }
    }

    // Must have at least one.
    if !seen_digits {
        return Err(SyntaxError);
    }

    // Try parsing the explicit exponent.
    // Saturate it - if it's huge we'll just end up returning inf.
    let mut explicit_exp: i32 = 0;
    if matches!(chars.peek(), Some('p' | 'P')) {
        chars.next();
        consumed += 1;
        // Exponent sign?
        let negative = match chars.peek() {
            Some('+' | '-') => {
                consumed += 1;
                chars.next() == Some('-')
            }
            _ => false,
        };
        // Decimal digit sequence.
        let before = consumed;
        while let Some(d) = chars.peek().and_then(|c| c.to_digit(10)) {
            explicit_exp = explicit_exp.saturating_mul(10).saturating_add(d as i32);
            consumed += 1;
            chars.next();
        }
        // Need at least one digit.
        if consumed == before {
            return Err(SyntaxError);
        }
        // Negating a non-negative value cannot overflow.
        if negative {
            explicit_exp = -explicit_exp;
        }
    }

    // Handle a zero mantissa.
    if mantissa == 0 {
        return Ok((0.0f64.copysign(sign), consumed));
    }

    // Normalize to leading 1.
    let zeros = mantissa.leading_zeros() as i32;
    mantissa <<= zeros;

    // Round the mantissa, halfways to even.
    // There are 53 bits in the mantissa (counting implicit 1, which we still have),
    // so look at the remaining bits to decide whether to round.
    let trim = 64 - 53;
    let halfway = 1 << (trim - 1);
    let rounding_bits = mantissa & ((1 << trim) - 1);
    mantissa ^= rounding_bits; // Clear the bits to trim.
    if rounding_bits > halfway || (rounding_bits == halfway && (mantissa & (1 << trim)) != 0) {
        // Round the mantissa up.
        // If it overflows, then increase the exponent's magnitude by 1, and set the mantissa to 1.
        mantissa = if let Some(m) = mantissa.checked_add(1 << trim) {
            m
        } else {
            explicit_exp = explicit_exp.saturating_add(1);
            1
        };
    }

    // Compute the exponent (base 2).
    // This has contributions from the explicit exponent,
    // hex digits (e.g. 0x1000p0 has an exponent of 8), and leading zeros, with a plus one
    // as hex 0001 should have an exponent of 0.
    let exponent = decimal_point_pos
        .saturating_mul(4)
        .saturating_add(explicit_exp)
        .saturating_sub(1 + zeros);

    // Handle infinity.
    if exponent > 1023 {
        return Ok((f64::INFINITY.copysign(sign), consumed));
    }

    // If the decimal is less than the minimum for normals, then record the
    // excess; we'll use it to scale the result later. Note remaining_exp is non-positive;
    // we are going to multiply by 2**remaining_exp.
    let remaining_exp = (1022 + exponent).min(0);
    let exponent = exponent.max(-1022);

    debug_assert!((-1022..=1023).contains(&exponent));
    debug_assert!(remaining_exp <= 0);
    let biased_exp: u64 = (exponent + F64_EXP_BIAS).try_into().unwrap();

    // Construct the float as sign, exponent, mantissa.
    // Trim implicit 1 and excess precision from mantissa.
    let bits = (u64::from(negative) << 63) | (biased_exp << 52) | ((mantissa << 1) >> (64 - 52));
    let mut f = f64::from_bits(bits);

    // Handle denormals by scaling via the remaining exponent.
    // Note this may zero out the result if it underflows.
    if remaining_exp < 0 {
        f *= 2.0f64.powi(remaining_exp);
    }

    Ok((f, consumed))
}

#[cfg(test)]
mod tests {
    use super::{parse_hex_float, SyntaxError};

    // Helper to parse a float, expecting to succeed and consume the entire string.
    fn parse(input: &str) -> f64 {
        let res = parse_hex_float(input.chars(), '.')
            .unwrap_or_else(|_| panic!("Failed to parse {}", input));
        // We expect to consume the entire string.
        assert_eq!(res.1, input.len());
        res.0
    }

    #[test]
    fn test_parse_hex_float_rounding() {
        // Helper to get the first float before a float.
        let nextbefore = |f: f64| f64::from_bits(f.to_bits() - 1);
        assert_eq!(parse("0x1.FFFFFFFFFFFFFp0"), nextbefore(2.0));
        assert_eq!(parse("0x1.FFFFFFFFFFFFF7p0"), nextbefore(2.0));
        assert_eq!(parse("0x1.FFFFFFFFFFFFF8p0"), 2.0); // halfway, should round up!
        assert_eq!(parse("0x1.FFFFFFFFFFFFF8p-4"), 0.125); // halfway, should round up!
        assert_eq!(parse("0x1.FFFFFFFFFFFFF9p0"), 2.0);
        assert_eq!(parse("0x1.FFFFFFFFFFFFFFp0"), 2.0);

        assert_eq!(parse("-0x1.FFFFFFFFFFFFFp0"), nextbefore(-2.0));
        assert_eq!(parse("-0x1.FFFFFFFFFFFFF7p0"), nextbefore(-2.0));
        assert_eq!(parse("-0x1.FFFFFFFFFFFFF8p0"), -2.0); // halfway, should round up!
        assert_eq!(parse("-0x1.FFFFFFFFFFFFF8p-4"), -0.125); // halfway, should round up!
        assert_eq!(parse("-0x1.FFFFFFFFFFFFF9p0"), -2.0);
        assert_eq!(parse("-0x1.FFFFFFFFFFFFFFp0"), -2.0);

        let nb2 = nextbefore(2.0);
        assert_eq!(parse("0x1.FFFFFFFFFFFFEp0"), nextbefore(nb2));
        assert_eq!(parse("0x1.FFFFFFFFFFFFE7p0"), nextbefore(nb2));
        assert_eq!(parse("0x1.FFFFFFFFFFFFE8p0"), nextbefore(nb2)); // halfway, should round down!
        assert_eq!(parse("0x1.FFFFFFFFFFFFE9p0"), nb2);
        assert_eq!(parse("0x1.FFFFFFFFFFFFEFp0"), nb2);

        let nb2 = nextbefore(-2.0);
        assert_eq!(parse("-0x1.FFFFFFFFFFFFEp0"), nextbefore(nb2));
        assert_eq!(parse("-0x1.FFFFFFFFFFFFE7p0"), nextbefore(nb2));
        assert_eq!(parse("-0x1.FFFFFFFFFFFFE8p0"), nextbefore(nb2)); // halfway, should round down!
        assert_eq!(parse("-0x1.FFFFFFFFFFFFE9p0"), nb2);
        assert_eq!(parse("-0x1.FFFFFFFFFFFFEFp0"), nb2);
    }

    #[test]
    fn test_parse_hex_float_valid() {
        assert_eq!(parse("0x0"), 0.0);
        assert_eq!(parse("0X0"), 0.0);
        assert_eq!(parse("0X000"), 0.0);
        assert_eq!(parse("0X0000.8"), 0.5);
        assert_eq!(parse("0x1"), 1.0);
        assert_eq!(parse("0x1p0"), 1.0);
        assert_eq!(parse("0x1P0"), 1.0);
        assert_eq!(parse("0x1.8p1"), 3.0);
        assert_eq!(parse("0x2p2"), 8.0);
        assert_eq!(parse("0x1.8"), 1.5);
        assert_eq!(parse("0x1.2p3"), 9.0);
        assert_eq!(parse("0x10p-1"), 8.0);
        assert_eq!(parse("0x1.p1"), 2.0);
        assert_eq!(parse("0x.8p0"), 0.5);
        assert_eq!(parse("0x.1p4"), 1.0);
        assert_eq!(parse("0x2"), 2.0);
        assert_eq!(parse("0x2P1"), 4.0);
        assert_eq!(parse("0x2.4"), 2.25);
        assert_eq!(parse("0x2.4p2"), 9.0);
        assert_eq!(parse("0x3p-2"), 0.75);
        assert_eq!(parse("0x4p-3"), 0.5);
        assert_eq!(parse("0x5"), 5.0);
        assert_eq!(parse("0x5p1"), 10.0);
        assert_eq!(parse("0x5.1p0"), 5.0625);
        assert_eq!(parse("0x5.1p1"), 10.125);
        assert_eq!(parse("0x8"), 8.0);
        assert_eq!(parse("0x8p0"), 8.0);
        assert_eq!(parse("0x8.8"), 8.5);
        assert_eq!(parse("0x9"), 9.0);
        assert_eq!(parse("0x9p-1"), 4.5);
        assert_eq!(parse("0xA"), 10.0);
        assert_eq!(parse("0xAp1"), 20.0);
        assert_eq!(parse("0xB"), 11.0);
        assert_eq!(parse("0xBp-1"), 5.5);
        assert_eq!(parse("0xC"), 12.0);
        assert_eq!(parse("0xCp2"), 48.0);
        assert_eq!(parse("0xF"), 15.0);
        assert_eq!(parse("0xFp-2"), 3.75);
        assert_eq!(parse("0x10"), 16.0);
        assert_eq!(parse("0x10p-4"), 1.0);
        assert_eq!(parse("0x1A"), 26.0);
        assert_eq!(parse("0x1Ap3"), 208.0);
        assert_eq!(parse("0x1F"), 31.0);
        assert_eq!(parse("0x1Fp1"), 62.0);
        assert_eq!(parse("0x20"), 32.0);
        assert_eq!(parse("0x20p-5"), 1.0);
    }

    #[test]
    fn test_parse_hex_float_length() {
        let parse_len = |input: &str| {
            let res = parse_hex_float(input.chars(), '.')
                .unwrap_or_else(|_| panic!("Failed to parse {}", input));
            res.1
        };
        assert_eq!(parse_len("0x0ZZZ"), 3);
        assert_eq!(parse_len("0x1.p1ZZZZ"), 6);
        assert_eq!(parse_len("0x1234ZZZZZZZ"), 6);
    }

    #[test]
    fn test_parse_hex_float_errors() {
        assert_eq!(parse_hex_float("".chars(), '.'), Err(SyntaxError));
        assert_eq!(parse_hex_float("0xZ".chars(), '.'), Err(SyntaxError));
        assert_eq!(parse_hex_float("1A3P1.1p2".chars(), '.'), Err(SyntaxError));
        assert_eq!(parse_hex_float("1A3G.1p2".chars(), '.'), Err(SyntaxError));
    }

    #[test]
    fn test_parse_hex_float_signed_zero() {
        let z1 = parse_hex_float("0x0p0".chars(), '.').unwrap().0;
        let z2 = parse_hex_float("-0x0p0".chars(), '.').unwrap().0;
        assert_eq!(z1, 0.0);
        assert_eq!(z2, 0.0);
        assert!(z1.is_sign_positive());
        assert!(!z2.is_sign_positive());
    }

    #[test]
    fn test_parse_hex_floats_saturating_exp() {
        // Huge and positive (negative) exponents give us inf (-inf).
        assert_eq!(parse("0x1p999999"), f64::INFINITY);
        assert_eq!(parse("-0x1p999999"), -f64::INFINITY);

        // Test an exponent that would overflow i64. We don't care.
        assert_eq!(parse("0x1p36893488147419103232"), f64::INFINITY);
        assert_eq!(parse("-0x1p36893488147419103232"), -f64::INFINITY);

        // Test a mantissa that would overflow the exponent.
        // The max exponent is 1024; each hex digit is worth 4 (with a -1).
        assert_eq!(parse(&format!("0x1{:0<260}", "")), f64::INFINITY);
        assert_eq!(parse(&format!("-0x1{:0<260}", "")), -f64::INFINITY);

        // The mantissa and exponent can cancel each other out!
        assert_eq!(parse(&format!("0x1{:0<512}p-2000", "")), 2.0f64.powi(48));
        assert_eq!(
            parse(&format!("-0x1{:0<512}p-2000", "")),
            -(2.0f64.powi(48))
        );
    }

    #[test]
    fn test_parse_hex_float_denormals() {
        assert_eq!(parse("0x1p-1022"), f64::MIN_POSITIVE);
        assert_eq!(parse("0x1p-1023"), f64::MIN_POSITIVE / 2.0);
        assert_eq!(parse("0x1p-1024"), f64::MIN_POSITIVE / 4.0);
        assert_eq!(parse("0x.8p-1024"), f64::MIN_POSITIVE / 8.0);
        assert_eq!(parse("0x.4p-1024"), f64::MIN_POSITIVE / 16.0);
        assert_eq!(parse("0x.4p-9999"), 0.0);
    }
}
