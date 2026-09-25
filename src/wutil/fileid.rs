use crate::wutil::wstr;
use fish_widestring::wcs2zstring;
use std::{
    ffi::{CStr, OsStr},
    fs::{self, File, Metadata},
    os::unix::prelude::*,
};

/// Struct for representing a file's inode. We use this to detect and avoid symlink loops, among
/// other things.
#[derive(Debug, Copy, Clone, Eq, PartialEq, PartialOrd, Hash)]
pub struct DevInode {
    pub device: u64,
    pub inode: u64,
}

/// While an inode / dev pair is sufficient to distinguish co-existing files, Linux
/// seems to aggressively reuse inodes, so it cannot determine if a file has been deleted
/// (ABA problem). Therefore we include richer information to detect file changes.
#[derive(Debug, Clone, Hash, PartialEq, PartialOrd)]
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
}

/// Get a FileId corresponding to a `file`, or `None` if it fails.
pub fn file_id_for_file(file: &File) -> Option<FileId> {
    file.metadata().ok().as_ref().map(FileId::from_md)
}

/// Get a FileId corresponding to a `path`, or `None` if it fails.
pub fn file_id_for_path(path: &wstr) -> Option<FileId> {
    file_id_for_path_narrow(&wcs2zstring(path))
}

pub fn file_id_for_path_narrow(path: &CStr) -> Option<FileId> {
    let path = OsStr::from_bytes(path.to_bytes());
    fs::metadata(path).ok().as_ref().map(FileId::from_md)
}
