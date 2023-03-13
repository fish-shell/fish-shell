use crate::wchar::{wstr, WString, L};
use crate::wutil::join_strings;

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
