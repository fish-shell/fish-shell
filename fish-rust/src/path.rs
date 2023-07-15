//! Directory utilities. This library contains functions for locating configuration directories,
//! for testing if a command with a given name can be found in the PATH, and various other
//! path-related issues.

use crate::common::wcs2zstring;
#[cfg(not(target_os = "linux"))]
use crate::compat::{MNT_LOCAL, ST_LOCAL};
use crate::env::{EnvMode, EnvStack, Environment};
use crate::expand::{expand_tilde, HOME_DIRECTORY};
use crate::flog::{FLOG, FLOGF};
use crate::wchar::prelude::*;
use crate::wutil::{normalize_path, path_normalize_for_cd, waccess, wdirname, wmkdir, wstat};
use errno::{errno, set_errno, Errno};
use libc::{EACCES, ENOENT, ENOTDIR, F_OK, X_OK};
use once_cell::sync::Lazy;
use std::ffi::OsStr;
use std::io::{ErrorKind, Write};
use std::os::unix::prelude::*;

/// Returns the user configuration directory for fish. If the directory or one of its parents
/// doesn't exist, they are first created.
///
/// \param path The directory as an out param
/// \return whether the directory was returned successfully
pub fn path_get_config() -> Option<WString> {
    let dir = get_config_directory();
    if dir.success() {
        Some(dir.path.to_owned())
    } else {
        None
    }
}

/// Returns the user data directory for fish. If the directory or one of its parents doesn't exist,
/// they are first created.
///
/// Volatile files presumed to be local to the machine, such as the fish_history and all the
/// generated_completions, will be stored in this directory.
///
/// \param path The directory as an out param
/// \return whether the directory was returned successfully
pub fn path_get_data() -> Option<WString> {
    let dir = get_data_directory();
    if dir.success() {
        Some(dir.path.to_owned())
    } else {
        None
    }
}

#[derive(Clone, Copy, Eq, PartialEq)]
pub enum DirRemoteness {
    /// directory status is unknown
    unknown,
    /// directory is known local
    local,
    /// directory is known remote
    remote,
}

/// \return the remoteness of the fish data directory.
/// This will be remote for filesystems like NFS, SMB, etc.
pub fn path_get_data_remoteness() -> DirRemoteness {
    get_data_directory().remoteness
}

/// Like path_get_data_remoteness but for the config directory.
pub fn path_get_config_remoteness() -> DirRemoteness {
    get_config_directory().remoteness
}

/// Emit any errors if config directories are missing.
/// Use the given environment stack to ensure this only occurs once.
#[widestrs]
pub fn path_emit_config_directory_messages(vars: &EnvStack) {
    let data = get_data_directory();
    if !data.success() {
        maybe_issue_path_warning(
            "data"L,
            &wgettext!("can not save history"),
            data.used_xdg,
            "XDG_DATA_HOME"L,
            &data.path,
            data.err,
            vars,
        );
    }
    if data.remoteness == DirRemoteness::remote {
        FLOG!(path, "data path appears to be on a network volume");
    }

    let config = get_config_directory();
    if !config.success() {
        maybe_issue_path_warning(
            "config"L,
            &wgettext!("can not save universal variables or functions"),
            config.used_xdg,
            "XDG_CONFIG_HOME"L,
            &config.path,
            config.err,
            vars,
        );
    }
    if config.remoteness == DirRemoteness::remote {
        FLOG!(path, "config path appears to be on a network volume");
    }
}

