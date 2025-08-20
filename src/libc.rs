// src/libc.rs — pure-Rust replacement for libc.c

use libc::{c_char, c_int};
use once_cell::sync::Lazy;
use std::sync::atomic::AtomicPtr; // No need for Ordering here — removes warning

// --- _PATH_BSHELL ------------------------------------------------------------

// Global: holds pointer to shell path, can be updated at runtime.
pub static _PATH_BSHELL: AtomicPtr<c_char> = AtomicPtr::new(std::ptr::null_mut());

pub fn C_PATH_BSHELL() -> *const c_char {
    #[cfg(any(unix, target_os = "wasi"))]
    {
        static BSHELL: &[u8] = b"/bin/sh\0";
        BSHELL.as_ptr() as *const c_char
    }
    #[cfg(windows)]
    {
        static BSHELL: &[u8] = b"/bin/sh\0";
        BSHELL.as_ptr() as *const c_char
    }
}

// --- _PC_CASE_SENSITIVE ------------------------------------------------------
// IMPORTANT: we make it optional, so we don’t clutter pathconf with junk on platforms without this constant.

pub static _PC_CASE_SENSITIVE: Lazy<Option<c_int>> = Lazy::new(C_PC_CASE_SENSITIVE);

#[inline]
pub fn C_PC_CASE_SENSITIVE() -> Option<c_int> {
    // Exists on Apple platforms
    #[cfg(target_vendor = "apple")]
    {
        Some(libc::_PC_CASE_SENSITIVE as c_int)
    }

    // No such name in pathconf on all other platforms
    #[cfg(not(any(target_vendor = "apple")))]
    {
        None
    }
}

/// Returns the pointer to the current stdout FILE object.
///
/// # Safety
///
/// The returned pointer must only be used according to C ABI rules for FILE pointers.
/// Misuse can cause undefined behavior.
pub unsafe fn stdout_stream() -> *mut libc::FILE {
    #[cfg(any(
        target_os = "linux",
        target_os = "android",
        target_os = "freebsd",
        target_os = "openbsd",
        target_os = "netbsd",
        target_os = "dragonfly"
    ))]
    {
        extern "C" {
            static mut stdout: *mut libc::FILE;
        }
        stdout
    }
    #[cfg(any(target_os = "macos", target_os = "ios"))]
    {
        extern "C" {
            static mut __stdoutp: *mut libc::FILE;
        }
        __stdoutp
    }
    #[cfg(windows)]
    {
        std::ptr::null_mut()
    }
}

/// Sets the given FILE stream to line-buffered mode.
///
/// # Safety
///
/// The caller must ensure the FILE pointer is valid and not aliased elsewhere.
pub unsafe fn setlinebuf(stream: *mut libc::FILE) {
    #[cfg(any(unix, target_os = "wasi"))]
    {
        libc::setvbuf(stream, std::ptr::null_mut(), libc::_IOLBF, 0);
    }
    #[cfg(windows)]
    {
        let _ = stream;
    }
}

// --- MB_CUR_MAX --------------------------------------------------------------

// Maximum number of bytes per character (UTF-8 practical maximum).
// Good enough for Windows as well.
pub fn MB_CUR_MAX() -> usize {
    4
}

// --- ST_LOCAL / MNT_LOCAL ----------------------------------------------------

// Filesystem mount flags.
// NOTE: On Darwin (macOS/iOS) there is no ST_LOCAL in libc; use MNT_LOCAL() instead.
pub fn ST_LOCAL() -> u64 {
    // BSDs that actually expose ST_LOCAL in libc
    #[cfg(any(
        target_os = "freebsd",
        target_os = "dragonfly",
        target_os = "openbsd",
        target_os = "netbsd"
    ))]
    {
        libc::ST_LOCAL as u64
    }
    // Darwin does not have ST_LOCAL in libc; return 0 and rely on MNT_LOCAL().
    #[cfg(any(target_os = "macos", target_os = "ios"))]
    {
        0
    }
    // Everything else: no concept of ST_LOCAL.
    #[cfg(not(any(
        target_os = "freebsd",
        target_os = "dragonfly",
        target_os = "openbsd",
        target_os = "netbsd",
        target_os = "macos",
        target_os = "ios"
    )))]
    {
        0
    }
}

