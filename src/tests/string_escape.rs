use std::sync::MutexGuard;

use crate::common::{
    ENCODE_DIRECT_BASE, ENCODE_DIRECT_END, EscapeFlags, EscapeStringStyle, UnescapeStringStyle,
    escape_string, fish_setlocale, str2wcstring, unescape_string, wcs2string,
};
use crate::locale::LOCALE_LOCK;
use crate::util::{get_rng_seed, get_seeded_rng};
use crate::wchar::{L, WString, wstr};
use crate::wutil::encoding::{
    AT_LEAST_MB_LEN_MAX, probe_is_multibyte_locale, wcrtomb, zero_mbstate,
};
use rand::{Rng, RngCore};

/// wcs2string is locale-dependent, so ensure we have a multibyte locale
/// before using it in a test.
fn setlocale() -> MutexGuard<'static, ()> {
    let guard = LOCALE_LOCK.lock().unwrap();

    #[rustfmt::skip]
    const UTF8_LOCALES: &[&str] = &[
        "C.UTF-8", "en_US.UTF-8", "en_GB.UTF-8", "de_DE.UTF-8", "C.utf8", "UTF-8",
    ];
    if probe_is_multibyte_locale() {
        return guard;
    }
    for locale in UTF8_LOCALES {
        let locale = std::ffi::CString::new(locale.to_owned()).unwrap();
        unsafe { libc::setlocale(libc::LC_CTYPE, locale.as_ptr()) };
        if probe_is_multibyte_locale() {
            fish_setlocale(); // Update cached locale information.
            return guard;
        }
    }
    panic!("No UTF-8 locale found");
}

#[test]
fn test_escape_string() {
    let regex = |input| escape_string(input, EscapeStringStyle::Regex);

    // plain text should not be needlessly escaped
    assert_eq!(regex(L!("hello world!")), L!("hello world!"));

    // all the following are intended to be ultimately matched literally - even if they
    // don't look like that's the intent - so we escape them.
    assert_eq!(regex(L!(".ext")), L!("\\.ext"));
    assert_eq!(regex(L!("{word}")), L!("\\{word\\}"));
    assert_eq!(regex(L!("hola-mundo")), L!("hola\\-mundo"));
    assert_eq!(
        regex(L!("$17.42 is your total?")),
        L!("\\$17\\.42 is your total\\?")
    );
    assert_eq!(
        regex(L!("not really escaped\\?")),
        L!("not really escaped\\\\\\?")
    );
}

#[test]
pub fn test_unescape_sane() {
    const TEST_CASES: &[(&wstr, &wstr)] = &[
        (L!("abcd"), L!("abcd")),
        (L!("'abcd'"), L!("abcd")),
        (L!("'abcd\\n'"), L!("abcd\\n")),
        (L!("\"abcd\\n\""), L!("abcd\\n")),
        (L!("\"abcd\\n\""), L!("abcd\\n")),
        (L!("\\143"), L!("c")),
        (L!("'\\143'"), L!("\\143")),
        (L!("\\n"), L!("\n")), // \n normally becomes newline
    ];

    for (input, expected) in TEST_CASES {
        let Some(output) = unescape_string(input, UnescapeStringStyle::default()) else {
            panic!("Failed to unescape string {input:?}");
        };

        assert_eq!(
            output, *expected,
            "In unescaping {input:?}, expected {expected:?} but got {output:?}\n"
        );
    }
}

#[test]
fn test_escape_var() {
    const TEST_CASES: &[(&wstr, &wstr)] = &[
        (L!(" a"), L!("_20_a")),
        (L!("a B "), L!("a_20_42_20_")),
        (L!("a b "), L!("a_20_b_20_")),
        (L!(" B"), L!("_20_42_")),
        (L!(" f"), L!("_20_f")),
        (L!(" 1"), L!("_20_31_")),
        (L!("a\nghi_"), L!("a_0A_ghi__")),
    ];

    for (input, expected) in TEST_CASES {
        let output = escape_string(input, EscapeStringStyle::Var);

        assert_eq!(
            output, *expected,
            "In escaping {input:?} with style var, expected {expected:?} but got {output:?}\n"
        );
    }
}

fn escape_test(escape_style: EscapeStringStyle, unescape_style: UnescapeStringStyle) {
    let _locale_guard = setlocale();
    let seed: u128 = 92348567983274852905629743984572;
    let mut rng = get_seeded_rng(seed);

    let mut random_string = WString::new();
    let mut escaped_string;
    for _ in 0..(ESCAPE_TEST_COUNT as u32) {
        random_string.clear();
        let length = rng.gen_range(0..=(2 * ESCAPE_TEST_LENGTH));
        for _ in 0..length {
            random_string
                .push(char::from_u32((rng.next_u32() % ESCAPE_TEST_CHAR as u32) + 1).unwrap());
        }

        escaped_string = escape_string(&random_string, escape_style);
        let Some(unescaped_string) = unescape_string(&escaped_string, unescape_style) else {
            let slice = escaped_string.as_char_slice();
            panic!("Failed to unescape string {slice:?}");
        };
        assert_eq!(
            random_string, unescaped_string,
            "Escaped and then unescaped string {random_string:?}, but got back a different string {unescaped_string:?}. The intermediate escape looked like {escaped_string:?}."
        );
    }
}

