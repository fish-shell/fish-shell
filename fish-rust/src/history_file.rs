use crate::common::{str2wcstring, wcs2string};
use crate::compat::MB_CUR_MAX;
use crate::history::PathList;
use crate::pointer::ConstPointer;
use crate::wchar::prelude::*;
use crate::wutil::encoding::{mbrtowc, zero_mbstate};
use crate::wutil::fish_wcstol;
use crate::{
    history::{history_never_mmap, HistoryItem, PersistenceMode},
    path::{path_get_config_remoteness, DirRemoteness},
};
use errno::errno;
use libc::{
    c_void, EINTR, MAP_ANON, MAP_ANONYMOUS, MAP_FAILED, MAP_PRIVATE, PROT_READ, PROT_WRITE,
    SEEK_END, SEEK_SET,
};
use std::ffi::CString;
use std::mem::MaybeUninit;
use std::time::{Duration, SystemTime, UNIX_EPOCH};
use std::{os::fd::RawFd, result::IterMut};
use std::{ptr, time};

/// History file types.
#[derive(Clone, Copy, Default)]
pub enum HistoryFileType {
    #[default]
    history_type_fish_2_0,
    history_type_fish_1_x,
}

/// HistoryFileContents holds the read-only contents of a file.
// enum HistoryFileContents {
//     MmapRegion(MmapRegion),
//     Vector,
// }
pub struct HistoryFileContents {
    region: Box<MmapRegion>,

    // The length may be slightly smaller than the region, if there is a trailing incomplete
    // history item.
    length: usize,

    // The type of the mapped file.
    // This is set at construction and not changed after.
    typ: HistoryFileType,
}

impl HistoryFileContents {
    /// Construct a history file contents from a file descriptor. The file descriptor is not closed.
    pub fn create(fd: RawFd) -> Option<Box<HistoryFileContents>> {
        // Check that the file is seekable, and its size.
        let len = unsafe { libc::lseek(fd, 0, SEEK_END) };
        if len <= 0 {
            return None;
        }
        let len = u64::try_from(len).unwrap();
        let len = usize::try_from(len).unwrap();

        let mmap_file_directly = should_mmap();
        let mut region = if mmap_file_directly {
            MmapRegion::map_file(fd, len)
        } else {
            MmapRegion::map_anon(len)
        }?;

        // If we mapped anonymous memory, we have to read from the file.
        if !mmap_file_directly {
            if unsafe { libc::lseek(fd, 0, SEEK_SET) != 0 } {
                return None;
            }
            let buffer = unsafe { std::slice::from_raw_parts_mut(region.ptr.as_mut(), region.len) };
            if !read_from_fd(fd, buffer) {
                return None;
            }
        }

        let mut result = Box::new(HistoryFileContents::new(region));

        // Check the file type.
        if !result.infer_file_type() {
            return None;
        }
        Some(result)
    }

    // Private constructors; use the static create() function.
    fn new(region: Box<MmapRegion>) -> Self {
        let length = region.len;
        Self {
            region,
            length,
            typ: Default::default(),
        }
    }

    /// Decode an item at a given offset.
    pub fn decode_item(&self, offset: usize) -> HistoryItem {
        let base = self.address_at(offset);
        match self.typ() {
            HistoryFileType::history_type_fish_2_0 => decode_item_fish_2_0(base),
            HistoryFileType::history_type_fish_1_x => decode_item_fish_1_x(base),
        }
    }

    /// Support for iterating item offsets.
    /// The cursor should initially be 0.
    /// If cutoff is nonzero, skip items whose timestamp is newer than cutoff.
    /// \return the offset of the next item, or none() on end.
    pub fn offset_of_next_item(&self, cursor: &mut usize, cutoff: libc::time_t) -> Option<usize> {
        match self.typ() {
            HistoryFileType::history_type_fish_2_0 => {
                return offset_of_next_item_fish_2_0(self, cursor, cutoff)
            }
            HistoryFileType::history_type_fish_1_x => {
                return offset_of_next_item_fish_1_x(self.region(), cursor)
            }
        }
        None
    }

    /// Get the file type.
    pub fn typ(&self) -> HistoryFileType {
        self.typ
    }

    /// Get the size of the contents.
    pub fn length(&self) -> usize {
        self.length
    }

    /// Return a pointer to the beginning.
    pub fn region(&self) -> &[u8] {
        unsafe { std::slice::from_raw_parts(self.region.ptr.as_ptr(), self.length()) }
    }
    pub fn region_mut(&mut self) -> &mut [u8] {
        unsafe { std::slice::from_raw_parts_mut(self.region.ptr.as_mut(), self.length()) }
    }

