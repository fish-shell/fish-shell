use crate::wutil::{wstat, wstr};
use std::cmp::Ordering;
use std::fs::{File, Metadata};
use std::os::unix::prelude::*;

/// Struct for representing a file's inode. We use this to detect and avoid symlink loops, among
/// other things. While an inode / dev pair is sufficient to distinguish co-existing files, Linux  
/// seems to aggressively re-use inodes, so it cannot determine if a file has been deleted (ABA  
/// problem). Therefore we include richer information.
#[derive(Debug, Copy, Clone, PartialEq, Eq, PartialOrd, Ord)]
pub struct FileId {
    pub device: u64,
    pub inode: u64,
    pub size: u64,
    pub change_seconds: i64,
    pub change_nanoseconds: i64,
    pub mod_seconds: i64,
    pub mod_nanoseconds: i64,
}

impl FileId {
    pub fn from_stat(buf: Metadata) -> Self {
        // These "into()" calls are because the various fields have different types
        // on different platforms.
        #[allow(clippy::useless_conversion)]
        FileId {
            device: buf.dev(),
            inode: buf.ino(),
            size: buf.size(),
            change_seconds: buf.ctime().into(),
            change_nanoseconds: buf.ctime_nsec().into(),
            mod_seconds: buf.mtime().into(),
            mod_nanoseconds: buf.mtime_nsec().into(),
        }
    }

    pub fn older_than(&self, rhs: &FileId) -> bool {
        match (self.change_seconds, self.change_nanoseconds)
            .cmp(&(rhs.change_seconds, rhs.change_nanoseconds))
        {
            Ordering::Less => true,
            Ordering::Equal | Ordering::Greater => false,
        }
    }
}

pub const INVALID_FILE_ID: FileId = FileId {
    device: u64::MAX,
    inode: u64::MAX,
    size: u64::MAX,
    change_seconds: i64::MIN,
    change_nanoseconds: -1,
    mod_seconds: i64::MIN,
    mod_nanoseconds: -1,
};

/// Get a FileID corresponding to a raw file descriptor, or INVALID_FILE_ID if it fails.
pub fn file_id_for_fd(fd: RawFd) -> FileId {
    // Safety: we just want fstat(). Rust makes this stupidly hard.
    // The only way to get fstat from an fd is to use a File as an intermediary,
    // but File assumes ownership; so we have to use into_raw_fd() to release it.
    let file = unsafe { File::from_raw_fd(fd) };
    let res = file
        .metadata()
        .map(FileId::from_stat)
        .unwrap_or(INVALID_FILE_ID);
    let fd2 = file.into_raw_fd();
    assert_eq!(fd, fd2);
    res
}

/// Get a FileID corresponding to a path, or INVALID_FILE_ID if it fails.
pub fn file_id_for_path(path: &wstr) -> FileId {
    wstat(path)
        .map(FileId::from_stat)
        .unwrap_or(INVALID_FILE_ID)
}
