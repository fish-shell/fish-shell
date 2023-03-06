use std::iter::repeat;

use crate::wchar::{wstr, WString, L};

pub fn normalize_path(path: &wstr, allow_leading_double_slashes: bool) -> WString {
    // Count the leading slashes.
    let sep = '/';
    let mut leading_slashes: usize = 0;
    for (i, &c) in path.as_char_slice().iter().enumerate() {
        if c != sep {
            leading_slashes = i;
            break;
        }
    }

    let comps = path
        .as_char_slice()
        .split(|&c| c == sep)
        .map(wstr::from_char_slice)
        .collect::<Vec<_>>();
    let mut new_comps = Vec::new();
    for comp in comps {
        if comp.is_empty() || comp == L!(".") {
            continue;
        } else if comp != L!("..") {
            new_comps.push(comp);
        } else if !new_comps.is_empty() && new_comps.last().map_or(L!(""), |&s| s) != L!("..") {
            // '..' with a real path component, drop that path component.
            new_comps.pop();
        } else if leading_slashes == 0 {
            // We underflowed the .. and are a relative (not absolute) path.
            new_comps.push(L!(".."));
        }
    }
    let mut result = new_comps.into_iter().fold(Vec::new(), |mut acc, x| {
        acc.extend_from_slice(x.as_char_slice());
        acc.push('/');
        acc
    });
    result.pop();
    // If we don't allow leading double slashes, collapse them to 1 if there are any.
    let mut numslashes = if leading_slashes > 0 { 1 } else { 0 };
    // If we do, prepend one or two leading slashes.
    // Yes, three+ slashes are collapsed to one. (!)
    if allow_leading_double_slashes && leading_slashes == 2 {
        numslashes = 2;
    }
    result.splice(0..0, repeat(sep).take(numslashes));
    // Ensure ./ normalizes to . and not empty.
    if result.is_empty() {
        result.push('.');
    }
    WString::from_chars(result)
}
