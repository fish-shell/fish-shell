use crate::{
    expand::HOME_DIRECTORY,
    wchar::{wstr, WExt, WString, L},
};

/// If the given path looks like it's relative to the working directory, then prepend that working
/// directory. This operates on unescaped paths only (so a ~ means a literal ~).
pub fn path_apply_working_directory(path: &wstr, working_directory: &wstr) -> WString {
    if path.is_empty() || working_directory.is_empty() {
        return path.to_owned();
    }

    // We're going to make sure that if we want to prepend the wd, that the string has no leading
    // "/".
    let prepend_wd = path.char_at(0) != '/' && path.char_at(0) != HOME_DIRECTORY;

    if !prepend_wd {
        // No need to prepend the wd, so just return the path we were given.
        return path.to_owned();
    }

    // Remove up to one "./".
    let mut path_component = path.to_owned();
    if path_component.starts_with("./") {
        path_component.replace_range(0..2, L!(""));
    }

    // Removing leading /s.
    while path_component.starts_with("/") {
        path_component.replace_range(0..1, L!(""));
    }

    // Construct and return a new path.
    let mut new_path = working_directory.to_owned();
    append_path_component(&mut new_path, &path_component);
    new_path
}

pub fn append_path_component(path: &mut WString, component: &wstr) {
    if path.is_empty() || component.is_empty() {
        path.push_utfstr(component);
    } else {
        let path_len = path.len();
        let path_slash = path.char_at(path_len - 1) == '/';
        let comp_slash = component.as_char_slice()[0] == '/';
        if !path_slash && !comp_slash {
            // Need a slash
            path.push('/');
        } else if path_slash && comp_slash {
            // Too many slashes.
            path.pop();
        }
        path.push_utfstr(component);
    }
}
