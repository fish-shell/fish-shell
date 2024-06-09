pub mod dir_iter;
pub mod encoding;
pub mod errors;
pub mod fileid;
pub mod gettext;
mod hex_float;
pub mod printf;
#[cfg(test)]
mod tests;
pub mod wcstod;
pub mod wcstoi;

use crate::common::{
    cstr2wcstring, fish_reserved_codepoint, str2wcstring, wcs2osstring, wcs2string, wcs2zstring,
};
use crate::fallback;
use crate::fds::AutoCloseFd;
use crate::flog::FLOGF;
use crate::wchar::{wstr, WString, L};
use crate::wchar_ext::WExt;
use crate::wcstringutil::{join_strings, wcs2string_callback};
use errno::errno;
pub use gettext::{wgettext, wgettext_fmt, wgettext_maybe_fmt, wgettext_str};
use std::ffi::{CStr, OsStr};
use std::fs::{self, canonicalize};
use std::io::{self, Write};
use std::os::unix::prelude::*;

extern crate fish_printf;
pub use fish_printf::sprintf;

pub use wcstoi::*;

/// Wide character version of opendir(). Note that opendir() is guaranteed to set close-on-exec by
/// POSIX (hooray).
pub fn wopendir(name: &wstr) -> *mut libc::DIR {
    let tmp = wcs2zstring(name);
    unsafe { libc::opendir(tmp.as_ptr()) }
}

/// Wide character version of stat().
pub fn wstat(file_name: &wstr) -> io::Result<fs::Metadata> {
    let tmp = wcs2osstring(file_name);
    fs::metadata(tmp)
}

/// Wide character version of lstat().
pub fn lwstat(file_name: &wstr) -> io::Result<fs::Metadata> {
    let tmp = wcs2osstring(file_name);
    fs::symlink_metadata(tmp)
}

/// Wide character version of access().
pub fn waccess(file_name: &wstr, mode: libc::c_int) -> libc::c_int {
    let tmp = wcs2zstring(file_name);
    unsafe { libc::access(tmp.as_ptr(), mode) }
}

/// Wide character version of unlink().
pub fn wunlink(file_name: &wstr) -> libc::c_int {
    let tmp = wcs2zstring(file_name);
    unsafe { libc::unlink(tmp.as_ptr()) }
}

pub fn wperror(s: &wstr) {
    let bytes = wcs2string(s);
    // We can't guarantee the string is 100% Unicode (why?), so we don't use std::str::from_utf8()
    let s = OsStr::from_bytes(&bytes).to_string_lossy();
    perror(&s)
}

