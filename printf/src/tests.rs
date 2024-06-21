use crate::arg::ToArg;
use crate::locale::{Locale, C_LOCALE, EN_US_LOCALE};
use crate::{sprintf_locale, Error};
use libc::c_char;
use std::f64::consts::{E, PI, TAU};
use std::fmt;
use widestring::{utf32str, Utf32Str};

// sprintf, checking length
macro_rules! sprintf_check {
    (
        $fmt:expr, // format string
        $($arg:expr),* // arguments
        $(,)? // optional trailing comma
    ) => {
        {
            let mut target = String::new();
            let chars: Vec<char> = $fmt.chars().collect();
            let len = $crate::sprintf_c_locale(
                &mut target,
                &chars,
                &mut [$($arg.to_arg()),*]
            ).expect("printf failed");
            assert!(len == target.len(), "Wrong length returned: {} vs {}", len, target.len());
            target
        }
    };
}

macro_rules! assert_fmt {
    ($fmt:expr $(, $arg:expr)* => $expected:expr) => {
        assert_eq!(sprintf_check!($fmt, $($arg),*), $expected)
    };
}

macro_rules! assert_fmt1 {
    ($fmt:expr, $arg:expr, $expected:expr) => {
        assert_fmt!($fmt, $arg => $expected)
    };
}

// sprintf, except we expect to return an error.
macro_rules! sprintf_err {
    ($fmt:expr, $($arg:expr),* => $expected:expr) => {
        {
            let chars: Vec<char> = $fmt.chars().collect();
            let err = $crate::sprintf_c_locale(
                &mut NullOutput,
                &chars,
                &mut [$($arg.to_arg()),*],
            ).unwrap_err();
            assert_eq!(err, $expected, "Wrong error returned: {:?}", err);
        }
    };
}

// sprintf, except we throw away the output and return only the count.
macro_rules! sprintf_count {
    ($fmt:expr $(, $arg:expr)*) => {
        {
            let chars: Vec<char> = $fmt.chars().collect();
            $crate::sprintf_c_locale(
                &mut NullOutput,
                &chars,
                &mut [$($arg.to_arg()),*],
            ).expect("printf failed")
        }
    };
}

// Null writer which ignores all input.
struct NullOutput;
impl fmt::Write for NullOutput {
    fn write_str(&mut self, _s: &str) -> fmt::Result {
        Ok(())
    }
}

#[test]
fn smoke() {
    assert_fmt!("Hello, %s!", "world"  => "Hello, world!");
    assert_fmt!("Hello, %ls!", "world" => "Hello, world!");
    assert_fmt!("Hello, world! %d %%%%", 3 => "Hello, world! 3 %%");
    assert_fmt!("" => "");
}

#[test]
fn test1() {
    // A convenient place to isolate a single test, e.g. cargo test -- test1
    assert_fmt!("%.0e", 0 => "0e+00");
}

#[test]
fn test_n() {
    // Test that the %n specifier correctly stores the number of characters written.
    let mut count: usize = 0;
    assert_fmt!("%d%n", 123, &mut count => "123");
    assert_eq!(count, 3);

    assert_fmt!("%256d%%%n", 123, &mut count => format!("{:>256}%", 123));
    assert_eq!(count, 257);

    assert_fmt!("%d %s%n", 123, "hello", &mut count => "123 hello");
    assert_eq!(count, 3 + 1 + 5);

    assert_fmt!("%%%n", &mut count => "%");
    assert_eq!(count, 1);
}

#[test]
fn test_plain() {
    assert_fmt!("abc" => "abc");
    assert_fmt!("" => "");
    assert_fmt!("%%" => "%");
    assert_fmt!("%% def" => "% def");
    assert_fmt!("abc %%" => "abc %");
    assert_fmt!("abc %% def" => "abc % def");
    assert_fmt!("abc %%%% def" => "abc %% def");
    assert_fmt!("%%%%%%" => "%%%");
}

#[test]
fn test_str() {
    assert_fmt!("hello %s", "world" => "hello world");
    assert_fmt!("hello %%%s", "world" => "hello %world");
    assert_fmt!("%10s", "world" => "     world");
    assert_fmt!("%.4s", "world" => "worl");
    assert_fmt!("%10.4s", "world" => "      worl");
    assert_fmt!("%-10.4s", "world" => "worl      ");
    assert_fmt!("%-10s", "world" => "world     ");

    assert_fmt!("test %% with string: %s yay\n", "FOO" => "test % with string: FOO yay\n");
    assert_fmt!("test char %c", '~' => "test char ~");

    assert_fmt!("%.0s", "test" => "");
    assert_fmt!("%.1s", "test" => "t");
    assert_fmt!("%.3s", "test" => "tes");
    assert_fmt!("%5.3s", "test" => "  tes");
    assert_fmt!("%.4s", "test" => "test");
    assert_fmt!("%.100s", "test" => "test");
}

