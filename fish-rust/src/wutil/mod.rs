pub mod format;
pub mod gettext;
mod wcstoi;
mod wrealpath;

use std::io::Write;

pub(crate) use format::printf::sprintf;
pub(crate) use gettext::{wgettext, wgettext_fmt};
pub use wcstoi::*;
pub use wrealpath::*;

/// Port of the wide-string wperror from `src/wutil.cpp` but for rust `&str`.
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
