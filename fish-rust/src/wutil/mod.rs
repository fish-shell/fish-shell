pub mod encoding;
pub mod errors;
pub mod gettext;
mod normalize_path;
pub mod printf;
pub mod wcstod;
pub mod wcstoi;
mod wrealpath;

use crate::common::fish_reserved_codepoint;
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
