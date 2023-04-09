//! Helper functions for working with wcstring.

use crate::compat::MB_CUR_MAX;
use crate::expand::INTERNAL_SEPARATOR;
use crate::flog::FLOGF;
use crate::wchar::{decode_byte_from_char, wstr, WString, L};
use crate::wutil::encoding::{wcrtomb, zero_mbstate, AT_LEAST_MB_LEN_MAX};

/// Implementation of wcs2string that accepts a callback.
/// This invokes \p func with (const char*, size_t) pairs.
/// If \p func returns false, it stops; otherwise it continues.
/// \return false if the callback returned false, otherwise true.
pub fn wcs2string_callback(input: &wstr, mut func: impl FnMut(&[u8]) -> bool) -> bool {
    let mut state = zero_mbstate();
    let mut converted = [0_u8; AT_LEAST_MB_LEN_MAX];

    for c in input.chars() {
        // TODO: this doesn't seem sound.
        if c == INTERNAL_SEPARATOR {
            // do nothing
        } else if let Some(byte) = decode_byte_from_char(c) {
            converted[0] = byte;
            if !func(&converted[..1]) {
                return false;
            }
        } else if MB_CUR_MAX() == 1 {
            // single-byte locale (C/POSIX/ISO-8859)
            // If `c` contains a wide character we emit a question-mark.
            converted[0] = u8::try_from(u32::from(c)).unwrap_or(b'?');
            if !func(&converted[..1]) {
                return false;
            }
        } else {
            converted = [0; AT_LEAST_MB_LEN_MAX];
            let len = unsafe {
                wcrtomb(
                    std::ptr::addr_of_mut!(converted[0]).cast(),
                    c as libc::wchar_t,
                    &mut state,
                )
            };
            if len == 0_usize.wrapping_sub(1) {
                wcs2string_bad_char(c);
                state = zero_mbstate();
            } else if !func(&converted[..len]) {
                return false;
            }
        }
    }
    true
}

fn wcs2string_bad_char(c: char) {
    FLOGF!(
        char_encoding,
        L!("Wide character U+%4X has no narrow representation"),
        c
    );
}

/// Joins strings with a separator.
pub fn join_strings(strs: &[&wstr], sep: char) -> WString {
    if strs.is_empty() {
        return WString::new();
    }
    let capacity = strs.iter().fold(0, |acc, s| acc + s.len()) + strs.len() - 1;
    let mut result = WString::with_capacity(capacity);
    for (i, s) in strs.iter().enumerate() {
        if i > 0 {
            result.push(sep);
        }
        result.push_utfstr(s);
    }
    result
}

#[test]
fn test_join_strings() {
    use crate::wchar::L;
    assert_eq!(join_strings(&[], '/'), "");
    assert_eq!(join_strings(&[L!("foo")], '/'), "foo");
    assert_eq!(
        join_strings(&[L!("foo"), L!("bar"), L!("baz")], '/'),
        "foo/bar/baz"
    );
}
