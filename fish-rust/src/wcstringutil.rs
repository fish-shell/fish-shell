//! Helper functions for working with wcstring.

use crate::wchar::{wstr, WString};

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
