use super::errors::Error;
use super::hex_float;
use crate::wchar::IntoCharIter;
use std::cell::RefCell;

#[cold]
#[inline(never)]
pub fn parse_inf_nan(
    mut chars: impl Iterator<Item = char>,
    b0: Option<u8>,
    c1: u8,
) -> Option<(f64, usize)> {
    let (mut consumed, neg) = match b0 {
        None => (3, false),
        Some(b'-') => (4, true),
        _ => (4, false),
    };
    let [c2, c3] = [chars.next()?, chars.next()?];
    // Using non-short-circuiting comparisons lets the compiler optimize it a bit more.
    if (c1 == b'n') & (c2 == 'a') & (c3 == 'n') {
        if !neg {
            return Some((f64::NAN, consumed));
        }
        // LLVM understands this and returns f64::from_bits(0xFFF8000000000000) directly
        return Some((f64::NAN.copysign(-1.0), consumed));
    }
    if (c1 == b'i') & (c2 == 'n') & (c3 == 'f') {
        // "xyz".chars().all(..) inlines nicely while "xyz".chars().eq(chars.take(3)) doesn't.
        if "inity".chars().all(|c| Some(c) == chars.next()) {
            consumed += 5;
        }
        if !neg {
            return Some((f64::INFINITY, consumed));
        }
        return Some((f64::NEG_INFINITY, consumed));
    }
    return None;
}

fn parse_partial_iter<I>(chars: I, decimal_sep: char) -> Option<(f64, usize)>
where
    I: Iterator<Item = char>,
{
    const F64_MAX_CHARS: usize = 45;
    thread_local! {
        static BUFFER: RefCell<String> = RefCell::new(String::with_capacity(F64_MAX_CHARS));
    }
    BUFFER.with(|cell| {
        let mut buffer = cell.borrow_mut();
        buffer.clear();

        let mut consumed = 0;
        let mut chars = chars.map(|c| c.to_ascii_lowercase()).peekable();

        if let Some(sign) = chars.next_if(|c| ['-', '+'].contains(c)) {
            consumed += 1;
            buffer.push(sign);
        }
        if let Some(c1) = chars.next_if(|c| c.is_ascii_alphabetic()) {
            return parse_inf_nan(chars, buffer.as_bytes().get(0).copied(), c1 as u8);
        }

        let mut have_e = false;
        let mut have_sep = false;
        while let Some(mut c) = chars.next() {
            match c {
                _ if c.is_ascii_digit() => (),
                _ if !have_sep && !have_e && decimal_sep == c => {
                    c = '.';
                    have_sep = true;
                }
                'e' if !have_e => {
                    // Allow a sign after e
                    if let Some(sign) = chars.next_if(|c| ['+', '-'].contains(c)) {
                        consumed += 1;
                        buffer.push(c);
                        c = sign;
                    }
                    have_e = true;
                }
                _ => break,
            };

            buffer.push(c);
            consumed += 1;
        }

        let value: f64 = (buffer).parse().ok()?;
        Some((value, consumed))
    })
}

fn wcstod_inner<I>(mut chars: I, decimal_sep: char, consumed: &mut usize) -> Result<f64, Error>
where
    I: Iterator<Item = char> + Clone,
{
    let mut whitespace_skipped = 0;
    // Skip leading whitespace.
    loop {
        match chars.clone().next() {
            Some(c) if c.is_whitespace() => {
                whitespace_skipped += 1;
                chars.next();
            }
            None => {
                *consumed = 0;
                return Err(Error::Empty);
            }
            _ => break,
        }
    }

    // If it's a hex float, parse it.
    if is_hex_float(chars.clone()) {
        return if let Ok((f, amt)) = hex_float::parse_hex_float(chars, decimal_sep) {
            *consumed = whitespace_skipped + amt;
            Ok(f)
        } else {
            Err(Error::InvalidChar)
        };
    }

    let ret = parse_partial_iter(chars.clone().fuse(), decimal_sep);
    if ret.is_none() {
        *consumed = 0;
        return Err(Error::InvalidChar);
    }
    let (val, n): (f64, usize) = ret.unwrap();

    // Fast-float does not return overflow errors; instead it just returns +/- infinity.
    // Check to see if the first character is a digit or the decimal; if so that indicates overflow.
    if val.is_infinite() {
        for c in chars {
            if c.is_whitespace() {
                continue;
            } else if c.is_ascii_digit() || c == decimal_sep {
                return Err(Error::Overflow);
            } else {
                break;
            }
        }
    }
    *consumed = n + whitespace_skipped;
    Ok(val)
}

