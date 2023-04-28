pub mod encoding;
pub mod errors;
pub mod gettext;
pub mod printf;
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
use crate::wcstringutil::{join_strings, split_string, wcs2string_callback};
use errno::{errno, set_errno, Errno};
pub(crate) use gettext::{wgettext, wgettext_fmt, wgettext_str};
use libc::{
    DT_BLK, DT_CHR, DT_DIR, DT_FIFO, DT_LNK, DT_REG, DT_SOCK, EACCES, EIO, ELOOP, ENAMETOOLONG,
    ENODEV, ENOENT, ENOTDIR, F_GETFL, F_SETFL, O_NONBLOCK, S_IFBLK, S_IFCHR, S_IFDIR, S_IFIFO,
    S_IFLNK, S_IFMT, S_IFREG, S_IFSOCK,
};
pub(crate) use printf::sprintf;
use std::ffi::OsStr;
use std::fs::canonicalize;
use std::io::Write;
use std::os::fd::RawFd;
use std::os::fd::{FromRawFd, IntoRawFd};
use std::os::unix::prelude::{OsStrExt, OsStringExt};
pub use wcstoi::*;
use widestring_suffix::widestrs;

/// Wide character version of opendir(). Note that opendir() is guaranteed to set close-on-exec by
/// POSIX (hooray).
pub fn wopendir(name: &wstr) -> *mut libc::DIR {
    let tmp = wcs2zstring(name);
    unsafe { libc::opendir(tmp.as_ptr()) }
}

/// Wide character version of stat().
pub fn wstat(file_name: &wstr) -> Result<std::fs::Metadata, std::io::Error> {
    let tmp = wcs2osstring(file_name);
    std::fs::metadata(tmp)
}

/// Wide character version of lstat().
pub fn lwstat(file_name: &wstr) -> Result<std::fs::Metadata, std::io::Error> {
    let tmp = wcs2osstring(file_name);
    std::fs::symlink_metadata(tmp)
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
    // TODO This should not crash on invalid UTF-8
    perror(std::str::from_utf8(&wcs2string(s)).unwrap())
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
        errno().0,
        "errno"
    );
    WString::new()
}

pub fn make_fd_nonblocking(fd: RawFd) -> libc::c_int {
    unsafe {
        let flags = libc::fcntl(fd, F_GETFL, 0);
        let mut err = 0;
        let nonblocking = (flags & O_NONBLOCK) != 0;
        if !nonblocking {
            err = libc::fcntl(fd, F_SETFL, flags | O_NONBLOCK);
        }
        if err == -1 {
            errno().0
        } else {
            0
        }
    }
}

