mod decimal;
#[cfg(test)]
mod tests;

use super::locale::Locale;
use super::printf_impl::{ConversionSpec, Error, ModifierFlags, pad};
use decimal::{DIGIT_WIDTH, Decimal, DigitLimit};
use std::cmp::min;
use std::fmt::Write;

// Number of binary digits in the mantissa, including any implicit 1.
const MANTISSA_BITS: usize = f64::MANTISSA_DIGITS as usize;

// Break a floating point number into a normalized fraction and a power of 2.
// The fraction's magnitude will either be 0, or in the range [1/2, 1).
// We have value = frac * 2^exp.
fn frexp(x: f64) -> (f64, i32) {
    const EXPLICIT_MANTISSA_BITS: i32 = MANTISSA_BITS as i32 - 1;
    const EXPONENT_BIAS: i32 = 1023;
    let mut i = x.to_bits();
    let ee = ((i >> EXPLICIT_MANTISSA_BITS) & 0x7ff) as i32; // exponent
    if ee == 0 {
        if x == 0.0 {
            (x, 0)
        } else {
            // Subnormal. Scale up.
            let (x, e) = frexp(x * 2.0f64.powi(64));
            (x, e - 64)
        }
    } else if ee == 0x7ff {
        // Inf or NaN.
        (x, 0)
    } else {
        // Normal.
        // The mantissa is conceptually in the range [1, 2), but we want to
        // return it in the range [1/2, 1); remove the exponent bias but increase the
        // exponent by 1.
        let e = ee - (EXPONENT_BIAS - 1);
        // Set the exponent to -1, so we are in the range [1/2, 1).
        i &= 0x800fffffffffffff;
        i |= (EXPONENT_BIAS as u64 - 1) << EXPLICIT_MANTISSA_BITS;
        (f64::from_bits(i), e)
    }
}

// Return floor of log base 10 of an unsigned value.
// The log base 10 of 0 is treated as 0, for convenience.
fn log10u(x: u32) -> i32 {
    if x >= 1_000_000_000 {
        return 9;
    }
    let mut result = 0;
    let mut prod = 10;
    while prod <= x {
        result += 1;
        prod *= 10;
    }
    result
}

// Returns the number of trailing decimal zeros in the given value.
// If the value is 0, return 9.
fn trailing_decimal_zeros(mut d: u32) -> i32 {
    if d == 0 {
        return 9;
    }
    let mut zeros = 0;
    while d % 10 == 0 {
        zeros += 1;
        d /= 10;
    }
    zeros
}

/// A helper type to store common formatting parameters.
struct FormatParams<'a, W: Write> {
    // The receiver of formatted output.
    f: &'a mut W,

    // Width of the output.
    width: usize,

    // Precision of the output. This defaults to 6.
    prec: usize,

    // Whether the precision was explicitly set.
    had_prec: bool,

    // Flags to control formatting options.
    flags: ModifierFlags,

    // The locale to apply.
    locale: &'a Locale,

    // The initial prefix such as sign or space. Not used for hex.
    prefix: &'static str,

    // Whether our conversion specifier was lowercase.
    lower: bool,

    // A buffer to use for temporary storage.
    buf: &'a mut String,
}