/// We separate this from path_create() for two reasons. First it's only caused if there is a
/// problem, and thus is not central to the behavior of that function. Second, we only want to issue
/// the message once. If the current shell starts a new fish shell (e.g., by running `fish -c` from
/// a function) we don't want that subshell to issue the same warnings.
#[widestrs]
fn maybe_issue_path_warning(
    which_dir: &wstr,
    custom_error_msg: &wstr,
    using_xdg: bool,
    xdg_var: &wstr,
    path: &wstr,
    saved_errno: libc::c_int,
    vars: &EnvStack,
) {
    let warning_var_name = "_FISH_WARNED_"L.to_owned() + which_dir;
    if vars
        .getf(&warning_var_name, EnvMode::GLOBAL | EnvMode::EXPORT)
        .is_some()
    {
        return;
    }
    vars.set_one(
        &warning_var_name,
        EnvMode::GLOBAL | EnvMode::EXPORT,
        "1"L.to_owned(),
    );

    FLOG!(error, custom_error_msg);
    if path.is_empty() {
        FLOG!(
            warning_path,
            wgettext_fmt!("Unable to locate the %ls directory.", which_dir)
        );
        FLOG!(
            warning_path,
            wgettext_fmt!(
                "Please set the %ls or HOME environment variable before starting fish.",
                xdg_var
            )
        );
    } else {
        let env_var = if using_xdg { xdg_var } else { "HOME"L };
        FLOG!(
            warning_path,
            wgettext_fmt!(
                "Unable to locate %ls directory derived from $%ls: '%ls'.",
                which_dir,
                env_var,
                path
            )
        );
        FLOG!(
            warning_path,
            wgettext_fmt!("The error was '%s'.", Errno(saved_errno).to_string())
        );
        FLOG!(
            warning_path,
            wgettext_fmt!(
                "Please set $%ls to a directory where you have write access.",
                env_var
            )
        );
    }
    let _ = std::io::stdout().write(&[b'\n']);
}

/// Finds the path of an executable named \p cmd, by looking in $PATH taken from \p vars.
/// \returns the path if found, none if not.
pub fn path_get_path(cmd: &wstr, vars: &dyn Environment) -> Option<WString> {
    let result = path_try_get_path(cmd, vars);
    if result.err.is_some() {
        None
    } else {
        Some(result.path)
    }
}

// PREFIX is defined at build time.
#[widestrs]
pub static DEFAULT_PATH: Lazy<[WString; 3]> = Lazy::new(|| {
    [
        // TODO This should use env!. The fallback is only to appease "cargo test" for now.
        WString::from_str(option_env!("PREFIX").unwrap_or("/usr/local")) + "/bin"L,
        "/usr/bin"L.to_owned(),
        "/bin"L.to_owned(),
    ]
});

/// Finds the path of an executable named \p cmd, by looking in $PATH taken from \p vars.
/// On success, err will be 0 and the path is returned.
/// On failure, we return the "best path" with err set appropriately.
/// For example, if we find a non-executable file, we will return its path and EACCESS.
/// If no candidate path is found, path will be empty and err will be set to ENOENT.
/// Possible err values are taken from access().
pub struct GetPathResult {
    pub err: Option<Errno>,
    pub path: WString,
}
impl GetPathResult {
    fn new(err: Option<Errno>, path: WString) -> Self {
        Self { err, path }
    }
}

pub fn path_try_get_path(cmd: &wstr, vars: &dyn Environment) -> GetPathResult {
    if let Some(path) = vars.get(L!("PATH")) {
        path_get_path_core(cmd, path.as_list())
    } else {
        path_get_path_core(cmd, &*DEFAULT_PATH)
    }
}

fn path_check_executable(path: &wstr) -> Result<(), std::io::Error> {
    if waccess(path, X_OK) != 0 {
        return Err(std::io::Error::last_os_error());
    }

    let buff = wstat(path)?;

    if buff.file_type().is_file() {
        Ok(())
    } else {
        Err(ErrorKind::PermissionDenied.into())
    }
}

/// Return all the paths that match the given command.
pub fn path_get_paths(cmd: &wstr, vars: &dyn Environment) -> Vec<WString> {
    FLOGF!(path, "path_get_paths('%ls')", cmd);
    let mut paths = vec![];

    // If the command has a slash, it must be an absolute or relative path and thus we don't bother
    // looking for matching commands in the PATH var.
    if cmd.contains('/') && path_check_executable(cmd).is_ok() {
        paths.push(cmd.to_owned());
        return paths;
    }

    let Some(path_var) = vars.get(L!("PATH")) else {
        return paths;
    };
    for path in path_var.as_list() {
        if path.is_empty() {
            continue;
        }
        let mut path = path.clone();
        append_path_component(&mut path, cmd);
        if path_check_executable(&path).is_ok() {
            paths.push(path);
        }
    }

    paths
}