pub fn make_fd_blocking(fd: RawFd) -> libc::c_int {
    unsafe {
        let flags = libc::fcntl(fd, F_GETFL, 0);
        let mut err = 0;
        let nonblocking = (flags & O_NONBLOCK) != 0;
        if nonblocking {
            err = libc::fcntl(fd, F_SETFL, flags & !O_NONBLOCK);
        }
        if err == -1 {
            errno().0
        } else {
            0
        }
    }
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

/// Given an input path \p path and a working directory \p wd, do a "normalizing join" in a way
/// appropriate for cd. That is, return effectively wd + path while resolving leading ../s from
/// path. The intent here is to allow 'cd' out of a directory which may no longer exist, without
/// allowing 'cd' into a directory that may not exist; see #5341.
#[widestrs]
pub fn path_normalize_for_cd(wd: &wstr, path: &wstr) -> WString {
    // Fast paths.
    const sep: char = '/';
    assert!(
        wd.as_char_slice().first() == Some(&'/') && wd.as_char_slice().last() == Some(&'/'),
        "Invalid working directory, it must start and end with /"
    );
    if path.is_empty() {
        return wd.to_owned();
    } else if path.as_char_slice().first() == Some(&sep) {
        return path.to_owned();
    } else if path.as_char_slice().first() != Some(&'.') {
        return wd.to_owned() + path;
    }

    // Split our strings by the sep.
    let mut wd_comps = split_string(wd, sep);
    let path_comps = split_string(path, sep);

    // Remove empty segments from wd_comps.
    // In particular this removes the leading and trailing empties.
    wd_comps.retain(|comp| !comp.is_empty());

    // Erase leading . and .. components from path_comps, popping from wd_comps as we go.
    let mut erase_count = 0;
    for comp in &path_comps {
        let mut erase_it = false;
        if comp.is_empty() || comp == "."L {
            erase_it = true;
        } else if comp == ".."L && !wd_comps.is_empty() {
            erase_it = true;
            wd_comps.pop();
        }
        if erase_it {
            erase_count += 1;
        } else {
            break;
        }
    }
    // Append un-erased elements to wd_comps and join them, then prepend the leading /.
    wd_comps.extend(path_comps.into_iter().skip(erase_count));

    let mut result = join_strings(&wd_comps, sep);
    result.insert(0, '/');
    result
}

/// Wide character version of dirname().
#[widestrs]
pub fn wdirname(mut path: WString) -> WString {
    // Do not use system-provided dirname (#7837).
    // On Mac it's not thread safe, and will error for paths exceeding PATH_MAX.
    // This follows OpenGroup dirname recipe.

    // 1: Double-slash stays.
    if path == "//"L {
        return path;
    }

    // 2: All slashes => return slash.
    if !path.is_empty() && path.chars().find(|c| *c == '/').is_none() {
        return "/"L.to_owned();
    }

    // 3: Trim trailing slashes.
    while path.as_char_slice().last() == Some(&'/') {
        path.pop();
    }

    // 4: No slashes left => return period.
    let Some(last_slash) = path.chars().rev().position(|c| c == '/') else {
        return "."L.to_owned()
    };

    // 5: Remove trailing non-slashes.
    path.truncate(last_slash + 1);

    // 6: Skip as permitted.
    // 7: Remove trailing slashes again.
    while path.as_char_slice().last() == Some(&'/') {
        path.pop();
    }

    // 8: Empty => return slash.
    if path.is_empty() {
        path = "/"L.to_owned();
    }
    path
}

/// Wide character version of basename().
#[widestrs]
pub fn wbasename(mut path: WString) -> WString {
    // This follows OpenGroup basename recipe.
    // 1: empty => allowed to return ".". This is what system impls do.
    if path.is_empty() {
        return "."L.to_owned();
    }

    // 2: Skip as permitted.
    // 3: All slashes => return slash.
    if !path.is_empty() && path.chars().find(|c| *c == '/').is_none() {
        return "/"L.to_owned();
    }

    // 4: Remove trailing slashes.
    // while (!path.is_empty() && path.back() == '/') path.pop_back();
    while path.as_char_slice().last() == Some(&'/') {
        path.pop();
    }

    // 5: Remove up to and including last slash.
    if let Some(last_slash) = path.chars().rev().position(|c| c == '/') {
        path.truncate(last_slash + 1);
    };
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

fn write_to_fd(input: &[u8], fd: RawFd) -> std::io::Result<usize> {
    let mut file = unsafe { std::fs::File::from_raw_fd(fd) };
    let amt = file.write(input);
    // Ensure the file is not closed.
    file.into_raw_fd();
    amt
}

/// Write a wide string to a file descriptor. This avoids doing any additional allocation.
/// This does NOT retry on EINTR or EAGAIN, it simply returns.
/// \return -1 on error in which case errno will have been set. In this event, the number of bytes
/// actually written cannot be obtained.
pub fn wwrite_to_fd(input: &wstr, fd: RawFd) -> Option<usize> {
    // Accumulate data in a local buffer.
    let mut accum = [b'\0'; 512];
    let mut accumlen = 0;
    let maxaccum: usize = std::mem::size_of_val(&accum);

    // Helper to perform a write to 'fd', looping as necessary.
    // \return true on success, false on error.
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

extern "C" {
    fn iswgraph(wc: libc::wchar_t) -> libc::c_int; // Technically it's wint_t
}

/// We need this because there are too many implementations that don't return the proper answer for
/// some code points. See issue #3050.
pub fn fish_iswgraph(c: char) -> bool {
    !fish_reserved_codepoint(c) && (fish_is_pua(c) || unsafe { iswgraph(c as libc::wchar_t) } != 0)
}

pub fn fish_wcswidth(s: &wstr) -> libc::c_int {
    fallback::fish_wcswidth(s)
}

/// Class for representing a file's inode. We use this to detect and avoid symlink loops, among
/// other things. While an inode / dev pair is sufficient to distinguish co-existing files, Linux
/// seems to aggressively re-use inodes, so it cannot determine if a file has been deleted (ABA
/// problem). Therefore we include richer information.
#[derive(Clone, Eq, Hash, Ord, PartialEq, PartialOrd)]
pub struct FileId {
    device: libc::dev_t,
    inode: libc::ino_t,
    size: u64,
    change_seconds: libc::time_t,
    change_nanoseconds: i64,
    mod_seconds: libc::time_t,
    mod_nanoseconds: i64,
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
        {
            result.change_nanoseconds = buf.st_ctime_nsec as _;
            result.mod_nanoseconds = buf.st_mtime_nsec as _;
        }
        result
    }

    /// \return true if \param rhs has higher mtime seconds than this file_id_t.
    /// If identical, nanoseconds are compared.
    pub fn older_than(&self, rhs: &FileId) -> bool {
        let lhs = (self.mod_seconds, self.mod_nanoseconds);
        let rhs = (rhs.mod_seconds, rhs.mod_nanoseconds);
        lhs.cmp(&rhs).is_lt()
    }

    pub fn dump(&self) -> WString {
        let mut result = WString::new();
        result += &sprintf!("     device: %lld\n", self.device)[..];
        result += &sprintf!("      inode: %lld\n", self.inode)[..];
        result += &sprintf!("       size: %lld\n", self.size)[..];
        result += &sprintf!("     change: %lld\n", self.change_seconds)[..];
        result += &sprintf!("change_nano: %lld\n", self.change_nanoseconds)[..];
        result += &sprintf!("        mod: %lld\n", self.mod_seconds)[..];
        result += &sprintf!("   mod_nano: %lld", self.mod_nanoseconds)[..];
        result
    }
}

pub const INVALID_FILE_ID: FileId = FileId::new();

pub fn file_id_for_fd(fd: RawFd) -> FileId {
    let mut result = INVALID_FILE_ID;
    let mut buf: libc::stat = unsafe { std::mem::zeroed() };
    if fd >= 0 && unsafe { libc::fstat(fd, &mut buf) } == 0 {
        result = FileId::from_stat(&buf);
    }
    result
}

pub fn file_id_for_autoclose_fd(fd: &AutoCloseFd) -> FileId {
    file_id_for_fd(fd.fd())
}

pub fn file_id_for_path(path: &wstr) -> FileId {
    let mut result = INVALID_FILE_ID;
    let path = wcs2zstring(path);
    let mut buf: libc::stat = unsafe { std::mem::zeroed() };
    if unsafe { libc::stat(path.as_ptr(), &mut buf) } == 0 {
        result = FileId::from_stat(&buf);
    }
    result
}

/// Types of files that may be in a directory.
#[derive(Clone, Copy, Eq, PartialEq)]
pub enum DirEntryType {
    fifo = 1, // FIFO file
    chr,      // character device
    dir,      // directory
    blk,      // block device
    reg,      // regular file
    lnk,      // symlink
    sock,     // socket
    whiteout, // whiteout (from BSD)
}

/// An entry returned by dir_iter_t.
#[derive(Default)]
pub struct DirEntry {
    /// File name of this entry.
    pub name: WString,

    /// inode of this entry.
    pub inode: libc::ino_t,

    // Stat buff for this entry, or none if not yet computed.
    stat: Option<libc::stat>,

    // The type of the entry. This is initially none; it may be populated eagerly via readdir()
    // on some filesystems, or later via stat(). If stat() fails, the error is silently ignored
    // and the type is left as none(). Note this is an unavoidable race.
    typ: Option<DirEntryType>,

    // fd of the DIR*, used for fstatat().
    dirfd: RawFd,
}

impl DirEntry {
    /// \return the type of this entry if it is already available, otherwise none().
    pub fn fast_type(&self) -> Option<DirEntryType> {
        self.typ
    }

    /// \return the type of this entry, falling back to stat() if necessary.
    /// If stat() fails because the file has disappeared, this will return none().
    /// If stat() fails because of a broken symlink, this will return type lnk.
    pub fn check_type(&mut self) -> Option<DirEntryType> {
        // Call stat if needed to populate our type, swallowing errors.
        if self.typ.is_none() {
            self.do_stat()
        }
        self.typ
    }

    /// \return whether this is a directory. This may call stat().
    pub fn is_dir(&mut self) -> bool {
        self.check_type() == Some(DirEntryType::dir)
    }

    /// \return the stat buff for this entry, invoking stat() if necessary.
    pub fn stat(&mut self) -> Option<libc::stat> {
        if self.stat.is_none() {
            self.do_stat();
        }
        self.stat
    }

    // Reset our fields.
    fn reset(&mut self) {
        self.name.clear();
        self.inode = unsafe { std::mem::zeroed() };
        self.typ = None;
        self.stat = None;
    }

    // Populate our stat buffer, and type. Errors are silently ignored.
    fn do_stat(&mut self) {
        // We want to set both our type and our stat buffer.
        // If we follow symlinks and stat() errors with a bad symlink, set the type to link, but do not
        // populate the stat buffer.
        if self.dirfd < 0 {
            return;
        }
        let narrow = wcs2zstring(&self.name);
        let mut s: libc::stat = unsafe { std::mem::zeroed() };
        if unsafe { libc::fstatat(self.dirfd, narrow.as_ptr(), &mut s, 0) } == 0 {
            self.stat = Some(s);
            self.typ = stat_mode_to_entry_type(s.st_mode);
        } else {
            match errno().0 {
                ELOOP => {
                    self.typ = Some(DirEntryType::lnk);
                }
                EACCES | EIO | ENOENT | ENOTDIR | ENAMETOOLONG | ENODEV => {
                    // These are "expected" errors.
                    self.typ = None;
                }
                _ => {
                    self.typ = None;
                    // This used to print an error, but given that we have seen
                    // both ENODEV (above) and ENOTCONN,
                    // and that the error isn't actionable and shows up while typing,
                    // let's not do that.
                    // perror("fstatat");
                }
            }
        }
    }
}

fn dirent_type_to_entry_type(dt: u8) -> Option<DirEntryType> {
    match dt {
        DT_FIFO => Some(DirEntryType::fifo),
        DT_CHR => Some(DirEntryType::chr),
        DT_DIR => Some(DirEntryType::dir),
        DT_BLK => Some(DirEntryType::blk),
        DT_REG => Some(DirEntryType::reg),
        DT_LNK => Some(DirEntryType::lnk),
        DT_SOCK => Some(DirEntryType::sock),
        // todo! whiteout
        _ => None,
    }
}

fn stat_mode_to_entry_type(m: libc::mode_t) -> Option<DirEntryType> {
    match m & S_IFMT {
        S_IFIFO => Some(DirEntryType::fifo),
        S_IFCHR => Some(DirEntryType::chr),
        S_IFDIR => Some(DirEntryType::dir),
        S_IFBLK => Some(DirEntryType::blk),
        S_IFREG => Some(DirEntryType::reg),
        S_IFLNK => Some(DirEntryType::lnk),
        S_IFSOCK => Some(DirEntryType::sock),
        _ => {
            // todo! whiteout
            None
        }
    }
}

/// Class for iterating over a directory, wrapping readdir().
/// This allows enumerating the contents of a directory, exposing the file type if the filesystem
/// itself exposes that from readdir(). stat() is incurred only if necessary: if the entry is a
/// symlink, or if the caller asks for the stat buffer.
/// Symlinks are followed.
pub struct DirIter {
    /// Whether this dir_iter considers the "." and ".." filesystem entries.
    withdot: bool,

    dir: *mut libc::DIR,
    error: libc::c_int,
    entry: DirEntry,
}

impl DirIter {
    /// Open a directory at a given path. On failure, \p error() will return the error code.
    /// Note opendir is guaranteed to set close-on-exec by POSIX (hooray).
    pub fn new(path: &wstr) -> Self {
        Self::new_impl(path, false)
    }
    pub fn with_dot(path: &wstr) -> Self {
        Self::new_impl(path, true)
    }
    fn new_impl(path: &wstr, withdot: bool) -> Self {
        let mut error = 0;
        let dir = wopendir(path);
        if dir.is_null() {
            error = errno().0;
        }
        let entry = DirEntry {
            dirfd: unsafe { libc::dirfd(dir) },
            ..Default::default()
        };
        DirIter {
            withdot,
            dir,
            error,
            entry,
        }
    }

    /// \return the errno value for the last error, or 0 if none.
    pub fn error(&self) -> libc::c_int {
        self.error
    }

    /// \return if we are valid: successfully opened a directory.
    pub fn valid(&self) -> bool {
        !self.dir.is_null()
    }

    /// \return the underlying file descriptor, or -1 if invalid.
    pub fn fd(&self) -> RawFd {
        if self.dir.is_null() {
            -1
        } else {
            unsafe { libc::dirfd(self.dir) }
        }
    }

    /// Rewind the directory to the beginning.
    pub fn rewind(&mut self) {
        if self.dir.is_null() {
            unsafe { libc::rewinddir(self.dir) };
        }
    }

    pub fn next(&mut self) -> Option<&mut DirEntry> {
        if self.dir.is_null() {
            return None;
        }
        set_errno(Errno(0));
        let dent = unsafe { libc::readdir(self.dir) };
        if dent.is_null() {
            self.error = errno().0;
            return None;
        }
        let dent = unsafe { &*dent };
        // Skip . and ..,
        // unless we've been told not to.
        if !self.withdot
            && [
                &[b'.' as i8, b'\0' as i8, b'\0' as i8][..],
                &[b'.' as i8, b'.' as i8, b'\0' as i8][..],
            ]
            .contains(&&dent.d_name[..3])
        {
            return self.next();
        }

        self.entry.reset();
        let d_name: Vec<u8> = dent.d_name.iter().map(|b| *b as u8).collect();
        self.entry.name = cstr2wcstring(&d_name);
        #[cfg(any(target_os = "freebsd", target_os = "netbsd", target_os = "openbsd"))]
        {
            self.entry.inode = dent.d_fileno;
        }
        #[cfg(not(any(target_os = "freebsd", target_os = "netbsd", target_os = "openbsd")))]
        {
            self.entry.inode = dent.d_ino;
        }
        let typ = dirent_type_to_entry_type(dent.d_type);
        // Do not store symlinks as we will need to resolve them.
        if typ != Some(DirEntryType::lnk) {
            self.entry.typ = typ;
        }

        Some(&mut self.entry)
    }
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
fn test_wstr_offset_in() {
    use crate::wchar::L;
    let base = L!("hello world");
    assert_eq!(wstr_offset_in(&base[6..], base), 6);
    assert_eq!(wstr_offset_in(&base[0..], base), 0);
    assert_eq!(wstr_offset_in(&base[6..], &base[6..]), 0);
    assert_eq!(wstr_offset_in(&base[base.len()..], base), base.len());
}
