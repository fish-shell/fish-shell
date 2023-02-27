// Adapted from https://github.com/tjol/sprintf-rs
// License follows:
//
// Copyright (c) 2021 Thomas Jollans
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is furnished
// to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
// FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS
// OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
// WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF
// OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

use super::printf::{sprintf, Printf};
use crate::wchar::{widestrs, WString, L};

fn check_fmt<T: Printf>(nfmt: &str, arg: T, expected: &str) {
    let fmt: WString = nfmt.into();
    let our_result = sprintf!(&fmt, arg);
    assert_eq!(our_result, expected);
}

fn check_fmt_2<T: Printf, T2: Printf>(nfmt: &str, arg: T, arg2: T2, expected: &str) {
    let fmt: WString = nfmt.into();
    let our_result = sprintf!(&fmt, arg, arg2);
    assert_eq!(our_result, expected);
}

#[test]
fn test_int() {
    check_fmt("%d", 12, "12");
    check_fmt("~%d~", 148, "~148~");
    check_fmt("00%dxx", -91232, "00-91232xx");
    check_fmt("%x", -9232, "ffffdbf0");
    check_fmt("%X", 432, "1B0");
    check_fmt("%09X", 432, "0000001B0");
    check_fmt("%9X", 432, "      1B0");
    check_fmt("%+9X", 492, "      1EC");
    check_fmt("% #9x", 4589, "   0x11ed");
    check_fmt("%2o", 4, " 4");
    check_fmt("% 12d", -4, "          -4");
    check_fmt("% 12d", 48, "          48");
    check_fmt("%ld", -4_i64, "-4");
    check_fmt("%lX", -4_i64, "FFFFFFFFFFFFFFFC");
    check_fmt("%ld", 48_i64, "48");
    check_fmt("%-8hd", -12_i16, "-12     ");
}

#[test]
fn test_float() {
    check_fmt("%f", -46.38, "-46.380000");
    check_fmt("%012.3f", 1.2, "00000001.200");
    check_fmt("%012.3e", 1.7, "0001.700e+00");
    check_fmt("%e", 1e300, "1.000000e+300");
    check_fmt("%012.3g%%!", 2.6, "0000000002.6%!");
    check_fmt("%012.5G", -2.69, "-00000002.69");
    check_fmt("%+7.4f", 42.785, "+42.7850");
    check_fmt("{}% 7.4E", 493.12, "{} 4.9312E+02");
    check_fmt("% 7.4E", -120.3, "-1.2030E+02");
    check_fmt("%-10F", f64::INFINITY, "INF       ");
    check_fmt("%+010F", f64::INFINITY, "      +INF");
    check_fmt("% f", f64::NAN, " nan");
    check_fmt("%+f", f64::NAN, "+nan");
    check_fmt("%.1f", 999.99, "1000.0");
    check_fmt("%.1f", 9.99, "10.0");
    check_fmt("%.1e", 9.99, "1.0e+01");
    check_fmt("%.2f", 9.99, "9.99");
    check_fmt("%.2e", 9.99, "9.99e+00");
    check_fmt("%.3f", 9.99, "9.990");
    check_fmt("%.3e", 9.99, "9.990e+00");
    check_fmt("%.1g", 9.99, "1e+01");
    check_fmt("%.1G", 9.99, "1E+01");
    check_fmt("%.1f", 2.99, "3.0");
    check_fmt("%.1e", 2.99, "3.0e+00");
    check_fmt("%.1g", 2.99, "3");
    check_fmt("%.1f", 2.599, "2.6");
    check_fmt("%.1e", 2.599, "2.6e+00");
    check_fmt("%.1g", 2.599, "3");
}

#[test]
fn test_str() {
    check_fmt(
        "test %% with string: %s yay\n",
        "FOO",
        "test % with string: FOO yay\n",
    );
    check_fmt("test char %c", '~', "test char ~");
    check_fmt_2("%*ls", 5, "^", "    ^");
}

#[test]
#[widestrs]
fn test_str_concat() {
    assert_eq!(sprintf!("%s-%ls"L, "abc", "def"L), "abc-def"L);
    assert_eq!(sprintf!("%s-%ls"L, "abc", "def"L), "abc-def"L);
}

#[test]
#[should_panic]
fn test_bad_format() {
    sprintf!(L!("%s"), 123);
}

#[test]
#[should_panic]
fn test_missing_arg() {
    sprintf!(L!("%s-%s"), "abc");
}

#[test]
#[should_panic]
fn test_too_many_args() {
    sprintf!(L!("%d"), 1, 2, 3);
}
