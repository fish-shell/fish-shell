use crate::common::wcs2zstring;
use crate::wutil::{fstat, wstr};
use std::ffi::{CStr, OsStr};
use std::fs::{self, Metadata};
use std::os::fd::AsRawFd;
use std::os::unix::prelude::*;

/// Struct for representing a file's inode. We use this to detect and avoid symlink loops, among
/// other things.
#[derive(Debug, Copy, Clone, PartialEq, Eq, PartialOrd, Ord, Hash)]
pub struct DevInode {
    pub device: u64,
    pub inode: u64,
}

/// While an inode / dev pair is sufficient to distinguish co-existing files, Linux
/// seems to aggressively re-use inodes, so it cannot determine if a file has been deleted
/// (ABA problem). Therefore we include richer information to detect file changes.
#[derive(Debug, Clone, Eq, Hash, Ord, PartialEq, PartialOrd)]
pub struct FileId {
    pub dev_inode: DevInode,
    pub size: u64,
    pub change_seconds: i64,
    pub change_nanoseconds: i64,
    pub mod_seconds: i64,
    pub mod_nanoseconds: i64,
}

impl FileId {
    pub fn from_md(buf: &Metadata) -> Self {
        // These "into()" calls are because the various fields have different types
        // on different platforms.
        #[allow(clippy::useless_conversion)]
        FileId {
            dev_inode: DevInode {
                device: buf.dev(),
                inode: buf.ino(),
            },
            size: buf.size(),
            change_seconds: buf.ctime().into(),
            change_nanoseconds: buf.ctime_nsec().into(),
            mod_seconds: buf.mtime().into(),
            mod_nanoseconds: buf.mtime_nsec().into(),
        }
    }

    #[allow(clippy::unnecessary_cast)] // platform-dependent
    pub fn from_stat(buf: &libc::stat) -> FileId {
        let device = buf.st_dev as _;
        let inode = buf.st_ino as _;
        let size = buf.st_size as _;
        let change_seconds = buf.st_ctime as _;
        let mod_seconds = buf.st_mtime as _;

        let change_nanoseconds;
        let mod_nanoseconds;

        #[cfg(not(target_os = "netbsd"))]
        {
            change_nanoseconds = buf.st_ctime_nsec as _;
            mod_nanoseconds = buf.st_mtime_nsec as _;
        }
        #[cfg(target_os = "netbsd")]
        {
            change_nanoseconds = buf.st_ctimensec as _;
            mod_nanoseconds = buf.st_mtimensec as _;
        }
        FileId {
            dev_inode: DevInode { device, inode },
            size,
            change_seconds,
            change_nanoseconds,
            mod_seconds,
            mod_nanoseconds,
        }
    }

    /// Return true if \param rhs has higher mtime seconds than this file_id_t.
    /// If identical, nanoseconds are compared.
    pub fn older_than(&self, rhs: &FileId) -> bool {
        let lhs = (self.mod_seconds, self.mod_nanoseconds);
        let rhs = (rhs.mod_seconds, rhs.mod_nanoseconds);
        lhs.cmp(&rhs).is_lt()
    }
}

pub const INVALID_FILE_ID: FileId = FileId {
    dev_inode: DevInode {
        device: u64::MAX,
        inode: u64::MAX,
    },
    size: u64::MAX,
    change_seconds: i64::MIN,
    change_nanoseconds: i64::MIN,
    mod_seconds: i64::MIN,
    mod_nanoseconds: i64::MIN,
};

/// Get a FileID corresponding to a raw file descriptor, or INVALID_FILE_ID if it fails.
pub fn file_id_for_fd(fd: impl AsRawFd) -> FileId {
    fstat(fd)
        .as_ref()
        .map(FileId::from_md)
        .unwrap_or(INVALID_FILE_ID)
}

/// Get a FileID corresponding to a path, or INVALID_FILE_ID if it fails.
pub fn file_id_for_path(path: &wstr) -> FileId {
    file_id_for_path_narrow(&wcs2zstring(path))
}

pub fn file_id_for_path_narrow(path: &CStr) -> FileId {
    let path = OsStr::from_bytes(path.to_bytes());
    fs::metadata(path)
        .as_ref()
        .map(FileId::from_md)
        .unwrap_or(INVALID_FILE_ID)
}