/// Port of the wide-string wperror from `src/wutil.cpp` but for rust `&str`.
pub fn perror(s: &str) {
    let e = errno().0;
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

pub fn perror_io(s: &str, e: &io::Error) {
    eprintln!("{}: {}", s, e);
}

/// Wide character version of getcwd().
pub fn wgetcwd() -> WString {
    let mut cwd = [b'\0'; libc::PATH_MAX as usize];
    let res = unsafe {
        libc::getcwd(
            std::ptr::addr_of_mut!(cwd).cast(),
            std::mem::size_of_val(&cwd),
        )
    };
    if !res.is_null() {
        return cstr2wcstring(&cwd);
    }

    FLOGF!(
        error,
        "getcwd() failed with errno %d/%s",
        errno::errno().0,
        errno::errno().to_string()
    );
    WString::new()
}

/// Wide character version of readlink().
pub fn wreadlink(file_name: &wstr) -> Option<WString> {
    let md = lwstat(file_name).ok()?;
    let bufsize = usize::try_from(md.len()).unwrap() + 1;
    let mut target_buf = vec![b'\0'; bufsize];
    let tmp = wcs2zstring(file_name);
    let nbytes = unsafe {
        libc::readlink(
            tmp.as_ptr(),
            std::ptr::addr_of_mut!(target_buf[0]).cast(),
            bufsize,
        )
    };
    if nbytes == -1 {
        perror("readlink");
        return None;
    }
    // The link might have been modified after our call to lstat.  If the link now points to a path
    // that's longer than the original one, we can't read everything in our buffer.  Simply give
    // up. We don't need to report an error since our only caller will already fall back to ENOENT.
    let nbytes = usize::try_from(nbytes).unwrap();
    if nbytes == bufsize {
        return None;
    }
    Some(str2wcstring(&target_buf[0..nbytes]))
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

            let Ok(narrow_result) = narrow_res else {
                return None;
            };

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

    let comps: Vec<&wstr> = path.split(sep).collect();
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

/// Given an input path `path` and a working directory `wd`, do a "normalizing join" in a way
/// appropriate for cd. That is, return effectively wd + path while resolving leading ../s from
/// path. The intent here is to allow 'cd' out of a directory which may no longer exist, without
/// allowing 'cd' into a directory that may not exist; see #5341.
pub fn path_normalize_for_cd(wd: &wstr, path: &wstr) -> WString {
    use std::collections::VecDeque;

    // Fast paths.
    const SEP: char = '/';
    assert!(
        wd.as_char_slice().first() == Some(&'/') && wd.as_char_slice().last() == Some(&'/'),
        "Invalid working directory, it must start and end with /"
    );
    if path.is_empty() {
        return wd.to_owned();
    } else if path.as_char_slice().first() == Some(&SEP) {
        return path.to_owned();
    } else if path.as_char_slice().first() != Some(&'.') {
        return wd.to_owned() + path;
    }

    // Split our strings by the sep.
    let mut wd_comps: VecDeque<_> = wd
        .split(SEP)
        // Remove empty segments from wd_comps, especially leading/trailing empties.
        .filter(|p| !p.is_empty())
        .collect();
    let mut path_comps = path.split(SEP).peekable();

    // Erase leading . and .. components from path_comps, popping from wd_comps as we go.
    while let Some(comp) = path_comps.peek() {
        if comp.is_empty() || comp == "." {
            path_comps.next();
        } else if comp == ".." && wd_comps.pop_back().is_some() {
            path_comps.next();
        } else {
            break;
        }
    }

    // Append un-erased elements to wd_comps and join them, prepending with a leading /
    let mut paths = wd_comps;
    paths.extend(path_comps);
    let mut result =
        WString::with_capacity(paths.iter().fold(0, |sum, s| sum + s.len()) + paths.len() + 1);
    for p in &paths {
        result.push(SEP);
        result.push_utfstr(*p);
    }
    result
}

#[cfg(test)]
mod path_cd_tests {
    use super::path_normalize_for_cd;
    use crate::wchar::L;

    #[test]
    fn relative_path() {
        let wd = L!("/home/user/");
        let path = L!("projects");
        eprintln!("({}, {})", wd, path);
        assert_eq!(path_normalize_for_cd(wd, path), L!("/home/user/projects"));
    }

    #[test]
    fn absolute_path() {
        let wd = L!("/home/user/");
        let path = L!("/etc");
        eprintln!("({}, {})", wd, path);
        assert_eq!(path_normalize_for_cd(wd, path), L!("/etc"));
    }

    #[test]
    fn parent_directory() {
        let wd = L!("/home/user/projects/");
        let path = L!("../docs");
        eprintln!("({}, {})", wd, path);
        assert_eq!(path_normalize_for_cd(wd, path), L!("/home/user/docs"));
    }

    #[test]
    fn current_directory() {
        let wd = L!("/home/user/");
        let path = L!("./");
        eprintln!("({}, {})", wd, path);
        assert_eq!(path_normalize_for_cd(wd, path), L!("/home/user"));
    }

    #[test]
    fn nested_parent_directory() {
        let wd = L!("/home/user/projects/");
        let path = L!("../../");
        eprintln!("({}, {})", wd, path);
        assert_eq!(path_normalize_for_cd(wd, path), L!("/home"));
    }

    #[test]
    fn complex_path() {
        let wd = L!("/home/user/projects/");
        let path = L!("./../other/projects/./.././../docs");
        eprintln!("({}, {})", wd, path);
        assert_eq!(
            path_normalize_for_cd(wd, path),
            L!("/home/user/other/projects/./.././../docs")
        );
    }

    #[test]
    fn root_directory() {
        let wd = L!("/");
        let path = L!("..");
        eprintln!("({}, {})", wd, path);
        assert_eq!(path_normalize_for_cd(wd, path), L!("/.."));
    }

    #[test]
    fn empty_path() {
        let wd = L!("/home/user/");
        let path = L!("");
        eprintln!("({}, {})", wd, path);
        assert_eq!(path_normalize_for_cd(wd, path), L!("/home/user/"));
    }

    #[test]
    fn trailing_slash() {
        let wd = L!("/home/user/projects/");
        let path = L!("docs/");
        eprintln!("({}, {})", wd, path);
        assert_eq!(
            path_normalize_for_cd(wd, path),
            L!("/home/user/projects/docs/")
        );
    }
}

/// Wide character version of dirname().
pub fn wdirname(mut path: &wstr) -> &wstr {
    // Do not use system-provided dirname (#7837).
    // On Mac it's not thread safe, and will error for paths exceeding PATH_MAX.
    // This follows OpenGroup dirname recipe.

    // 1: Double-slash stays.
    if path == "//" {
        return path;
    }

    // 2: All slashes => return slash.
    if !path.is_empty() && path.chars().all(|c| c == '/') {
        return L!("/");
    }

    // 3: Trim trailing slashes.
    while path.as_char_slice().last() == Some(&'/') {
        path = path.slice_to(path.char_count() - 1);
    }

    // 4: No slashes left => return period.
    let Some(last_slash) = path.chars().rposition(|c| c == '/') else {
        return L!(".");
    };

    // 5: Remove trailing non-slashes.
    path = path.slice_to(last_slash + 1);

    // 6: Skip as permitted.
    // 7: Remove trailing slashes again.
    while path.as_char_slice().last() == Some(&'/') {
        path = path.slice_to(path.char_count() - 1);
    }

    // 8: Empty => return slash.
    if path.is_empty() {
        return L!("/");
    }
    path
}

/// Wide character version of basename().
pub fn wbasename(mut path: &wstr) -> &wstr {
    // This follows OpenGroup basename recipe.
    // 1: empty => allowed to return ".". This is what system impls do.
    if path.is_empty() {
        return L!(".");
    }

    // 2: Skip as permitted.
    // 3: All slashes => return slash.
    if !path.is_empty() && path.chars().all(|c| c == '/') {
        return L!("/");
    }

    // 4: Remove trailing slashes.
    while path.as_char_slice().last() == Some(&'/') {
        path = path.slice_to(path.char_count() - 1);
    }

    // 5: Remove up to and including last slash.
    if let Some(last_slash) = path.chars().rposition(|c| c == '/') {
        path = path.slice_from(last_slash + 1);
    }
    path
}

/// Wide character version of mkdir.
pub fn wmkdir(name: &wstr, mode: libc::mode_t) -> libc::c_int {
    let name_narrow = wcs2zstring(name);
    unsafe { libc::mkdir(name_narrow.as_ptr(), mode) }
}

/// Wide character version of rename.
pub fn wrename(old_name: &wstr, new_name: &wstr) -> libc::c_int {
    let old_narrow = wcs2zstring(old_name);
    let new_narrow = wcs2zstring(new_name);
    unsafe { libc::rename(old_narrow.as_ptr(), new_narrow.as_ptr()) }
}

pub fn write_to_fd(input: &[u8], fd: RawFd) -> nix::Result<usize> {
    nix::unistd::write(unsafe { BorrowedFd::borrow_raw(fd) }, input)
}

/// Write a wide string to a file descriptor. This avoids doing any additional allocation.
/// This does NOT retry on EINTR or EAGAIN, it simply returns.
/// Return -1 on error in which case errno will have been set. In this event, the number of bytes
/// actually written cannot be obtained.
pub fn wwrite_to_fd(input: &wstr, fd: RawFd) -> Option<usize> {
    // Accumulate data in a local buffer.
    let mut accum = [b'\0'; 512];
    let mut accumlen = 0;
    let maxaccum: usize = std::mem::size_of_val(&accum);

    // Helper to perform a write to 'fd', looping as necessary.
    // Return true on success, false on error.
    let mut total_written = 0;

    fn do_write(fd: RawFd, total_written: &mut usize, mut buf: &[u8]) -> bool {
        while !buf.is_empty() {
            let Ok(amt) = write_to_fd(buf, fd) else {
                return false;
            };
            *total_written += amt;
            assert!(amt <= buf.len(), "Wrote more than requested");
            buf = &buf[amt..];
        }
        true
    }

    // Helper to flush the accumulation buffer.
    let flush_accum = |total_written: &mut usize, accum: &[u8], accumlen: &mut usize| {
        if !do_write(fd, total_written, &accum[..*accumlen]) {
            return false;
        }
        *accumlen = 0;
        true
    };

    let mut success = wcs2string_callback(input, |buff: &[u8]| {
        if buff.len() + accumlen > maxaccum {
            // We have to flush.
            if !flush_accum(&mut total_written, &accum, &mut accumlen) {
                return false;
            }
        }
        if buff.len() + accumlen <= maxaccum {
            // Accumulate more.
            unsafe {
                std::ptr::copy(&buff[0], &mut accum[accumlen], buff.len());
            }
            accumlen += buff.len();
            true
        } else {
            // Too much data to even fit, just write it immediately.
            do_write(fd, &mut total_written, buff)
        }
    });
    // Flush any remaining.
    if success {
        success = flush_accum(&mut total_written, &accum, &mut accumlen);
    }
    if success {
        Some(total_written)
    } else {
        None
    }
}

const PUA1_START: char = '\u{E000}';
const PUA1_END: char = '\u{F900}';
// const PUA2_START: char = '\u{F0000}';
// const PUA2_END: char = '\u{FFFFE}';
// const PUA3_START: char = '\u{100000}';
// const PUA3_END: char = '\u{10FFFE}';

/// Return one if the code point is in a Unicode private use area.
pub(crate) fn fish_is_pua(c: char) -> bool {
    PUA1_START <= c && c < PUA1_END
}

/// We need this because there are too many implementations that don't return the proper answer for
/// some code points. See issue #3050.
pub fn fish_iswalnum(c: char) -> bool {
    !fish_reserved_codepoint(c) && !fish_is_pua(c) && c.is_alphanumeric()
}

extern "C" {
    fn iswgraph(wc: libc::wchar_t) -> libc::c_int; // Technically it's wint_t
}

/// We need this because there are too many implementations that don't return the proper answer for
/// some code points. See issue #3050.
pub fn fish_iswgraph(c: char) -> bool {
    !fish_reserved_codepoint(c) && (fish_is_pua(c) || unsafe { iswgraph(c as libc::wchar_t) } != 0)
}

pub fn fish_wcswidth(s: &wstr) -> isize {
    fallback::fish_wcswidth(s)
}

/// Class for representing a file's inode. We use this to detect and avoid symlink loops, among
/// other things. While an inode / dev pair is sufficient to distinguish co-existing files, Linux
/// seems to aggressively re-use inodes, so it cannot determine if a file has been deleted (ABA
/// problem). Therefore we include richer information.
#[derive(Debug, Clone, Eq, Hash, Ord, PartialEq, PartialOrd)]
pub struct FileId {
    pub device: libc::dev_t,
    pub inode: libc::ino_t,
    pub size: u64,
    pub change_seconds: libc::time_t,
    pub change_nanoseconds: i64,
    pub mod_seconds: libc::time_t,
    pub mod_nanoseconds: i64,
}

impl FileId {
    pub const fn new() -> Self {
        FileId {
            device: -1 as _,
            inode: -1 as _,
            size: -1 as _,
            change_seconds: libc::time_t::MIN,
            change_nanoseconds: i64::MIN,
            mod_seconds: libc::time_t::MIN,
            mod_nanoseconds: -1 as _,
        }
    }
    pub fn from_stat(buf: &libc::stat) -> FileId {
        let mut result = FileId::new();
        result.device = buf.st_dev;
        result.inode = buf.st_ino;
        result.size = buf.st_size as u64;
        result.change_seconds = buf.st_ctime;
        result.mod_seconds = buf.st_mtime;
        #[allow(clippy::unnecessary_cast)] // platform-dependent
        #[cfg(not(target_os = "netbsd"))]
        {
            result.change_nanoseconds = buf.st_ctime_nsec as _;
            result.mod_nanoseconds = buf.st_mtime_nsec as _;
        }
        #[cfg(target_os = "netbsd")]
        {
            result.change_nanoseconds = buf.st_ctimensec as _;
            result.mod_nanoseconds = buf.st_mtimensec as _;
        }
        result
    }

    /// Return true if \param rhs has higher mtime seconds than this file_id_t.
    /// If identical, nanoseconds are compared.
    pub fn older_than(&self, rhs: &FileId) -> bool {
        let lhs = (self.mod_seconds, self.mod_nanoseconds);
        let rhs = (rhs.mod_seconds, rhs.mod_nanoseconds);
        lhs.cmp(&rhs).is_lt()
    }
}

pub const INVALID_FILE_ID: FileId = FileId::new();

pub fn file_id_for_fd(fd: BorrowedFd<'_>) -> FileId {
    let mut result = INVALID_FILE_ID;
    let mut buf: libc::stat = unsafe { std::mem::zeroed() };
    if unsafe { libc::fstat(fd.as_raw_fd(), &mut buf) } == 0 {
        result = FileId::from_stat(&buf);
    }
    result
}

pub fn file_id_for_autoclose_fd(fd: &AutoCloseFd) -> FileId {
    file_id_for_fd(fd.as_fd())
}

pub fn file_id_for_path(path: &wstr) -> FileId {
    file_id_for_path_narrow(&wcs2zstring(path))
}

pub fn file_id_for_path_narrow(path: &CStr) -> FileId {
    let mut result = INVALID_FILE_ID;
    let mut buf: libc::stat = unsafe { std::mem::zeroed() };
    if unsafe { libc::stat(path.as_ptr(), &mut buf) } == 0 {
        result = FileId::from_stat(&buf);
    }
    result
}
/// Given that `cursor` is a pointer into `base`, return the offset in characters.
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
fn test_wstr_offset_in() {
    use crate::wchar::L;
    let base = L!("hello world");
    assert_eq!(wstr_offset_in(&base[6..], base), 6);
    assert_eq!(wstr_offset_in(&base[0..], base), 0);
    assert_eq!(wstr_offset_in(&base[6..], &base[6..]), 0);
    assert_eq!(wstr_offset_in(&base[base.len()..], base), base.len());
}
