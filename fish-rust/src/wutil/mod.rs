pub mod encoding;
pub mod errors;
pub mod gettext;
pub mod printf;
pub mod wcstod;
pub mod wcstoi;

use crate::common::fish_reserved_codepoint;
use crate::common::{str2wcstring, wcs2zstring};
use crate::wchar::{wstr, WString, L};
use crate::wcstringutil::join_strings;
pub(crate) use gettext::{wgettext, wgettext_fmt};
pub(crate) use printf::sprintf;
use std::ffi::OsStr;
use std::fs::canonicalize;
use std::os::unix::prelude::{OsStrExt, OsStringExt};
pub use wcstoi::*;

/// Port of the wide-string wperror from `src/wutil.cpp` but for rust `&str`.
use std::io::Write;
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

/// Wide character realpath. The last path component does not need to be valid. If an error occurs,
/// `wrealpath()` returns `None`
pub fn wrealpath(pathname: &wstr) -> Option<WString> {
    if pathname.is_empty() {
        return None;
    }

    let mut narrow_path: Vec<u8> = wcs2zstring(pathname).into();

    // Strip trailing slashes. This is treats "/a//" as equivalent to "/a" if /a is a non-directory.
    while narrow_path.len() > 1 && narrow_path[narrow_path.len() - 1] == b'/' {
        narrow_path.pop();
    }

    let narrow_res = canonicalize(OsStr::from_bytes(&narrow_path));

    let real_path = if let Ok(result) = narrow_res {
        result.into_os_string().into_vec()
    } else {
        // Check if everything up to the last path component is valid.
        let pathsep_idx = narrow_path.iter().rposition(|&c| c == b'/');

        if pathsep_idx == Some(0) {
            // If the only pathsep is the first character then it's an absolute path with a
            // single path component and thus doesn't need conversion.
            narrow_path
        } else {
            // Only call realpath() on the portion up to the last component.
            let narrow_res = if let Some(pathsep_idx) = pathsep_idx {
                // Only call realpath() on the portion up to the last component.
                canonicalize(OsStr::from_bytes(&narrow_path[0..pathsep_idx]))
            } else {
                // If there is no "/", this is a file in $PWD, so give the realpath to that.
                canonicalize(".")
            };

            let Ok(narrow_result) = narrow_res else { return None; };

            let pathsep_idx = pathsep_idx.map_or(0, |idx| idx + 1);

            let mut real_path = narrow_result.into_os_string().into_vec();

            // This test is to deal with cases such as /../../x => //x.
            if real_path.len() > 1 {
                real_path.push(b'/');
            }

            real_path.extend_from_slice(&narrow_path[pathsep_idx..]);

            real_path
        }
    };

    Some(str2wcstring(&real_path))
}

/// Given an input path, "normalize" it:
/// 1. Collapse multiple /s into a single /, except maybe at the beginning.
/// 2. .. goes up a level.
/// 3. Remove /./ in the middle.
pub fn normalize_path(path: &wstr, allow_leading_double_slashes: bool) -> WString {
    // Count the leading slashes.
    let sep = '/';
    let mut leading_slashes: usize = 0;
    for c in path.chars() {
        if c != sep {
            break;
        }
        leading_slashes += 1;
    }

    let comps = path
        .as_char_slice()
        .split(|&c| c == sep)
        .map(wstr::from_char_slice)
        .collect::<Vec<_>>();
    let mut new_comps = Vec::new();
    for comp in comps {
        if comp.is_empty() || comp == "." {
            continue;
        } else if comp != ".." {
            new_comps.push(comp);
        } else if !new_comps.is_empty() && new_comps.last().unwrap() != ".." {
            // '..' with a real path component, drop that path component.
            new_comps.pop();
        } else if leading_slashes == 0 {
            // We underflowed the .. and are a relative (not absolute) path.
            new_comps.push(L!(".."));
        }
    }
    let mut result = join_strings(&new_comps, sep);
    // If we don't allow leading double slashes, collapse them to 1 if there are any.
    let mut numslashes = if leading_slashes > 0 { 1 } else { 0 };
    // If we do, prepend one or two leading slashes.
    // Yes, three+ slashes are collapsed to one. (!)
    if allow_leading_double_slashes && leading_slashes == 2 {
        numslashes = 2;
    }
    for _ in 0..numslashes {
        result.insert(0, sep);
    }
    // Ensure ./ normalizes to . and not empty.
    if result.is_empty() {
        result.push('.');
    }
    result
}

