//! Implementation of the YAML-like history file format.

use std::{
    fs::File,
    io::Read,
    ops::{Deref, DerefMut},
    os::fd::AsRawFd,
    time::{SystemTime, UNIX_EPOCH},
};

use libc::{ENODEV, MAP_ANONYMOUS, MAP_FAILED, MAP_PRIVATE, PROT_READ, PROT_WRITE};

use super::HistoryItem;
use super::yaml_backend::{
    decode_item_fish_2_0, escape_yaml_fish_2_0, offset_of_next_item_fish_2_0,
};
use crate::{
    common::wcs2bytes,
    flog::flog,
    path::{DirRemoteness, path_get_data_remoteness},
    wutil::FileId,
};

/// History file types.
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum HistoryFileType {
    Fish1_x, // old format with just timestamp and item
    Fish2_0, // YAML-style format
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

    /// Map a region `[0, len)` from a locked file.
    pub fn map_file(file: &File, len: usize) -> std::io::Result<Self> {
        let ptr = unsafe {
            libc::mmap(
                std::ptr::null_mut(),
                len,
                PROT_READ,
                MAP_PRIVATE,
                file.as_raw_fd(),
                0,
            )
        };

        if ptr == MAP_FAILED {
            return Err(std::io::Error::last_os_error());
        }

        // SAFETY: mmap of `len` was successful and returned `ptr`
        Ok(unsafe { Self::new(ptr.cast(), len) })
    }

    /// Map anonymous memory of a given length.
    pub fn map_anon(len: usize) -> std::io::Result<Self> {
        let ptr = unsafe {
            libc::mmap(
                std::ptr::null_mut(),
                len,
                PROT_READ | PROT_WRITE,
                MAP_PRIVATE | MAP_ANONYMOUS,
                -1,
                0,
            )
        };
        if ptr == MAP_FAILED {
            return Err(std::io::Error::last_os_error());
        }

        // SAFETY: mmap of `len` was successful and returned `ptr`
        Ok(unsafe { Self::new(ptr.cast(), len) })
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
        unsafe { libc::munmap(self.ptr.cast(), self.len) };
    }
}

/// RawHistoryFile holds the read-only contents of a file, before decoding it.
pub struct RawHistoryFile {
    region: MmapRegion,
}

impl RawHistoryFile {
    /// Construct a history file contents from a [`File`] reference and its file id.
    pub fn create(history_file: &File, file_id: FileId) -> std::io::Result<Self> {
        // Check the file size.
        let len: usize = match file_id.size.try_into() {
            Ok(len) => len,
            Err(err) => {
                return Err(std::io::Error::new(
                    std::io::ErrorKind::Unsupported,
                    format!("Cannot convert u64 to usize: {err}"),
                ));
            }
        };
        if len == 0 {
            return Err(std::io::Error::other(
                "History file is empty. Cannot create memory mapping with length 0.",
            ));
        }
        let map_anon = |mut file: &File, len: usize| -> std::io::Result<MmapRegion> {
            let mut region = MmapRegion::map_anon(len)?;
            // If we mapped anonymous memory, we have to read from the file.
            file.read_exact(&mut region)?;
            Ok(region)
        };
        let region = if should_mmap() {
            match MmapRegion::map_file(history_file, len) {
                Ok(region) => region,
                Err(err) => {
                    if err.raw_os_error() == Some(ENODEV) {
                        // Our mmap failed with ENODEV, which means the underlying
                        // filesystem does not support mapping.
                        // Create an anonymous mapping and read() the file into it.
                        map_anon(history_file, len)?
                    } else {
                        return Err(err);
                    }
                }
            }
        } else {
            map_anon(history_file, len)?
        };

        region.try_into()
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
    fn offset_of_next_item(&self, cursor: &mut usize, cutoff: Option<SystemTime>) -> Option<usize> {
        offset_of_next_item_fish_2_0(self.contents(), cursor, cutoff)
    }

    /// Returns an iterator over item offsets with an optional cutoff time.
    /// If cutoff is given, skip items whose timestamp is newer than cutoff.
    pub fn offsets(&self, cutoff: Option<SystemTime>) -> impl Iterator<Item = usize> + '_ {
        HistoryFileOffsetIter {
            contents: self,
            cursor: 0,
            cutoff,
        }
    }

