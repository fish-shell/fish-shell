//! Safe wrappers around various libc functions that we might want to reuse across modules.

use std::time::Duration;

use crate::wutil::perror;
use libc::mode_t;

#[allow(clippy::unnecessary_cast)]
pub const fn timeval_to_duration(val: &libc::timeval) -> Duration {
    let micros = val.tv_sec as i64 * (1E6 as i64) + val.tv_usec as i64;
    Duration::from_micros(micros as u64)
}

pub trait TimevalExt {
    fn as_micros(&self) -> i64;
    fn as_duration(&self) -> Duration;
}

impl TimevalExt for libc::timeval {
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
pub fn getpgrp() -> i32 {
    unsafe { libc::getpgrp() }
}
pub fn getpid() -> i32 {
    unsafe { libc::getpid() }
}
pub fn isatty(fd: i32) -> bool {
    // This returns false if the fd is valid but not a tty, or is invalid.
    // No place we currently call it really cares about the difference.
    return unsafe { libc::isatty(fd) } == 1;
}

/// An enumeration of supported libc rusage types used by [`getrusage()`].
/// NB: RUSAGE_THREAD is not supported on macOS.
pub enum RUsage {
    RSelf, // "Self" is a reserved keyword
    RChildren,
}

/// A safe wrapper around `libc::getrusage()`.
pub fn getrusage(resource: RUsage) -> libc::rusage {
    let mut rusage = std::mem::MaybeUninit::uninit();
    let result = unsafe {
        libc::getrusage(
            match resource {
                RUsage::RSelf => libc::RUSAGE_SELF,
                RUsage::RChildren => libc::RUSAGE_CHILDREN,
            },
            rusage.as_mut_ptr(),
        )
    };

    // getrusage(2) says the syscall can only fail if the dest address is invalid (EFAULT) or if the
    // requested resource type is invalid. Since we're in control of both, we can assume it won't
    // fail. In case it does anyway (e.g. OS where the syscall isn't implemented), we can just
    // return an empty value.
    match result {
        0 => unsafe { rusage.assume_init() },
        _ => {
            perror("getrusage");
            unsafe { std::mem::zeroed() }
        }
    }
}

pub fn umask(mask: mode_t) -> mode_t {
    unsafe { libc::umask(mask) }
}