const PUA1_START: char = '\u{E000}';
const PUA1_END: char = '\u{F900}';
const PUA2_START: char = '\u{F0000}';
const PUA2_END: char = '\u{FFFFE}';
const PUA3_START: char = '\u{100000}';
const PUA3_END: char = '\u{10FFFE}';

/// Return one if the code point is in a Unicode private use area.
fn fish_is_pua(c: char) -> bool {
    PUA1_START <= c && c < PUA1_END
}

/// We need this because there are too many implementations that don't return the proper answer for
/// some code points. See issue #3050.
pub fn fish_iswalnum(c: char) -> bool {
    !fish_reserved_codepoint(c) && !fish_is_pua(c) && c.is_alphanumeric()
}

/// Given that \p cursor is a pointer into \p base, return the offset in characters.
/// This emulates C pointer arithmetic:
///    `wstr_offset_in(cursor, base)` is equivalent to C++ `cursor - base`.
pub fn wstr_offset_in(cursor: &wstr, base: &wstr) -> usize {
    let cursor = cursor.as_slice();
    let base = base.as_slice();
    // cursor may be a zero-length slice at the end of base,
    // which base.as_ptr_range().contains(cursor.as_ptr()) will reject.
    let base_range = base.as_ptr_range();
    let curs_range = cursor.as_ptr_range();
    assert!(
        base_range.start <= curs_range.start && curs_range.end <= base_range.end,
        "cursor should be a subslice of base"
    );
    let offset = unsafe { cursor.as_ptr().offset_from(base.as_ptr()) };
    assert!(offset >= 0, "offset should be non-negative");
    offset as usize
}

#[test]
fn test_normalize_path() {
    fn norm_path(path: &wstr) -> WString {
        normalize_path(path, true)
    }
    assert_eq!(norm_path(L!("")), ".");
    assert_eq!(norm_path(L!("..")), "..");
    assert_eq!(norm_path(L!("./")), ".");
    assert_eq!(norm_path(L!("./.")), ".");
    assert_eq!(norm_path(L!("/")), "/");
    assert_eq!(norm_path(L!("//")), "//");
    assert_eq!(norm_path(L!("///")), "/");
    assert_eq!(norm_path(L!("////")), "/");
    assert_eq!(norm_path(L!("/.///")), "/");
    assert_eq!(norm_path(L!(".//")), ".");
    assert_eq!(norm_path(L!("/.//../")), "/");
    assert_eq!(norm_path(L!("////abc")), "/abc");
    assert_eq!(norm_path(L!("/abc")), "/abc");
    assert_eq!(norm_path(L!("/abc/")), "/abc");
    assert_eq!(norm_path(L!("/abc/..def/")), "/abc/..def");
    assert_eq!(norm_path(L!("//abc/../def/")), "//def");
    assert_eq!(norm_path(L!("abc/../abc/../abc/../abc")), "abc");
    assert_eq!(norm_path(L!("../../")), "../..");
    assert_eq!(norm_path(L!("foo/./bar")), "foo/bar");
    assert_eq!(norm_path(L!("foo/../")), ".");
    assert_eq!(norm_path(L!("foo/../foo")), "foo");
    assert_eq!(norm_path(L!("foo/../foo/")), "foo");
    assert_eq!(norm_path(L!("foo/././bar/.././baz")), "foo/baz");
}

#[test]
fn test_wstr_offset_in() {
    use crate::wchar::L;
    let base = L!("hello world");
    assert_eq!(wstr_offset_in(&base[6..], base), 6);
    assert_eq!(wstr_offset_in(&base[0..], base), 0);
    assert_eq!(wstr_offset_in(&base[6..], &base[6..]), 0);
    assert_eq!(wstr_offset_in(&base[base.len()..], base), base.len());
}