    /// Returns a view of the file contents.
    pub fn contents(&self) -> &[u8] {
        &self.region
    }

    /// Decode this history file.
    /// If cutoff is given, skip items whose timestamp is newer than cutoff.
    pub fn decode(self, cutoff: Option<SystemTime>) -> HistoryFile {
        let offsets = self.offsets(cutoff).collect();
        HistoryFile {
            contents: Some(self),
            offsets,
        }
    }
}

/// A combination of a history file and its offsets.
pub struct HistoryFile {
    // Contents of the file. May be None if there was no file.
    contents: Option<RawHistoryFile>,
    // Offsets of items within the file. Always empty if contents is None.
    offsets: Vec<usize>,
}

impl HistoryFile {
    /// Create an empty history file.
    pub fn create_empty() -> Self {
        Self {
            contents: None,
            offsets: Vec::new(),
        }
    }

    /// Return whether this file is empty.
    pub fn is_empty(&self) -> bool {
        self.offsets.is_empty()
    }

    /// Return the offsets of items in this file.
    pub fn offsets(&self) -> &[usize] {
        &self.offsets
    }

    /// Decode an item at a given offset.
    pub fn decode_item(&self, offset: usize) -> Option<HistoryItem> {
        self.contents.as_ref()?.decode_item(offset)
    }
}

/// Iterator over offsets within a history file.
struct HistoryFileOffsetIter<'a> {
    // The file contents.
    contents: &'a RawHistoryFile,
    // Current offset within the file.
    cursor: usize,
    // Optional cutoff time. If given, skip items newer than this.
    cutoff: Option<SystemTime>,
}

impl<'a> Iterator for HistoryFileOffsetIter<'a> {
    type Item = usize;

    fn next(&mut self) -> Option<Self::Item> {
        self.contents
            .offset_of_next_item(&mut self.cursor, self.cutoff)
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

impl TryFrom<MmapRegion> for RawHistoryFile {
    type Error = std::io::Error;

    fn try_from(region: MmapRegion) -> std::io::Result<Self> {
        let type_ = infer_file_type(&region);
        if type_ == HistoryFileType::Fish1_x {
            let error_message = "unsupported history file format 1.x";
            flog!(error, error_message);
            return Err(std::io::Error::new(
                std::io::ErrorKind::InvalidInput,
                error_message,
            ));
        }
        Ok(Self { region })
    }
}

impl HistoryItem {
    /// Write this history item to some writer.
    pub fn write_to(&self, writer: &mut impl std::io::Write) -> std::io::Result<()> {
        assert!(self.should_write_to_disk(), "Item should not be persisted");

        let mut cmd = wcs2bytes(self.str());
        escape_yaml_fish_2_0(&mut cmd);
        writer.write_all(b"- cmd: ")?;
        writer.write_all(&cmd)?;
        writer.write_all(b"\n")?;
        writeln!(writer, "  when: {}", time_to_seconds(self.timestamp()))?;

        let paths = self.get_required_paths();
        if !paths.is_empty() {
            writeln!(writer, "  paths:")?;
            for path in paths {
                let mut path = wcs2bytes(path);
                escape_yaml_fish_2_0(&mut path);
                writer.write_all(b"    - ")?;
                writer.write_all(&path)?;
                writer.write_all(b"\n")?;
            }
        }
        Ok(())
    }
}

/// Check if we should mmap the file.
/// Don't try mmap() on non-local filesystems.
fn should_mmap() -> bool {
    // mmap only if we are known not-remote.
    path_get_data_remoteness() != DirRemoteness::Remote
}

pub fn time_to_seconds(ts: SystemTime) -> i64 {
    match ts.duration_since(UNIX_EPOCH) {
        Ok(d) => {
            // after epoch
            i64::try_from(d.as_secs()).unwrap()
        }
        Err(e) => {
            // before epoch
            -i64::try_from(e.duration().as_secs()).unwrap()
        }
    }
}