/// Parses a 64-bit floating point number.
///
/// Leading whitespace and trailing characters are ignored. If the input
/// string does not contain a valid floating point number (where e.g.
/// `"."` is seen as a valid floating point number), `None` is returned.
/// Otherwise the parsed floating point number is returned.
///
/// The `decimal_sep` parameter is used to specify the decimal separator.
/// '.' is a normal default.
///
/// The `consumed` parameter is used to return the number of characters
/// consumed, similar to the "end" parameter to strtod.
/// This is only meaningful if parsing succeeds.
///
/// Error::Overflow is returned if the value is too large in magnitude.
pub fn wcstod<Chars>(input: Chars, decimal_sep: char, consumed: &mut usize) -> Result<f64, Error>
where
    Chars: IntoCharIter,
{
    wcstod_inner(input.chars(), decimal_sep, consumed)
}

/// Check if a character iterator appears to be a hex float.
/// That is, an optional + or -, followed by 0x or 0X, and a hex digit.
pub fn is_hex_float<Chars: Iterator<Item = char>>(mut chars: Chars) -> bool {
    match chars.next() {
        Some('+' | '-') => {
            if chars.next() != Some('0') {
                return false;
            }
        }
        Some('0') => (),
        _ => return false,
    };
    match chars.next() {
        Some('x') | Some('X') => (),
        _ => return false,
    };
    match chars.next() {
        Some(c) => c.is_ascii_hexdigit(),
        None => false,
    }
}

/// Like [`wcstod()`], but allows underscore separators. Leading, trailing, and multiple underscores
/// are allowed, as are underscores next to decimal (`.`), exponent (`E`/`e`/`P`/`p`), and
/// hexadecimal (`X`/`x`) delimiters. This consumes trailing underscores -- `consumed` will include
/// the last underscore which is legal to include in a parse (according to the above rules).
/// Free-floating leading underscores (`"_ 3"`) are not allowed and will result in a no-parse.
/// Underscores are not allowed before or inside of `"infinity"` or `"nan"` input. Trailing
/// underscores after `"infinity"` or `"nan"` are not consumed.
pub fn wcstod_underscores<Chars>(s: Chars, consumed: &mut usize) -> Result<f64, Error>
where
    Chars: IntoCharIter,
{
    let mut chars = s.chars().peekable();

    let mut leading_whitespace = 0;
    // Skip leading whitespace.
    while let Some(c) = chars.peek() {
        if c.is_ascii_whitespace() {
            leading_whitespace += 1;
            chars.next();
        } else {
            break;
        }
    }

    let is_sign = |c: char| "+-".contains(c);
    let is_inf_or_nan_char = |c: char| "iInN".contains(c);

    // We don't do any underscore-stripping for infinity/NaN.
    let mut is_inf_nan = false;
    if let Some(&c1) = chars.peek() {
        if is_inf_or_nan_char(c1) {
            is_inf_nan = true;
        } else if is_sign(c1) {
            // FIXME make this more efficient
            let mut copy = chars.clone();
            copy.next();
            if let Some(&c2) = copy.peek() {
                if is_inf_or_nan_char(c2) {
                    is_inf_nan = true;
                }
            }
        }
    }
    if is_inf_nan {
        let f = wcstod_inner(chars, '.', consumed)?;
        *consumed += leading_whitespace;
        return Ok(f);
    }
    // We build a string to pass to the system wcstod, pruned of underscores. We will take all
    // leading alphanumeric characters that can appear in a strtod numeric literal, dots (.), and
    // signs (+/-). In order to be more clever, for example to stop earlier in the case of strings
    // like "123xxxxx", we would need to do a full parse, because sometimes 'a' is a hex digit and
    // sometimes it is the end of the parse, sometimes a dot '.' is a decimal delimiter and
    // sometimes it is the end of the valid parse, as in "1_2.3_4.5_6", etc.
    let mut pruned = vec![];
    // We keep track of the positions *in the pruned string* where there used to be underscores. We
    // will pass the pruned version of the input string to the system wcstod, which in turn will
    // tell us how many characters it consumed. Then we will set our own endptr based on (1) the
    // number of characters consumed from the pruned string, and (2) how many underscores came
    // before the last consumed character. The alternative to doing it this way (for example, "only
    // deleting the correct underscores") would require actually parsing the input string, so that
    // we can know when to stop grabbing characters and dropping underscores, as in "1_2.3_4.5_6".
    let mut underscores = vec![];
    // If we wanted to future-proof against a strtod from the future that, say, allows octal
    // literals using 0o, etc., we could just use iswalnum, instead of iswxdigit and P/p/X/x checks.
    for c in chars.take_while(|&c| c.is_ascii_hexdigit() || "PpXx._".contains(c) || is_sign(c)) {
        if c == '_' {
            underscores.push(pruned.len());
        } else {
            pruned.push(c)
        }
    }

    let mut pruned_consumed = 0;
    let f = wcstod_inner(pruned.into_iter(), '.', &mut pruned_consumed)?;
    let underscores_consumed = underscores
        .into_iter()
        .take_while(|&n| n <= pruned_consumed)
        .count();

    *consumed = leading_whitespace + pruned_consumed + underscores_consumed;
    Ok(f)
}