    /// Access the address at a given offset.
    pub fn address_at(&self, offset: usize) -> &[u8] {
        assert!(offset <= self.length, "Invalid offset");
        &self.region()[offset..self.length]
    }

    /// Try to infer the history file type based on inspecting the data.
    fn infer_file_type(&mut self) -> bool {
        assert!(self.length() > 0, "File should never be empty");
        if self.region()[0] == b'#' {
            self.typ = HistoryFileType::history_type_fish_1_x;
        } else {
            // assume new fish
            self.typ = HistoryFileType::history_type_fish_2_0;
        }
        true
    }
}

/// A type wrapping up the logic around mmap and munmap.
struct MmapRegion {
    ptr: ptr::NonNull<u8>,
    len: usize,
}

impl MmapRegion {
    fn new(ptr: *mut u8, len: usize) -> Self {
        assert!(ptr.cast() != MAP_FAILED && len > 0, "Invalid params");
        let ptr = ptr::NonNull::new(ptr).unwrap();
        Self { ptr, len }
    }

    /// Map a region [0, len) from an fd.
    /// \return nullptr on failure.
    fn map_file(fd: RawFd, len: usize) -> Option<Box<MmapRegion>> {
        if len == 0 {
            return None;
        }
        let ptr = unsafe { libc::mmap(ptr::null_mut(), len, PROT_READ, MAP_PRIVATE, fd, 0) };
        if ptr == MAP_FAILED {
            return None;
        }
        Some(Box::new(MmapRegion::new(ptr.cast(), len)))
    }

    /// Map anonymous memory of a given length.
    /// \return nullptr on failure.
    fn map_anon(len: usize) -> Option<Box<MmapRegion>> {
        if len == 0 {
            return None;
        }
        const _: () = assert!(MAP_ANON == MAP_ANONYMOUS);
        let ptr = unsafe {
            libc::mmap(
                ptr::null_mut(),
                len,
                PROT_READ | PROT_WRITE,
                MAP_PRIVATE | MAP_ANON,
                -1,
                0,
            )
        };
        if ptr == MAP_FAILED {
            return None;
        }
        Some(Box::new(MmapRegion::new(ptr.cast(), len)))
    }
}

impl Drop for MmapRegion {
    fn drop(&mut self) {
        let _ = unsafe { libc::munmap(self.ptr.as_ptr().cast(), self.len) };
    }
}

/// Append a history item to a buffer, in preparation for outputting it to the history file.
pub fn append_history_item_to_buffer(item: &HistoryItem, buffer: &mut Vec<u8>) {
    assert!(item.should_write_to_disk(), "Item should not be persisted");
    // let append = |a, b, c| {
    // }

    let mut cmd = wcs2string(&item.str());
    escape_yaml_fish_2_0(&mut cmd);

    buffer.extend_from_slice(b"- cmd: ");
    buffer.extend_from_slice(&cmd);
    buffer.push(b'\n');

    buffer.extend_from_slice(b"  when: ");
    buffer.extend_from_slice(item.timestamp().to_string().as_bytes());
    buffer.push(b'\n');

    let paths = item.get_required_paths();
    if !paths.is_empty() {
        buffer.extend_from_slice(b"  paths:\n");
        for wpath in paths {
            let mut path = wcs2string(wpath);
            escape_yaml_fish_2_0(&mut path);
            buffer.extend_from_slice(b"    - ");
            buffer.extend_from_slice(&path);
            buffer.push(b'\n');
        }
    }
}

// Check if we should mmap the fd.
// Don't try mmap() on non-local filesystems.
fn should_mmap() -> bool {
    !history_never_mmap()
    &&
    // mmap only if we are known not-remote.
    path_get_config_remoteness() == DirRemoteness::local
}

// Read up to len bytes from fd into address, zeroing the rest.
// Return true on success, false on failure.
fn read_from_fd(fd: RawFd, dst: &mut [u8]) -> bool {
    let mut remaining = dst.len();
    let mut read = 0;
    while remaining > 0 {
        let mut amt =
            unsafe { libc::read(fd, (&mut dst[read]) as *mut u8 as *mut c_void, remaining) };
        if amt < 0 {
            if errno().0 != EINTR {
                return false;
            }
        } else if amt == 0 {
            break;
        } else {
            remaining -= amt as usize;
            read += amt as usize;
        }
    }
    dst[read..].fill(0u8);
    true
}