#[test]
fn test_int() {
    assert_fmt!("% 0*i", 23125, 17 => format!(" {:023124}", 17));
    assert_fmt!("% 010i", 23125 => " 000023125");
    assert_fmt!("% 10i", 23125 => "     23125");
    assert_fmt!("% 5i", 23125 => " 23125");
    assert_fmt!("% 4i", 23125 => " 23125");
    assert_fmt!("%- 010i", 23125 => " 23125    ");
    assert_fmt!("%- 10i", 23125 => " 23125    ");
    assert_fmt!("%- 5i", 23125 => " 23125");
    assert_fmt!("%- 4i", 23125 => " 23125");
    assert_fmt!("%+ 010i", 23125 => "+000023125");
    assert_fmt!("%+ 10i", 23125 => "    +23125");
    assert_fmt!("%+ 5i", 23125 => "+23125");
    assert_fmt!("%+ 4i", 23125 => "+23125");
    assert_fmt!("%-010i", 23125 => "23125     ");
    assert_fmt!("%-10i", 23125 => "23125     ");
    assert_fmt!("%-5i", 23125 => "23125");
    assert_fmt!("%-4i", 23125 => "23125");

    assert_fmt!("%d", 12 => "12");
    assert_fmt!("%d", -123 => "-123");
    assert_fmt!("~%d~", 148 => "~148~");
    assert_fmt!("00%dxx", -91232 => "00-91232xx");
    assert_fmt!("%x", -9232 => "ffffdbf0");
    assert_fmt!("%X", 432 => "1B0");
    assert_fmt!("%09X", 432 => "0000001B0");
    assert_fmt!("%9X", 432 => "      1B0");
    assert_fmt!("%+9X", 492 => "      1EC");
    assert_fmt!("% #9x", 4589 => "   0x11ed");
    assert_fmt!("%2o", 4 => " 4");
    assert_fmt!("% 12d", -4 => "          -4");
    assert_fmt!("% 12d", 48 => "          48");
    assert_fmt!("%ld", -4_i64 => "-4");
    assert_fmt!("%lld", -4_i64 => "-4");
    assert_fmt!("%lX", -4_i64 => "FFFFFFFFFFFFFFFC");
    assert_fmt!("%ld", 48_i64 => "48");
    assert_fmt!("%lld", 48_i64 => "48");
    assert_fmt!("%-8hd", -12_i16 => "-12     ");

    assert_fmt!("%u", 12 => "12");
    assert_fmt!("~%u~", 148 => "~148~");
    assert_fmt!("00%uxx", 91232 => "0091232xx");
    assert_fmt!("%x", 9232 => "2410");
    assert_fmt!("%9X", 492 => "      1EC");
    assert_fmt!("% 12u", 4 => "           4");
    assert_fmt!("% 12u", 48 => "          48");
    assert_fmt!("%lu", 4_u64 => "4");
    assert_fmt!("%llu", 4_u64 => "4");
    assert_fmt!("%lX", 4_u64 => "4");
    assert_fmt!("%lu", 48_u64 => "48");
    assert_fmt!("%llu", 48_u64 => "48");
    assert_fmt!("%-8hu", 12_u16 => "12      ");

    // Gross combinations of padding and precision.
    assert_fmt!("%30d", 1234565678 => "                    1234565678");
    assert_fmt!("%030d", 1234565678 => "000000000000000000001234565678");
    assert_fmt!("%30.20d", 1234565678 => "          00000000001234565678");
    // Here we specify both a precision and request zero-padding.
    // "If a precision is given with a numeric conversion (d, i, o, u, x, and X), the 0 flag is ignored."
    assert_fmt!("%030.20d", 1234565678 => "          00000000001234565678");
    assert_fmt!("%030.0d", 1234565678 => "                    1234565678");

    // width, precision, alignment
    assert_fmt1!("%04d", 12, "0012");
    assert_fmt1!("%.3d", 12, "012");
    assert_fmt1!("%3d", 12, " 12");
    assert_fmt1!("%-3d", 12, "12 ");
    assert_fmt1!("%+3d", 12, "+12");
    assert_fmt1!("%+-5d", 12, "+12  ");
    assert_fmt1!("%+- 5d", 12, "+12  ");
    assert_fmt1!("%- 5d", 12, " 12  ");
    assert_fmt1!("% d", 12, " 12");
    assert_fmt1!("%0-5d", 12, "12   ");
    assert_fmt1!("%-05d", 12, "12   ");

    // ...explicit precision of 0 shall be no characters except for alt-octal.
    assert_fmt1!("%.0d", 0, "");
    assert_fmt1!("%.0o", 0, "");
    assert_fmt1!("%#.0d", 0, "");
    assert_fmt1!("%#.0o", 0, "0");
    assert_fmt1!("%#.0x", 0, "");

    // ...but it still has to honor width and flags.
    assert_fmt1!("%2.0u", 0, "  ");
    assert_fmt1!("%02.0u", 0, "  ");
    assert_fmt1!("%2.0d", 0, "  ");
    assert_fmt1!("%02.0d", 0, "  ");
    assert_fmt1!("% .0d", 0, " ");
    assert_fmt1!("%+.0d", 0, "+");
}