#[cfg(test)]
mod test {
    use crate::wutil::wcstod::parse_partial_iter;

    use super::{wcstod, Error};

    #[test]
    #[allow(clippy::all)]
    pub fn tests() {
        test_consumed("12.345", Ok(12.345), 6);
        test_consumed("  12.345", Ok(12.345), 8);
        test_consumed("  12.345  ", Ok(12.345), 8);
        test_consumed("12.345    ", Ok(12.345), 6);
        test("12.345e19", Ok(12.345e19));
        test("-.1e+9", Ok(-0.1e+9));
        test(".125", Ok(0.125));
        test("1e20", Ok(1e20));
        test("0e-19", Ok(0.0));
        test_consumed("4\00012", Ok(4.0), 1);
        test("5.9e-76", Ok(5.9e-76));
        test("", Err(Error::Empty));
        test("Inf", Ok(f64::INFINITY));
        test("-Inf", Ok(f64::NEG_INFINITY));
        test("+InFiNiTy", Ok(f64::INFINITY));
        test("1e-324", Ok(0.0));
        test(
            "+1.000000000116415321826934814453125",
            Ok(1.000000000116415321826934814453125),
        );
        test("42.0000000000000000001", Ok(42.0000000000000000001));
        test("42.00000000000000000001", Ok(42.00000000000000000001));
        test("42.000000000000000000001", Ok(42.000000000000000000001));
        test("179769313486231570814527423731704356798070567525844996598917476803157260780028538760589558632766878171540458953514382464234321326889464182768467546703537516986049910576551282076245490090389328944075868508455133942304583236903222948165808559332123348274797826204144723168738177180919299881250404026184124858368", Ok(179769313486231570814527423731704356798070567525844996598917476803157260780028538760589558632766878171540458953514382464234321326889464182768467546703537516986049910576551282076245490090389328944075868508455133942304583236903222948165808559332123348274797826204144723168738177180919299881250404026184124858368.000000));
        test_consumed("1y", Ok(1.0), 1);
        test_consumed("0.y", Ok(0.0), 2);
        test_consumed(".0y", Ok(0.0), 2);
        test_consumed("000,,,e1", Ok(0.0), 3);
        test_consumed("0x1", Ok(1.0), 3);
        test_consumed("0X1p2", Ok(4.0), 5);
        test_consumed("0X1P3", Ok(8.0), 5);
        test("000e1", Ok(0.0));
        test_consumed("000,1e1", Ok(0.0), 3);
        test("0", Ok(0.0));
        test("000", Ok(0.0));
        test("-0", Ok(-0.0));
        test("-000", Ok(-0.0));
        test_consumed("0,", Ok(0.0), 1);
        test_consumed("-0,", Ok(-0.0), 2);
        test_consumed("0,0", Ok(0.0), 1);
        test_consumed("-0,0", Ok(-0.0), 2);
        test("0e-10", Ok(0.0));
        test("-0e-10", Ok(-0.0));
        test_consumed("0,e-10", Ok(0.0), 1);
        test_consumed("-0,e-10", Ok(-0.0), 2);
        test_consumed("0,0e-10", Ok(0.0), 1);
        test_consumed("-0,0e-10", Ok(-0.0), 2);
        test("0e-1000000", Ok(0.0));
        test("-0e-1000000", Ok(-0.0));
        test_consumed("0,0e-1000000", Ok(0.0), 1);
        test_consumed("-0,0e-1000000", Ok(-0.0), 2);
        test("0", Ok(0.0));
        test("000", Ok(0.0));
        test("-0", Ok(-0.0));
        test("-000", Ok(-0.0));
        test("0e-10", Ok(0.0));
        test("-0e-10", Ok(-0.0));
        test("0e-1000000", Ok(0.0));
        test("-0e-1000000", Ok(-0.0));
        test("1", Ok(1_f64));
        test("1.1", Ok(1.1));
        test("1.1e1", Ok(1.1e1));
        test("1234.1234", Ok(1234.1234));
        test("1234.12345678", Ok(1234.12345678));
        test("1234.123456789012", Ok(1234.123456789012));
        test(
            "1.797693134862315708145274237317e+10",
            Ok(1.797693134862315708145274237317e+10),
        );
        test(
            "1.797693134862315708145274237317e+308",
            Ok(1.797693134862315708145274237317e+308_f64),
        );
        test("000000000e123", Ok(0.0));
        test("0000000010000e-329", Ok(0.0));
        test("000000001e-325", Ok(0.0));
        test("0000000020000e-328", Ok(0.0));
        test("0000000090000e-329", Ok(0.0));
        test("0e+999", Ok(0.0));
        test("0e1", Ok(0.0));
        test("0e12345", Ok(0.0));
        test("0e2", Ok(0.0));
        test("0e-2", Ok(0.0));
        test("0e-999", Ok(0.0));
        test("10000e-329", Ok(0.0));
        test("1e-325", Ok(0.0));
        test("20000e-328", Ok(0.0));
        test("2e-324", Ok(0.0));
        test("90000e-329", Ok(0.0));
        test_consumed("e1324", Err(Error::InvalidChar), 0);
        test("1e0", Ok(1.0));
        test("17976931348623157e292", Ok(1.7976931348623157E+308));
        test("17976931348623158e292", Ok(1.7976931348623158E+308));
        test("1e1", Ok(10.0));
        test("1e2", Ok(100.0));
        test(
            "10141204801825834086073718800384e0",
            Ok(10141204801825834086073718800384.0),
        );
        test(
            "1014120480182583464902367222169599999e-5",
            Ok(10141204801825834086073718800384.0),
        );
        test(
            "1014120480182583464902367222169600001e-5",
            Ok(10141204801825835211973625643008.0),
        );
        test(
            "10141204801825834649023672221696e0",
            Ok(10141204801825835211973625643008.0),
        );
        test(
            "10141204801825835211973625643008e0",
            Ok(10141204801825835211973625643008.0),
        );
        test("104110013277974872254e-225", Ok(104110013277974872254e-225));
        test("12345e0", Ok(12345.0));
        test("12345e1", Ok(123450.0));
        test("12345e2", Ok(1234500.0));
        test("12345678901234e0", Ok(12345678901234.0));
        test("12345678901234e1", Ok(123456789012340.0));
        test("12345678901234e2", Ok(1234567890123400.0));
        test("123456789012345e0", Ok(123456789012345.0));
        test("123456789012345e1", Ok(1234567890123450.0));
        test("123456789012345e2", Ok(12345678901234500.0));
        test(
            "1234567890123456789012345e108",
            Ok(1234567890123456789012345e108),
        );
        test(
            "1234567890123456789012345e109",
            Ok(1234567890123456789012345e109),
        );
        test(
            "1234567890123456789012345e110",
            Ok(1234567890123456789012345e110),
        );
        test(
            "1234567890123456789012345e111",
            Ok(1234567890123456789012345e111),
        );
        test(
            "1234567890123456789012345e112",
            Ok(1234567890123456789012345e112),
        );
        test(
            "1234567890123456789012345e113",
            Ok(1234567890123456789012345e113),
        );
        test(
            "1234567890123456789012345e114",
            Ok(1234567890123456789012345e114),
        );
        test(
            "1234567890123456789012345e115",
            Ok(1234567890123456789012345e115),
        );
        test(
            "1234567890123456789052345e108",
            Ok(1234567890123456789052345e108),
        );
        test(
            "1234567890123456789052345e109",
            Ok(1234567890123456789052345e109),
        );
        test(
            "1234567890123456789052345e110",
            Ok(1234567890123456789052345e110),
        );
        test(
            "1234567890123456789052345e111",
            Ok(1234567890123456789052345e111),
        );
        test(
            "1234567890123456789052345e112",
            Ok(1234567890123456789052345e112),
        );
        test(
            "1234567890123456789052345e113",
            Ok(1234567890123456789052345e113),
        );
        test(
            "1234567890123456789052345e114",
            Ok(1234567890123456789052345e114),
        );
        test(
            "1234567890123456789052345e115",
            Ok(1234567890123456789052345e115),
        );
        test("123456789012345e-1", Ok(123456789012345e-1));
        test("123456789012345e-2", Ok(123456789012345e-2));
        test("123456789012345e20", Ok(123456789012345e20));
        test("123456789012345e-20", Ok(123456789012345e-20));
        test("123456789012345e22", Ok(123456789012345e22));
        test("123456789012345e-22", Ok(123456789012345e-22));
        test("123456789012345e23", Ok(123456789012345e23));
        test("123456789012345e-23", Ok(123456789012345e-23));
        test("123456789012345e-25", Ok(123456789012345e-25));
        test("123456789012345e35", Ok(123456789012345e35));
        test("123456789012345e36", Ok(123456789012345e36));
        test("123456789012345e37", Ok(123456789012345e37));
        test("123456789012345e39", Ok(123456789012345e39));
        test("123456789012345e-39", Ok(123456789012345e-39));
        test("123456789012345e-5", Ok(123456789012345e-5));
        test("12345678901234e-1", Ok(12345678901234e-1));
        test("12345678901234e-2", Ok(12345678901234e-2));
        test("12345678901234e20", Ok(12345678901234e20));
        test("12345678901234e-20", Ok(12345678901234e-20));
        test("12345678901234e22", Ok(12345678901234e22));
        test("12345678901234e-22", Ok(12345678901234e-22));
        test("12345678901234e23", Ok(12345678901234e23));
        test("12345678901234e-23", Ok(12345678901234e-23));
        test("12345678901234e-25", Ok(12345678901234e-25));
        test("12345678901234e30", Ok(12345678901234e30));
        test("12345678901234e31", Ok(12345678901234e31));
        test("12345678901234e32", Ok(12345678901234e32));
        test("12345678901234e35", Ok(12345678901234e35));
        test("12345678901234e36", Ok(12345678901234e36));
        test("12345678901234e37", Ok(12345678901234e37));
        test("12345678901234e-39", Ok(12345678901234e-39));
        test("12345678901234e-5", Ok(12345678901234e-5));
        test("123456789e108", Ok(123456789e108));
        test("123456789e109", Ok(123456789e109));
        test("123456789e110", Ok(123456789e110));
        test("123456789e111", Ok(123456789e111));
        test("123456789e112", Ok(123456789e112));
        test("123456789e113", Ok(123456789e113));
        test("123456789e114", Ok(123456789e114));
        test("123456789e115", Ok(123456789e115));
        test("12345e-1", Ok(12345e-1));
        test("12345e-2", Ok(12345e-2));
        test("12345e20", Ok(12345e20));
        test("12345e-20", Ok(12345e-20));
        test("12345e22", Ok(12345e22));
        test("12345e-22", Ok(12345e-22));
        test("12345e23", Ok(12345e23));
        test("12345e-23", Ok(12345e-23));
        test("12345e-25", Ok(12345e-25));
        test("12345e30", Ok(12345e30));
        test("12345e31", Ok(12345e31));
        test("12345e32", Ok(12345e32));
        test("12345e35", Ok(12345e35));
        test("12345e36", Ok(12345e36));
        test("12345e37", Ok(12345e37));
        test("12345e-39", Ok(12345e-39));
        test("12345e-5", Ok(12345e-5));
        test("000000001234e304", Ok(1234e304));
        test("0000000123400000e299", Ok(1234e304));
        test("123400000e299", Ok(1234e304));
        test("1234e304", Ok(1234e304));
        test("00000000123400000e300", Ok(1234e305));
        test("00000001234e305", Ok(1234e305));
        test("123400000e300", Ok(1234e305));
        test("1234e305", Ok(1234e305));
        test("00000000170000000e300", Ok(17e307));
        test("0000000017e307", Ok(17e307));
        test("170000000e300", Ok(17e307));
        test("17e307", Ok(17e307));
        test("1e-1", Ok(1e-1));
        test("1e-2", Ok(1e-2));
        test("1e20", Ok(1e20));
        test("1e-20", Ok(1e-20));
        test("1e22", Ok(1e22));
        test("1e-22", Ok(1e-22));
        test("1e23", Ok(1e23));
        test("1e-23", Ok(1e-23));
        test("1e-25", Ok(1e-25));
        test("000000000000100000e303", Ok(1e308));
        test("00000001e308", Ok(1e308));
        test("100000e303", Ok(1e308));
        test("1e308", Ok(1e308));
        test("1e35", Ok(1e35));
        test("1e36", Ok(1e36));
        test("1e37", Ok(1e37));
        test("1e-39", Ok(1e-39));
        test("1e-5", Ok(1e-5));
        test("2e0", Ok(2.0));
        test("22250738585072011e-324", Ok(2.225073858507201e-308));
        test("2e1", Ok(20.0));
        test("2e2", Ok(200.0));
        test("2e-1", Ok(2e-1));
        test("2e-2", Ok(2e-2));
        test("2e20", Ok(2e20));
        test("2e-20", Ok(2e-20));
        test("2e22", Ok(2e22));
        test("2e-22", Ok(2e-22));
        test("2e23", Ok(2e23));
        test("2e-23", Ok(2e-23));
        test("2e-25", Ok(2e-25));
        test("2e35", Ok(2e35));
        test("2e36", Ok(2e36));
        test("2e37", Ok(2e37));
        test("2e-39", Ok(2e-39));
        test("2e-5", Ok(2e-5));
        test("358416272e-33", Ok(358416272e-33));
        test("00000030000e-328", Ok(40000e-328));
        test("30000e-328", Ok(40000e-328));
        test("3e-324", Ok(4e-324));
        test("5445618932859895362967233318697132813618813095743952975439298223406969961560047552942717636670910728746893019786283454139917900193169748259349067524939840552682198095012176093045431437495773903922425632551857520884625114624126588173520906670968542074438852601438992904761759703022688483745081090292688986958251711580854575674815074162979705098246243690189880319928315307816832576838178256307401454285988871020923752587330172447966674453785790265533466496640456213871241930958703059911787722565044368663670643970181259143319016472430928902201239474588139233890135329130660705762320235358869874608541509790266400643191187286648422874774910682648288516244021893172769161449825765517353755844373640588822904791244190695299838293263075467057383813882521706545084301049855505888186560731e-1035", Ok(5.445618932859895e-255));
        test(
            "5708990770823838890407843763683279797179383808e0",
            Ok(5708990770823838890407843763683279797179383808.0),
        );
        test(
            "5708990770823839207320493820740630171355185151999e-3",
            Ok(5708990770823838890407843763683279797179383808.0),
        );
        test(
            "5708990770823839207320493820740630171355185152001e-3",
            Ok(5708990770823839524233143877797980545530986496.0),
        );
        test(
            "5708990770823839207320493820740630171355185152e0",
            Ok(5708990770823839524233143877797980545530986496.0),
        );
        test(
            "5708990770823839524233143877797980545530986496e0",
            Ok(5708990770823839524233143877797980545530986496.0),
        );
        test("72057594037927928e0", Ok(72057594037927928.0));
        test("7205759403792793199999e-5", Ok(72057594037927928.0));
        test("7205759403792793200001e-5", Ok(72057594037927936.0));
        test("72057594037927932e0", Ok(72057594037927936.0));
        test("72057594037927936e0", Ok(72057594037927936.0));
        test("89255e-22", Ok(89255e-22));
        test("9e0", Ok(9.0));
        test("9e1", Ok(90.0));
        test("9e2", Ok(900.0));
        test("9223372036854774784e0", Ok(9223372036854774784.0));
        test("922337203685477529599999e-5", Ok(9223372036854774784.0));
        test("922337203685477529600001e-5", Ok(9223372036854775808.0));
        test("9223372036854775296e0", Ok(9223372036854775808.0));
        test("9223372036854775808e0", Ok(9223372036854775808.0));
        test("9e-1", Ok(9e-1));
        test("9e-2", Ok(9e-2));
        test("9e20", Ok(9e20));
        test("9e-20", Ok(9e-20));
        test("9e22", Ok(9e22));
        test("9e-22", Ok(9e-22));
        test("9e23", Ok(9e23));
        test("9e-23", Ok(9e-23));
        test("9e-25", Ok(9e-25));
        test("9e35", Ok(9e35));
        test("9e36", Ok(9e36));
        test("9e37", Ok(9e37));
        test("9e-39", Ok(9e-39));
        test("9e-5", Ok(9e-5));
        test("00000000180000000e300", Err(Error::Overflow));
        test("0000000018e307", Err(Error::Overflow));
        test("00000001000000e303", Err(Error::Overflow));
        test("0000001e309", Err(Error::Overflow));
        test("1000000e303", Err(Error::Overflow));
        test("17976931348623159e292", Err(Error::Overflow));
        test("180000000e300", Err(Error::Overflow));
        test("18e307", Err(Error::Overflow));
        test("1e309", Err(Error::Overflow));
        test("1e-409", Ok(0.0));
        // IEE754 says it's OK to lose the sign bit when dealing with NaN, but fish has
        // traditionally returned an explicitly negative NaN for this:
        {
            let (f, n) = parse_partial_iter("-NaN".chars(), '.').unwrap();
            assert_eq!(f.to_bits(), 0xFFF8000000000000);
            assert_eq!(n, 4);
        }

        test_sep("1,1e1", Ok(1.1e1), ',');

        test_consumed("12345e37randomstuff", Ok(12345e37), 8);
        test_consumed("infzoo", Ok(f64::INFINITY), 3);
        test_consumed("infinit", Ok(f64::INFINITY), 3);
        test_consumed("infinity", Ok(f64::INFINITY), 8);
    }

