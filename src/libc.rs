// src/libc.rs — чисто-Rust замена libc.c

use once_cell::sync::Lazy;
use std::sync::atomic::AtomicPtr; // Ordering здесь не нужен — убираем предупреждение
use libc::{c_char, c_int};
use std::{fs, io, path::{Path, PathBuf}}; // для pure-Rust фоллбэка ниже

// --- _PATH_BSHELL ------------------------------------------------------------

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
// ВАЖНО: делаем опциональным, чтобы не пихать мусор в pathconf на платформах без этой константы.

pub static _PC_CASE_SENSITIVE: Lazy<Option<c_int>> = Lazy::new(|| C_PC_CASE_SENSITIVE());

#[inline]
pub fn C_PC_CASE_SENSITIVE() -> Option<c_int> {
    // На Apple имя есть
    #[cfg(any(target_vendor = "apple"))]
    { Some(libc::_PC_CASE_SENSITIVE as c_int) }

    // На всех остальных такого имени у pathconf нет
    #[cfg(not(any(target_vendor = "apple")))]
    { None }
}

// --- stdout_stream / setlinebuf ---------------------------------------------

pub unsafe fn stdout_stream() -> *mut libc::FILE {
    #[cfg(any(
        target_os = "linux", target_os = "android",
        target_os = "freebsd", target_os = "openbsd", target_os = "netbsd", target_os = "dragonfly"
    ))]
    {
        extern "C" { static mut stdout: *mut libc::FILE; }
        stdout
    }
    #[cfg(any(target_os = "macos", target_os = "ios"))]
    {
        extern "C" { static mut __stdoutp: *mut libc::FILE; }
        __stdoutp
    }
    #[cfg(windows)]
    { std::ptr::null_mut() }
}

pub unsafe fn setlinebuf(stream: *mut libc::FILE) {
    #[cfg(any(unix, target_os = "wasi"))]
    { libc::setvbuf(stream, std::ptr::null_mut(), libc::_IOLBF, 0); }
    #[cfg(windows)]
    { let _ = stream; }
}

// --- MB_CUR_MAX --------------------------------------------------------------

pub fn MB_CUR_MAX() -> usize {
    4 // UTF-8 практический максимум. Подойдёт и для Win.
}

// --- ST_LOCAL / MNT_LOCAL ----------------------------------------------------

pub fn ST_LOCAL() -> u64 {
    #[cfg(any(
        target_os = "macos", target_os = "ios",
        target_os = "freebsd", target_os = "dragonfly", target_os = "openbsd", target_os = "netbsd"
    ))]
    { libc::ST_LOCAL as u64 }
    #[cfg(not(any(
        target_os = "macos", target_os = "ios",
        target_os = "freebsd", target_os = "dragonfly", target_os = "openbsd", target_os = "netbsd"
    )))]
    { 0 }
}

pub fn MNT_LOCAL() -> u64 {
    #[cfg(any(
        target_os = "macos", target_os = "ios",
        target_os = "freebsd", target_os = "dragonfly", target_os = "openbsd", target_os = "netbsd"
    ))]
    { libc::MNT_LOCAL as u64 }
    #[cfg(not(any(
        target_os = "macos", target_os = "ios",
        target_os = "freebsd", target_os = "dragonfly", target_os = "openbsd", target_os = "netbsd"
    )))]
    { 0 }
}

// --- _CS_PATH / confstr ------------------------------------------------------

pub fn _CS_PATH() -> c_int {
    #[cfg(any(
        target_os = "linux", target_os = "android",
        target_os = "freebsd", target_os = "openbsd", target_os = "netbsd", target_os = "dragonfly",
        target_os = "macos", target_os = "ios"
    ))]
    { libc::_CS_PATH as c_int }
    #[cfg(not(any(
        target_os = "linux", target_os = "android",
        target_os = "freebsd", target_os = "openbsd", target_os = "netbsd", target_os = "dragonfly",
        target_os = "macos", target_os = "ios"
    )))]
    { -1 }
}

#[cfg(any(unix, target_os = "wasi"))]
pub use libc::confstr;

#[cfg(windows)]
pub unsafe fn confstr(_name: c_int, _buf: *mut c_char, _len: usize) -> usize { 0 }

// --- RLIMIT_* ----------------------------------------------------------------

#[cfg(windows)]
macro_rules! rlimit_all_neg1 {
    ($($name:ident),* $(,)?) => { $(pub fn $name() -> i32 { -1 })* }
}

#[cfg(any(unix, target_os = "wasi"))]
macro_rules! rlimit_simple {
    ($($name:ident),* $(,)?) => { $(pub fn $name() -> i32 { libc::$name as i32 })* }
}

#[cfg(any(unix, target_os = "wasi"))]
rlimit_simple!(RLIMIT_CORE, RLIMIT_DATA, RLIMIT_FSIZE, RLIMIT_NOFILE, RLIMIT_STACK, RLIMIT_CPU);

#[cfg(any(unix, target_os = "wasi"))]
pub fn RLIMIT_AS() -> i32 {
    #[cfg(any(
        target_os = "linux", target_os = "android",
        target_os = "freebsd", target_os = "openbsd", target_os = "netbsd", target_os = "dragonfly",
        target_os = "macos", target_os = "ios"
    ))] { libc::RLIMIT_AS as i32 } #[cfg(not(any(
        target_os = "linux", target_os = "android",
        target_os = "freebsd", target_os = "openbsd", target_os = "netbsd", target_os = "dragonfly",
        target_os = "macos", target_os = "ios"
    )))] { -1 }
}