/// Formats a floating-point number `y` into a provided writer `f` with specified formatting options.
///
/// # Parameters
/// - `f`: The receiver of formatted output.
/// - `y`: The value to format.
/// - `width`: The minimum width of the formatted string. If the result is shorter, it will be padded.
/// - `prec`: The precision, i.e., the number of digits after the decimal point, or None if not given.
/// - `flags`: ModifierFlags to control formatting options.
/// - `locale`: The locale.
/// - `conv_spec`: The type of formatting : 'e', 'f', 'g', 'a', 'E', 'F', 'G', 'A'.
/// - `buf`: A buffer to use for temporary storage.
///
/// # Returns
/// A `Result` which is `Ok` containing the number of bytes written on success, or an `Error`.
#[allow(clippy::too_many_arguments)]
pub(crate) fn format_float(
    f: &mut impl Write,
    y: f64,
    width: usize,
    prec: Option<usize>,
    flags: ModifierFlags,
    locale: &Locale,
    conv_spec: ConversionSpec,
    buf: &mut String,
) -> Result<usize, Error> {
    // Only float conversions are expected.
    type CS = ConversionSpec;
    debug_assert!(matches!(
        conv_spec,
        CS::e | CS::E | CS::f | CS::F | CS::g | CS::G | CS::a | CS::A
    ));
    let prefix = match (y.is_sign_negative(), flags.mark_pos, flags.pad_pos) {
        (true, _, _) => "-",
        (false, true, _) => "+",
        (false, false, true) => " ",
        (false, false, false) => "",
    };

    // "If the precision is missing, it is taken as 6" (except for %a and %A, which care about a missing precision).
    let had_prec = prec.is_some();
    let prec = prec.unwrap_or(6);

    let params = FormatParams {
        f,
        width,
        prec,
        had_prec,
        flags,
        locale,
        prefix,
        lower: conv_spec.is_lower(),
        buf,
    };

    // Handle infinities and NaNs.
    if !y.is_finite() {
        return format_nonfinite(y, params);
    }

    // Handle hex formatting.
    if matches!(conv_spec, CS::a | CS::A) {
        return format_a(y, params);
    }

    // As an optimization, allow the precision to limit the number of digits we compute.
    // Count this as number of desired decimal digits, converted to our base, rounded up, +1 for
    // rounding off.
    // For 'f'/'F', precision is after the decimal; for others it is total number of digits.
    let prec_limit = match conv_spec {
        CS::f | CS::F => DigitLimit::Fractional(prec / DIGIT_WIDTH + 2),
        _ => DigitLimit::Total(prec / DIGIT_WIDTH + 2),
    };

    // Construct our digits.
    let mut decimal = Decimal::new(y, prec_limit);

    // Compute the number of desired fractional digits - possibly negative.
    let mut desired_frac_digits: i32 = prec.try_into().map_err(|_| Error::Overflow)?;
    if matches!(conv_spec, CS::e | CS::E | CS::g | CS::G) {
        // For 'e' and 'E', the precision is the number of digits after the decimal point.
        // We are going to divide by 10^e, so adjust desired_frac_digits accordingly.
        // Note that e10 may be negative, so guard against overflow in the positive direction.
        let e10 = decimal.exponent();
        desired_frac_digits = desired_frac_digits.saturating_sub(e10);
    }
    if matches!(conv_spec, CS::g | CS::G) && prec != 0 {
        desired_frac_digits -= 1;
    }
    decimal.round_to_fractional_digits(desired_frac_digits);

    match conv_spec {
        CS::e | CS::E => format_e_f(&mut decimal, params, true),
        CS::f | CS::F => format_e_f(&mut decimal, params, false),
        CS::g | CS::G => format_g(&mut decimal, params),
        _ => unreachable!(),
    }
}

// Format a non-finite float.
fn format_nonfinite(y: f64, params: FormatParams<'_, impl Write>) -> Result<usize, Error> {
    let FormatParams {
        f,
        width,
        flags,
        prefix,
        lower,
        ..
    } = params;
    let s = match (y.is_nan(), lower) {
        (true, true) => "nan",
        (true, false) => "NAN",
        (false, true) => "inf",
        (false, false) => "INF",
    };
    let unpadded_width = s.len() + prefix.len();
    if !flags.left_adj {
        pad(f, ' ', width, unpadded_width)?;
    }
    f.write_str(prefix)?;
    f.write_str(s)?;
    if flags.left_adj {
        pad(f, ' ', width, unpadded_width)?;
    }
    Ok(width.max(unpadded_width))
}

