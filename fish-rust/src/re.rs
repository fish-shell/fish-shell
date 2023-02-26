use crate::wchar::{wstr, WString, L};

/// Adjust a pattern so that it is anchored at both beginning and end.
/// This is a workaround for the fact that PCRE2_ENDANCHORED is unavailable on pre-2017 PCRE2
/// (e.g. 10.21, on Xenial).
pub fn regex_make_anchored(pattern: &wstr) -> WString {
    let mut anchored = pattern.to_owned();
    // PATTERN -> ^(:?PATTERN)$.
    let prefix = L!("^(?:");
    let suffix = L!(")$");
    anchored.reserve(pattern.len() + prefix.len() + suffix.len());
    anchored.insert_utfstr(0, prefix);
    anchored.push_utfstr(suffix);
    anchored
}

use crate::ffi_tests::add_test;
add_test!("test_regex_make_anchored", || {
    use crate::ffi;
    use crate::wchar::L;
    use crate::wchar_ffi::WCharToFFI;

    let flags = ffi::re::flags_t { icase: false };
    let mut result = ffi::try_compile(&regex_make_anchored(L!("ab(.+?)")), &flags);
    assert!(!result.has_error());

    let re = result.as_mut().get_regex();

    assert!(!re.is_null());
    assert!(!re.matches_ffi(&L!("").to_ffi()));
    assert!(!re.matches_ffi(&L!("ab").to_ffi()));
    assert!(re.matches_ffi(&L!("abcd").to_ffi()));
    assert!(!re.matches_ffi(&L!("xabcd").to_ffi()));
    assert!(re.matches_ffi(&L!("abcdefghij").to_ffi()));

    let mut result = ffi::try_compile(&regex_make_anchored(L!("(a+)|(b+)")), &flags);
    assert!(!result.has_error());

    let re = result.as_mut().get_regex();
    assert!(!re.is_null());
    assert!(!re.matches_ffi(&L!("").to_ffi()));
    assert!(!re.matches_ffi(&L!("aabb").to_ffi()));
    assert!(re.matches_ffi(&L!("aaaa").to_ffi()));
    assert!(re.matches_ffi(&L!("bbbb").to_ffi()));
    assert!(!re.matches_ffi(&L!("aaaax").to_ffi()));
});