// MNT_LOCAL — same as above, for mount table
pub fn MNT_LOCAL() -> u64 {
    #[cfg(any(
        target_os = "macos",
        target_os = "ios",
        target_os = "freebsd",
        target_os = "dragonfly",
        target_os = "openbsd",
        target_os = "netbsd"
    ))]
    {
        libc::MNT_LOCAL as u64
    }
    #[cfg(not(any(
        target_os = "macos",
        target_os = "ios",
        target_os = "freebsd",
        target_os = "dragonfly",
        target_os = "openbsd",
        target_os = "netbsd"
    )))]
    {
        0
    }
}

// --- _CS_PATH / confstr ------------------------------------------------------

// _CS_PATH — system path for utilities, if available
pub fn _CS_PATH() -> c_int {
    #[cfg(any(
        target_os = "linux",
        target_os = "android",
        target_os = "freebsd",
        target_os = "openbsd",
        target_os = "netbsd",
        target_os = "dragonfly",
        target_os = "macos",
        target_os = "ios"
    ))]
    {
        libc::_CS_PATH as c_int
    }
    #[cfg(not(any(
        target_os = "linux",
        target_os = "android",
        target_os = "freebsd",
        target_os = "openbsd",
        target_os = "netbsd",
        target_os = "dragonfly",
        target_os = "macos",
        target_os = "ios"
    )))]
    {
        -1
    }
}

#[cfg(any(unix, target_os = "wasi"))]
pub use libc::confstr;

#[cfg(windows)]
pub unsafe fn confstr(_name: c_int, _buf: *mut c_char, _len: usize) -> usize {
    0
}

// --- RLIMIT_* ----------------------------------------------------------------

// On Windows: all RLIMIT_* constants are missing, stub as -1.
#[cfg(windows)]
macro_rules! rlimit_all_neg1 {
    ($($name:ident),* $(,)?) => { $(pub fn $name() -> i32 { -1 })* }
}

// On Unix: most RLIMIT_* are available, map directly to libc.
#[cfg(any(unix, target_os = "wasi"))]
macro_rules! rlimit_simple {
    ($($name:ident),* $(,)?) => { $(pub fn $name() -> i32 { libc::$name as i32 })* }
}

#[cfg(any(unix, target_os = "wasi"))]
rlimit_simple!(
    RLIMIT_CORE,
    RLIMIT_DATA,
    RLIMIT_FSIZE,
    RLIMIT_NOFILE,
    RLIMIT_STACK,
    RLIMIT_CPU
);

#[cfg(any(unix, target_os = "wasi"))]
// Some RLIMIT_* may not exist everywhere, so handle per-OS
pub fn RLIMIT_AS() -> i32 {
    #[cfg(any(
        target_os = "linux",
        target_os = "android",
        target_os = "freebsd",
        target_os = "openbsd",
        target_os = "netbsd",
        target_os = "dragonfly",
        target_os = "macos",
        target_os = "ios"
    ))]
    {
        libc::RLIMIT_AS as i32
    }
    #[cfg(not(any(
        target_os = "linux",
        target_os = "android",
        target_os = "freebsd",
        target_os = "openbsd",
        target_os = "netbsd",
        target_os = "dragonfly",
        target_os = "macos",
        target_os = "ios"
    )))]
    {
        -1
    }
}

#[cfg(any(unix, target_os = "wasi"))]
pub fn RLIMIT_MSGQUEUE() -> i32 {
    #[cfg(any(target_os = "linux", target_os = "android"))]
    {
        libc::RLIMIT_MSGQUEUE as i32
    }
    #[cfg(not(any(target_os = "linux", target_os = "android")))]
    {
        -1
    }
}

#[cfg(any(unix, target_os = "wasi"))]
pub fn RLIMIT_RTPRIO() -> i32 {
    #[cfg(any(target_os = "linux", target_os = "android"))]
    {
        libc::RLIMIT_RTPRIO as i32
    }
    #[cfg(not(any(target_os = "linux", target_os = "android")))]
    {
        -1
    }
}

#[cfg(any(unix, target_os = "wasi"))]
pub fn RLIMIT_RTTIME() -> i32 {
    #[cfg(any(target_os = "linux", target_os = "android"))]
    {
        libc::RLIMIT_RTTIME as i32
    }
    #[cfg(not(any(target_os = "linux", target_os = "android")))]
    {
        -1
    }
}

#[cfg(any(unix, target_os = "wasi"))]
pub fn RLIMIT_KQUEUES() -> i32 {
    #[cfg(target_os = "freebsd")]
    {
        libc::RLIMIT_KQUEUES as i32
    }
    #[cfg(not(target_os = "freebsd"))]
    {
        -1
    }
}