#[test]
fn test_octal() {
    assert_fmt!("% 010o", 23125 => "0000055125");
    assert_fmt!("% 10o", 23125 => "     55125");
    assert_fmt!("% 5o", 23125 => "55125");
    assert_fmt!("% 4o", 23125 => "55125");
    assert_fmt!("%- 010o", 23125 => "55125     ");
    assert_fmt!("%- 10o", 23125 => "55125     ");
    assert_fmt!("%- 5o", 23125 => "55125");
    assert_fmt!("%- 4o", 23125 => "55125");
    assert_fmt!("%+ 010o", 23125 => "0000055125");
    assert_fmt!("%+ 10o", 23125 => "     55125");
    assert_fmt!("%+ 5o", 23125 => "55125");
    assert_fmt!("%+ 4o", 23125 => "55125");
    assert_fmt!("%-010o", 23125 => "55125     ");
    assert_fmt!("%-10o", 23125 => "55125     ");
    assert_fmt!("%-5o", 23125 => "55125");
    assert_fmt!("%-4o", 23125 => "55125");
    assert_fmt1!("%o", 15, "17");
    assert_fmt1!("%#o", 15, "017");
    assert_fmt1!("%#o", 0, "0");
    assert_fmt1!("%#.0o", 0, "0");
    assert_fmt1!("%#.1o", 0, "0");
    assert_fmt1!("%#o", 1, "01");
    assert_fmt1!("%#.0o", 1, "01");
    assert_fmt1!("%#.1o", 1, "01");
    assert_fmt1!("%#04o", 1, "0001");
    assert_fmt1!("%#04.0o", 1, "  01");
    assert_fmt1!("%#04.1o", 1, "  01");
    assert_fmt1!("%04o", 1, "0001");
    assert_fmt1!("%04.0o", 1, "   1");
    assert_fmt1!("%04.1o", 1, "   1");
}

#[test]
fn test_hex() {
    assert_fmt!("% 010x", 23125 => "0000005a55");
    assert_fmt!("% 10x", 23125 => "      5a55");
    assert_fmt!("% 5x", 23125 => " 5a55");
    assert_fmt!("% 4x", 23125 => "5a55");
    assert_fmt!("%- 010x", 23125 => "5a55      ");
    assert_fmt!("%- 10x", 23125 => "5a55      ");
    assert_fmt!("%- 5x", 23125 => "5a55 ");
    assert_fmt!("%- 4x", 23125 => "5a55");
    assert_fmt!("%+ 010x", 23125 => "0000005a55");
    assert_fmt!("%+ 10x", 23125 => "      5a55");
    assert_fmt!("%+ 5x", 23125 => " 5a55");
    assert_fmt!("%+ 4x", 23125 => "5a55");
    assert_fmt!("%-010x", 23125 => "5a55      ");
    assert_fmt!("%-10x", 23125 => "5a55      ");
    assert_fmt!("%-5x", 23125 => "5a55 ");
    assert_fmt!("%-4x", 23125 => "5a55");

    assert_fmt!("%# 010x", 23125 => "0x00005a55");
    assert_fmt!("%# 10x", 23125 => "    0x5a55");
    assert_fmt!("%# 5x", 23125 => "0x5a55");
    assert_fmt!("%# 4x", 23125 => "0x5a55");
    assert_fmt!("%#- 010x", 23125 => "0x5a55    ");
    assert_fmt!("%#- 10x", 23125 => "0x5a55    ");
    assert_fmt!("%#- 5x", 23125 => "0x5a55");
    assert_fmt!("%#- 4x", 23125 => "0x5a55");
    assert_fmt!("%#+ 010x", 23125 => "0x00005a55");
    assert_fmt!("%#+ 10x", 23125 => "    0x5a55");
    assert_fmt!("%#+ 5x", 23125 => "0x5a55");
    assert_fmt!("%#+ 4x", 23125 => "0x5a55");
    assert_fmt!("%#-010x", 23125 => "0x5a55    ");
    assert_fmt!("%#-10x", 23125 => "0x5a55    ");
    assert_fmt!("%#-5x", 23125 => "0x5a55");
    assert_fmt!("%#-4x", 23125 => "0x5a55");

    assert_fmt!("% 010X", 23125 => "0000005A55");
    assert_fmt!("% 10X", 23125 => "      5A55");
    assert_fmt!("% 5X", 23125 => " 5A55");
    assert_fmt!("% 4X", 23125 => "5A55");
    assert_fmt!("%- 010X", 23125 => "5A55      ");
    assert_fmt!("%- 10X", 23125 => "5A55      ");
    assert_fmt!("%- 5X", 23125 => "5A55 ");
    assert_fmt!("%- 4X", 23125 => "5A55");
    assert_fmt!("%+ 010X", 23125 => "0000005A55");
    assert_fmt!("%+ 10X", 23125 => "      5A55");
    assert_fmt!("%+ 5X", 23125 => " 5A55");
    assert_fmt!("%+ 4X", 23125 => "5A55");
    assert_fmt!("%-010X", 23125 => "5A55      ");
    assert_fmt!("%-10X", 23125 => "5A55      ");
    assert_fmt!("%-5X", 23125 => "5A55 ");
    assert_fmt!("%-4X", 23125 => "5A55");

    assert_fmt!("%#x", 234834 => "0x39552");
    assert_fmt!("%#X", 234834 => "0X39552");
    assert_fmt!("%#.10o", 54834 => "0000153062");

    assert_fmt1!("%x", 63, "3f");
    assert_fmt1!("%#x", 63, "0x3f");
    assert_fmt1!("%X", 63, "3F");
}

#[test]
fn test_char() {
    assert_fmt!("%c", 'a' => "a");
    assert_fmt!("%10c", 'a' => "         a");
    assert_fmt!("%-10c", 'a' => "a         ");
}

#[test]
fn test_ptr() {
    assert_fmt!("%p", core::ptr::null::<u8>() => "0");
    assert_fmt!("%p", 0xDEADBEEF_usize as *const u8 => "0xdeadbeef");
}

