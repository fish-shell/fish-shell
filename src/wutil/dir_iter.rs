use super::wopendir;
use crate::common::{bytes2wcstring, wcs2zstring};
use crate::wchar::{WString, wstr};
use crate::wutil::DevInode;
use cfg_if::cfg_if;
use libc::{
    DT_BLK, DT_CHR, DT_DIR, DT_FIFO, DT_LNK, DT_REG, DT_SOCK, EACCES, EIO, ELOOP, ENAMETOOLONG,
    ENODEV, ENOENT, ENOTDIR, S_IFBLK, S_IFCHR, S_IFDIR, S_IFIFO, S_IFLNK, S_IFMT, S_IFREG,
    S_IFSOCK,
};
use std::cell::Cell;
use std::io;
use std::mem::MaybeUninit;
use std::os::fd::RawFd;
use std::ptr::{NonNull, addr_of};
use std::rc::Rc;

/// Types of files that may be in a directory.
#[derive(Clone, Copy, Debug, Eq, PartialEq)]
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

/// An entry returned by DirIter.
#[derive(Clone)]
pub struct DirEntry {
    /// File name of this entry.
    pub name: WString,

    /// inode of this entry.
    pub inode: libc::ino_t,

    // Device, inode pair for this entry, or none if not yet computed.
    dev_inode: Cell<Option<DevInode>>,

    // The type of the entry. This is initially none; it may be populated eagerly via readdir()
    // on some filesystems, or later via stat(). If stat() fails, the error is silently ignored
    // and the type is left as none(). Note this is an unavoidable race.
    typ: Cell<Option<DirEntryType>>,

    // whether this could be a link, false if we know definitively it isn't.
    possible_link: Option<bool>,

    // fd of the DIR*, used for fstatat().
    dirfd: Rc<DirFd>,
}

impl DirEntry {
    /// Return the type of this entry if it is already available, otherwise none().
    pub fn fast_type(&self) -> Option<DirEntryType> {
        self.typ.get()
    }

    /// Return the type of this entry, falling back to stat() if necessary.
    /// If stat() fails because the file has disappeared, this will return none().
    /// If stat() fails because of a broken symlink, this will return type lnk.
    pub fn check_type(&self) -> Option<DirEntryType> {
        // Call stat if needed to populate our type, swallowing errors.
        if self.typ.get().is_none() {
            self.do_stat()
        }
        self.typ.get()
    }

    /// Return whether this is a directory. This may call stat().
    pub fn is_dir(&self) -> bool {
        self.check_type() == Some(DirEntryType::dir)
    }

    /// Return false if we know this can't be a link via d_type, true if it could be.
    pub fn is_possible_link(&self) -> Option<bool> {
        self.possible_link
    }
    /// Return the device, inode pair for this entry, invoking stat() if necessary.
    pub fn dev_inode(&self) -> Option<DevInode> {
        if self.dev_inode.get().is_none() {
            self.do_stat();
        }
        self.dev_inode.get()
    }

    // Reset our fields.
    fn reset(&mut self) {
        self.name.clear();
        self.inode = unsafe { std::mem::zeroed() };
        self.typ.set(None);
        self.dev_inode.set(None);
    }

