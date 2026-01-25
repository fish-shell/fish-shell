// Support for testing whether files exist and have the correct permissions,
// to support highlighting.
// Because this may perform blocking I/O, we compute results in a separate thread,
// and provide them optimistically.
use crate::common::{UnescapeFlags, UnescapeStringStyle, unescape_string};
use crate::expand::{
    BRACE_BEGIN, BRACE_END, BRACE_SEP, INTERNAL_SEPARATOR, PROCESS_EXPAND_SELF, VARIABLE_EXPAND,
    VARIABLE_EXPAND_SINGLE, expand_one,
};
use crate::expand::{ExpandFlags, HOME_DIRECTORY, expand_tilde};
use crate::operation_context::OperationContext;
use crate::path::path_apply_working_directory;
use crate::redirection::RedirectionMode;
use crate::threads::assert_is_background_thread;
use crate::wildcard::{ANY_CHAR, ANY_STRING, ANY_STRING_RECURSIVE};
use crate::wutil::{
    dir_iter::DirIter, fish_wcstoi, normalize_path, waccess, wbasename, wdirname, wstat,
};
use fish_wcstringutil::{
    string_prefixes_string, string_prefixes_string_case_insensitive, string_suffixes_string,
};
use fish_widestring::{L, WExt, WString, wstr};
use libc::PATH_MAX;
use nix::unistd::AccessFlags;
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
            std::slice::from_ref(&self.working_directory),
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

    // Test if a the given string is a valid redirection target, and if so, whether
    // it is a path to an existing file.
    pub fn test_redirection_target(&self, target: &wstr, mode: RedirectionMode) -> FileTestResult {
        // Skip targets exceeding PATH_MAX. See #7837.
        if target.len() > (PATH_MAX as usize) {
            return Err(IsErr);
        }
        let mut target = target.to_owned();
        if !expand_one(&mut target, ExpandFlags::FAIL_ON_CMDSUBST, self.ctx, None) {
            // Could not be expanded.
            return Err(IsErr);
        }
        // Ok, we successfully expanded our target. Now verify that it works with this
        // redirection. We will probably need it as a path (but not in the case of fd
        // redirections). Note that the target is now unescaped.
        let target_path = path_apply_working_directory(&target, &self.working_directory);
        match mode {
            RedirectionMode::Fd => {
                if target == "-" {
                    return Ok(IsFile(false));
                }
                match fish_wcstoi(&target) {
                    Ok(fd) if fd >= 0 => Ok(IsFile(false)),
                    _ => Err(IsErr),
                }
            }
            RedirectionMode::Input | RedirectionMode::TryInput => {
                // Input redirections must have a readable non-directory.
                // Note we color "try_input" files as errors if they are invalid,
                // even though it's possible to execute these (replaced via /dev/null).
                if waccess(&target_path, AccessFlags::R_OK).is_ok()
                    && wstat(&target_path).is_ok_and(|md| !md.file_type().is_dir())
                {
                    Ok(IsFile(true))
                } else {
                    Err(IsErr)
                }
            }
            RedirectionMode::Overwrite | RedirectionMode::Append | RedirectionMode::NoClob => {
                if string_suffixes_string(L!("/"), &target) {
                    // Redirections to things that are directories is definitely not
                    // allowed.
                    return Err(IsErr);
                }
                // Test whether the file exists, and whether it's writable (possibly after
                // creating it). access() returns failure if the file does not exist.
                let file_exists;
                let file_is_writable;
                match wstat(&target_path) {
                    Ok(md) => {
                        // No err. We can write to it if it's not a directory and we have
                        // permission.
                        file_exists = true;
                        file_is_writable = !md.file_type().is_dir()
                            && waccess(&target_path, AccessFlags::W_OK).is_ok();
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
                            file_is_writable = waccess(&parent, AccessFlags::W_OK).is_ok();
                        } else {
                            // Other errors we treat as not writable. This includes things like
                            // ENOTDIR.
                            file_exists = false;
                            file_is_writable = false;
                        }
                    }
                }
                // NoClob means that we must not overwrite files that exist.
                if !file_is_writable || (mode == RedirectionMode::NoClob && file_exists) {
                    return Err(IsErr);
                }
                Ok(IsFile(file_exists))
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

#[cfg(any(target_os = "macos", target_os = "ios"))]
fn fs_is_case_insensitive(
    path: &wstr,
    fd: RawFd,
    case_sensitivity_cache: &mut CaseSensitivityCache,
) -> bool {
    if let Some(cached) = case_sensitivity_cache.get(path) {
        return *cached;
    }
    // Ask the system. A -1 value means error (so assume case sensitive), a 1 value means case
    // sensitive, and a 0 value means case insensitive.
    let ret = unsafe { libc::fpathconf(fd, libc::_PC_CASE_SENSITIVE) };
    let icase = ret == 0;
    case_sensitivity_cache.insert(path.to_owned(), icase);
    icase
}

#[cfg(not(any(target_os = "macos", target_os = "ios")))]
pub fn fs_is_case_insensitive(
    _path: &wstr,
    _fd: RawFd,
    _case_sensitivity_cache: &mut CaseSensitivityCache,
) -> bool {
    // Other platforms don’t have _PC_CASE_SENSITIVE.
    false
}

#[cfg(test)]
mod tests {
    use super::{FileTester, IsErr, IsFile, PathFlags, is_potential_path};
    use crate::common::osstr2wcstring;
    use crate::env::EnvStack;
    use crate::operation_context::{EXPANSION_LIMIT_DEFAULT, OperationContext};
    use crate::prelude::*;
    use crate::tests::prelude::*;

    use crate::redirection::RedirectionMode;
    use std::fs::{self, File, Permissions, create_dir_all};
    use std::os::unix::fs::PermissionsExt;
    use std::path::PathBuf;

    struct TempDirWithCtx {
        tempdir: fish_tempfile::TempDir,
        ctx: OperationContext<'static>,
    }

    impl TempDirWithCtx {
        fn new() -> TempDirWithCtx {
            TempDirWithCtx {
                tempdir: fish_tempfile::new_dir().unwrap(),
                ctx: OperationContext::empty(),
            }
        }

        fn filepath(&self, name: &str) -> PathBuf {
            self.tempdir.path().join(name)
        }

        fn file_tester(&self) -> FileTester<'_> {
            FileTester::new(osstr2wcstring(self.tempdir.path()), &self.ctx)
        }
    }

    #[test]
    fn test_ispath() {
        let temp = TempDirWithCtx::new();
        let tester = temp.file_tester();

        let file_path = temp.filepath("file.txt");
        File::create(file_path).unwrap();

        let result = tester.test_path(L!("file.txt"), false);
        assert!(result);

        let result = tester.test_path(L!("file.txt"), true);
        assert!(result);

        let result = tester.test_path(L!("fi"), false);
        assert!(!result);

        let result = tester.test_path(L!("fi"), true);
        assert!(result);

        let result = tester.test_path(L!("file.txt-more"), false);
        assert!(!result);

        let result = tester.test_path(L!("file.txt-more"), true);
        assert!(!result);

        let result = tester.test_path(L!("ffiledfk.txt"), false);
        assert!(!result);

        let result = tester.test_path(L!("ffiledfk.txt"), true);
        assert!(!result);

        // Directories are also files.
        let dir_path = temp.filepath("somedir");
        create_dir_all(dir_path).unwrap();

        let result = tester.test_path(L!("somedir"), false);
        assert!(result);

        let result = tester.test_path(L!("somedir"), true);
        assert!(result);

        let result = tester.test_path(L!("some"), false);
        assert!(!result);

        let result = tester.test_path(L!("some"), true);
        assert!(result);
    }

    #[test]
    fn test_iscdpath() {
        let temp = TempDirWithCtx::new();
        let tester = temp.file_tester();

        // Note cd (unlike file paths) should report IsErr for invalid cd paths,
        // rather than IsFile(false).

        let dir_path = temp.filepath("somedir");
        create_dir_all(dir_path).unwrap();

        let result = tester.test_cd_path(L!("somedir"), false);
        assert_eq!(result, Ok(IsFile(true)));

        let result = tester.test_cd_path(L!("somedir"), true);
        assert_eq!(result, Ok(IsFile(true)));

        let result = tester.test_cd_path(L!("some"), false);
        assert_eq!(result, Err(IsErr));

        let result = tester.test_cd_path(L!("some"), true);
        assert_eq!(result, Ok(IsFile(true)));

        let result = tester.test_cd_path(L!("notdir"), false);
        assert_eq!(result, Err(IsErr));

        let result = tester.test_cd_path(L!("notdir"), true);
        assert_eq!(result, Err(IsErr));
    }

    #[test]
    fn test_redirections() {
        // Note we use is_ok and is_err since we don't care about the IsFile part.
        let temp = TempDirWithCtx::new();
        let tester = temp.file_tester();
        let file_path = temp.filepath("file.txt");
        File::create(&file_path).unwrap();

        let dir_path = temp.filepath("somedir");
        create_dir_all(&dir_path).unwrap();

        // Normal redirection.
        let result = tester.test_redirection_target(L!("file.txt"), RedirectionMode::Input);
        assert_eq!(result, Ok(IsFile(true)));

        // Can't redirect from a missing file
        let result = tester.test_redirection_target(L!("notfile.txt"), RedirectionMode::Input);
        assert_eq!(result, Err(IsErr));
        let result =
            tester.test_redirection_target(L!("bogus_path/file.txt"), RedirectionMode::Input);
        assert_eq!(result, Err(IsErr));

        // Can't redirect from a directory.
        let result = tester.test_redirection_target(L!("somedir"), RedirectionMode::Input);
        assert_eq!(result, Err(IsErr));

        // Can't redirect from an unreadable file.
        #[cfg(not(cygwin))] // Can't mark a file write-only on MSYS, this may work on true Cygwin
        {
            fs::set_permissions(&file_path, Permissions::from_mode(0o200)).unwrap();
            let result = tester.test_redirection_target(L!("file.txt"), RedirectionMode::Input);
            assert_eq!(result, Err(IsErr));
            fs::set_permissions(&file_path, Permissions::from_mode(0o600)).unwrap();
        }

        // try_input syntax highlighting reports an error even though the command will succeed.
        let result = tester.test_redirection_target(L!("file.txt"), RedirectionMode::TryInput);
        assert_eq!(result, Ok(IsFile(true)));
        let result = tester.test_redirection_target(L!("notfile.txt"), RedirectionMode::TryInput);
        assert_eq!(result, Err(IsErr));
        let result =
            tester.test_redirection_target(L!("bogus_path/file.txt"), RedirectionMode::TryInput);
        assert_eq!(result, Err(IsErr));

        // Test write redirections.
        // Overwrite an existing file.
        let result = tester.test_redirection_target(L!("file.txt"), RedirectionMode::Overwrite);
        assert_eq!(result, Ok(IsFile(true)));

        // Append to an existing file.
        let result = tester.test_redirection_target(L!("file.txt"), RedirectionMode::Append);
        assert_eq!(result, Ok(IsFile(true)));

        // Write to a missing file.
        let result = tester.test_redirection_target(L!("newfile.txt"), RedirectionMode::Overwrite);
        assert_eq!(result, Ok(IsFile(false)));

        // No-clobber write to existing file should fail.
        let result = tester.test_redirection_target(L!("file.txt"), RedirectionMode::NoClob);
        assert_eq!(result, Err(IsErr));

        // No-clobber write to missing file should succeed.
        let result = tester.test_redirection_target(L!("unique.txt"), RedirectionMode::NoClob);
        assert_eq!(result, Ok(IsFile(false)));

        let write_modes = &[
            RedirectionMode::Overwrite,
            RedirectionMode::Append,
            RedirectionMode::NoClob,
        ];

        // Can't write to a directory.
        for mode in write_modes {
            assert_eq!(
                tester.test_redirection_target(L!("somedir"), *mode),
                Err(IsErr),
                "Should not be able to write to a directory with mode {:?}",
                mode
            );
        }

        // Can't write without write permissions.
        fs::set_permissions(&file_path, Permissions::from_mode(0o400)).unwrap(); // Read-only.
        for mode in write_modes {
            assert_eq!(
                tester.test_redirection_target(L!("file.txt"), *mode),
                Err(IsErr),
                "Should not be able to write to a read-only file with mode {:?}",
                mode
            );
        }
        fs::set_permissions(&file_path, Permissions::from_mode(0o600)).unwrap(); // Restore permissions.

        // Writing into a directory without write permissions (loop through all modes).
        #[cfg(not(cygwin))] // Can't mark a file write-only on MSYS, this may work on true Cygwin
        {
            fs::set_permissions(&dir_path, Permissions::from_mode(0o500)).unwrap(); // Read and execute, no write.
            for mode in write_modes {
                assert_eq!(
                    tester.test_redirection_target(L!("somedir/newfile.txt"), *mode),
                    Err(IsErr),
                    "Should not be able to create/write in a read-only directory with mode {:?}",
                    mode
                );
            }
            fs::set_permissions(&dir_path, Permissions::from_mode(0o700)).unwrap(); // Restore permissions.
        }

        // Test fd redirections.
        assert_eq!(
            tester.test_redirection_target(L!("-"), RedirectionMode::Fd),
            Ok(IsFile(false)),
        );
        assert_eq!(
            tester.test_redirection_target(L!("0"), RedirectionMode::Fd),
            Ok(IsFile(false)),
        );
        assert_eq!(
            tester.test_redirection_target(L!("1"), RedirectionMode::Fd),
            Ok(IsFile(false)),
        );
        assert_eq!(
            tester.test_redirection_target(L!("2"), RedirectionMode::Fd),
            Ok(IsFile(false)),
        );
        assert_eq!(
            tester.test_redirection_target(L!("3"), RedirectionMode::Fd),
            Ok(IsFile(false)),
        );
        assert_eq!(
            tester.test_redirection_target(L!("500"), RedirectionMode::Fd),
            Ok(IsFile(false)),
        );

        // We are base 10, despite the leading 0.
        assert_eq!(
            tester.test_redirection_target(L!("000"), RedirectionMode::Fd),
            Ok(IsFile(false)),
        );
        assert_eq!(
            tester.test_redirection_target(L!("01"), RedirectionMode::Fd),
            Ok(IsFile(false)),
        );
        assert_eq!(
            tester.test_redirection_target(L!("07"), RedirectionMode::Fd),
            Ok(IsFile(false)),
        );

        // Invalid fd redirections.
        assert_eq!(
            tester.test_redirection_target(L!("0x2"), RedirectionMode::Fd),
            Err(IsErr),
        );
        assert_eq!(
            tester.test_redirection_target(L!("0x3F"), RedirectionMode::Fd),
            Err(IsErr),
        );
        assert_eq!(
            tester.test_redirection_target(L!("0F"), RedirectionMode::Fd),
            Err(IsErr),
        );
        assert_eq!(
            tester.test_redirection_target(L!("-1"), RedirectionMode::Fd),
            Err(IsErr),
        );
        assert_eq!(
            tester.test_redirection_target(L!("-0009"), RedirectionMode::Fd),
            Err(IsErr),
        );
        assert_eq!(
            tester.test_redirection_target(L!("--"), RedirectionMode::Fd),
            Err(IsErr),
        );
        assert_eq!(
            tester.test_redirection_target(L!("derp"), RedirectionMode::Fd),
            Err(IsErr),
        );
        assert_eq!(
            tester.test_redirection_target(L!("123boo"), RedirectionMode::Fd),
            Err(IsErr),
        );
        assert_eq!(
            tester.test_redirection_target(L!("18446744073709551616"), RedirectionMode::Fd),
            Err(IsErr),
        );
    }

    #[test]
    #[serial]
    fn test_is_potential_path() {
        let _cleanup = test_init();
        // Directories
        std::fs::create_dir_all("test/is_potential_path_test/alpha/").unwrap();
        std::fs::create_dir_all("test/is_potential_path_test/beta/").unwrap();

        // Files
        std::fs::write("test/is_potential_path_test/aardvark", []).unwrap();
        std::fs::write("test/is_potential_path_test/gamma", []).unwrap();

        let wd = L!("test/is_potential_path_test/").to_owned();
        let wds = [L!(".").to_owned(), wd];

        let vars = EnvStack::new();
        let ctx = OperationContext::background(&vars, EXPANSION_LIMIT_DEFAULT);

        let path_require_dir = PathFlags {
            require_dir: true,
            ..Default::default()
        };

        assert!(is_potential_path(
            L!("al"),
            true,
            &wds[..],
            &ctx,
            path_require_dir
        ));

        assert!(is_potential_path(
            L!("alpha/"),
            true,
            &wds[..],
            &ctx,
            path_require_dir
        ));
        assert!(is_potential_path(
            L!("aard"),
            true,
            &wds[..],
            &ctx,
            PathFlags::default()
        ));
        assert!(!is_potential_path(
            L!("aard"),
            false,
            &wds[..],
            &ctx,
            PathFlags::default()
        ));
        assert!(!is_potential_path(
            L!("alp/"),
            true,
            &wds[..],
            &ctx,
            PathFlags {
                require_dir: true,
                for_cd: true,
                ..Default::default()
            }
        ));

        assert!(!is_potential_path(
            L!("balpha/"),
            true,
            &wds[..],
            &ctx,
            path_require_dir
        ));
        assert!(!is_potential_path(
            L!("aard"),
            true,
            &wds[..],
            &ctx,
            path_require_dir
        ));
        assert!(!is_potential_path(
            L!("aarde"),
            true,
            &wds[..],
            &ctx,
            path_require_dir
        ));
        assert!(!is_potential_path(
            L!("aarde"),
            true,
            &wds[..],
            &ctx,
            PathFlags::default()
        ));

        assert!(is_potential_path(
            L!("test/is_potential_path_test/aardvark"),
            true,
            &wds[..],
            &ctx,
            PathFlags::default()
        ));
        assert!(is_potential_path(
            L!("test/is_potential_path_test/al"),
            true,
            &wds[..],
            &ctx,
            path_require_dir
        ));
        assert!(is_potential_path(
            L!("test/is_potential_path_test/aardv"),
            true,
            &wds[..],
            &ctx,
            PathFlags::default()
        ));

        assert!(!is_potential_path(
            L!("test/is_potential_path_test/aardvark"),
            true,
            &wds[..],
            &ctx,
            path_require_dir
        ));
        assert!(!is_potential_path(
            L!("test/is_potential_path_test/al/"),
            true,
            &wds[..],
            &ctx,
            PathFlags::default()
        ));
        assert!(!is_potential_path(
            L!("test/is_potential_path_test/ar"),
            true,
            &wds[..],
            &ctx,
            PathFlags::default()
        ));
        assert!(is_potential_path(
            L!("/usr"),
            true,
            &wds[..],
            &ctx,
            path_require_dir
        ));
    }
}