#[test]
#[cfg_attr(
    all(target_arch = "x86", not(target_feature = "sse2")),
    ignore = "i586 has inherent accuracy issues, see rust-lang/rust#114479"
)]
fn test_float() {
    // Basic form, handling of exponent/precision for 0
    assert_fmt1!("%a", 0.0, "0x0p+0");
    assert_fmt1!("%e", 0.0, "0.000000e+00");
    assert_fmt1!("%f", 0.0, "0.000000");
    assert_fmt1!("%g", 0.0, "0");
    assert_fmt1!("%#g", 0.0, "0.00000");
    assert_fmt1!("%la", 0.0, "0x0p+0");
    assert_fmt1!("%le", 0.0, "0.000000e+00");
    assert_fmt1!("%lf", 0.0, "0.000000");
    assert_fmt1!("%lg", 0.0, "0");
    assert_fmt1!("%#lg", 0.0, "0.00000");

    // rounding
    assert_fmt1!("%f", 1.1, "1.100000");
    assert_fmt1!("%f", 1.2, "1.200000");
    assert_fmt1!("%f", 1.3, "1.300000");
    assert_fmt1!("%f", 1.4, "1.400000");
    assert_fmt1!("%f", 1.5, "1.500000");
    assert_fmt1!("%.4f", 1.06125, "1.0613"); /* input is not representible exactly as double */
    assert_fmt1!("%.4f", 1.03125, "1.0312"); /* 0x1.08p0 */
    assert_fmt1!("%.2f", 1.375, "1.38");
    assert_fmt1!("%.1f", 1.375, "1.4");
    assert_fmt1!("%.1lf", 1.375, "1.4");
    assert_fmt1!("%.15f", 1.1, "1.100000000000000");
    assert_fmt1!("%.16f", 1.1, "1.1000000000000001");
    assert_fmt1!("%.17f", 1.1, "1.10000000000000009");
    assert_fmt1!("%.2e", 1500001.0, "1.50e+06");
    assert_fmt1!("%.2e", 1505000.0, "1.50e+06");
    assert_fmt1!("%.2e", 1505000.0000009537, "1.51e+06");
    assert_fmt1!("%.2e", 1505001.0, "1.51e+06");
    assert_fmt1!("%.2e", 1506000.0, "1.51e+06");

    // pi in double precision, printed to a few extra places
    assert_fmt1!("%.15f", PI, "3.141592653589793");
    assert_fmt1!("%.18f", PI, "3.141592653589793116");

    // exact conversion of large integers
    assert_fmt1!(
        "%.0f",
        340282366920938463463374607431768211456.0,
        "340282366920938463463374607431768211456"
    );

    let tiny = f64::exp2(-1021.0);
    assert_fmt1!("%.1022f", tiny, format!("{:.1022}", tiny));

    let tiny = f64::exp2(-1022.0);
    assert_fmt1!("%.1022f", tiny, format!("{:.1022}", tiny));

    assert_fmt1!("%.12g", 1000000000005.0, "1e+12");
    assert_fmt1!("%.12g", 100000000002500.0, "1.00000000002e+14");

    assert_fmt1!("%.50g", 100000000000000.5, "100000000000000.5");
    assert_fmt1!("%.50g", 987654321098765.0, "987654321098765");
    assert_fmt1!("%.1f", 123123123123123.0, "123123123123123.0");
    assert_fmt1!("%g", 999999999.0, "1e+09");
    assert_fmt1!("%.3e", 999999999.75, "1.000e+09");

    assert_fmt!("%f", 1234f64 => "1234.000000");
    assert_fmt!("%.5f", 1234f64 => "1234.00000");
    assert_fmt!("%.*f", 6, 1234.56f64 => "1234.560000");
    assert_fmt!("%f", -46.38 => "-46.380000");
    assert_fmt!("%012.3f", 1.2 => "00000001.200");
    assert_fmt!("%012.3e", 1.7 => "0001.700e+00");
    assert_fmt!("%e", 1e300 => "1.000000e+300");
    assert_fmt!("%012.3g%%!", 2.6 => "0000000002.6%!");
    assert_fmt!("%012.5G", -2.69 => "-00000002.69");
    assert_fmt!("%+7.4f", 42.785 => "+42.7850");
    assert_fmt!("{}% 7.4E", 493.12 => "{} 4.9312E+02");
    assert_fmt!("% 7.4E", -120.3 => "-1.2030E+02");
    assert_fmt!("%-10F", f64::INFINITY => "INF       ");
    assert_fmt!("%+010F", f64::INFINITY => "      +INF");
    assert_fmt!("% f", f64::NAN => " nan");
    assert_fmt!("%+f", f64::NAN => "+nan");
    assert_fmt!("%.1f", 999.99 => "1000.0");
    assert_fmt!("%.1f", 9.99 => "10.0");
    assert_fmt!("%.1e", 9.99 => "1.0e+01");
    assert_fmt!("%.2f", 9.99 => "9.99");
    assert_fmt!("%.2e", 9.99 => "9.99e+00");
    assert_fmt!("%.3f", 9.99 => "9.990");
    assert_fmt!("%.3e", 9.99 => "9.990e+00");
    assert_fmt!("%.1g", 9.99 => "1e+01");
    assert_fmt!("%.1G", 9.99 => "1E+01");
    assert_fmt!("%.1f", 2.99 => "3.0");
    assert_fmt!("%.1e", 2.99 => "3.0e+00");
    assert_fmt!("%.1g", 2.99 => "3");
    assert_fmt!("%.1f", 2.599 => "2.6");
    assert_fmt!("%.1e", 2.599 => "2.6e+00");

    assert_fmt!("%30.15f", 1234565678.0 => "    1234565678.000000000000000");
    assert_fmt!("%030.15f", 1234565678.0 => "00001234565678.000000000000000");

    assert_fmt!("%05.3a", 123.456 => "0x1.eddp+6");
    assert_fmt!("%05.3A", 123.456 => "0X1.EDDP+6");

    // Regression test using smallest denormal.
    assert_fmt!("%.0f", f64::from_bits(1) => "0");
    assert_fmt!("%.1f", f64::from_bits(1) => "0.0");

    // More regression tests.
    assert_fmt!("%0.6f", 1e15 => "1000000000000000.000000");
    assert_fmt!("%.0e", 0 => "0e+00");
}