    #[test]
    /// Tests from libc-test's `/src/functional/strtod.c`, adapted to work in rust.
    fn libc_test_strtod() {
        use std::f64::INFINITY;

        for (i, (s, f)) in [
            ("0", 0.0),
            ("00.00", 0.0),
            ("-.00000", -0.0),
            ("1e+1000000", INFINITY),
            ("1e-1000000", 0.0),
            // 2^-1074 * 0.5 - eps
            (".2470328229206232720882843964341106861825299013071623822127928412503377536351043e-323", 0.0),
            // 2^-1074 * 0.5 + eps
            (".2470328229206232720882843964341106861825299013071623822127928412503377536351044e-323",
                 hfloat("0x1p-1074") /* f64::from_bits(0x1) */),
            // 2^-1074 * 1.5 - eps
            (".7410984687618698162648531893023320585475897039214871466383785237510132609053131e-323",
                 hfloat("0x1p-1074") /* f64::from_bits(0x1) */),
            // 2^-1074 * 1.5 + eps
            (".7410984687618698162648531893023320585475897039214871466383785237510132609053132e-323",
                 hfloat("0x1p-1073") /* f64::from_bits(0x2) */),
            // 2^-1022 + 2^-1075 - eps
            (".2225073858507201630123055637955676152503612414573018013083228724049586647606759e-307",
                 hfloat("0x1p-1022") /* f64::from_bits(0x10000000000000) */),
            // 2^-1022 + 2^-1075 + eps
            (".2225073858507201630123055637955676152503612414573018013083228724049586647606760e-307",
                 hfloat("0x1.0000000000001p-1022") /* f64::from_bits(0x10000000000001) */),
            // 2^1024 - 2^970 - eps
            (concat!("17976931348623158079372897140530341507993413271003782693617377898044",
            "49682927647509466490179775872070963302864166928879109465555478519404",
            "02630657488671505820681908902000708383676273854845817711531764475730",
            "27006985557136695962284291481986083493647529271907416844436551070434",
            "2711559699508093042880177904174497791.999999999999999999999999999999"),
                hfloat("0x1.fffffffffffffp1023") /*f64::from_bits(0x7fefffffffffffff)*/),
            // 2^1024 - 2^970
            (concat!("17976931348623158079372897140530341507993413271003782693617377898044",
            "49682927647509466490179775872070963302864166928879109465555478519404",
            "02630657488671505820681908902000708383676273854845817711531764475730",
            "27006985557136695962284291481986083493647529271907416844436551070434",
            "2711559699508093042880177904174497792"), INFINITY),
            // some random numbers
            (".5961860348131807091861002266453941950428e00", 0.59618603481318067), // 0x1.313f4bc3b584cp-1
            ("1.815013169218038729887460898733526957442e-1", 0.18150131692180388), // 0x1.73b6f662e1712p-3
            ("42.07082357534453600681618685682257590772e-2", 0.42070823575344535), // 0x1.aece23c6e028dp-2
            ("665.4686306516261456328973225579833470816e-3", 0.66546863065162609), // 0x1.54b84dea53453p-1
            ("6101.852922970868621786690495485449831753e-4", 0.61018529229708685), // 0x1.386a34e5d516bp-1
            ("76966.95208236968077849464348875471158549e-5", 0.76966952082369677), // 0x1.8a121f9954dfap-1
            ("250506.5322228682496132604807222923702304e-6", 0.25050653222286823), // 0x1.0084c8cd538c2p-2
            ("2740037.230228005325852424697698331177377e-7", 0.27400372302280052), // 0x1.18946e9575ef4p-2
            ("20723093.50049742645941529268715428324490e-8", 0.20723093500497428), // 0x1.a868b14486e4dp-3
            ("0.7900280238081604956226011047460238748912e1", 7.9002802380816046), // 0x1.f99e3100f2eaep+2
            ("0.9822860653737296848190558448760465863597e2", 98.228606537372968), // 0x1.88ea17d506accp+6
            ("0.7468949723190370809405570560160405324869e3", 746.89497231903704), // 0x1.75728e73f48b7p+9
            ("0.1630268320282728475980459844271031751665e4", 1630.2683202827284), // 0x1.97912c28d5cbp+10
            ("0.4637168629719170695109918769645492022088e5", 46371.686297191707), // 0x1.6a475f6258737p+15
            ("0.6537805944497711554209461686415872067523e6", 653780.59444977110), // 0x1.3f3a9305bb86cp+19
            ("0.2346324356502437045212230713960457676531e6", 234632.43565024371), // 0x1.ca4437c3631eap+17
            ("0.9709481716420048341897258980454298205278e8", 97094817.164200485), // 0x1.7263284a8242cp+26
            ("0.4996908522051874110779982354932499499602e9", 499690852.20518744), // 0x1.dc8ad6434872ap+28
        ].iter().enumerate() {
            eprintln!("{i}: {s}");
            // These expect Ok(f64::INFINITY) not Err(Overflow), so use parse_partial_iter()
            // directly and not via the test() wrapper.
            assert_eq!(parse_partial_iter(s.chars(), '.').map(|(f, _)| f), Some(*f));
        }
    }

