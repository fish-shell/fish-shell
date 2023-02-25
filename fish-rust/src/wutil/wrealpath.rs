use std::{ffi::OsStr, fs::canonicalize, os::unix::prelude::OsStrExt};

use cxx::{CxxString, UniquePtr};

use crate::{
    ffi::{make_string, str2wcstring, wcs2string},
    wchar::{wstr, WString},
    wchar_ffi::{WCharFromFFI, WCharToFFI},
};

pub fn wrealpath(pathname: &wstr) -> Option<WString> {
    if pathname.is_empty() {
        return None;
    }

    let mut real_path: UniquePtr<CxxString> = make_string("");
    let narrow_path_ptr: UniquePtr<CxxString> = wcs2string(&pathname.to_ffi());
    // We can't `pop` from a `UniquePtr<CxxString>` so we copy it into a vector
    let mut narrow_path = narrow_path_ptr.as_bytes().to_vec();

    // Strip trailing slashes. This is treats "/a//" as equivalent to "/a" if /a is a non-directory.
    while narrow_path.len() > 1 && narrow_path[narrow_path.len() - 1] as char == '/' {
        narrow_path.pop();
    }

    // `from_bytes` is Unix specific but there isn't really any other way to do this
    // since `libc::realpath` is also Unix specific. I also don't think we support Windows
    // outside of WSL + Cygwin (which should be fairly Unix-like anyways)
    let narrow_res = canonicalize(OsStr::from_bytes(&narrow_path));

    if let Ok(result) = narrow_res {
        real_path
            .pin_mut()
            .push_bytes(result.as_os_str().as_bytes());
    } else {
        // Check if everything up to the last path component is valid.
        let pathsep_idx = narrow_path.iter().rposition(|&c| c as char == '/');

        if let Some(0) = pathsep_idx {
            // If the only pathsep is the first character then it's an absolute path with a
            // single path component and thus doesn't need conversion.
            real_path.pin_mut().clear();
            real_path.pin_mut().push_bytes(&narrow_path);
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

            real_path
                .pin_mut()
                .push_bytes(narrow_result.as_os_str().as_bytes());

            // This test is to deal with cases such as /../../x => //x.
            if real_path.len() > 1 {
                real_path.pin_mut().push_str("/");
            }

            real_path.pin_mut().push_bytes(&narrow_path[pathsep_idx..]);
        }
    }

    Some(str2wcstring(real_path.as_ref()?).from_ffi())
}
