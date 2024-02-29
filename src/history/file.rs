//! Implemention of history files.

use std::{
    io::Write,
    os::fd::AsRawFd,
    time::{Duration, SystemTime, UNIX_EPOCH},
};

use super::{HistoryItem, PersistenceMode};
use crate::{
    common::{read_loop, str2wcstring, subslice_position, wcs2string},
    flog::FLOG,
    path::{path_get_config_remoteness, DirRemoteness},
};

use memmap2::{Mmap, MmapOptions};
use nix::unistd::{lseek, Whence};

/// History file types.
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum HistoryFileType {
    Fish2_0,
    Fish1_x,
}

/// HistoryFileContents holds the read-only contents of a file.
pub struct HistoryFileContents {
    region: Mmap,
}

impl HistoryFileContents {
    /// Construct a history file contents from a file descriptor. The file descriptor is not closed.
    ///
    /// Callers should ensure that the underlying `fd` is locked.
    pub fn create(fd: &impl AsRawFd) -> Option<Self> {
        // Check that the file is seekable, and its size.
        let mut options = MmapOptions::new();
        options.len(
            lseek(fd.as_raw_fd(), 0, Whence::SeekEnd)
                .ok()?
                .try_into()
                .ok()?,
        );

        let region = if should_mmap() {
            // SAFETY: It is not possible to ensure that
            // the underlying file is locked. Callers need
            // to ensure this.
            unsafe { options.map(fd) }
        } else {
            let mut region = options.map_anon().ok()?;
            lseek(fd.as_raw_fd(), 0, Whence::SeekSet).ok()?;
            // If we mapped anonymous memory, we have to read from the file.
            let buf = region.as_mut();
            read_loop(fd, buf).ok()?;
            buf.fill(0u8);
            region.make_read_only()
        };
        region.ok()?.try_into().ok()
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

impl TryFrom<Mmap> for HistoryFileContents {
    type Error = ();

    fn try_from(region: Mmap) -> Result<Self, Self::Error> {
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

/// This function is called frequently, so it ought to be fast.
fn unescape_yaml_fish_2_0(s: &mut Vec<u8>) {
    let mut cursor = 0;
    while cursor < s.len() {
        // Look for a backslash.
        let Some(backslash) = s[cursor..].iter().position(|&c| c == b'\\') else {
            // No more backslashes
            break;
        };

        // Add back the start offset
        let backslash = backslash + cursor;

        // Backslash found. Maybe we'll do something about it.
        let Some(escaped_char) = s.get(backslash + 1) else {
            // Backslash was final character
            break;
        };

        match escaped_char {
            b'\\' => {
                // Two backslashes in a row. Delete the second one.
                s.remove(backslash + 1);
            }
            b'n' => {
                // Backslash + n. Replace with a newline.
                s.splice(backslash..(backslash + 2), [b'\n']);
            }
            _ => {
                // Unknown backslash escape, keep as-is
            }
        };

        // The character at index backslash has now been made whole; start at the next
        // character.
        cursor = backslash + 1;
    }
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

fn extract_prefix_and_unescape_yaml(line: &[u8]) -> Option<(Vec<u8>, Vec<u8>)> {
    let mut split = line.splitn(2, |c| *c == b':');
    let key = split.next().unwrap();
    let value = split.next()?;
    assert!(split.next().is_none());

    let mut key = key.to_owned();
    // Skip a space after the : if necessary.
    let mut value = trim_start(value).to_owned();

    unescape_yaml_fish_2_0(&mut key);
    unescape_yaml_fish_2_0(&mut value);
    Some((key, value))
}

/// Decode an item via the fish 2.0 format.
fn decode_item_fish_2_0(mut data: &[u8]) -> Option<HistoryItem> {
    let (advance, line) = read_line(data);
    let line = trim_start(line);
    let Some((key, value)) = extract_prefix_and_unescape_yaml(line) else {
        return None;
    };

    if key != b"- cmd" {
        return None;
    }

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

        if key == b"when" {
            // Parse an int from the timestamp. Should this fail, 0 is acceptable.
            when = time_from_seconds(
                std::str::from_utf8(&value)
                    .ok()
                    .and_then(|s| s.parse().ok())
                    .unwrap_or(0),
            );
        } else if key == b"paths" {
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

                let mut line = line.to_owned();
                unescape_yaml_fish_2_0(&mut line);
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

/// Parse a timestamp line that looks like this: spaces, "when:", spaces, timestamp, newline
/// We know the string contains a newline, so stop when we reach it.
fn parse_timestamp(s: &[u8]) -> Option<SystemTime> {
    let s = trim_start(s);

    let Some(s) = s.strip_prefix(b"when:") else {
        return None;
    };

    let s = trim_start(s);

    std::str::from_utf8(s)
        .ok()
        .and_then(|s| s.parse().ok())
        .map(time_from_seconds)
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
