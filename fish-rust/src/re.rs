use crate::wchar::prelude::*;

/// Adjust a pattern so that it is anchored at both beginning and end.
/// This is a workaround for the fact that PCRE2_ENDANCHORED is unavailable on pre-2017 PCRE2
/// (e.g. 10.21, on Xenial).
pub fn regex_make_anchored(pattern: &wstr) -> WString {
    // PATTERN -> ^(:?PATTERN)$.
    let prefix = L!("^(?:");
    let suffix = L!(")$");
    let mut anchored = WString::with_capacity(prefix.len() + pattern.len() + suffix.len());
    anchored.push_utfstr(prefix);
    anchored.push_utfstr(pattern);
    anchored.push_utfstr(suffix);
    anchored
}

/// Copy a wstr to a Box<[char]>.
pub fn to_boxed_chars(s: &wstr) -> Box<[char]> {
    let chars = s.as_char_slice();
    chars.into()
}

#[test]
fn test_regex_make_anchored() {
    use pcre2::utf32::{Regex, RegexBuilder};

    fn test_match(re: &Regex, subject: &wstr) -> bool {
        re.is_match(&to_boxed_chars(subject)).unwrap()
    }

    let builder = RegexBuilder::new();
    let result = builder.build(to_boxed_chars(&regex_make_anchored(L!("ab(.+?)"))));
    assert!(result.is_ok());
    let re = &result.unwrap();

    assert!(!test_match(re, L!("")));
    assert!(!test_match(re, L!("ab")));
    assert!(test_match(re, L!("abcd")));
    assert!(!test_match(re, L!("xabcd")));
    assert!(test_match(re, L!("abcdefghij")));

    let result = builder.build(to_boxed_chars(&regex_make_anchored(L!("(a+)|(b+)"))));
    assert!(result.is_ok());

    let re = &result.unwrap();
    assert!(!test_match(re, L!("")));
    assert!(!test_match(re, L!("aabb")));
    assert!(test_match(re, L!("aaaa")));
    assert!(test_match(re, L!("bbbb")));
    assert!(!test_match(re, L!("aaaax")));
}
