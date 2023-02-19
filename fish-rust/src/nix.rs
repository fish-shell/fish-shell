//! Safe wrappers around various libc functions that we might want to reuse across modules.

use std::time::Duration;

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