#[test]
#[cfg_attr(
    all(target_arch = "x86", not(target_feature = "sse2")),
    ignore = "i586 has inherent accuracy issues, see rust-lang/rust#114479"
)]
fn test_float_g() {
    // correctness in DBL_DIG places
    assert_fmt1!("%.15g", 1.23456789012345, "1.23456789012345");

    // correct choice of notation for %g
    assert_fmt1!("%g", 0.0001, "0.0001");
    assert_fmt1!("%g", 0.00001, "1e-05");
    assert_fmt1!("%g", 123456, "123456");
    assert_fmt1!("%g", 1234567, "1.23457e+06");
    assert_fmt1!("%.7g", 1234567, "1234567");
    assert_fmt1!("%.7g", 12345678, "1.234568e+07");
    assert_fmt1!("%.8g", 0.1, "0.1");
    assert_fmt1!("%.9g", 0.1, "0.1");
    assert_fmt1!("%.10g", 0.1, "0.1");
    assert_fmt1!("%.11g", 0.1, "0.1");

    // %g with precisions
    assert_fmt1!("%.5g", 12345, "12345");
    assert_fmt1!("%.4g", 12345, "1.234e+04");
    assert_fmt1!("%.3g", 12345, "1.23e+04");
    assert_fmt1!("%.2g", 12345, "1.2e+04");
    assert_fmt1!("%.1g", 12345, "1e+04");
    assert_fmt1!("%.5g", 0.000123456, "0.00012346");
    assert_fmt1!("%.4g", 0.000123456, "0.0001235");
    assert_fmt1!("%.3g", 0.000123456, "0.000123");
    assert_fmt1!("%.2g", 0.000123456, "0.00012");
    assert_fmt1!("%.1g", 0.000123456, "0.0001");
    assert_fmt1!("%.5g", 99999, "99999");
    assert_fmt1!("%.4g", 99999, "1e+05");
    assert_fmt1!("%.5g", 0.00001, "1e-05");
    assert_fmt1!("%.6g", 0.00001, "1e-05");

    // %g with precision and alt form
    assert_fmt1!("%#.5g", 12345, "12345.");
    assert_fmt1!("%#.4g", 12345, "1.234e+04");
    assert_fmt1!("%#.3g", 12345, "1.23e+04");
    assert_fmt1!("%#.2g", 12345, "1.2e+04");
    assert_fmt1!("%#.1g", 12345, "1.e+04");
    assert_fmt1!("%#.5g", 0.000123456, "0.00012346");
    assert_fmt1!("%#.4g", 0.000123456, "0.0001235");
    assert_fmt1!("%#.3g", 0.000123456, "0.000123");
    assert_fmt1!("%#.2g", 0.000123456, "0.00012");
    assert_fmt1!("%#.1g", 0.000123456, "0.0001");
    assert_fmt1!("%#.5g", 99999, "99999.");
    assert_fmt1!("%#.4g", 99999, "1.000e+05");
    assert_fmt1!("%#.5g", 0.00001, "1.0000e-05");
    assert_fmt1!("%#.6g", 0.00001, "1.00000e-05");

    // 'g' specifier changes meaning of precision to number of sigfigs.
    // This applies both to explicit precision, and the default precision, which is 6.
    assert_fmt!("%.1g", 2.599 => "3");
    assert_fmt!("%g", 3.0 => "3");
    assert_fmt!("%G", 3.0 => "3");
    assert_fmt!("%g", 1234234.532234234 => "1.23423e+06");
    assert_fmt!("%g", 23490234723.234239 => "2.34902e+10");
    assert_fmt!("%G", 23490234723.234239 => "2.34902E+10");

    assert_fmt!("%g", 0.0 => "0");
    assert_fmt!("%G", 0.0 => "0");
}

