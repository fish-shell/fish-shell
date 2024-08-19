//! Implemention of history files.

use serde::{Deserialize, Serialize};
use std::{
    borrow::Cow,
    fs::File,
    io::{Read, Seek, SeekFrom},
    ops::{Deref, DerefMut},
    os::fd::{AsRawFd, RawFd},
    time::{Duration, SystemTime, UNIX_EPOCH},
};

use libc::{mmap, munmap, ENODEV, MAP_ANONYMOUS, MAP_FAILED, MAP_PRIVATE, PROT_READ, PROT_WRITE};

use super::{HistoryItem, PersistenceMode};
use crate::{
    common::str2wcstring,
    flog::FLOG,
    history::history_impl,
    path::{path_get_config_remoteness, DirRemoteness},
    wchar::WString,
};

/// History file types.
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum HistoryFileType {
    /// JSON format, where each "line" is an individual history item.
    /// See [RFC 7464](https://datatracker.ietf.org/doc/rfc7464/) for details of the format used.
    FishJson,
    Fish2_0,
    Fish1_x,
}

/// Default history file type for new files or re-written ones.
pub const DEFAULT_HISTORY_FILE_TYPE: HistoryFileType = HistoryFileType::FishJson;

/// The record separator that marks the beginning of a history entry.
/// See [RFC 7464](https://datatracker.ietf.org/doc/rfc7464/).
const RECORD_SEPARATOR: u8 = 0x1E;

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
    pub fn map_file(fd: RawFd, len: usize) -> std::io::Result<Self> {
        assert!(len != 0);
        let ptr = unsafe { mmap(std::ptr::null_mut(), len, PROT_READ, MAP_PRIVATE, fd, 0) };
        if ptr == MAP_FAILED {
            return Err(std::io::Error::last_os_error());
        }

        // SAFETY: mmap of `len` was successful and returned `ptr`
        Ok(unsafe { Self::new(ptr.cast(), len) })
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
    type_: HistoryFileType,
}

impl HistoryFileContents {
    /// Construct a history file contents from a File reference.
    pub fn create(file: &mut File) -> Option<Self> {
        // Check that the file is seekable, and its size.
        let len: usize = file.seek(SeekFrom::End(0)).ok()?.try_into().ok()?;
        if len == 0 {
            return None;
        }
        let map_anon = |file: &mut File, len: usize| {
            let mut region = MmapRegion::map_anon(len)?;
            // If we mapped anonymous memory, we have to read from the file.
            file.seek(SeekFrom::Start(0)).ok()?;
            read_zero_padded(&mut *file, region.as_mut()).ok()?;
            Some(region)
        };
        let region = if should_mmap() {
            match MmapRegion::map_file(file.as_raw_fd(), len) {
                Ok(region) => region,
                Err(err) if err.raw_os_error() == Some(ENODEV) => {
                    // Our mmap failed with ENODEV, which means the underlying
                    // filesystem does not support mapping. Treat this as a hint
                    // that the filesystem is remote, and so disable locks for
                    // the history file.
                    super::ABANDONED_LOCKING.store(true);
                    // Create an anonymous mapping and read() the file into it.
                    map_anon(file, len)?
                }
                Err(_err) => return None,
            }
        } else {
            map_anon(file, len)?
        };

        region.try_into().ok()
    }

    /// Decode an item at a given offset.
    pub fn decode_item(&self, offset: usize) -> Option<HistoryItem> {
        let contents = &self.region[offset..];
        match self.get_type() {
            HistoryFileType::FishJson => decode_json_item(contents),
            HistoryFileType::Fish2_0 => decode_item_fish_2_0(contents),
            HistoryFileType::Fish1_x => None,
        }
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
        match self.get_type() {
            HistoryFileType::FishJson => {
                let offset = offset_of_next_item_fish_json(self.contents(), &cursor, cutoff);
                *cursor = offset?;
                offset
            }
            HistoryFileType::Fish2_0 => {
                offset_of_next_item_fish_2_0(self.contents(), cursor, cutoff)
            }
            HistoryFileType::Fish1_x => None,
        }
    }

    /// Returns a view of the file contents.
    pub fn contents(&self) -> &[u8] {
        &self.region
    }

    /// Returns the file type of these contents. If empty, [`DEFAULT_HISTORY_FILE_TYPE`] is used.
    pub fn get_type(&self) -> HistoryFileType {
        self.type_
    }
}

/// Try to infer the history file type based on inspecting the data.
fn infer_file_type(contents: &[u8]) -> HistoryFileType {
    assert!(!contents.is_empty(), "File should never be empty");
    match contents[0] {
        b'#' => HistoryFileType::Fish1_x,
        RECORD_SEPARATOR => HistoryFileType::FishJson,
        _ => HistoryFileType::Fish2_0,
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
        Ok(Self { region, type_ })
    }
}