fn path_get_path_core<S: AsRef<wstr>>(cmd: &wstr, pathsv: &[S]) -> GetPathResult {
    let noent_res = GetPathResult::new(Some(Errno(ENOENT)), WString::new());
    // Test if the given path can be executed.
    // \return 0 on success, an errno value on failure.
    let test_path = |path: &wstr| -> Result<(), Errno> {
        let narrow = wcs2zstring(path);
        if unsafe { libc::access(narrow.as_ptr(), X_OK) } != 0 {
            return Err(errno());
        }
        let narrow: Vec<u8> = narrow.into();
        let Ok(md) = std::fs::metadata(OsStr::from_bytes(&narrow)) else {
            return Err(errno());
        };
        if md.is_file() {
            Ok(())
        } else {
            Err(Errno(EACCES))
        }
    };

    if cmd.is_empty() {
        return noent_res;
    }

    // Commands cannot contain NUL byte.
    if cmd.contains('\0') {
        return noent_res;
    }

    // If the command has a slash, it must be an absolute or relative path and thus we don't bother
    // looking for a matching command.
    if cmd.contains('/') {
        return GetPathResult::new(test_path(cmd).err(), cmd.to_owned());
    }

    let mut best = noent_res;
    for next_path in pathsv {
        let next_path: &wstr = next_path.as_ref();
        if next_path.is_empty() {
            continue;
        }
        let mut proposed_path = next_path.to_owned();
        append_path_component(&mut proposed_path, cmd);
        match test_path(&proposed_path) {
            Ok(()) => {
                // We found one.
                return GetPathResult::new(None, proposed_path);
            }
            Err(err) => {
                if err.0 != ENOENT && best.err == Some(Errno(ENOENT)) {
                    // Keep the first *interesting* error and path around.
                    // ENOENT isn't interesting because not having a file is the normal case.
                    // Ignore if the parent directory is already inaccessible.
                    if waccess(wdirname(&proposed_path), X_OK) == 0 {
                        best = GetPathResult::new(Some(err), proposed_path);
                    }
                }
            }
        }
    }
    best
}

/// Returns the full path of the specified directory, using the CDPATH variable as a list of base
/// directories for relative paths.
///
/// If no valid path is found, false is returned and errno is set to ENOTDIR if at least one such
/// path was found, but it did not point to a directory, or ENOENT if no file of the specified
/// name was found.
///
/// \param dir The name of the directory.
/// \param wd The working directory. The working directory must end with a slash.
/// \param vars The environment variables to use (for the CDPATH variable)
/// \return the command, or none() if it could not be found.
pub fn path_get_cdpath(dir: &wstr, wd: &wstr, vars: &dyn Environment) -> Option<WString> {
    let mut err = ENOENT;
    if dir.is_empty() {
        return None;
    }
    assert!(wd.chars().last() == Some('/'));
    let paths = path_apply_cdpath(dir, wd, vars);

    for a_dir in paths {
        if let Ok(md) = wstat(&a_dir) {
            if md.is_dir() {
                return Some(a_dir);
            }
            err = ENOTDIR;
        }
    }

    set_errno(Errno(err));
    None
}

/// Returns the given directory with all CDPATH components applied.
#[widestrs]
pub fn path_apply_cdpath(dir: &wstr, wd: &wstr, env_vars: &dyn Environment) -> Vec<WString> {
    let mut paths = vec![];
    if dir.chars().next() == Some('/') {
        // Absolute path.
        paths.push(dir.to_owned());
    } else if dir.starts_with("./"L) || dir.starts_with("../"L) || ["."L, ".."L].contains(&dir) {
        // Path is relative to the working directory.
        paths.push(path_normalize_for_cd(wd, dir));
    } else {
        // Respect CDPATH.
        let mut cdpathsv = vec![];
        if let Some(cdpaths) = env_vars.get("CDPATH"L) {
            cdpathsv = cdpaths.as_list().to_vec();
        }
        // Always append $PWD
        cdpathsv.push("."L.to_owned());
        for path in cdpathsv {
            let mut abspath = WString::new();
            // We want to return an absolute path (see issue 6220)
            if ![Some('/'), Some('~')].contains(&path.chars().next()) {
                abspath = wd.to_owned();
                abspath.push('/');
            }
            abspath.push_utfstr(&path);

            expand_tilde(&mut abspath, env_vars);
            if abspath.is_empty() {
                continue;
            }
            abspath = normalize_path(&abspath, true);

            let mut whole_path = abspath;
            append_path_component(&mut whole_path, dir);
            paths.push(whole_path);
        }
    }
    paths
}

