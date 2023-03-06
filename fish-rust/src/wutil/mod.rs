pub mod errors;
pub mod format;
pub mod gettext;
mod normalize_path;
pub mod wcstod;
pub mod wcstoi;
mod wrealpath;

use std::io::Write;

use crate::wchar::{wstr, WString};
pub(crate) use format::printf::sprintf;
pub(crate) use gettext::{wgettext, wgettext_fmt};
pub use normalize_path::*;
pub use wcstoi::*;
pub use wrealpath::*;

/// Port of the wide-string wperror from `src/wutil.cpp` but for rust `&str`.
pub fn perror(s: &str) {
    let e = errno::errno().0;
    let mut stderr = std::io::stderr().lock();
    if !s.is_empty() {
        let _ = write!(stderr, "{s}: ");
    }
    let slice = unsafe {
        let msg = libc::strerror(e) as *const u8;
        let len = libc::strlen(msg as *const _);
        std::slice::from_raw_parts(msg, len)
    };
    let _ = stderr.write_all(slice);
    let _ = stderr.write_all(b"\n");
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
