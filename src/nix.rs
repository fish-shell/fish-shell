//! Safe wrappers around various libc functions that we might want to reuse across modules.

pub fn isatty(fd: i32) -> bool {
    // This returns false if the fd is valid but not a tty, or is invalid.
    // No place we currently call it really cares about the difference.
    (unsafe { libc::isatty(fd) }) == 1
}
