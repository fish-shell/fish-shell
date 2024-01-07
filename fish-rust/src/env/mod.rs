mod env_ffi;
pub mod environment;
mod environment_impl;
pub mod var;

use crate::common::ToCString;
pub use env_ffi::{EnvDynFFI, EnvStackRefFFI, EnvStackSetResult};
pub use environment::*;
use std::sync::{
    atomic::{AtomicBool, AtomicUsize},
    Mutex,
};
pub use var::*;

/// Limit `read` to 100 MiB (bytes, not wide chars) by default. This can be overridden with the
/// `fish_read_limit` variable.
pub const DEFAULT_READ_BYTE_LIMIT: usize = 100 * 1024 * 1024;

/// The actual `read` limit in effect, defaulting to [`DEFAULT_READ_BYTE_LIMIT`] but overridable
/// with `$fish_read_limit`.
#[no_mangle]
pub static READ_BYTE_LIMIT: AtomicUsize = AtomicUsize::new(DEFAULT_READ_BYTE_LIMIT);

/// The curses `cur_term` TERMINAL pointer has been set up.
#[no_mangle]
pub static CURSES_INITIALIZED: AtomicBool = AtomicBool::new(false);

/// Does the terminal have the "eat new line" glitch.
#[no_mangle]
pub static TERM_HAS_XN: AtomicBool = AtomicBool::new(false);

static SETENV_LOCK: Mutex<()> = Mutex::new(());

mod ffi {
    extern "C" {
        pub fn setenv(
            name: *const libc::c_char,
            value: *const libc::c_char,
            overwrite: libc::c_int,
        );
        pub fn unsetenv(name: *const libc::c_char);
    }
}

/// Sets an environment variable after obtaining a lock, to try and improve the safety of
/// environment variables.
///
/// As values could contain non-unicode characters, they must first be converted from &wstr to a
/// `CString` with [`crate::common::wcs2zstring()`].
pub fn setenv_lock<S1: ToCString, S2: ToCString>(name: S1, value: S2, overwrite: bool) {
    let name = name.to_cstring();
    let value = value.to_cstring();
    let _lock = SETENV_LOCK.lock();
    unsafe {
        self::ffi::setenv(name.as_ptr(), value.as_ptr(), libc::c_int::from(overwrite));
    }
}

/// Unsets an environment variable after obtaining a lock, to try and improve the safety of
/// environment variables.
pub fn unsetenv_lock<S: ToCString>(name: S) {
    let name = name.to_cstring();
    let _lock = SETENV_LOCK.lock();
    unsafe {
        self::ffi::unsetenv(name.as_ptr());
    }
}