#[test]
fn test_float_hex() {
    assert_fmt1!("%.0a", 0.0, "0x0p+0");
    assert_fmt1!("%.1a", 0.0, "0x0.0p+0");
    assert_fmt1!("%.2a", 0.0, "0x0.00p+0");
    assert_fmt1!("%.3a", 0.0, "0x0.000p+0");

    // Test mixed precision and padding with left-adjust.
    assert_fmt!("%-10.5a", 1.23456 => "0x1.3c0c2p+0");
    assert_fmt!("%-15.3a", -123.456 => "-0x1.eddp+6    ");
    assert_fmt!("%-20.1a", 0.00001234 => "0x1.ap-17           ");

    assert_fmt!("%.0a", PI => "0x2p+1");
    assert_fmt!("%.1a", PI => "0x1.9p+1");
    assert_fmt!("%.2a", PI => "0x1.92p+1");
    assert_fmt!("%.3a", PI => "0x1.922p+1");
    assert_fmt!("%.4a", PI => "0x1.9220p+1");
    assert_fmt!("%.5a", PI => "0x1.921fbp+1");
    assert_fmt!("%.6a", PI => "0x1.921fb5p+1");
    assert_fmt!("%.7a", PI => "0x1.921fb54p+1");
    assert_fmt!("%.8a", PI => "0x1.921fb544p+1");
    assert_fmt!("%.9a", PI => "0x1.921fb5444p+1");
    assert_fmt!("%.10a", PI => "0x1.921fb54443p+1");
    assert_fmt!("%.11a", PI => "0x1.921fb54442dp+1");
    assert_fmt!("%.12a", PI => "0x1.921fb54442d2p+1");
    assert_fmt!("%.13a", PI => "0x1.921fb54442d18p+1");
    assert_fmt!("%.14a", PI => "0x1.921fb54442d180p+1");
    assert_fmt!("%.15a", PI => "0x1.921fb54442d1800p+1");
    assert_fmt!("%.16a", PI => "0x1.921fb54442d18000p+1");
    assert_fmt!("%.17a", PI => "0x1.921fb54442d180000p+1");
    assert_fmt!("%.18a", PI => "0x1.921fb54442d1800000p+1");
    assert_fmt!("%.19a", PI => "0x1.921fb54442d18000000p+1");
    assert_fmt!("%.20a", PI => "0x1.921fb54442d180000000p+1");
}

#[test]
fn test_prefixes() {
    // Test the valid prefixes.
    // Note that we generally ignore prefixes, since we know the width of the actual passed-in type.
    // We don't test prefixed 'n'.
    // Integer prefixes.
    use Error::BadFormatString;
    for spec in "diouxX".chars() {
        let expected = sprintf_check!(format!("%{}", spec), 5);
        for prefix in ["", "h", "hh", "l", "ll", "z", "j", "t"] {
            let actual = sprintf_check!(format!("%{}{}", prefix, spec), 5);
            assert_eq!(actual, expected);
        }

        for prefix in ["L", "B", "!"] {
            sprintf_err!(format!("%{}{}", prefix, spec), 5 => BadFormatString);
        }
    }

    // Floating prefixes.
    for spec in "aAeEfFgG".chars() {
        let expected = sprintf_check!(format!("%{}", spec), 5.0);
        for prefix in ["", "l", "L"] {
            let actual = sprintf_check!(format!("%{}{}", prefix, spec), 5.0);
            assert_eq!(actual, expected);
        }

        for prefix in ["h", "hh", "z", "j", "t", "!"] {
            sprintf_err!(format!("%{}{}", prefix, spec), 5.0 => BadFormatString);
        }
    }

    // Character prefixes.
    assert_eq!(sprintf_check!("%c", 'c'), "c");
    assert_eq!(sprintf_check!("%lc", 'c'), "c");
    assert_eq!(sprintf_check!("%s", "cs"), "cs");
    assert_eq!(sprintf_check!("%ls", "cs"), "cs");
}

#[test]
#[cfg_attr(
    all(target_arch = "x86", not(target_feature = "sse2")),
    ignore = "i586 has inherent accuracy issues, see rust-lang/rust#114479"
)]
#[allow(clippy::approx_constant)]
fn negative_precision_width() {
    assert_fmt!("%*s", -10, "hello" => "hello     ");
    assert_fmt!("%*s", -5, "world" => "world");
    assert_fmt!("%-*s", 10, "rust" => "rust      ");
    assert_fmt!("%.*s", -3, "example" => "example");

    assert_fmt!("%*d", -8, 456 => "456     ");
    assert_fmt!("%*i", -4, -789 => "-789");
    assert_fmt!("%-*o", 6, 123 => "173   ");
    assert_fmt!("%.*x", -2, 255 => "ff");
    assert_fmt!("%-*X", 7, 255 => "FF     ");
    assert_fmt!("%.*u", -5, 5000 => "5000");

    assert_fmt!("%*f", -12, 78.9 => "78.900000   ");
    assert_fmt!("%*g", -10, 12345.678 => "12345.7   ");
    assert_fmt!("%-*e", 15, 0.00012 => "1.200000e-04   ");
    assert_fmt!("%-*e", -15, 0.00012 => "1.200000e-04   ");
    assert_fmt!("%.*G", -2, 123.456 => "123.456");
    assert_fmt!("%-*E", 14, 123456.789 => "1.234568E+05  ");
    assert_fmt!("%-*E", -14, 123456.789 => "1.234568E+05  ");
    assert_fmt!("%.*f", -6, 3.14159 => "3.141590");

    assert_fmt!("%*.*f", -12, -6, 78.9 => "78.900000   ");
    assert_fmt!("%*.*g", -10, -3, 12345.678 => "12345.7   ");
    assert_fmt!("%*.*e", -15, -8, 0.00012 => "1.200000e-04   ");
    assert_fmt!("%*.*E", -14, -4, 123456.789 => "1.234568E+05  ");

    assert_fmt!("%*.*d", -6, -4, 2024 => "2024  ");
    assert_fmt!("%*.*x", -8, -3, 255 => "ff      ");

    assert_fmt!("%*.*f", -10, -2, 3.14159 => "3.141590  ");
    assert_fmt!("%*.*g", -12, -5, 123.456 => "123.456     ");
    assert_fmt!("%*.*e", -14, -3, 0.000789 => "7.890000e-04  ");
    assert_fmt!("%*.*E", -16, -5, 98765.4321 => "9.876543E+04    ");
}

