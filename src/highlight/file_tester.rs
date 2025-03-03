// Support for testing whether files exist and have the correct permissions,
// to support highlighting.
// Because this may perform blocking I/O, we compute results in a separate thread,
// and provide them optimistically.
use crate::common::{unescape_string, UnescapeFlags, UnescapeStringStyle};
use crate::expand::{
    expand_one, BRACE_BEGIN, BRACE_END, BRACE_SEP, INTERNAL_SEPARATOR, PROCESS_EXPAND_SELF,
    VARIABLE_EXPAND, VARIABLE_EXPAND_SINGLE,
};
use crate::expand::{expand_tilde, ExpandFlags, HOME_DIRECTORY};
use crate::libc::_PC_CASE_SENSITIVE;
use crate::operation_context::OperationContext;
use crate::path::path_apply_working_directory;
use crate::redirection::RedirectionMode;
use crate::threads::assert_is_background_thread;
use crate::wchar::{wstr, WString, L};
use crate::wchar_ext::WExt;
use crate::wcstringutil::{
    string_prefixes_string, string_prefixes_string_case_insensitive, string_suffixes_string,
};
use crate::wildcard::{ANY_CHAR, ANY_STRING, ANY_STRING_RECURSIVE};
use crate::wutil::{
    dir_iter::DirIter, fish_wcstoi, normalize_path, waccess, wbasename, wdirname, wstat,
};
use libc::PATH_MAX;
use std::collections::hash_map::Entry;
use std::collections::{HashMap, HashSet};
use std::os::fd::RawFd;

// This is used only internally to this file, and is exposed only for testing.
#[derive(Clone, Copy, Default)]
pub struct PathFlags {
    // The path must be to a directory.
    pub require_dir: bool,
    // Expand any leading tilde in the path.
    pub expand_tilde: bool,
    // Normalize directories before resolving, as "cd".
    pub for_cd: bool,
}

// When a file test is OK, we may also return whether this was a file.
// This is used for underlining and is dependent on the particular file test.
#[derive(Debug, Copy, Clone, PartialEq, Eq)]
pub struct IsFile(pub bool);

#[derive(Debug, Copy, Clone, PartialEq, Eq)]
pub struct IsErr;

/// The result of a file test.
pub type FileTestResult = Result<IsFile, IsErr>;

pub struct FileTester<'s> {
    // The working directory, for resolving paths against.
    working_directory: WString,
    // The operation context.
    ctx: &'s OperationContext<'s>,
}

impl<'s> FileTester<'s> {
    pub fn new(working_directory: WString, ctx: &'s OperationContext<'s>) -> Self {
        Self {
            working_directory,
            ctx,
        }
    }

    /// Test whether a file exists and is readable.
    /// The input string 'token' is given as an escaped string (as the user may type in).
    /// If 'prefix' is true, instead check if the path is a prefix of a valid path.
    /// Returns false on cancellation.
    pub fn test_path(&self, token: &wstr, prefix: bool) -> bool {
        // Skip strings exceeding PATH_MAX. See #7837.
        // Note some paths may exceed PATH_MAX, but this is just for highlighting.
        if token.len() > (PATH_MAX as usize) {
            return false;
        }

        // Unescape the token.
        let Some(mut token) =
            unescape_string(token, UnescapeStringStyle::Script(UnescapeFlags::SPECIAL))
        else {
            return false;
        };

        // Big hack: is_potential_path expects a tilde, but unescape_string gives us HOME_DIRECTORY.
        // Put it back.
        if token.char_at(0) == HOME_DIRECTORY {
            token.as_char_slice_mut()[0] = '~';
        }

        is_potential_path(
            &token,
            prefix,
            &[self.working_directory.to_owned()],
            self.ctx,
            PathFlags {
                expand_tilde: true,
                ..Default::default()
            },
        )
    }

    // Test if the string is a prefix of a valid path we could cd into, or is some other token
    // we recognize (primarily --help).
    // If is_prefix is true, we test if the string is a prefix of a valid path we could cd into.
    pub fn test_cd_path(&self, token: &wstr, is_prefix: bool) -> FileTestResult {
        let mut param = token.to_owned();
        if !expand_one(&mut param, ExpandFlags::FAIL_ON_CMDSUBST, self.ctx, None) {
            // Failed expansion (e.g. may contain a command substitution). Ignore it.
            return FileTestResult::Ok(IsFile(false));
        }
        // Maybe it's just --help.
        if string_prefixes_string(&param, L!("--help")) || string_prefixes_string(&param, L!("-h"))
        {
            return FileTestResult::Ok(IsFile(false));
        }
        let valid_path = is_potential_cd_path(
            &param,
            is_prefix,
            &self.working_directory,
            self.ctx,
            PathFlags {
                expand_tilde: true,
                ..Default::default()
            },
        );
        // cd into an invalid path is an error.
        if valid_path {
            Ok(IsFile(valid_path))
        } else {
            Err(IsErr)
        }
    }