#[cfg(any(unix, target_os = "wasi"))]
pub fn RLIMIT_NPTS() -> i32 {
    #[cfg(target_os = "freebsd")]
    {
        libc::RLIMIT_NPTS as i32
    }
    #[cfg(not(target_os = "freebsd"))]
    {
        -1
    }
}

#[cfg(any(unix, target_os = "wasi"))]
pub fn RLIMIT_NTHR() -> i32 {
    #[cfg(any(target_os = "freebsd", target_os = "dragonfly"))]
    {
        libc::RLIMIT_NTHR as i32
    }
    #[cfg(not(any(target_os = "freebsd", target_os = "dragonfly")))]
    {
        -1
    }
}

#[cfg(any(unix, target_os = "wasi"))]
pub fn RLIMIT_NPROC() -> i32 {
    #[cfg(any(
        target_os = "linux",
        target_os = "android",
        target_os = "freebsd",
        target_os = "dragonfly",
        target_os = "openbsd"
    ))]
    {
        libc::RLIMIT_NPROC as i32
    }
    #[cfg(not(any(
        target_os = "linux",
        target_os = "android",
        target_os = "freebsd",
        target_os = "dragonfly",
        target_os = "openbsd"
    )))]
    {
        -1
    }
}

#[cfg(any(unix, target_os = "wasi"))]
pub fn RLIMIT_MEMLOCK() -> i32 {
    #[cfg(any(target_os = "linux", target_os = "android", target_os = "freebsd"))]
    {
        libc::RLIMIT_MEMLOCK as i32
    }
    #[cfg(not(any(target_os = "linux", target_os = "android", target_os = "freebsd")))]
    {
        -1
    }
}

#[cfg(any(unix, target_os = "wasi"))]
pub fn RLIMIT_RSS() -> i32 {
    #[cfg(any(
        target_os = "linux",
        target_os = "android",
        target_os = "freebsd",
        target_os = "dragonfly",
        target_os = "openbsd",
        target_os = "netbsd"
    ))]
    {
        libc::RLIMIT_RSS as i32
    }
    #[cfg(not(any(
        target_os = "linux",
        target_os = "android",
        target_os = "freebsd",
        target_os = "dragonfly",
        target_os = "openbsd",
        target_os = "netbsd"
    )))]
    {
        -1
    }
}

#[cfg(any(unix, target_os = "wasi"))]
pub fn RLIMIT_SBSIZE() -> i32 {
    #[cfg(any(target_os = "freebsd", target_os = "dragonfly"))]
    {
        libc::RLIMIT_SBSIZE as i32
    }
    #[cfg(not(any(target_os = "freebsd", target_os = "dragonfly")))]
    {
        -1
    }
}

#[cfg(any(unix, target_os = "wasi"))]
pub fn RLIMIT_NICE() -> i32 {
    #[cfg(target_os = "freebsd")]
    {
        libc::RLIMIT_NICE as i32
    }
    #[cfg(not(target_os = "freebsd"))]
    {
        -1
    }
}

#[cfg(any(unix, target_os = "wasi"))]
pub fn RLIMIT_SIGPENDING() -> i32 {
    #[cfg(any(target_os = "linux", target_os = "android"))]
    {
        libc::RLIMIT_SIGPENDING as i32
    }
    #[cfg(not(any(target_os = "linux", target_os = "android")))]
    {
        -1
    }
}

#[cfg(any(unix, target_os = "wasi"))]
pub fn RLIMIT_SWAP() -> i32 {
    #[cfg(target_os = "freebsd")]
    {
        libc::RLIMIT_SWAP as i32
    }
    #[cfg(not(target_os = "freebsd"))]
    {
        -1
    }
}

// Windows: all RLIMIT_* functions stubbed to -1.
#[cfg(windows)]
rlimit_all_neg1!(
    RLIMIT_SBSIZE,
    RLIMIT_CORE,
    RLIMIT_DATA,
    RLIMIT_NICE,
    RLIMIT_FSIZE,
    RLIMIT_SIGPENDING,
    RLIMIT_MEMLOCK,
    RLIMIT_RSS,
    RLIMIT_NOFILE,
    RLIMIT_MSGQUEUE,
    RLIMIT_RTPRIO,
    RLIMIT_STACK,
    RLIMIT_CPU,
    RLIMIT_NPROC,
    RLIMIT_AS,
    RLIMIT_SWAP,
    RLIMIT_RTTIME,
    RLIMIT_KQUEUES,
    RLIMIT_NPTS,
    RLIMIT_NTHR,
);
