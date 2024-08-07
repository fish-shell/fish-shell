//! Safe wrappers around various libc functions that we might want to reuse across modules.

use std::time::Duration;

use crate::libc::timeval64;

pub(crate) const fn timeval_to_duration(val: &timeval64) -> Duration {
    let micros = val.tv_sec * (1E6 as i64) + val.tv_usec;
    Duration::from_micros(micros as u64)
}

pub trait TimevalExt {
    fn as_micros(&self) -> i64;
    fn as_duration(&self) -> Duration;
}

impl TimevalExt for timeval64 {
    fn as_micros(&self) -> i64 {
        timeval_to_duration(self).as_micros() as i64
    }

    fn as_duration(&self) -> Duration {
        timeval_to_duration(self)
    }
}

pub fn geteuid() -> u32 {
    unsafe { libc::geteuid() }
}
pub fn getegid() -> u32 {
    unsafe { libc::getegid() }
}
pub fn getpid() -> i32 {
    unsafe { libc::getpid() }
}
pub fn isatty(fd: i32) -> bool {
    // This returns false if the fd is valid but not a tty, or is invalid.
    // No place we currently call it really cares about the difference.
    return unsafe { libc::isatty(fd) } == 1;
}