/// Formats a floating-point number `y` as hex (%a/%A).
///
/// # Parameters
/// - `y`: The value to format. This is always finite.
/// - `params`: Params controlling formatting.
///
/// # Returns
/// A `Result` which is `Ok` containing the number of bytes written on success, or an `Error`.
fn format_a(mut y: f64, params: FormatParams<'_, impl Write>) -> Result<usize, Error> {
    debug_assert!(y.is_finite());
    let negative = y.is_sign_negative();
    y = y.abs();

    let FormatParams {
        f,
        width,
        had_prec,
        prec,
        flags,
        locale,
        lower,
        buf,
        ..
    } = params;

    let (mut y, mut e2) = frexp(y);

    // normalize to range [1, 2), or 0.0.
    if y != 0.0 {
        y *= 2.0;
        e2 -= 1;
    }

    let prefix = if lower {
        match (negative, flags.mark_pos, flags.pad_pos) {
            (true, _, _) => "-0x",
            (false, true, _) => "+0x",
            (false, false, true) => " 0x",
            (false, false, false) => "0x",
        }
    } else {
        match (negative, flags.mark_pos, flags.pad_pos) {
            (true, _, _) => "-0X",
            (false, true, _) => "+0X",
            (false, false, true) => " 0X",
            (false, false, false) => "0X",
        }
    };

    // Compute the number of hex digits in the mantissa after the decimal.
    // -1 for leading 1 bit (we are to the range [1, 2)), then divide by 4, rounding up.
    #[allow(unknown_lints)] // for old clippy
    #[allow(clippy::manual_div_ceil)]
    const MANTISSA_HEX_DIGITS: usize = (MANTISSA_BITS - 1 + 3) / 4;
    if had_prec && prec < MANTISSA_HEX_DIGITS {
        // Decide how many least-significant bits to round off the mantissa.
        let desired_bits = prec * 4;
        let bits_to_round = MANTISSA_BITS - 1 - desired_bits;
        debug_assert!(bits_to_round > 0 && bits_to_round < MANTISSA_BITS);
        let round = 2.0f64.powi(bits_to_round as i32);
        if negative {
            y = -y;
            y -= round;
            y += round;
            y = -y;
        } else {
            y += round;
            y -= round;
        }
    }
    let estr = format!(
        "{}{}{}",
        if lower { 'p' } else { 'P' },
        if e2 < 0 { '-' } else { '+' },
        e2.unsigned_abs()
    );

    let xdigits: &[u8; 16] = if lower {
        b"0123456789abcdef"
    } else {
        b"0123456789ABCDEF"
    };
    let body = buf;
    loop {
        let x = y as i32;
        body.push(xdigits[x as usize] as char);
        y = 16.0 * (y - (x as f64));
        if body.len() == 1 && (y != 0.0 || (had_prec && prec > 0) || flags.alt_form) {
            body.push(locale.decimal_point);
        }
        if y == 0.0 {
            break;
        }
    }

    let mut body_exp_len = body.len() + estr.len();
    if had_prec && prec > 0 {
        // +2 for leading digit and decimal.
        let len_with_prec = prec.checked_add(2 + estr.len()).ok_or(Error::Overflow)?;
        body_exp_len = body_exp_len.max(len_with_prec);
    }

    let prefix_len = prefix.len();
    let unpadded_width = prefix_len
        .checked_add(body_exp_len)
        .ok_or(Error::Overflow)?;

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

    // Output the actual value.
    f.write_str(body)?;

    // Pad the body with zeros on the right (reflecting precision)?
    pad(f, '0', body_exp_len - estr.len() - body.len(), 0)?;

    // Output the exponent.
    f.write_str(&estr)?;

    // Pad on the right with spaces to the desired width?
    if flags.left_adj {
        pad(f, ' ', width, prefix_len + body_exp_len)?;
    }
    Ok(width.max(unpadded_width))
}

/// Formats a floating-point number in formats %e/%E/%f/%F.
///
/// # Parameters
/// - `digits`: The extracted digits of the value.
/// - `params`: Params controlling formatting.
/// - `is_e`: If true, the conversion specifier is 'e' or 'E', otherwise 'f' or 'F'.
fn format_e_f(
    decimal: &mut Decimal,
    params: FormatParams<'_, impl Write>,
    is_e: bool,
) -> Result<usize, Error> {
    let FormatParams {
        f,
        width,
        prec,
        flags,
        locale,
        prefix,
        lower,
        buf,
        ..
    } = params;

    // Exponent base 10.
    let e10 = decimal.exponent();

    // Compute an exponent string for 'e' / 'E'.
    let estr = if is_e {
        // "The exponent always contains at least two digits."
        let sign = if e10 < 0 { '-' } else { '+' };
        let e = if lower { 'e' } else { 'E' };
        format!("{}{}{:02}", e, sign, e10.unsigned_abs())
    } else {
        // No exponent for 'f' / 'F'.
        String::new()
    };

    // Compute the body length.
    // For 'f' / 'F' formats, the precision is after the decimal point, so a positive exponent
    // will increase the body length. We also must consider insertion of separators.
    // Note the body length must be correct, as it is used to compute the width.
    let integer_len = if is_e {
        1
    } else {
        let mut len = 1 + e10.max(0) as usize;
        if flags.grouped {
            len += locale.separator_count(len);
        }
        len
    };
    let decimal_len = if prec > 0 || flags.alt_form { 1 } else { 0 };
    let body_len = integer_len + decimal_len + prec + estr.len();

    let prefix_len = prefix.len();
    // Emit the prefix and any padding.
    if !flags.left_adj && !flags.zero_pad {
        pad(f, ' ', width, prefix_len + body_len)?;
    }
    f.write_str(prefix)?;
    if !flags.left_adj && flags.zero_pad {
        pad(f, '0', width, prefix_len + body_len)?;
    }

    if is_e {
        format_mantissa_e(decimal, prec, flags, locale, f, buf)?;
        // Emit the exponent.
        f.write_str(&estr)?;
    } else {
        format_mantissa_f(decimal, prec, flags, locale, f, buf)?;
    }
    if flags.left_adj && !flags.zero_pad {
        pad(f, ' ', width, prefix_len + body_len)?;
    }
    Ok(width.max(prefix_len + body_len))
}