    // Populate our stat buffer, and type. Errors are silently ignored.
    fn do_stat(&self) {
        // We want to set both our type and our stat buffer.
        // If we follow symlinks and stat() errors with a bad symlink, set the type to link, but do not
        // populate the stat buffer.
        let fd = self.dirfd.fd();
        if fd < 0 {
            return;
        }
        let narrow = wcs2zstring(&self.name);
        let mut s = MaybeUninit::uninit();
        if unsafe { libc::fstatat(fd, narrow.as_ptr(), s.as_mut_ptr(), 0) } == 0 {
            let s = unsafe { s.assume_init() };
            // st_dev is a dev_t, which is i32 on OpenBSD/Haiku and u32 in FreeBSD 11
            #[allow(clippy::unnecessary_cast)]
            let dev_inode = DevInode {
                device: s.st_dev as u64,
                inode: s.st_ino as u64,
            };
            self.dev_inode.set(Some(dev_inode));
            self.typ.set(stat_mode_to_entry_type(s.st_mode));
        } else {
            match errno::errno().0 {
                ELOOP => {
                    self.typ.set(Some(DirEntryType::lnk));
                }
                EACCES | EIO | ENOENT | ENOTDIR | ENAMETOOLONG | ENODEV => {
                    // These are "expected" errors.
                    self.typ.set(None);
                }
                _ => {
                    self.typ.set(None);
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
        // todo!("whiteout")
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
            // todo!("whiteout")
            None
        }
    }
}

struct DirFd(NonNull<libc::DIR>);

impl DirFd {
    /// Return the underlying file descriptor.
    #[inline]
    fn fd(&self) -> RawFd {
        unsafe { libc::dirfd(self.dir()) }
    }

    /// Return the underlying DIR*.
    #[inline]
    fn dir(&self) -> *mut libc::DIR {
        self.0.as_ptr()
    }
}

/// Autoclose wrapper for DIR*.
impl Drop for DirFd {
    fn drop(&mut self) {
        unsafe {
            let _ = libc::closedir(self.dir());
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

    /// A reference to the underlying directory fd.
    dir: Rc<DirFd>,

    /// The storage for our entry. This allows us to iterate without allocating.
    entry: DirEntry,
}

impl DirIter {
    /// Open a directory at a given path.
    /// Note opendir is guaranteed to set close-on-exec by POSIX (hooray).
    pub fn new(path: &wstr) -> io::Result<Self> {
        Self::new_impl(path, false)
    }

    pub fn new_with_dots(path: &wstr) -> io::Result<Self> {
        Self::new_impl(path, true)
    }

    fn new_impl(path: &wstr, withdot: bool) -> io::Result<Self> {
        let dir: *mut libc::DIR = wopendir(path);
        let Some(dir) = NonNull::new(dir) else {
            return Err(io::Error::last_os_error());
        };
        let dir = Rc::new(DirFd(dir));
        let entry = DirEntry {
            name: WString::new(),
            inode: 0,
            dev_inode: Cell::new(None),
            typ: Cell::new(None),
            dirfd: dir.clone(),
            possible_link: None,
        };
        Ok(DirIter {
            withdot,
            dir,
            entry,
        })
    }

    /// Return the underlying file descriptor.
    pub fn fd(&self) -> RawFd {
        self.dir.fd()
    }

    /// Rewind the directory to the beginning. This cannot fail.
    pub fn rewind(&mut self) {
        unsafe { libc::rewinddir(self.dir.dir()) };
    }

    /// Read the next entry in the directory.
    /// This returns an error if readir errors, or Ok(None) if there are no more entries; else an Ok entry.
    /// This is slightly more efficient than the Iterator version, as it avoids allocating.
    #[allow(clippy::should_implement_trait)]
    pub fn next(&mut self) -> Option<io::Result<&DirEntry>> {
        errno::set_errno(errno::Errno(0));
        let dent = unsafe { libc::readdir(self.dir.dir()).as_ref() };
        let Some(dent) = dent else {
            // readdir distinguishes between EOF and error via errno.
            let err = errno::errno().0;
            if err == 0 {
                return None;
            } else {
                return Some(Err(io::Error::from_raw_os_error(err)));
            }
        };

        // dent.d_name is c_char; pretend it's u8.
        assert!(std::mem::size_of::<libc::c_char>() == std::mem::size_of::<u8>());

        // Do not rely on `libc::dirent::d_name.len()` as dirent names may exceed
        // the nominal buffer size; instead use the terminating nul byte.
        // TODO: This should use &raw from Rust 1.82 on
        // https://github.com/rust-lang/libc/issues/2669
        // https://github.com/fish-shell/fish-shell/issues/11221
        let d_name_ptr = addr_of!(dent.d_name);
        let d_name = unsafe { std::ffi::CStr::from_ptr(d_name_ptr.cast()) }.to_bytes();

        // Skip . and ..,
        // unless we've been told not to.
        if !self.withdot && (d_name == b"." || d_name == b"..") {
            return self.next();
        }

        self.entry.reset();
        self.entry.name = bytes2wcstring(d_name);
        cfg_if!(
            if #[cfg(bsd)] {
                self.entry.inode = dent.d_fileno;
            } else {
                self.entry.inode = dent.d_ino;
            }
        );
        let typ = dirent_type_to_entry_type(dent.d_type);
        // Do not store symlinks as we will need to resolve them.
        if typ != Some(DirEntryType::lnk) {
            self.entry.typ.set(typ);
        }
        // This entry could be a link if it is a link or unknown.
        self.entry.possible_link = typ.map(|t| t == DirEntryType::lnk);

        Some(Ok(&self.entry))
    }
}

impl IntoIterator for DirIter {
    type Item = io::Result<DirEntry>;
    type IntoIter = Iter;
    fn into_iter(self) -> Self::IntoIter {
        Iter(self)
    }
}

/// A convenient iterator over the entries in a directory.
/// This differs from DirIter::next() in that it allocates.
pub struct Iter(DirIter);
impl Iterator for Iter {
    type Item = io::Result<DirEntry>;
    fn next(&mut self) -> Option<Self::Item> {
        match self.0.next()? {
            Ok(entry) => Some(Ok(entry.clone())),
            Err(e) => Some(Err(e)),
        }
    }
}

#[test]
fn test_dir_iter_bad_path() {
    // Regression test: DirIter does not crash given a bad path.
    use crate::wchar::L;
    let dir = DirIter::new(L!("/a/bogus/path/which/does/notexist"));
    assert!(dir.is_err());
}

#[test]
fn test_no_dots() {
    use crate::wchar::L;
    // DirIter does not return . or .. by default.
    let dir = DirIter::new(L!(".")).expect("Should be able to open CWD");
    for entry in dir {
        let entry = entry.unwrap();
        assert_ne!(entry.name, ".");
        assert_ne!(entry.name, "..");
    }
}

#[test]
fn test_dots() {
    use crate::wchar::L;
    // DirIter returns . or .. if you ask nicely.
    let dir = DirIter::new_with_dots(L!(".")).expect("Should be able to open CWD");
    let mut seen_dot = false;
    let mut seen_dotdot = false;
    for entry in dir {
        let entry = entry.unwrap();
        if entry.name == "." {
            seen_dot = true;
        } else if entry.name == ".." {
            seen_dotdot = true;
        }
    }
    assert!(seen_dot);
    assert!(seen_dotdot);
}

// Test ported from C++.
#[test]
#[allow(clippy::if_same_then_else)]
fn test_dir_iter() {
    use crate::common::charptr2wcstring;
    use crate::common::wcs2osstring;
    use crate::wchar::L;
    #[cfg(not(cygwin))]
    use libc::symlink;
    use libc::{O_CREAT, O_WRONLY, close, mkfifo, open};
    use std::ffi::CString;

    let baditer = DirIter::new(L!("/definitely/not/a/valid/directory/for/sure"));
    assert!(baditer.is_err());
    let Err(err) = baditer else {
        panic!("Expected error");
    };
    let err = err.raw_os_error().expect("Should have an errno value");
    assert!(err == ENOENT || err == EACCES);

    let mut t1: [u8; 31] = *b"/tmp/fish_test_dir_iter.XXXXXX\0";
    let basepath_narrow = unsafe { libc::mkdtemp(t1.as_mut_ptr().cast()) };
    assert!(!basepath_narrow.is_null(), "mkdtemp failed");
    let basepath: WString = charptr2wcstring(basepath_narrow);

    let makepath = |s: &str| -> CString {
        let mut tmp = basepath.clone();
        tmp.push('/');
        tmp.push_str(s);
        wcs2zstring(&tmp)
    };

    let dirname = "dir";
    let regname = "reg";
    let reglinkname = "reglink"; // link to regular file
    let dirlinkname = "dirlink"; // link to directory
    let badlinkname = "badlink"; // link to nowhere
    let selflinkname = "selflink"; // link to self
    let fifoname = "fifo";
    #[rustfmt::skip]
    let names = if cfg!(not(cygwin)) {
        vec![
            dirname, regname, reglinkname, dirlinkname,
            badlinkname, selflinkname, fifoname,
        ]
    } else {
        // Symbolic links on Windows are complicated. Their behavior depends
        // on the CYGWIN or MSYS env variable. So we skip that part of the test
        vec![dirname, regname, fifoname]
    };

    #[cfg(not(cygwin))]
    let is_link_name = |name: &wstr| -> bool {
        name == reglinkname || name == dirlinkname || name == badlinkname || name == selflinkname
    };

    // Make our different file types
    unsafe {
        let mut ret = libc::mkdir(makepath(dirname).as_ptr(), 0o700);
        assert!(ret == 0);
        ret = open(makepath(regname).as_ptr(), O_CREAT | O_WRONLY, 0o600);
        assert!(ret >= 0);
        close(ret);
        #[cfg(not(cygwin))]
        {
            ret = symlink(makepath(regname).as_ptr(), makepath(reglinkname).as_ptr());
            assert!(ret == 0);
            ret = symlink(makepath(dirname).as_ptr(), makepath(dirlinkname).as_ptr());
            assert!(ret == 0);
            ret = symlink(
                c"/this/is/an/invalid/path".as_ptr().cast(),
                makepath(badlinkname).as_ptr(),
            );
            assert!(ret == 0);
            ret = symlink(
                makepath(selflinkname).as_ptr(),
                makepath(selflinkname).as_ptr(),
            );
            assert!(ret == 0);
        }

        ret = mkfifo(makepath(fifoname).as_ptr(), 0o600);
        assert!(ret == 0);
    }

    let mut iter1 = DirIter::new(&basepath).expect("Should be able to open directory");
    let mut seen = 0;
    while let Some(entry) = iter1.next() {
        let entry = entry.expect("Should not have gotten error");
        seen += 1;
        assert!(entry.name != "." && entry.name != "..");
        assert!(names.iter().any(|&n| entry.name == n));

        let expected = if entry.name == dirname {
            Some(DirEntryType::dir)
        } else if entry.name == regname {
            Some(DirEntryType::reg)
        } else if entry.name == reglinkname {
            Some(DirEntryType::reg)
        } else if entry.name == dirlinkname {
            Some(DirEntryType::dir)
        } else if entry.name == badlinkname {
            None
        } else if entry.name == selflinkname {
            Some(DirEntryType::lnk)
        } else if entry.name == fifoname {
            Some(DirEntryType::fifo)
        } else {
            panic!("Unexpected file type");
        };

        // Links should never have a fast type if we are resolving them, since we cannot resolve a
        // symlink from readdir.
        #[cfg(not(cygwin))]
        if is_link_name(&entry.name) {
            assert!(entry.fast_type().is_none());
        }
        // If we have a fast type, it should be correct.
        assert!(entry.fast_type().is_none() || entry.fast_type() == expected);
        assert!(
            entry.check_type() == expected,
            "Wrong type for {}. Expected {:?}, got {:?}",
            entry.name,
            expected,
            entry.check_type()
        );
    }
    assert_eq!(seen, names.len());

    // Clean up.
    let _ = std::fs::remove_dir_all(wcs2osstring(&basepath));
}