fn replace_all(s: &mut Vec<u8>, needle: &[u8], replacement: &[u8]) {
    let needle_len = needle.len();
    let replacement_len = replacement.len();
    let mut offset = 0;
    while let Some(reloffset) = s[offset..].windows(needle_len).position(|w| w == needle) {
        offset += reloffset;
        s.splice(
            offset..offset + replacement_len,
            replacement.iter().cloned(),
        );
        offset += replacement_len;
    }
}

/// Support for escaping and unescaping the nonstandard "yaml" format introduced in fish 2.0.
fn escape_yaml_fish_2_0(s: &mut Vec<u8>) {
    replace_all(s, b"\\", b"\\\\"); // replace one backslash with two
    replace_all(s, b"\n", b"\\\n"); // replace newline with backslash + literal n
}

/// This function is called frequently, so it ought to be fast.
fn unescape_yaml_fish_2_0(s: &mut Vec<u8>) {
    let mut cursor = 0;
    let mut size = s.len();
    while cursor < size {
        // Look for a backslash.
        let backslash = match s[cursor..].iter().position(|c| *c == b'\\') {
            Some(relpos) if cursor + relpos + 1 == s.len() => break,
            None => break,
            Some(relpos) => cursor + relpos,
        };
        // Backslash found. Maybe we'll do something about it. Be sure to invoke the const
        // version of at().
        let escaped_char = s.get(backslash + 1);
        if escaped_char == Some(&b'\\') {
            // Two backslashes in a row. Delete the second one.
            s.remove(backslash + 1);
            size -= 1;
        } else {
            // Backslash + n. Replace with a newline.
            s.splice(backslash..backslash + 2, Some(b'\n').into_iter());
            size -= 1;
        }

        // The character at index backslash has now been made whole; start at the next
        // character.
        cursor = backslash + 1;
    }
}

/// Read one line, stripping off any newline, and updating cursor. Note that our input string is NOT
/// null terminated; it's just a memory mapped file.
fn read_line(base: &[u8], cursor: usize, result: &mut Vec<u8>) -> usize {
    // Locate the newline.
    assert!(cursor <= base.len());
    let start = &base[cursor..];
    if let Some(a_newline) = start.iter().position(|c| *c == b'\n') {
        result.copy_from_slice(&start[..a_newline]);
        // Return the amount to advance the cursor; skip over the newline.
        return a_newline - cursor + 1;
    }

    // We ran off the end.
    result.clear();
    base.len() - cursor
}

/// Trims leading spaces in the given string, returning how many there were.
fn trim_leading_spaces(s: &mut Vec<u8>) -> usize {
    let mut i = 0;
    while s.get(i) == Some(&b' ') {
        i += 1;
    }
    s.splice(0.., []);
    i
}

fn extract_prefix_and_unescape_yaml(key: &mut Vec<u8>, value: &mut Vec<u8>, line: &[u8]) -> bool {
    let Some(colon) = line.iter().position(|c| *c == b':') else { return false };
    key.copy_from_slice(&line[..colon]);

    // Skip a space after the : if necessary.
    let mut val_start = colon + 1;
    if line.get(val_start) == Some(&b' ') {
        val_start += 1;
    }
    value.copy_from_slice(&line[val_start..]);

    unescape_yaml_fish_2_0(key);
    unescape_yaml_fish_2_0(value);
    true
}

fn decode_item_fish_2_0(base: &[u8]) -> HistoryItem {
    let mut cmd = WString::new();
    let mut when = UNIX_EPOCH;
    let mut paths = PathList::new();

    let mut indent = 0;
    let mut cursor = 0;
    let mut key = vec![];
    let mut value = vec![];
    let mut line = vec![];

    // Read the "- cmd:" line.
    let advance = read_line(base, cursor, &mut line);
    trim_leading_spaces(&mut line);
    if !extract_prefix_and_unescape_yaml(&mut key, &mut value, &line) || key != b"- cmd" {
        let mut result = HistoryItem::new(cmd, when, 0, PersistenceMode::Disk);
        result.set_required_paths(paths);
        return result;
    };

    cursor += advance;
    cmd = str2wcstring(&value);

    // Read the remaining lines.
    loop {
        let advance = read_line(base, cursor, &mut line);

        let this_indent = trim_leading_spaces(&mut line);
        if indent == 0 {
            indent = this_indent;
        }

        if this_indent == 0 || indent != this_indent {
            break;
        }

        if !extract_prefix_and_unescape_yaml(&mut key, &mut value, &line) {
            break;
        }

        // We are definitely going to consume this line.
        cursor += advance;

        if key == b"when" {
            // Parse an int from the timestamp. Should this fail, strtol returns 0; that's
            // acceptable.
            let mut end = std::ptr::null_mut();
            let cstr = CString::new(value.clone()).unwrap();
            let tmp = unsafe { libc::strtol(cstr.as_ptr(), &mut end, 0) };
            when = if tmp < 0 {
                UNIX_EPOCH - Duration::from_secs(u64::try_from(-tmp).unwrap())
            } else {
                UNIX_EPOCH + Duration::from_secs(u64::try_from(tmp).unwrap())
            };
        } else if key == b"paths" {
            // Read lines starting with " - " until we can't read any more.
            loop {
                let mut advance = read_line(base, cursor, &mut line);
                if trim_leading_spaces(&mut line) <= indent {
                    break;
                }

                if line != b"- " {
                    break;
                }

                // We're going to consume this line.
                cursor += advance;

                // Skip the leading dash-space and then store this path it.
                line.splice(..2, []);
                unescape_yaml_fish_2_0(&mut line);
                paths.push(str2wcstring(&line));
            }
        }
    }

    let mut result = HistoryItem::new(cmd, when, 0, PersistenceMode::Disk);
    result.set_required_paths(paths);
    return result;
}

