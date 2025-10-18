//! Implementation of history files.

use std::{
    borrow::Cow,
    fs::File,
    io::{Read, Seek, SeekFrom, Write},
    ops::{Deref, DerefMut},
    os::fd::AsRawFd,
    time::{Duration, SystemTime, UNIX_EPOCH},
};

use libc::{ENODEV, MAP_ANONYMOUS, MAP_FAILED, MAP_PRIVATE, PROT_READ, PROT_WRITE, mmap, munmap};

use super::{HistoryItem, PersistenceMode};
use crate::{
    common::{bytes2wcstring, subslice_position, wcs2bytes},
    flog::FLOG,
    path::{DirRemoteness, path_get_data_remoteness},
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

    /// Map a region `[0, len)` from a locked file.
    pub fn map_file(file: &File, len: usize) -> std::io::Result<Self> {
        let ptr = unsafe {
            mmap(
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
        unsafe { munmap(self.ptr.cast(), self.len) };
    }
}

/// HistoryFileContents holds the read-only contents of a file.
pub struct HistoryFileContents {
    region: MmapRegion,
}

impl HistoryFileContents {
    /// Construct a history file contents from a [`File`] reference.
    pub fn create(mut history_file: &File) -> std::io::Result<Self> {
        // Check that the file is seekable, and its size.
        let len: usize = match history_file.seek(SeekFrom::End(0))?.try_into() {
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
            file.seek(SeekFrom::Start(0))?;
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
    type Error = std::io::Error;

    fn try_from(region: MmapRegion) -> std::io::Result<Self> {
        let type_ = infer_file_type(&region);
        if type_ == HistoryFileType::Fish1_x {
            let error_message = "unsupported history file format 1.x";
            FLOG!(error, error_message);
            return Err(std::io::Error::new(
                std::io::ErrorKind::InvalidInput,
                error_message,
            ));
        }
        Ok(Self { region })
    }
}

/// Append a history item to a buffer, in preparation for outputting it to the history file.
pub fn append_history_item_to_buffer(item: &HistoryItem, buffer: &mut Vec<u8>) {
    assert!(item.should_write_to_disk(), "Item should not be persisted");

    let mut cmd = wcs2bytes(item.str());
    escape_yaml_fish_2_0(&mut cmd);
    buffer.extend(b"- cmd: ");
    buffer.extend(&cmd);
    buffer.push(b'\n');
    writeln!(buffer, "  when: {}", time_to_seconds(item.timestamp())).unwrap();

    let paths = item.get_required_paths();
    if !paths.is_empty() {
        writeln!(buffer, "  paths:").unwrap();
        for path in paths {
            let mut path = wcs2bytes(path);
            escape_yaml_fish_2_0(&mut path);
            buffer.extend(b"    - ");
            buffer.extend(&path);
            buffer.push(b'\n');
        }
    }
}

/// Check if we should mmap the file.
/// Don't try mmap() on non-local filesystems.
fn should_mmap() -> bool {
    // mmap only if we are known not-remote.
    path_get_data_remoteness() != DirRemoteness::remote
}

fn replace_all(s: &mut Vec<u8>, needle: &[u8], replacement: &[u8]) {
    let mut offset = 0;
    while let Some(relpos) = subslice_position(&s[offset..], needle) {
        offset += relpos;
        s.splice(offset..(offset + needle.len()), replacement.iter().copied());
        offset += replacement.len();
    }
}

/// Support for escaping and unescaping the nonstandard "yaml" format introduced in fish 2.0.
fn escape_yaml_fish_2_0(s: &mut Vec<u8>) {
    replace_all(s, b"\\", b"\\\\"); // replace one backslash with two
    replace_all(s, b"\n", b"\\n"); // replace newline with backslash + literal n
}

#[inline(always)]
/// Unescapes the fish-specific yaml variant, if it requires it.
fn maybe_unescape_yaml_fish_2_0(s: &[u8]) -> Cow<'_, [u8]> {
    // This is faster than s.contains(b'\\') and can be auto-vectorized to SIMD. See benchmark note
    // on unescape_yaml_fish_2_0().
    if !s.iter().copied().fold(false, |acc, b| acc | (b == b'\\')) {
        return s.into();
    }
    unescape_yaml_fish_2_0(s).into()
}

// Unescapes the fish-specific yaml variant. Use [`maybe_unescape_yaml_fish_2_0()`] if you're not
// positive the input contains an escape.
//
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
fn extract_prefix_and_unescape_yaml(line: &[u8]) -> Option<(Cow<'_, [u8]>, Cow<'_, [u8]>)> {
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
    if !line.starts_with(b"- cmd") {
        return None;
    }

    let (_key, value) = extract_prefix_and_unescape_yaml(line)?;

    data = &data[advance..];
    let cmd = bytes2wcstring(&value);

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
                paths.push(bytes2wcstring(&line));
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