/// Formats a floating point number in "g" / "G" form.
///
/// # Parameters
/// - `digits`: The extracted digits of the value.
/// - `params`: Params controlling formatting.
fn format_g(
    decimal: &mut Decimal,
    mut params: FormatParams<'_, impl Write>,
) -> Result<usize, Error> {
    // "If the precision is zero, it is treated as 1."
    params.prec = params.prec.max(1);

    // "Style e is used if the exponent from its conversion is less than -4 or greater than or equal to the precision."
    let use_style_e;
    let e10 = decimal.exponent();
    let e10mag = e10.unsigned_abs() as usize;
    if e10 < -4 || (e10 >= 0 && e10mag >= params.prec) {
        use_style_e = true;
        params.prec -= 1;
    } else {
        use_style_e = false;
        params.prec -= 1;
        // prec -= e10. Overflow is impossible since prec <= i32::MAX.
        params.prec = if e10 < 0 {
            params.prec.checked_add(e10mag).unwrap()
        } else {
            params.prec.checked_sub(e10mag).unwrap()
        };
    }
    if !params.flags.alt_form {
        // Count trailing zeros in last place.
        let trailing_zeros = trailing_decimal_zeros(decimal.last().unwrap_or(0));
        let mut computed_prec = decimal.fractional_digit_count() - trailing_zeros;
        if use_style_e {
            computed_prec += e10;
        }
        params.prec = params.prec.min(computed_prec.max(0) as usize);
    }
    format_e_f(decimal, params, use_style_e)
}

// Helper to format the mantissa of a floating point number in "e" / "E" form.
fn format_mantissa_e(
    decimal: &Decimal,
    prec: usize,
    flags: ModifierFlags,
    locale: &Locale,
    f: &mut impl Write,
    buf: &mut String,
) -> Result<(), Error> {
    let mut prec_left = prec;
    // The decimal may be empty, so ensure we loop at least once.
    for d in 0..decimal.len_i32().max(1) {
        let digit = if d < decimal.len_i32() { decimal[d] } else { 0 };
        let min_width = if d > 0 { DIGIT_WIDTH } else { 1 };
        buf.clear();
        write!(buf, "{digit:0min_width$}")?;
        let mut s = buf.as_str();
        if d == 0 {
            // First digit. Emit it, and likely also a decimal point.
            f.write_str(&s[..1])?;
            s = &s[1..];
            if prec_left > 0 || flags.alt_form {
                f.write_char(locale.decimal_point)?;
            }
        }
        let outlen = s.len().min(prec_left);
        f.write_str(&s[..outlen])?;
        prec_left -= outlen;
        if prec_left == 0 {
            break;
        }
    }
    // Emit trailing zeros for excess precision.
    pad(f, '0', prec_left, 0)?;
    Ok(())
}

// Helper to format the mantissa of a floating point number in "f" / "F" form.
fn format_mantissa_f(
    decimal: &mut Decimal,
    prec: usize,
    flags: ModifierFlags,
    locale: &Locale,
    f: &mut impl Write,
    buf: &mut String,
) -> Result<(), Error> {
    // %f conversions (almost) always have at least one digit before the decimal,
    // so ensure that the radix is not-negative and the decimal covers the radix.
    while decimal.radix < 0 {
        decimal.push_front(0);
    }
    while decimal.len_i32() <= decimal.radix {
        decimal.push_back(0);
    }

    // Emit digits before the decimal.
    // We may need thousands grouping here (but for no other floating point types).
    let do_grouping = flags.grouped && locale.thousands_sep.is_some();
    for d in 0..=decimal.radix {
        let min_width = if d > 0 { DIGIT_WIDTH } else { 1 };
        if do_grouping {
            // Emit into our buffer so we can later apply thousands grouping.
            write!(buf, "{:0width$}", decimal[d], width = min_width)?;
        } else {
            // Write digits directly.
            write!(f, "{:0width$}", decimal[d], width = min_width)?;
        }
    }
    if do_grouping {
        f.write_str(&locale.apply_grouping(buf))?;
    }

    // Emit decimal point.
    if prec != 0 || flags.alt_form {
        f.write_char(locale.decimal_point)?;
    }
    // Emit prec digits after the decimal, stopping if we run out.
    let mut prec_left: usize = prec;
    for d in (decimal.radix + 1)..decimal.len_i32() {
        if prec_left == 0 {
            break;
        }
        let max_digits = min(DIGIT_WIDTH, prec_left);
        buf.clear();
        write!(buf, "{:0width$}", decimal[d], width = DIGIT_WIDTH)?;
        f.write_str(&buf[..max_digits])?;
        prec_left -= max_digits;
    }
    // Emit trailing zeros for excess precision.
    pad(f, '0', prec_left, 0)?;
    Ok(())
}