/// Returns the path resolved as an implicit cd command, or none() if none. This requires it to
/// start with one of the allowed prefixes (., .., ~) and resolve to a directory.
#[widestrs]
pub fn path_as_implicit_cd(path: &wstr, wd: &wstr, vars: &dyn Environment) -> Option<WString> {
    let mut exp_path = path.to_owned();
    expand_tilde(&mut exp_path, vars);
    if exp_path.starts_with("/"L)
        || exp_path.starts_with("./"L)
        || exp_path.starts_with("../"L)
        || exp_path.ends_with("/"L)
        || exp_path == ".."L
    {
        // These paths can be implicit cd, so see if you cd to the path. Note that a single period
        // cannot (that's used for sourcing files anyways).
        return path_get_cdpath(&exp_path, wd, vars);
    }
    None
}

/// Remove double slashes and trailing slashes from a path, e.g. transform foo//bar/ into foo/bar.
/// The string is modified in-place.
pub fn path_make_canonical(path: &mut WString) {
    let chars: &mut [char] = path.as_char_slice_mut();

    // Ignore trailing slashes, unless it's the first character.
    let mut len = chars.len();
    while len > 1 && chars[len - 1] == '/' {
        len -= 1;
    }

    // Turn runs of slashes into a single slash.
    let mut trailing = 0;
    let mut prev_was_slash = false;
    for leading in 0..len {
        let c = chars[leading];
        let is_slash = c == '/';
        if !prev_was_slash || !is_slash {
            // This is either the first slash in a run, or not a slash at all.
            chars[trailing] = c;
            trailing += 1;
        }
        prev_was_slash = is_slash;
    }
    assert!(trailing <= len);
    if trailing < len {
        path.truncate(trailing);
    }
}

/// Check if two paths are equivalent, which means to ignore runs of multiple slashes (or trailing
/// slashes).
pub fn paths_are_equivalent(p1: &wstr, p2: &wstr) -> bool {
    let p1 = p1.as_char_slice();
    let p2 = p2.as_char_slice();

    if p1 == p2 {
        return true;
    }

    // Ignore trailing slashes after the first character.
    let mut len1 = p1.len();
    let mut len2 = p2.len();
    while len1 > 1 && p1[len1 - 1] == '/' {
        len1 -= 1
    }
    while len2 > 1 && p2[len2 - 1] == '/' {
        len2 -= 1
    }

    // Start walking
    let mut idx1 = 0;
    let mut idx2 = 0;
    while idx1 < len1 && idx2 < len2 {
        let c1 = p1[idx1];
        let c2 = p2[idx2];

        // If the characters are different, the strings are not equivalent.
        if c1 != c2 {
            break;
        }

        idx1 += 1;
        idx2 += 1;

        // If the character was a slash, walk forwards until we hit the end of the string, or a
        // non-slash. Note the first condition is invariant within the loop.
        while c1 == '/' && p1.get(idx1) == Some(&'/') {
            idx1 += 1;
        }
        while c2 == '/' && p2.get(idx2) == Some(&'/') {
            idx2 += 1;
        }
    }

    // We matched if we consumed all of the characters in both strings.
    idx1 == len1 && idx2 == len2
}

#[widestrs]
pub fn path_is_valid(path: &wstr, working_directory: &wstr) -> bool {
    // Some special paths are always valid.
    if path.is_empty() {
        false
    } else if ["."L, "./"L].contains(&path) {
        true
    } else if [".."L, "../"L].contains(&path) {
        !working_directory.is_empty() && working_directory != "/"L
    } else if path.chars().next() != Some('/') {
        // Prepend the working directory. Note that we know path is not empty here.
        let mut tmp = working_directory.to_owned();
        tmp.push_utfstr(path);
        waccess(&tmp, F_OK) == 0
    } else {
        // Simple check.
        waccess(path, F_OK) == 0
    }
}

