use std::fs::canonicalize;

use crate::wchar::{wstr, WExt, WString};

/// Wide character realpath. The last path component does not need to be valid. If an error occurs,
/// `wrealpath()` returns `None`
pub fn wrealpath(pathname: &wstr) -> Option<WString> {
    if pathname.is_empty() {
        return None;
    }

    let mut real_path = WString::new();
    let mut narrow_path = pathname.to_owned();

    // Strip trailing slashes. This is treats "/a//" as equivalent to "/a" if /a is a non-directory.
    while narrow_path.len() > 1 && narrow_path.char_at(narrow_path.len() - 1) == '/' {
        narrow_path.pop();
    }

    let mut narrow_result = canonicalize(narrow_path.to_string());

    if let Ok(result) = narrow_result {
        real_path.push_str(result.to_str()?);
    } else {
        // Check if everything up to the last path component is valid.
        let pathsep_idx = narrow_path.rfind('/');

        if let Some(0) = pathsep_idx {
            // If the only pathsep is the first character then it's an absolute path with a
            // single path component and thus doesn't need conversion.
            real_path = narrow_path;
        } else {
            // Only call realpath() on the portion up to the last component.
            if let Some(pathsep_idx) = pathsep_idx {
                // Only call realpath() on the portion up to the last component.
                narrow_result = canonicalize(narrow_path[0..pathsep_idx].to_string());
            } else {
                // If there is no "/", this is a file in $PWD, so give the realpath to that.
                narrow_result = canonicalize(".");
            }

            let Ok(narrow_result) = narrow_result else { return None; };

            let pathsep_idx = pathsep_idx.map_or(0, |idx| idx + 1);
            real_path.push_str(narrow_result.to_str()?);

            // This test is to deal with cases such as /../../x => //x.
            if real_path.len() > 1 {
                real_path.push('/');
            }

            real_path.push_utfstr(&narrow_path[pathsep_idx..]);
        }
    }

    Some(real_path)
}
