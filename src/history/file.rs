//! Implemention of history files.

use std::{
    fs::File,
    io::{Read, Seek, SeekFrom, Write},
    ops::{Deref, DerefMut},
    os::fd::{AsRawFd, RawFd},
    time::SystemTime,
};

use libc::{mmap, munmap, MAP_ANONYMOUS, MAP_FAILED, MAP_PRIVATE, PROT_READ, PROT_WRITE};

use super::HistoryItem;
use crate::{
    common::wcs2string,
    flog::FLOG,
    path::{path_get_config_remoteness, DirRemoteness},
    yaml::{
        decode_item_fish_2_0, escape_yaml_fish_2_0, offset_of_next_item_fish_2_0, time_to_seconds,
    },
};

/// History file types.
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum HistoryFileType {
    Fish2_0,
    Fish1_x,
}

/// A type wrapping up the logic around mmap and munmap.
struct MmapRegion {
    ptr: *mut u8,
    len: usize,
}

impl MmapRegion {
    /// Creates a new mmap'ed region.
    ///
    /// # Safety
    ///
    /// `ptr` must be the result of a successful `mmap()` call with length `len`.
    unsafe fn new(ptr: *mut u8, len: usize) -> Self {
        assert!(ptr.cast() != MAP_FAILED);
        assert!(len > 0);
        Self { ptr, len }
    }

    /// Map a region `[0, len)` from an `fd`.
    /// Returns [`None`] on failure.
    pub fn map_file(fd: RawFd, len: usize) -> Option<Self> {
        if len == 0 {
            return None;
        }

        let ptr = unsafe { mmap(std::ptr::null_mut(), len, PROT_READ, MAP_PRIVATE, fd, 0) };
        if ptr == MAP_FAILED {
            return None;
        }

        // SAFETY: mmap of `len` was successful and returned `ptr`
        Some(unsafe { Self::new(ptr.cast(), len) })
    }

    /// Map anonymous memory of a given length.
    /// Returns [`None`] on failure.
    pub fn map_anon(len: usize) -> Option<Self> {
        if len == 0 {
            return None;
        }

        let ptr = unsafe {
            mmap(
                std::ptr::null_mut(),
                len,
                PROT_READ | PROT_WRITE,
                MAP_PRIVATE | MAP_ANONYMOUS,
                -1,
                0,
            )
        };
        if ptr == MAP_FAILED {
            return None;
        }

        // SAFETY: mmap of `len` was successful and returned `ptr`
        Some(unsafe { Self::new(ptr.cast(), len) })
    }
}

// SAFETY: MmapRegion has exclusive mutable access to the region
unsafe impl Send for MmapRegion {}
// SAFETY: MmapRegion does not offer interior mutability
unsafe impl Sync for MmapRegion {}

impl Deref for MmapRegion {
    type Target = [u8];

    fn deref(&self) -> &[u8] {
        unsafe { std::slice::from_raw_parts(self.ptr, self.len) }
    }
}

impl DerefMut for MmapRegion {
    fn deref_mut(&mut self) -> &mut [u8] {
        unsafe { std::slice::from_raw_parts_mut(self.ptr, self.len) }
    }
}

impl Drop for MmapRegion {
    fn drop(&mut self) {
        unsafe { munmap(self.ptr.cast(), self.len) };
    }
}

/// HistoryFileContents holds the read-only contents of a file.
pub struct HistoryFileContents {
    region: MmapRegion,
}

impl HistoryFileContents {
    /// Construct a history file contents from a File reference.
    pub fn create(file: &mut File) -> Option<Self> {
        // Check that the file is seekable, and its size.
        let len: usize = file.seek(SeekFrom::End(0)).ok()?.try_into().ok()?;
        let mmap_file_directly = should_mmap();
        let mut region = if mmap_file_directly {
            MmapRegion::map_file(file.as_raw_fd(), len)
        } else {
            MmapRegion::map_anon(len)
        }?;

        // If we mapped anonymous memory, we have to read from the file.
        if !mmap_file_directly {
            file.seek(SeekFrom::Start(0)).ok()?;
            read_zero_padded(&mut *file, region.as_mut()).ok()?;
        }

        region.try_into().ok()
    }

    /// Decode an item at a given offset.
    pub fn decode_item(&self, offset: usize) -> Option<HistoryItem> {
        let contents = &self.region[offset..];
        decode_item_fish_2_0(contents)
    }

    /// Support for iterating item offsets.
    /// The cursor should initially be 0.
    /// If cutoff is given, skip items whose timestamp is newer than cutoff.
    /// Returns the offset of the next item, or [`None`] on end.
    pub fn offset_of_next_item(
        &self,
        cursor: &mut usize,
        cutoff: Option<SystemTime>,
    ) -> Option<usize> {
        offset_of_next_item_fish_2_0(self.contents(), cursor, cutoff)
    }

    /// Returns a view of the file contents.
    pub fn contents(&self) -> &[u8] {
        &self.region
    }
}

/// Try to infer the history file type based on inspecting the data.
fn infer_file_type(contents: &[u8]) -> HistoryFileType {
    assert!(!contents.is_empty(), "File should never be empty");
    if contents[0] == b'#' {
        HistoryFileType::Fish1_x
    } else {
        // assume new fish
        HistoryFileType::Fish2_0
    }
}

impl TryFrom<MmapRegion> for HistoryFileContents {
    type Error = ();

    fn try_from(region: MmapRegion) -> Result<Self, Self::Error> {
        let type_ = infer_file_type(&region);
        if type_ == HistoryFileType::Fish1_x {
            FLOG!(error, "unsupported history file format 1.x");
            return Err(());
        }
        Ok(Self { region })
    }
}

/// Append a history item to a buffer, in preparation for outputting it to the history file.
pub fn append_history_item_to_buffer(item: &HistoryItem, buffer: &mut Vec<u8>) {
    assert!(item.should_write_to_disk(), "Item should not be persisted");

    let mut cmd = wcs2string(item.str());
    escape_yaml_fish_2_0(&mut cmd);
    buffer.extend(b"- cmd: ");
    buffer.extend(&cmd);
    buffer.push(b'\n');
    writeln!(buffer, "  when: {}", time_to_seconds(item.timestamp())).unwrap();

    let paths = item.get_required_paths();
    if !paths.is_empty() {
        writeln!(buffer, "  paths:").unwrap();
        for path in paths {
            let mut path = wcs2string(path);
            escape_yaml_fish_2_0(&mut path);
            buffer.extend(b"    - ");
            buffer.extend(&path);
            buffer.push(b'\n');
        }
    }
}

/// Check if we should mmap the fd.
/// Don't try mmap() on non-local filesystems.
fn should_mmap() -> bool {
    if super::NEVER_MMAP.load() {
        return false;
    }

    // mmap only if we are known not-remote.
    return path_get_config_remoteness() == DirRemoteness::local;
}

/// Read from `fd` to fill `dest`, zeroing any unused space.
fn read_zero_padded(file: &mut File, mut dest: &mut [u8]) -> std::io::Result<()> {
    while !dest.is_empty() {
        match file.read(dest) {
            Ok(0) => break,
            Ok(amt) => {
                dest = &mut dest[amt..];
            }
            Err(e) if e.kind() == std::io::ErrorKind::Interrupted => continue,
            Err(err) => return Err(err),
        }
    }
    dest.fill(0u8);
    Ok(())
}