/// Returns whether the two paths refer to the same file.
pub fn paths_are_same_file(path1: &wstr, path2: &wstr) -> bool {
    if paths_are_equivalent(path1, path2) {
        return true;
    }

    match (wstat(path1), wstat(path2)) {
        (Ok(s1), Ok(s2)) => s1.ino() == s2.ino() && s1.dev() == s2.dev(),
        _ => false,
    }
}

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

/// The following type wraps up a user's "base" directories, corresponding (conceptually if not
/// actually) to XDG spec.
struct BaseDirectory {
    /// the path where we attempted to create the directory.
    path: WString,
    /// whether the dir is remote
    remoteness: DirRemoteness,
    /// the error code if creating the directory failed, or 0 on success.
    err: libc::c_int,
    /// whether an XDG variable was used in resolving the directory.
    used_xdg: bool,
}

impl BaseDirectory {
    fn success(&self) -> bool {
        self.err == 0
    }
}

/// Attempt to get a base directory, creating it if necessary. If a variable named \p xdg_var is
/// set, use that directory; otherwise use the path \p non_xdg_homepath rooted in $HOME. \return the
/// result; see the base_directory_t fields.
#[widestrs]
fn make_base_directory(xdg_var: &wstr, non_xdg_homepath: &wstr) -> BaseDirectory {
    // The vars we fetch must be exported. Allowing them to be universal doesn't make sense and
    // allowing that creates a lock inversion that deadlocks the shell since we're called before
    // uvars are available.
    let vars = EnvStack::globals();

    let mut path = WString::new();
    let used_xdg;
    if let Some(xdg_dir) = vars.getf_unless_empty(xdg_var, EnvMode::GLOBAL | EnvMode::EXPORT) {
        path = xdg_dir.as_string() + "/fish"L;
        used_xdg = true;
    } else {
        if let Some(home) = vars.getf_unless_empty("HOME"L, EnvMode::GLOBAL | EnvMode::EXPORT) {
            path = home.as_string() + non_xdg_homepath;
        }
        used_xdg = false;
    }

    set_errno(Errno(0));
    let err;
    let mut remoteness = DirRemoteness::unknown;
    if path.is_empty() {
        err = ENOENT;
    } else if !create_directory(&path) {
        err = errno().0;
    } else {
        err = 0;
        // Need to append a trailing slash to check the contents of the directory, not its parent.
        let mut tmp = path.clone();
        tmp.push('/');
        remoteness = path_remoteness(&tmp);
    }

    BaseDirectory {
        path,
        remoteness,
        err,
        used_xdg,
    }
}

/// Make sure the specified directory exists. If needed, try to create it and any currently not
/// existing parent directories, like mkdir -p,.
///
/// \return 0 if, at the time of function return the directory exists, -1 otherwise.
fn create_directory(d: &wstr) -> bool {
    let md = loop {
        match wstat(d) {
            Err(md) if md.kind() == ErrorKind::Interrupted => continue,
            md => break md,
        }
    };
    match md {
        Ok(md) if md.is_dir() => true,
        Err(e) if e.kind() == ErrorKind::NotFound => {
            let dir: &wstr = wdirname(d);
            return create_directory(dir) && wmkdir(d, 0o700) == 0;
        }
        _ => false,
    }
}