#[derive(Serialize, Deserialize)]
struct JsonHistoryItem {
    cmd: String,
    ts: i64,
    paths: Vec<String>,
}

/// Append a history item to a buffer, in preparation for outputting it to the history file.
/// Always outputs in JSON format.
pub fn append_history_item_to_buffer(item: &HistoryItem, buffer: &mut Vec<u8>) {
    assert!(item.should_write_to_disk(), "Item should not be persisted");

    let json_item = JsonHistoryItem {
        cmd: item.str().to_string(),
        ts: time_to_seconds(item.timestamp()),
        paths: item
            .get_required_paths()
            .iter()
            .map(|p| p.to_string())
            .collect(),
    };

    buffer.push(RECORD_SEPARATOR); // Record separator, as required by rfc7464.
    buffer.extend(serde_json::to_vec(&json_item).unwrap().iter());
    buffer.push(b'\n');
}

fn decode_json_item(data: &[u8]) -> Option<HistoryItem> {
    let json_item: JsonHistoryItem = serde_json::from_slice(trim_leading_separators(data)).ok()?;
    let paths: Vec<WString> = json_item
        .paths
        .iter()
        .map(|s| str2wcstring(s.trim().as_bytes()))
        .collect();
    let mut item = HistoryItem::new(
        str2wcstring(&json_item.cmd.as_bytes()),
        time_from_seconds(json_item.ts),
        0,
        PersistenceMode::Disk,
    );
    item.set_required_paths(paths);

    Some(item)
}

/// Returns a view of the `data` slice that has all `RECORD_SEPARATOR` characters removed.
fn trim_leading_separators(data: &[u8]) -> &[u8] {
    if data.len() > 0 && data[0] == RECORD_SEPARATOR {
        trim_leading_separators(&data[1..])
    } else {
        data
    }
}