#[cfg(any(unix, target_os = "wasi"))]
pub fn RLIMIT_MSGQUEUE() -> i32 {
    #[cfg(any(target_os = "linux", target_os = "android"))] { libc::RLIMIT_MSGQUEUE as i32 }
    #[cfg(not(any(target_os = "linux", target_os = "android")))] { -1 }
}

#[cfg(any(unix, target_os = "wasi"))]
pub fn RLIMIT_RTPRIO() -> i32 {
    #[cfg(any(target_os = "linux", target_os = "android"))] { libc::RLIMIT_RTPRIO as i32 }
    #[cfg(not(any(target_os = "linux", target_os = "android")))] { -1 }
}

#[cfg(any(unix, target_os = "wasi"))]
pub fn RLIMIT_RTTIME() -> i32 {
    #[cfg(any(target_os = "linux", target_os = "android"))] { libc::RLIMIT_RTTIME as i32 }
    #[cfg(not(any(target_os = "linux", target_os = "android")))] { -1 }
}

#[cfg(any(unix, target_os = "wasi"))]
pub fn RLIMIT_KQUEUES() -> i32 {
    #[cfg(target_os = "freebsd")] { libc::RLIMIT_KQUEUES as i32 }
    #[cfg(not(target_os = "freebsd"))] { -1 }
}

#[cfg(any(unix, target_os = "wasi"))]
pub fn RLIMIT_NPTS() -> i32 {
    #[cfg(target_os = "freebsd")] { libc::RLIMIT_NPTS as i32 }
    #[cfg(not(target_os = "freebsd"))] { -1 }
}

#[cfg(any(unix, target_os = "wasi"))]
pub fn RLIMIT_NTHR() -> i32 {
    #[cfg(any(target_os = "freebsd", target_os = "dragonfly"))] { libc::RLIMIT_NTHR as i32 }
    #[cfg(not(any(target_os = "freebsd", target_os = "dragonfly")))] { -1 }
}

#[cfg(any(unix, target_os = "wasi"))]
pub fn RLIMIT_NPROC() -> i32 {
    #[cfg(any(target_os = "linux", target_os = "android", target_os = "freebsd", target_os = "dragonfly", target_os = "openbsd"))]
    { libc::RLIMIT_NPROC as i32 }
    #[cfg(not(any(target_os = "linux", target_os = "android", target_os = "freebsd", target_os = "dragonfly", target_os = "openbsd")))]
    { -1 }
}

#[cfg(any(unix, target_os = "wasi"))]
pub fn RLIMIT_MEMLOCK() -> i32 {
    #[cfg(any(target_os = "linux", target_os = "android", target_os = "freebsd"))]
    { libc::RLIMIT_MEMLOCK as i32 }
    #[cfg(not(any(target_os = "linux", target_os = "android", target_os = "freebsd")))]
    { -1 }
}

#[cfg(any(unix, target_os = "wasi"))]
pub fn RLIMIT_RSS() -> i32 {
    #[cfg(any(target_os = "linux", target_os = "android", target_os = "freebsd", target_os = "dragonfly", target_os = "openbsd", target_os = "netbsd"))]
    { libc::RLIMIT_RSS as i32 }
    #[cfg(not(any(target_os = "linux", target_os = "android", target_os = "freebsd", target_os = "dragonfly", target_os = "openbsd", target_os = "netbsd")))]
    { -1 }
}

#[cfg(any(unix, target_os = "wasi"))]
pub fn RLIMIT_SBSIZE() -> i32 {
    #[cfg(any(target_os = "freebsd", target_os = "dragonfly"))] { libc::RLIMIT_SBSIZE as i32 }
    #[cfg(not(any(target_os = "freebsd", target_os = "dragonfly")))] { -1 }
}

#[cfg(any(unix, target_os = "wasi"))]
pub fn RLIMIT_NICE() -> i32 {
    #[cfg(target_os = "freebsd")] { libc::RLIMIT_NICE as i32 }
    #[cfg(not(target_os = "freebsd"))] { -1 }
}

#[cfg(any(unix, target_os = "wasi"))]
pub fn RLIMIT_SIGPENDING() -> i32 {
    #[cfg(any(target_os = "linux", target_os = "android"))] { libc::RLIMIT_SIGPENDING as i32 }
    #[cfg(not(any(target_os = "linux", target_os = "android")))] { -1 }
}

#[cfg(any(unix, target_os = "wasi"))]
pub fn RLIMIT_SWAP() -> i32 {
    #[cfg(target_os = "freebsd")] { libc::RLIMIT_SWAP as i32 }
    #[cfg(not(target_os = "freebsd"))] { -1 }
}

#[cfg(windows)]
rlimit_all_neg1!(
    RLIMIT_SBSIZE, RLIMIT_CORE, RLIMIT_DATA, RLIMIT_NICE, RLIMIT_FSIZE, RLIMIT_SIGPENDING,
    RLIMIT_MEMLOCK, RLIMIT_RSS, RLIMIT_NOFILE, RLIMIT_MSGQUEUE, RLIMIT_RTPRIO, RLIMIT_STACK,
    RLIMIT_CPU, RLIMIT_NPROC, RLIMIT_AS, RLIMIT_SWAP, RLIMIT_RTTIME, RLIMIT_KQUEUES,
    RLIMIT_NPTS, RLIMIT_NTHR,
);