    // Test if a the given string is a valid redirection target, given the mode.
    // Note we return bool, because we never underline redirection targets.
    pub fn test_redirection_target(&self, target: &wstr, mode: RedirectionMode) -> bool {
        // Skip targets exceeding PATH_MAX. See #7837.
        if target.len() > (PATH_MAX as usize) {
            return false;
        }
        let mut target = target.to_owned();
        if !expand_one(&mut target, ExpandFlags::FAIL_ON_CMDSUBST, self.ctx, None) {
            // Could not be expanded.
            return false;
        }
        // Ok, we successfully expanded our target. Now verify that it works with this
        // redirection. We will probably need it as a path (but not in the case of fd
        // redirections). Note that the target is now unescaped.
        let target_path = path_apply_working_directory(&target, &self.working_directory);
        match mode {
            RedirectionMode::fd => {
                if target == "-" {
                    return true;
                }
                match fish_wcstoi(&target) {
                    Ok(fd) => fd >= 0,
                    Err(_) => false,
                }
            }
            RedirectionMode::input | RedirectionMode::try_input => {
                // Input redirections must have a readable non-directory.
                // Note we color "try_input" files as errors if they are invalid,
                // even though it's possible to execute these (replaced via /dev/null).
                waccess(&target_path, libc::R_OK) == 0
                    && wstat(&target_path).is_ok_and(|md| !md.file_type().is_dir())
            }
            RedirectionMode::overwrite | RedirectionMode::append | RedirectionMode::noclob => {
                if string_suffixes_string(L!("/"), &target) {
                    // Redirections to things that are directories is definitely not
                    // allowed.
                    return false;
                }
                // Test whether the file exists, and whether it's writable (possibly after
                // creating it). access() returns failure if the file does not exist.
                // TODO: we do not need to compute file_exists for an 'overwrite' redirection.
                let file_exists;
                let file_is_writable;
                match wstat(&target_path) {
                    Ok(md) => {
                        // No err. We can write to it if it's not a directory and we have
                        // permission.
                        file_exists = true;
                        file_is_writable =
                            !md.file_type().is_dir() && waccess(&target_path, libc::W_OK) == 0;
                    }
                    Err(err) => {
                        if err.raw_os_error() == Some(libc::ENOENT) {
                            // File does not exist. Check if its parent directory is writable.
                            let mut parent = wdirname(&target_path).to_owned();

                            // Ensure that the parent ends with the path separator. This will ensure
                            // that we get an error if the parent directory is not really a
                            // directory.
                            if !string_suffixes_string(L!("/"), &parent) {
                                parent.push('/');
                            }

                            // Now the file is considered writable if the parent directory is
                            // writable.
                            file_exists = false;
                            file_is_writable = waccess(&parent, libc::W_OK) == 0;
                        } else {
                            // Other errors we treat as not writable. This includes things like
                            // ENOTDIR.
                            file_exists = false;
                            file_is_writable = false;
                        }
                    }
                }
                // NOCLOB means that we must not overwrite files that exist.
                file_is_writable && !(file_exists && mode == RedirectionMode::noclob)
            }
        }
    }
}