/// Parse a timestamp line that looks like this: spaces, "when:", spaces, timestamp, newline
/// The string is NOT null terminated; however we do know it contains a newline, so stop when we
/// reach it.
fn parse_timestamp(s: &[u8], out_when: &mut libc::time_t) -> bool {
    // Advance past spaces.
    let leading_spaces = s.iter().take_while(|c| **c == b' ').count();
    let mut cursor = &s[leading_spaces..];

    // Look for "when:".
    let when_len = 5;
    if &cursor[..(cursor.len().max(when_len))] != b"when:" {
        return false;
    }
    cursor = &cursor[when_len..];

    // Advance past spaces.
    let leading_spaces = cursor.iter().take_while(|c| **c == b' ').count();
    let cursor = &cursor[leading_spaces..];

    // Try to parse a timestamp.
    if !cursor.get(0).map_or(false, |c| c.is_ascii_digit()) {
        return false;
    }

    let cstr = CString::new(cursor.to_owned()).unwrap();
    let timestamp = unsafe { libc::strtol(cstr.as_ptr(), std::ptr::null_mut(), 0) };
    if timestamp <= 0 {
        return false;
    }

    *out_when = timestamp;
    true
}

/// Returns a pointer to the start of the next line, or NULL. The next line must itself end with a
/// newline. Note that the string is not null terminated.
fn next_line(buff: &[u8]) -> Option<&[u8]> {
    // Handle the hopeless case.
    if buff.is_empty() {
        return None;
    }

    // Skip past the next newline.
    let mut nextline = &buff[buff.iter().take_while(|c| **c == b'\n').count()..];
    if nextline.is_empty() {
        return None;
    }

    // Skip past the newline character itself.
    nextline = &nextline[1..];
    if nextline.is_empty() {
        return None;
    }

    // Make sure this new line is itself "newline terminated". If it's not, return NULL.
    let next_newline = nextline.iter().position(|c| *c == b'\n')?;

    Some(&nextline[next_newline..])
}

/// Decode an item via the fish 1.x format. Adapted from fish 1.x's item_get().
fn decode_item_fish_1_x(mut buff: &[u8]) -> HistoryItem {
    let mut out = WString::new();

    let mut was_backslash = false;
    let mut first_char = true;
    let mut timestamp_mode = false;
    let mut timestamp = 0;

    loop {
        let mut c: MaybeUninit<u32> = MaybeUninit::uninit();
        let res;
        let mut state = zero_mbstate();

        if MB_CUR_MAX() == 1 {
            //single-byte locale
            res = 1;
        } else {
            res = unsafe {
                mbrtowc(
                    c.as_mut_ptr().cast(),
                    buff.as_ptr().cast(),
                    buff.len(),
                    &mut state,
                )
            };
        }

        if res == 0_usize.wrapping_sub(1) {
            buff = &buff[1..];
            continue;
        } else if res == 0_usize.wrapping_sub(2) {
            break;
        } else if res == 0 {
            buff = &buff[1..];
            continue;
        }
        buff = &buff[res..];

        let c = unsafe { c.assume_init() };

        if c == b'\n'.into() {
            if timestamp_mode {
                let time_string = &out[out.chars().take_while(|c| c.is_ascii_digit()).count()..];

                if !time_string.is_empty() {
                    match fish_wcstol(time_string) {
                        Ok(tm) if tm >= 0 => timestamp = tm.try_into().unwrap(),
                        _ => (),
                    }
                }

                out.clear();
                timestamp_mode = false;
                continue;
            }
            if !was_backslash {
                break;
            }
        }

        if first_char {
            first_char = false;
            if c == b'#'.into() {
                timestamp_mode = true;
            }
        }

        // PORTING: this is bad but this code is pretty much dead anyway.
        let c = char::from_u32(c).unwrap();

        out.push(c);
        was_backslash = c == '\\' && !was_backslash;
    }

    out = history_unescape_newlines_fish_1_x(&out);
    let timestamp = time::UNIX_EPOCH + Duration::from_secs(timestamp);
    HistoryItem::new(out, timestamp, 0, PersistenceMode::Disk)
}