#[test]
fn test_precision_overflow() {
    // Disallow precisions larger than i32::MAX.
    sprintf_err!("%.*g", usize::MAX, 1.0 => Error::Overflow);
    sprintf_err!("%.2147483648g", usize::MAX, 1.0 => Error::Overflow);
    sprintf_err!("%.*g", i32::MAX as usize + 1, 1.0 => Error::Overflow);
    sprintf_err!("%.2147483648g", i32::MAX as usize + 1, 1.0 => Error::Overflow);
}

#[test]
fn test_huge_precision_g() {
    let f = 1e-100;
    assert_eq!(sprintf_count!("%.2147483647g", f), 288);
    assert_eq!(sprintf_count!("%.*g", i32::MAX, f), 288);
    assert_fmt!("%.*g", i32::MAX, 2.0_f64.powi(-4) => "0.0625");

    sprintf_err!("%.*g", usize::MAX, f => Error::Overflow);
    sprintf_err!("%.2147483648g", f => Error::Overflow);
}

#[test]
fn test_errors() {
    use Error::*;
    sprintf_err!("%", => BadFormatString);
    sprintf_err!("%1", => BadFormatString);
    sprintf_err!("%%%k", => BadFormatString);
    sprintf_err!("%B", =>  BadFormatString);
    sprintf_err!("%lC", 'q' =>  BadFormatString);
    sprintf_err!("%lS", 'q' =>  BadFormatString);
    sprintf_err!("%d", => MissingArg);
    sprintf_err!("%d %u", 1 => MissingArg);
    sprintf_err!("%*d", 5 => MissingArg);
    sprintf_err!("%.*d", 5 => MissingArg);
    sprintf_err!("%%", 1 => ExtraArg);
    sprintf_err!("%d %d", 1, 2, 3 => ExtraArg);
    sprintf_err!("%d", "abc" => BadArgType);
    sprintf_err!("%s", 5 => BadArgType);
    sprintf_err!("%*d", "s", 5 => BadArgType);
    sprintf_err!("%.*d", "s", 5 => BadArgType);
    sprintf_err!("%18446744073709551616d", 5 => Overflow);
    sprintf_err!("%.18446744073709551616d", 5 => Overflow);

    // We allow passing an int for a float, but not a float for an int.
    assert_fmt!("%f", 3 => "3.000000");
    sprintf_err!("%d", 3.0 => BadArgType);

    // We allow passing an int for a char, reporting "overflow" for ints
    // which cannot be converted to char (treating surrogates as "overflow").
    assert_fmt!("%c", 0 => "\0");
    assert_fmt!("%c", 'Z' as u32 => "Z");
    sprintf_err!("%c", 5.0 => BadArgType);
    sprintf_err!("%c", -1 => Overflow);
    sprintf_err!("%c", u64::MAX => Overflow);
    sprintf_err!("%c", 0xD800 => Overflow);
    sprintf_err!("%c", 0xD8FF => Overflow);

    // Apostrophe only works for d,u,i,f,F
    sprintf_err!("%'c", 0 => BadFormatString);
    sprintf_err!("%'o", 0 => BadFormatString);
    sprintf_err!("%'x", 0 => BadFormatString);
    sprintf_err!("%'X", 0 => BadFormatString);
    sprintf_err!("%'n", 0 => BadFormatString);
    sprintf_err!("%'a", 0 => BadFormatString);
    sprintf_err!("%'A", 0 => BadFormatString);
    sprintf_err!("%'e", 0 => BadFormatString);
    sprintf_err!("%'E", 0 => BadFormatString);
    sprintf_err!("%'g", 0 => BadFormatString);
    sprintf_err!("%'G", 0 => BadFormatString);
}