    #[test]
    /// Tests from libc-test's `/src/functional/sscanf.c` with `%lf` format strings, adapted to work in rust.
    ///
    /// These tests intentionally include overflowing literals that will be converted to f64::INFINITY.
    #[allow(overflowing_literals)]
    fn libc_test_sscanf() {
        macro_rules! test_f {
            ($float:literal) => {
                println!("{}", stringify!($float));
                // Some of these expect f64::INFINITY instead of Err(Overflow)
                assert_eq!(
                    parse_partial_iter(stringify!($float).chars(), '.')
                        .unwrap()
                        .0,
                    $float as f64
                );
            };
        }

        test_f!(123);
        test_f!(123.0);
        test_f!(123.0e+0);
        test_f!(123.0e+4);
        test_f!(1.234e1234);
        test_f!(1.234e-1234);
        test_f!(1.234e56789);
        test_f!(1.234e-56789);
        test_f!(-0.5);
        test_f!(0.1);
        test_f!(0.2);
        test_f!(0.1e-10);
        // test_f(0x1234p56);
        test_f!(1234e56);

        // These should fail
        assert!(parse_partial_iter("10e".chars(), '.').is_none());
        assert!(parse_partial_iter("".chars(), '.').is_none());
    }

    #[cfg(test)]
    fn hfloat(s: &str) -> f64 {
        super::hex_float::parse_hex_float(s.chars(), '.').unwrap().0
    }