/// Tests whether the specified string cpath is the prefix of anything we could cd to. directories
/// is a list of possible parent directories (typically either the working directory, or the
/// cdpath). This does I/O!
///
/// We expect the path to already be unescaped.
pub fn is_potential_path(
    potential_path_fragment: &wstr,
    at_cursor: bool,
    directories: &[WString],
    ctx: &OperationContext<'_>,
    flags: PathFlags,
) -> bool {
    // This function is expected to be called from the background thread.
    // But in tests, threads get weird.
    if cfg!(not(test)) {
        assert_is_background_thread();
    }

    if ctx.check_cancel() {
        return false;
    }

    let require_dir = flags.require_dir;
    let mut clean_potential_path_fragment = WString::new();
    let mut has_magic = false;

    let mut path_with_magic = potential_path_fragment.to_owned();
    if flags.expand_tilde {
        expand_tilde(&mut path_with_magic, ctx.vars());
    }

    for c in path_with_magic.chars() {
        match c {
            PROCESS_EXPAND_SELF
            | VARIABLE_EXPAND
            | VARIABLE_EXPAND_SINGLE
            | BRACE_BEGIN
            | BRACE_END
            | BRACE_SEP
            | ANY_CHAR
            | ANY_STRING
            | ANY_STRING_RECURSIVE => {
                has_magic = true;
            }
            INTERNAL_SEPARATOR => (),
            _ => clean_potential_path_fragment.push(c),
        }
    }

    if has_magic || clean_potential_path_fragment.is_empty() {
        return false;
    }

    // Don't test the same path multiple times, which can happen if the path is absolute and the
    // CDPATH contains multiple entries.
    let mut checked_paths = HashSet::new();

    // Keep a cache of which paths / filesystems are case sensitive.
    let mut case_sensitivity_cache = CaseSensitivityCache::new();

    for wd in directories {
        if ctx.check_cancel() {
            return false;
        }
        let mut abs_path = path_apply_working_directory(&clean_potential_path_fragment, wd);
        let must_be_full_dir = abs_path.chars().next_back() == Some('/');
        if flags.for_cd {
            abs_path = normalize_path(&abs_path, /*allow_leading_double_slashes=*/ true);
        }

        // Skip this if it's empty or we've already checked it.
        if abs_path.is_empty() || checked_paths.contains(&abs_path) {
            continue;
        }
        checked_paths.insert(abs_path.clone());

        // If the user is still typing the argument, we want to highlight it if it's the prefix
        // of a valid path. This means we need to potentially walk all files in some directory.
        // There are two easy cases where we can skip this:
        // 1. If the argument ends with a slash, it must be a valid directory, no prefix.
        // 2. If the cursor is not at the argument, it means the user is definitely not typing it,
        //    so we can skip the prefix-match.
        if must_be_full_dir || !at_cursor {
            if let Ok(md) = wstat(&abs_path) {
                if !at_cursor || md.file_type().is_dir() {
                    return true;
                }
            }
        } else {
            // We do not end with a slash; it does not have to be a directory.
            let dir_name = wdirname(&abs_path);
            let filename_fragment = wbasename(&abs_path);
            if dir_name == "/" && filename_fragment == "/" {
                // cd ///.... No autosuggestion.
                return true;
            }

            if let Ok(mut dir) = DirIter::new(dir_name) {
                // Check if we're case insensitive.
                let do_case_insensitive =
                    fs_is_case_insensitive(dir_name, dir.fd(), &mut case_sensitivity_cache);

                // We opened the dir_name; look for a string where the base name prefixes it.
                while let Some(entry) = dir.next() {
                    let Ok(entry) = entry else { continue };
                    if ctx.check_cancel() {
                        return false;
                    }

                    // Maybe skip directories.
                    if require_dir && !entry.is_dir() {
                        continue;
                    }

                    if string_prefixes_string(filename_fragment, &entry.name)
                        || (do_case_insensitive
                            && string_prefixes_string_case_insensitive(
                                filename_fragment,
                                &entry.name,
                            ))
                    {
                        return true;
                    }
                }
            }
        }
    }
    false
}

// Given a string, return whether it prefixes a path that we could cd into. Return that path in
// out_path. Expects path to be unescaped.
pub fn is_potential_cd_path(
    path: &wstr,
    at_cursor: bool,
    working_directory: &wstr,
    ctx: &OperationContext<'_>,
    mut flags: PathFlags,
) -> bool {
    let mut directories = vec![];

    if string_prefixes_string(L!("./"), path) {
        // Ignore the CDPATH in this case; just use the working directory.
        directories.push(working_directory.to_owned());
    } else {
        // Get the CDPATH.
        let cdpath = ctx.vars().get_unless_empty(L!("CDPATH"));
        let mut pathsv = match cdpath {
            None => vec![L!(".").to_owned()],
            Some(cdpath) => cdpath.as_list().to_vec(),
        };
        // The current $PWD is always valid.
        pathsv.push(L!(".").to_owned());

        for mut next_path in pathsv {
            if next_path.is_empty() {
                next_path = L!(".").to_owned();
            }
            // Ensure that we use the working directory for relative cdpaths like ".".
            directories.push(path_apply_working_directory(&next_path, working_directory));
        }
    }

    // Call is_potential_path with all of these directories.
    flags.require_dir = true;
    flags.for_cd = true;
    is_potential_path(path, at_cursor, &directories, ctx, flags)
}

/// Determine if the filesystem containing the given fd is case insensitive for lookups regardless
/// of whether it preserves the case when saving a pathname.
///
/// Returns:
///     false: the filesystem is not case insensitive
///     true: the file system is case insensitive
pub type CaseSensitivityCache = HashMap<WString, bool>;
fn fs_is_case_insensitive(
    path: &wstr,
    fd: RawFd,
    case_sensitivity_cache: &mut CaseSensitivityCache,
) -> bool {
    let mut result = false;
    if *_PC_CASE_SENSITIVE != 0 {
        // Try the cache first.
        match case_sensitivity_cache.entry(path.to_owned()) {
            Entry::Occupied(e) => {
                /* Use the cached value */
                result = *e.get();
            }
            Entry::Vacant(e) => {
                // Ask the system. A -1 value means error (so assume case sensitive), a 1 value means case
                // sensitive, and a 0 value means case insensitive.
                let ret = unsafe { libc::fpathconf(fd, *_PC_CASE_SENSITIVE) };
                result = ret == 0;
                e.insert(result);
            }
        }
    }
    result
}