#[test]
#[cfg_attr(
    all(target_arch = "x86", not(target_feature = "sse2")),
    ignore = "i586 has inherent accuracy issues, see rust-lang/rust#114479"
)]
fn test_locale() {
    fn test_printf_loc<'a>(expected: &str, locale: &Locale, format: &str, arg: impl ToArg<'a>) {
        let mut target = String::new();
        let format_chars: Vec<char> = format.chars().collect();
        let len = sprintf_locale(&mut target, &format_chars, locale, &mut [arg.to_arg()])
            .expect("printf failed");
        assert_eq!(len, target.len());
        assert_eq!(target, expected);
    }

    let mut locale = C_LOCALE;
    locale.decimal_point = ',';
    locale.thousands_sep = Some('!');
    locale.grouping = [3, 1, 0, 0];

    test_printf_loc("-46,380000", &locale, "%f", -46.38);
    test_printf_loc("00000001,200", &locale, "%012.3f", 1.2);
    test_printf_loc("1234", &locale, "%d", 1234);
    test_printf_loc("0x1,9p+3", &locale, "%a", 12.5);
    test_printf_loc("12345!6!789", &locale, "%'d", 123456789);
    test_printf_loc("123!4!567", &locale, "%'d", 1234567);
    test_printf_loc("214748!3!647", &locale, "%'u", 2147483647);
    test_printf_loc("-123!4!567", &locale, "%'i", -1234567);
    test_printf_loc("-123!4!567,890000", &locale, "%'f", -1234567.89);
    test_printf_loc("123!4!567,8899999999", &locale, "%'.10f", 1234567.89);
    test_printf_loc("12!3!456,789", &locale, "%'.3F", 123456.789);
    test_printf_loc("00000000001!234", &locale, "%'015d", 1234);
    test_printf_loc("1!2!345", &locale, "%'7d", 12345);
    test_printf_loc(" 1!2!345", &locale, "%'8d", 12345);
    test_printf_loc("+1!2!345", &locale, "%'+d", 12345);

    // Thousands seps count as width, and so remove some leading zeros.
    // Padding does NOT use thousands sep.
    test_printf_loc("0001234567", &EN_US_LOCALE, "%010d", 1234567);
    test_printf_loc("01,234,567", &EN_US_LOCALE, "%'010d", 1234567);
    test_printf_loc(
        "000000000000000001,222,333,444",
        &EN_US_LOCALE,
        "%'0.30d",
        1222333444,
    );
}

#[test]
#[ignore]
fn test_float_hex_prec() {
    // Check that libc and our hex float formatting agree for each precision.
    // Note that our hex float formatting rounds according to the rounding mode,
    // while libc may not; as a result we may differ in the last digit. So this
    // requires manual comparison.
    let mut c_storage = [0u8; 256];
    let c_storage_ptr = c_storage.as_mut_ptr() as *mut c_char;
    let mut rust_str = String::with_capacity(256);

    let c_fmt = b"%.*a\0".as_ptr() as *const c_char;
    let mut failed = false;
    for sign in [1.0, -1.0].into_iter() {
        for mut v in [0.0, 0.5, 1.0, 1.5, PI, TAU, E].into_iter() {
            v *= sign;
            for preci in 1..=200_i32 {
                rust_str.clear();
                crate::sprintf!(=> &mut rust_str, utf32str!("%.*a"), preci, v);

                let printf_str = unsafe {
                    let len = libc::snprintf(c_storage_ptr, c_storage.len(), c_fmt, preci, v);
                    assert!(len >= 0);
                    let sl = std::slice::from_raw_parts(c_storage_ptr as *const u8, len as usize);
                    std::str::from_utf8(sl).unwrap()
                };
                if rust_str != printf_str {
                    println!(
                        "Our printf and libc disagree on hex formatting of float: {v}
                        with precision: {preci}
                            our output: <{rust_str}>
                           libc output: <{printf_str}>"
                    );
                    failed = true;
                }
            }
        }
    }
    assert!(!failed);
}

fn test_exhaustive(rust_fmt: &Utf32Str, c_fmt: *const c_char) {
    // "There's only 4 billion floats so test them all."
    // This tests a format string expected to be of the form "%.*g" or "%.*e".
    // That is, it takes a precision and a double.
    println!("Testing {}", rust_fmt);
    let mut rust_str = String::with_capacity(128);
    let mut c_storage = [0u8; 128];
    let c_storage_ptr = c_storage.as_mut_ptr() as *mut c_char;

    for i in 0..=u32::MAX {
        if i % 1000000 == 0 {
            println!("{:.2}%", (i as f64) / (u32::MAX as f64) * 100.0);
        }
        let f = f32::from_bits(i);
        let ff = f as f64;
        for preci in 0..=10 {
            rust_str.clear();
            crate::sprintf!(=> &mut rust_str, rust_fmt, preci, ff);

            let printf_str = unsafe {
                let len = libc::snprintf(c_storage_ptr, c_storage.len(), c_fmt, preci, ff);
                assert!(len >= 0);
                let sl = std::slice::from_raw_parts(c_storage_ptr as *const u8, len as usize);
                std::str::from_utf8(sl).unwrap()
            };
            if rust_str != printf_str {
                println!(
                    "Rust and libc disagree on formatting float {i:x}: {ff}\n
                             with precision: {preci}
                              format string: {rust_fmt}
                             rust output: <{rust_str}>
                             libc output: <{printf_str}>"
                );
                assert_eq!(rust_str, printf_str);
            }
        }
    }
}

#[test]
#[ignore]
fn test_float_g_exhaustive() {
    // To run: cargo test test_float_g_exhaustive --release -- --ignored --nocapture
    test_exhaustive(
        widestring::utf32str!("%.*g"),
        b"%.*g\0".as_ptr() as *const c_char,
    );
}

#[test]
#[ignore]
fn test_float_e_exhaustive() {
    // To run: cargo test test_float_e_exhaustive --release -- --ignored --nocapture
    test_exhaustive(
        widestring::utf32str!("%.*e"),
        b"%.*e\0".as_ptr() as *const c_char,
    );
}

#[test]
#[ignore]
fn test_float_f_exhaustive() {
    // To run: cargo test test_float_f_exhaustive --release -- --ignored --nocapture
    test_exhaustive(
        widestring::utf32str!("%.*f"),
        b"%.*f\0".as_ptr() as *const c_char,
    );
}