#[test]
fn test_escape_random_script() {
    escape_test(EscapeStringStyle::default(), UnescapeStringStyle::default());
}

#[test]
fn test_escape_random_var() {
    escape_test(EscapeStringStyle::Var, UnescapeStringStyle::Var);
}

#[test]
fn test_escape_random_url() {
    escape_test(EscapeStringStyle::Url, UnescapeStringStyle::Url);
}

#[test]
fn test_escape_no_printables() {
    // Verify that ESCAPE_NO_PRINTABLES also escapes backslashes so we don't regress on issue #3892.
    let random_string = L!("line 1\\n\nline 2").to_owned();
    let escaped_string = escape_string(
        &random_string,
        EscapeStringStyle::Script(EscapeFlags::NO_PRINTABLES | EscapeFlags::NO_QUOTED),
    );
    let Some(unescaped_string) = unescape_string(&escaped_string, UnescapeStringStyle::default())
    else {
        panic!("Failed to unescape string <{escaped_string}>");
    };

    assert_eq!(
        random_string, unescaped_string,
        "Escaped and then unescaped string '{random_string}', but got back a different string '{unescaped_string}'"
    );
}

/// The number of tests to run.
const ESCAPE_TEST_COUNT: usize = 20_000;
/// The average length of strings to unescape.
const ESCAPE_TEST_LENGTH: usize = 100;
/// The highest character number of character to try and escape.
pub const ESCAPE_TEST_CHAR: usize = 4000;

/// Helper to convert a narrow string to a sequence of hex digits.
fn str2hex(input: &[u8]) -> String {
    let mut output = "".to_string();
    for byte in input {
        output += &format!("0x{:2X} ", *byte);
    }
    output
}

/// Test wide/narrow conversion by creating random strings and verifying that the original
/// string comes back through double conversion.
#[test]
fn test_convert() {
    let _locale_guard = setlocale();
    let seed = get_rng_seed();
    let mut rng = get_seeded_rng(seed);
    let mut origin = Vec::new();

    for _ in 0..ESCAPE_TEST_COUNT {
        let length: usize = rng.gen_range(0..=(2 * ESCAPE_TEST_LENGTH));
        origin.resize(length, 0);
        rng.fill_bytes(&mut origin);

        let w = str2wcstring(&origin[..]);
        let n = wcs2string(&w);
        assert_eq!(
            origin,
            n,
            "Conversion cycle of string:\n{:4} chars: {}\n\
                produced different string:\n\
                {:4} chars: {}\n
                Use this seed to reproduce: {}",
            origin.len(),
            &str2hex(&origin),
            n.len(),
            &str2hex(&n),
            seed,
        );
    }
}

/// Verify that ASCII narrow->wide conversions are correct.
#[test]
fn test_convert_ascii() {
    let mut s = vec![b'\0'; 4096];
    for (i, c) in s.iter_mut().enumerate() {
        *c = u8::try_from(i % 10).unwrap() + b'0';
    }

    // Test a variety of alignments.
    for left in 0..16 {
        for right in 0..16 {
            let len = s.len() - left - right;
            let input = &s[left..left + len];
            let wide = str2wcstring(input);
            let narrow = wcs2string(&wide);
            assert_eq!(narrow, input);
        }
    }

    // Put some non-ASCII bytes in and ensure it all still works.
    for i in 0..s.len() {
        let saved = s[i];
        s[i] = 0xF7;
        assert_eq!(wcs2string(&str2wcstring(&s)), s);
        s[i] = saved;
    }
}

/// fish uses the private-use range to encode bytes that could not be decoded using the
/// user's locale. If the input could be decoded, but decoded to private-use codepoints,
/// then fish should also use the direct encoding for those bytes. Verify that characters
/// in the private use area are correctly round-tripped. See #7723.
#[test]
fn test_convert_private_use() {
    for c in ENCODE_DIRECT_BASE..ENCODE_DIRECT_END {
        // Encode the char via the locale. Do not use fish functions which interpret these
        // specially.
        let mut converted = [0_u8; AT_LEAST_MB_LEN_MAX];
        let mut state = zero_mbstate();
        let len = unsafe {
            wcrtomb(
                std::ptr::addr_of_mut!(converted[0]).cast(),
                c as u32,
                &mut state,
            )
        };
        if len == 0_usize.wrapping_sub(1) {
            // Could not be encoded in this locale.
            continue;
        }
        let s = &converted[..len];

        // Ask fish to decode this via str2wcstring.
        // str2wcstring should notice that the decoded form collides with its private use
        // and encode it directly.
        let ws = str2wcstring(s);

        // Each byte should be encoded directly, and round tripping should work.
        assert_eq!(ws.len(), s.len());
        assert_eq!(wcs2string(&ws), s);
    }
}