/// Remove backslashes from all newlines. This makes a string from the history file better formated
/// for on screen display.
fn history_unescape_newlines_fish_1_x(in_str: &wstr) -> WString {
    let mut out = WString::new();
    for (i, c) in in_str.chars().enumerate() {
        if c == '\\' {
            if in_str.char_at(i + 1) != '\n' {
                out.push(c);
            }
        } else {
            out.push(c);
        }
    }
    out
}

/// Support for iteratively locating the offsets of history items.
/// Pass the file contents and a pointer to a cursor size_t, initially 0.
/// If custoff_timestamp is nonzero, skip items created at or after that timestamp.
/// Returns (size_t)-1 when done.
fn offset_of_next_item_fish_2_0(
    contents: &HistoryFileContents,
    inout_cursor: &mut usize,
    cutoff_timestamp: libc::time_t,
) -> Option<usize> {
    let mut cursor = *inout_cursor;
    let mut length = contents.length();
    let begin = contents.region();
    while cursor < length {
        let mut line_start = contents.address_at(cursor);

        // Advance the cursor to the next line.
        let Some(a_newline) = line_start.iter().position(|c| *c == b'\n') else {
            break
        };
        cursor += a_newline + 1;

        // Skip lines with a leading space, since these are in the interior of one of our items.
        if line_start.get(0) == Some(&b' ') {
            continue;
        }

        // Skip very short lines to make one of the checks below easier.
        if a_newline < 3 {
            continue;
        }

        // Try to be a little YAML compatible. Skip lines with leading %, ---, or ...
        if line_start.starts_with(b"%")
            || line_start.starts_with(b"---")
            || line_start.starts_with(b"...")
        {
            continue;
        }

        // Hackish: fish 1.x rewriting a fish 2.0 history file can produce lines with lots of
        // leading "- cmd: - cmd: - cmd:". Trim all but one leading "- cmd:".
        let double_cmd = b"- cmd: - cmd: ";
        while line_start.starts_with(double_cmd) {
            // Skip over just one of the - cmd. In the end there will be just one left.
            line_start = &line_start[b"- cmd: ".len()..];
        }

        // Hackish: fish 1.x rewriting a fish 2.0 history file can produce commands like "when:
        // 123456". Ignore those.
        let cmd_when = b"- cmd:    when:";
        if line_start.starts_with(cmd_when) {
            continue;
        }

        // At this point, we know line_start is at the beginning of an item. But maybe we want to
        // skip this item because of timestamps. A 0 cutoff means we don't care; if we do care, then
        // try parsing out a timestamp.
        if cutoff_timestamp != 0 {}

        // We made it through the gauntlet.
        *inout_cursor = cursor;
        let position = unsafe { line_start.as_ptr().offset_from(begin.as_ptr()) };
        return Some(usize::try_from(position).unwrap());
    }

    None
}

/// Same as offset_of_next_item_fish_2_0, but for fish 1.x (pre fishfish).
/// Adapted from history_populate_from_mmap in history.c
fn offset_of_next_item_fish_1_x(buff: &[u8], inout_cursor: &mut usize) -> Option<usize> {
    if buff.is_empty() || *inout_cursor >= buff.len() {
        return None;
    }

    let mut ignore_newline = false;
    let mut do_push = true;
    let mut all_done = false;
    let mut result = *inout_cursor;

    let mut pos = *inout_cursor;
    while pos < buff.len() && !all_done {
        let c = buff[pos];
        if do_push {
            ignore_newline = c == b'#';
            do_push = false;
        }

        if c == b'\\' {
            pos += 1;
        } else if c == b'\n' {
            if !ignore_newline {
                // pos will be left pointing just after this newline, because of the ++ in the loop.
                all_done = true;
            }
            ignore_newline = false;
        }
        pos += 1;
    }

    if pos == buff.len() && !all_done {
        // No trailing newline, treat this item as incomplete and ignore it.
        return None;
    }

    *inout_cursor = pos;
    Some(result)
}