/// Check if we should mmap the fd.
/// Don't try mmap() on non-local filesystems.
fn should_mmap() -> bool {
    if history_impl::NEVER_MMAP.load() {
        return false;
    }

    // mmap only if we are known not-remote.
    path_get_config_remoteness() != DirRemoteness::remote
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

#[inline(always)]
/// Unescapes the fish-specific yaml variant, if it requires it.
fn maybe_unescape_yaml_fish_2_0(s: &[u8]) -> Cow<[u8]> {
    // This is faster than s.contains(b'\\') and can be auto-vectorized to SIMD. See benchmark note
    // on unescape_yaml_fish_2_0().
    if !s.iter().copied().fold(false, |acc, b| acc | (b == b'\\')) {
        return s.into();
    }
    unescape_yaml_fish_2_0(s).into()
}

/// Unescapes the fish-specific yaml variant. Use [`maybe_unescape_yaml_fish_2_0()`] if you're not
/// positive the input contains an escape.

// This function is called on every input event and shows up heavily in all flamegraphs.
// Various approaches were benchmarked against real-world fish-history files on lines with escapes,
// and this implementation (chunk_loop_box) won out. Make changes with care!
//
// Benchmarks and code: https://github.com/mqudsi/fish-yaml-unescape-benchmark
fn unescape_yaml_fish_2_0(s: &[u8]) -> Vec<u8> {
    // This function is in a very hot loop and the usage of boxed uninit memory benchmarks around 8%
    // faster on real-world escaped yaml samples from the fish history file.

    // This is a very long way around of writing `Box::new_uninit_slice(s.len())`, which
    // requires the unstablized nightly-only feature new_unit (#63291). It optimizes away.
    let mut result: Box<[_]> = std::iter::repeat_with(std::mem::MaybeUninit::uninit)
        .take(s.len())
        .collect();
    let mut chars = s.iter().copied();
    let mut src_idx = 0;
    let mut dst_idx = 0;
    loop {
        // While inspecting the asm reveals the compiler does not elide the bounds check from
        // the writes to `result`, benchmarking shows that using `result.get_unchecked_mut()`
        // everywhere does not result in a statistically significant improvement to the
        // performance of this function.
        let to_copy = chars.by_ref().take_while(|b| *b != b'\\').count();
        unsafe {
            let src = s[src_idx..].as_ptr();
            // Can use the following when feature(maybe_uninit_slice) is stabilized:
            // let dst = std::mem::MaybeUninit::slice_as_mut_ptr(&mut result[dst_idx..]);
            let dst = result[dst_idx..].as_mut_ptr().cast();
            std::ptr::copy_nonoverlapping(src, dst, to_copy);
        }
        dst_idx += to_copy;

        match chars.next() {
            Some(b'\\') => result[dst_idx].write(b'\\'),
            Some(b'n') => result[dst_idx].write(b'\n'),
            _ => break,
        };
        src_idx += to_copy + 2;
        dst_idx += 1;
    }

    let result = Box::leak(result);
    unsafe { Vec::from_raw_parts(result.as_mut_ptr().cast(), dst_idx, result.len()) }
}

/// Read one line, stripping off any newline, returning the number of bytes consumed.
fn read_line(data: &[u8]) -> (usize, &[u8]) {
    // Locate the newline.
    if let Some(newline) = data.iter().position(|&c| c == b'\n') {
        // we found a newline
        let line = &data[..newline];
        // Return the amount to advance the cursor; skip over the newline.
        (newline + 1, line)
    } else {
        // We ran off the end.
        (data.len(), b"")
    }
}

fn trim_start(s: &[u8]) -> &[u8] {
    &s[s.iter().take_while(|c| c.is_ascii_whitespace()).count()..]
}

/// Trims leading spaces in the given string, returning how many there were.
fn trim_leading_spaces(s: &[u8]) -> (usize, &[u8]) {
    let count = s.iter().take_while(|c| **c == b' ').count();
    (count, &s[count..])
}

// This function is forcibly inlined because we sometimes call it but discard one of the return
// values. Hopefully that will be sufficient for the compiler to skip the unnecessary call to
// unescape_yaml_fish_2_0() for the discarded key.
#[inline(always)]
#[allow(clippy::type_complexity)]
fn extract_prefix_and_unescape_yaml(line: &[u8]) -> Option<(Cow<[u8]>, Cow<[u8]>)> {
    let mut split = line.splitn(2, |c| *c == b':');
    let key = split.next().unwrap();
    let value = split.next()?;
    debug_assert!(split.next().is_none());

    let key = maybe_unescape_yaml_fish_2_0(key);

    // Skip a space after the : if necessary.
    let value = trim_start(value);
    let value = maybe_unescape_yaml_fish_2_0(value);

    Some((key, value))
}

/// Decode an item via the fish 2.0 format.
fn decode_item_fish_2_0(mut data: &[u8]) -> Option<HistoryItem> {
    let (advance, line) = read_line(data);
    let line = trim_start(line);
    assert!(line.starts_with(b"- cmd"));

    let (_key, value) = extract_prefix_and_unescape_yaml(line)?;

    data = &data[advance..];
    let cmd = str2wcstring(&value);

    // Read the remaining lines.
    let mut indent = None;
    let mut when = UNIX_EPOCH;
    let mut paths = Vec::new();
    loop {
        let (advance, line) = read_line(data);

        let (this_indent, line) = trim_leading_spaces(line);
        let indent = *indent.get_or_insert(this_indent);
        if this_indent == 0 || indent != this_indent {
            break;
        }

        let Some((key, value)) = extract_prefix_and_unescape_yaml(line) else {
            break;
        };

        // We are definitely going to consume this line.
        data = &data[advance..];

        if *key == *b"when" {
            // Parse an int from the timestamp. Should this fail, 0 is acceptable.
            when = time_from_seconds(
                std::str::from_utf8(&value)
                    .ok()
                    .and_then(|s| s.parse().ok())
                    .unwrap_or(0),
            );
        } else if *key == *b"paths" {
            // Read lines starting with " - " until we can't read any more.
            loop {
                let (advance, line) = read_line(data);
                let (leading_spaces, line) = trim_leading_spaces(line);
                if leading_spaces <= indent {
                    break;
                }

                let Some(line) = line.strip_prefix(b"- ") else {
                    break;
                };

                // We're going to consume this line.
                data = &data[advance..];

                let line = maybe_unescape_yaml_fish_2_0(line);
                paths.push(str2wcstring(&line));
            }
        }
    }

    let mut result = HistoryItem::new(cmd, when, 0, PersistenceMode::Disk);
    result.set_required_paths(paths);
    Some(result)
}

fn time_from_seconds(offset: i64) -> SystemTime {
    if let Ok(n) = u64::try_from(offset) {
        UNIX_EPOCH + Duration::from_secs(n)
    } else {
        UNIX_EPOCH - Duration::from_secs(offset.unsigned_abs())
    }
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

/// Returns the number of bytes until the next record in the given `contents` given the `current_offset`.
/// If no new suitable record could be found, then returns [`None`].
fn offset_of_next_item_fish_json(
    contents: &[u8],
    current_offset: &usize,
    cutoff_timestamp: Option<SystemTime>,
) -> Option<usize> {
    let start_of_next_record = contents[*current_offset..]
        .iter()
        .skip_while(|c| **c == RECORD_SEPARATOR)
        .position(|c| *c == RECORD_SEPARATOR)?;
    println!("start_of_next_record: {:?}", start_of_next_record);
    if start_of_next_record == contents.len() {
        None
    } else {
        Some(start_of_next_record)
    }
}

/// Parse a timestamp line that looks like this: spaces, "when:", spaces, timestamp, newline
/// We know the string contains a newline, so stop when we reach it.
fn parse_timestamp(s: &[u8]) -> Option<SystemTime> {
    let s = trim_start(s);
    let s = s.strip_prefix(b"when:")?;
    let s = trim_start(s);

    let t = std::str::from_utf8(s).ok()?.parse().ok()?;
    Some(time_from_seconds(t))
}

fn complete_lines(s: &[u8]) -> impl Iterator<Item = &[u8]> {
    let mut lines = s.split(|&c| c == b'\n');
    // Remove either the last empty element (in case last line is newline-terminated) or the
    // trailing non-newline-terminated line
    lines.next_back();
    lines
}

/// Support for iteratively locating the offsets of history items.
/// Pass the file contents and a mutable reference to a `cursor`, initially 0.
/// If `cutoff_timestamp` is given, skip items created at or after that timestamp.
/// Returns [`None`] when done.
fn offset_of_next_item_fish_2_0(
    contents: &[u8],
    cursor: &mut usize,
    cutoff_timestamp: Option<SystemTime>,
) -> Option<usize> {
    let mut lines = complete_lines(&contents[*cursor..]).peekable();
    while let Some(mut line) = lines.next() {
        // Skip lines with a leading space, since these are in the interior of one of our items.
        if line.starts_with(b" ") {
            continue;
        }

        // Try to be a little YAML compatible. Skip lines with leading %, ---, or ...
        if line.starts_with(b"%") || line.starts_with(b"---") || line.starts_with(b"...") {
            continue;
        }

        // Hackish: fish 1.x rewriting a fish 2.0 history file can produce lines with lots of
        // leading "- cmd: - cmd: - cmd:". Trim all but one leading "- cmd:".
        while line.starts_with(b"- cmd: - cmd: ") {
            // Skip over just one of the - cmd. In the end there will be just one left.
            line = line.strip_prefix(b"- cmd: ").unwrap();
        }

        // Hackish: fish 1.x rewriting a fish 2.0 history file can produce commands like "when:
        // 123456". Ignore those.
        if line.starts_with(b"- cmd:    when:") {
            continue;
        }

        if line.starts_with(b"\0") {
            FLOG!(
                error,
                "ignoring corrupted history entry around offset",
                *cursor
            );
            continue;
        }

        if !line.starts_with(b"- cmd") {
            FLOG!(
                history,
                "ignoring corrupted history entry around offset",
                *cursor
            );
            continue;
        }

        // At this point, we know `line` is at the beginning of an item. But maybe we want to
        // skip this item because of timestamps. A `None` cutoff means we don't care; if we do care,
        // then try parsing out a timestamp.
        if let Some(cutoff_timestamp) = cutoff_timestamp {
            // Hackish fast way to skip items created after our timestamp. This is the mechanism by
            // which we avoid "seeing" commands from other sessions that started after we started.
            // We try hard to ensure that our items are sorted by their timestamps, so in theory we
            // could just break, but I don't think that works well if (for example) the clock
            // changes. So we'll read all subsequent items.
            // Walk over lines that we think are interior. These lines are not null terminated, but
            // are guaranteed to contain a newline.
            let mut timestamp = None;
            loop {
                let Some(interior_line) = lines.next_if(|l| l.starts_with(b" ")) else {
                    // If the first character is not a space, it's not an interior line, so we're done.
                    break;
                };

                // Try parsing a timestamp from this line. If we succeed, the loop will break.
                timestamp = parse_timestamp(interior_line);
                if timestamp.is_some() {
                    break;
                }
            }

            // Skip this item if the timestamp is past our cutoff.
            if let Some(timestamp) = timestamp {
                if timestamp > cutoff_timestamp {
                    continue;
                }
            }
        }

        // We made it through the gauntlet.

        /// # Safety
        ///
        /// Both `from` and `to` must be derived from the same slice.
        unsafe fn offset(from: &[u8], to: &[u8]) -> usize {
            let from = from.as_ptr();
            let to = to.as_ptr();
            // SAFETY: from and to are derived from the same slice, slices can't be longer than
            // isize::MAX
            let offset = unsafe { to.offset_from(from) };
            offset.try_into().unwrap()
        }

        // Advance the cursor past the last line of this entry
        *cursor = match lines.next() {
            Some(next_line) => unsafe { offset(contents, next_line) },
            None => contents.len(),
        };

        return Some(unsafe { offset(contents, line) });
    }

    None
}