    fn test(input: &str, val: Result<f64, Error>) {
        test_sep(input, val, '.')
    }

    fn test_sep(input: &str, val: Result<f64, Error>, decimalsep: char) {
        let mut consumed = 0;
        let result = wcstod(input, decimalsep, &mut consumed);
        // There are fundamental issues with f64 accuracy under x87. See #10474 and https://github.com/rust-lang/rust/issues/114479
        if cfg!(any(not(target_arch = "x86"), target_feature = "sse2")) {
            assert_eq!(result, val);
        } else {
            // Make sure the result is at least somewhat sane. We might need to change f64::EPSILON
            // to a bigger value if we are testing the result after operations that have multiple
            // opportunities for rounding loss in the future.
            assert!(result == val || (result.unwrap() - val.unwrap()).abs() < f64::EPSILON);
        }
        if result.is_ok() {
            assert_eq!(consumed, input.chars().count());
            assert_eq!(
                result.unwrap().is_sign_positive(),
                val.unwrap().is_sign_positive()
            );
        }
    }

    fn test_consumed(input: &str, val: Result<f64, Error>, exp_consumed: usize) {
        let mut consumed = 0;
        let result = wcstod(input, '.', &mut consumed);
        assert_eq!(result, val);
        assert_eq!(consumed, exp_consumed);
    }