/// \return whether the given path is on a remote filesystem.
fn path_remoteness(path: &wstr) -> DirRemoteness {
    let narrow = wcs2zstring(path);
    #[cfg(target_os = "linux")]
    {
        let mut buf: libc::statfs = unsafe { std::mem::zeroed() };
        if unsafe { libc::statfs(narrow.as_ptr(), &mut buf) } < 0 {
            return DirRemoteness::unknown;
        }
        // Linux has constants for these like NFS_SUPER_MAGIC, SMB_SUPER_MAGIC, CIFS_MAGIC_NUMBER but
        // these are in varying headers. Simply hard code them.
        // NOTE: The cast is necessary for 32-bit systems because of the 4-byte CIFS_MAGIC_NUMBER
        match usize::try_from(buf.f_type).unwrap() {
            0x6969 |     // NFS_SUPER_MAGIC
            0x517B |     // SMB_SUPER_MAGIC
            0xFE534D42 | // SMB2_MAGIC_NUMBER - not in the manpage
            0xFF534D42   // CIFS_MAGIC_NUMBER
                => DirRemoteness::remote,
            _ => {
                // Other FSes are assumed local.
                DirRemoteness::local
            }
        }
    }
    #[cfg(not(target_os = "linux"))]
    {
        let st_local = ST_LOCAL();
        if st_local != 0 {
            // ST_LOCAL is a flag to statvfs, which is itself standardized.
            // In practice the only system to use this path is NetBSD.
            let mut buf: libc::statvfs = unsafe { std::mem::zeroed() };
            if unsafe { libc::statvfs(narrow.as_ptr(), &mut buf) } < 0 {
                return DirRemoteness::unknown;
            }
            // statvfs::f_flag is `unsigned long`, which is 4-bytes on most 32-bit targets.
            #[cfg_attr(target_pointer_width = "64", allow(clippy::useless_conversion))]
            return if u64::from(buf.f_flag) & st_local != 0 {
                DirRemoteness::local
            } else {
                DirRemoteness::remote
            };
        }
        let mnt_local = MNT_LOCAL();
        if mnt_local != 0 {
            let mut buf: libc::statfs = unsafe { std::mem::zeroed() };
            if unsafe { libc::statfs(narrow.as_ptr(), &mut buf) } < 0 {
                return DirRemoteness::unknown;
            }
            // statfs::f_flag is hard-coded as 64-bits on 32/64-bit FreeBSD but it's a (4-byte)
            // long on 32-bit NetBSD.. and always 4-bytes on macOS (even on 64-bit builds).
            #[allow(clippy::useless_conversion)]
            return if u64::from(buf.f_flags) & mnt_local != 0 {
                DirRemoteness::local
            } else {
                DirRemoteness::remote
            };
        }
        DirRemoteness::unknown
    }
}

#[widestrs]
fn get_data_directory() -> &'static BaseDirectory {
    static DIR: Lazy<BaseDirectory> =
        Lazy::new(|| make_base_directory("XDG_DATA_HOME"L, "/.local/share/fish"L));
    &*DIR
}

#[widestrs]
fn get_config_directory() -> &'static BaseDirectory {
    static DIR: Lazy<BaseDirectory> =
        Lazy::new(|| make_base_directory("XDG_CONFIG_HOME"L, "/.config/fish"L));
    &*DIR
}

/// Appends a path component, with a / if necessary.
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

#[test]
fn test_path_make_canonical() {
    let mut path = L!("//foo//////bar/").to_owned();
    path_make_canonical(&mut path);
    assert_eq!(path, "/foo/bar");

    path = L!("/").to_owned();
    path_make_canonical(&mut path);
    assert_eq!(path, "/");
}

#[test]
fn test_path() {
    let mut path = L!("//foo//////bar/").to_owned();
    path_make_canonical(&mut path);
    assert_eq!(&path, L!("/foo/bar"));

    path = L!("/").to_owned();
    path_make_canonical(&mut path);
    assert_eq!(&path, L!("/"));

    assert!(!paths_are_equivalent(L!("/foo/bar/baz"), L!("foo/bar/baz")));
    assert!(paths_are_equivalent(
        L!("///foo///bar/baz"),
        L!("/foo/bar////baz//")
    ));
    assert!(paths_are_equivalent(L!("/foo/bar/baz"), L!("/foo/bar/baz")));
    assert!(paths_are_equivalent(L!("/"), L!("/")));

    assert_eq!(
        path_apply_working_directory(L!("abc"), L!("/def/")),
        L!("/def/abc")
    );
    assert_eq!(
        path_apply_working_directory(L!("abc/"), L!("/def/")),
        L!("/def/abc/")
    );
    assert_eq!(
        path_apply_working_directory(L!("/abc/"), L!("/def/")),
        L!("/abc/")
    );
    assert_eq!(
        path_apply_working_directory(L!("/abc"), L!("/def/")),
        L!("/abc")
    );
    assert!(path_apply_working_directory(L!(""), L!("/def/")).is_empty());
    assert_eq!(path_apply_working_directory(L!("abc"), L!("")), L!("abc"));
}
