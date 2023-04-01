pub mod encoding;
pub mod errors;
pub mod gettext;
mod normalize_path;
pub mod printf;
pub mod wcstod;
pub mod wcstoi;
mod wrealpath;

use crate::common::fish_reserved_codepoint;
use crate::wchar::wstr;
pub(crate) use gettext::{wgettext, wgettext_fmt};
pub use normalize_path::*;
pub(crate) use printf::sprintf;
pub use wcstoi::*;
pub use wrealpath::*;

/// Port of the wide-string wperror from `src/wutil.cpp` but for rust `&str`.
use std::io::Write;
pub fn perror(s: &str) {
    let e = errno::errno().0;
    let mut stderr = std::io::stderr().lock();
    if !s.is_empty() {
        let _ = write!(stderr, "{s}: ");
    }
    let slice = unsafe {
        let msg = libc::strerror(e) as *const u8;
        let len = libc::strlen(msg as *const _);
        std::slice::from_raw_parts(msg, len)
    };
    let _ = stderr.write_all(slice);
    let _ = stderr.write_all(b"\n");
}

const PUA1_START: char = '\u{E000}';
const PUA1_END: char = '\u{F900}';
const PUA2_START: char = '\u{F0000}';
const PUA2_END: char = '\u{FFFFE}';
const PUA3_START: char = '\u{100000}';
const PUA3_END: char = '\u{10FFFE}';

/// Return one if the code point is in a Unicode private use area.
fn fish_is_pua(c: char) -> bool {
    PUA1_START <= c && c < PUA1_END
}

/// We need this because there are too many implementations that don't return the proper answer for
/// some code points. See issue #3050.
pub fn fish_iswalnum(c: char) -> bool {
    !fish_reserved_codepoint(c) && !fish_is_pua(c) && c.is_alphanumeric()
}

/// Given that \p cursor is a pointer into \p base, return the offset in characters.
/// This emulates C pointer arithmetic:
///    `wstr_offset_in(cursor, base)` is equivalent to C++ `cursor - base`.
pub fn wstr_offset_in(cursor: &wstr, base: &wstr) -> usize {
    let cursor = cursor.as_slice();
    let base = base.as_slice();
    // cursor may be a zero-length slice at the end of base,
    // which base.as_ptr_range().contains(cursor.as_ptr()) will reject.
    let base_range = base.as_ptr_range();
    let curs_range = cursor.as_ptr_range();
    assert!(
        base_range.start <= curs_range.start && curs_range.end <= base_range.end,
        "cursor should be a subslice of base"
    );
    let offset = unsafe { cursor.as_ptr().offset_from(base.as_ptr()) };
    assert!(offset >= 0, "offset should be non-negative");
    offset as usize
}

#[test]
fn test_wstr_offset_in() {
    use crate::wchar::L;
    let base = L!("hello world");
    assert_eq!(wstr_offset_in(&base[6..], base), 6);
    assert_eq!(wstr_offset_in(&base[0..], base), 0);
    assert_eq!(wstr_offset_in(&base[6..], &base[6..]), 0);
    assert_eq!(wstr_offset_in(&base[base.len()..], base), base.len());
}