    #[test]
    fn wcstod_underscores() {
        let test = |s| {
            let mut consumed = 0;
            super::wcstod_underscores(s, &mut consumed).map(|f| (f, consumed))
        };

        assert_eq!(test("123"), Ok((123.0, 3)));

        assert_eq!(test("123"), Ok((123.0, 3)));
        assert_eq!(test("1_2.3_4.5_6"), Ok((12.34, 7)));
        assert_eq!(test("1_2"), Ok((12.0, 3)));
        assert_eq!(test("1_._2"), Ok((1.2, 5)));
        assert_eq!(test("1__2"), Ok((12.0, 4)));
        assert_eq!(test(" 1__2 3__4 "), Ok((12.0, 5)));
        assert_eq!(test("1_2 3_4"), Ok((12.0, 3)));
        assert_eq!(test(" 1"), Ok((1.0, 2)));
        assert_eq!(test(" 1_"), Ok((1.0, 3)));
        assert_eq!(test(" 1__"), Ok((1.0, 4)));
        assert_eq!(test(" 1___"), Ok((1.0, 5)));
        assert_eq!(test(" 1___ 2___"), Ok((1.0, 5)));
        assert_eq!(test(" _1"), Ok((1.0, 3)));
        assert_eq!(test("1 "), Ok((1.0, 1)));
        assert_eq!(test("infinity_"), Ok((f64::INFINITY, 8)));
        assert_eq!(test(" -INFINITY"), Ok((f64::NEG_INFINITY, 10)));
        assert_eq!(test("_infinity"), Err(Error::Empty));
        {
            let (f, n) = test("nan(0)").unwrap();
            assert!(f.is_nan());
            assert_eq!(n, 3);
        }
        {
            let (f, n) = test("nan(0)_").unwrap();
            assert!(f.is_nan());
            assert_eq!(n, 3);
        }
        assert_eq!(test("_nan(0)"), Err(Error::Empty));
        // We don't strip the underscores in this commented-out test case, and the behavior is
        // implementation-defined, so we don't actually know how many characters will get consumed. On
        // macOS the strtod man page only says what happens with an alphanumeric string passed to nan(),
        // but the strtod consumes all of the characters even if there are underscores.
        // assert_eq!(test("nan(0_1_2)"), Ok((nan(0_1_2), 3)));
        assert_eq!(test(" _ 1"), Err(Error::Empty));
        assert_eq!(test("0x_dead_beef"), Ok((0xdeadbeef_u32 as f64, 12)));
        assert_eq!(test("None"), Err(Error::InvalidChar));
        assert_eq!(test(" None"), Err(Error::InvalidChar));
        assert_eq!(test("Also none"), Err(Error::InvalidChar));
        assert_eq!(test(" Also none"), Err(Error::InvalidChar));
    }
}
