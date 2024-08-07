use std::{ffi::CStr, sync::atomic::AtomicPtr};

use libc::{c_char, c_int, fd_set};
use once_cell::sync::Lazy;

pub static _PATH_BSHELL: AtomicPtr<c_char> = AtomicPtr::new(std::ptr::null_mut());
extern "C" {
    pub fn C_PATH_BSHELL() -> *const c_char;
}

pub static _PC_CASE_SENSITIVE: Lazy<c_int> = Lazy::new(|| unsafe { C_PC_CASE_SENSITIVE() });
extern "C" {
    fn C_PC_CASE_SENSITIVE() -> c_int;
}

extern "C" {
    pub(crate) fn confstr(
        name: libc::c_int,
        buf: *mut libc::c_char,
        len: libc::size_t,
    ) -> libc::size_t;
    pub fn stdout_stream() -> *mut libc::FILE;
    pub fn setlinebuf(stream: *mut libc::FILE);
}

macro_rules! CVAR {
    ($cfn:ident, $cvar:ident, $type:ident) => {
        pub fn $cvar() -> $type {
            extern "C" {
                fn $cfn() -> $type;
            }
            unsafe { $cfn() }
        }
    };
}

CVAR!(C_MB_CUR_MAX, MB_CUR_MAX, usize);
CVAR!(C_ST_LOCAL, ST_LOCAL, u64);
CVAR!(C_MNT_LOCAL, MNT_LOCAL, u64);
CVAR!(C_CS_PATH, _CS_PATH, i32);

CVAR!(C_RLIMIT_SBSIZE, RLIMIT_SBSIZE, i32);
CVAR!(C_RLIMIT_CORE, RLIMIT_CORE, i32);
CVAR!(C_RLIMIT_DATA, RLIMIT_DATA, i32);
CVAR!(C_RLIMIT_NICE, RLIMIT_NICE, i32);
CVAR!(C_RLIMIT_FSIZE, RLIMIT_FSIZE, i32);
CVAR!(C_RLIMIT_SIGPENDING, RLIMIT_SIGPENDING, i32);
CVAR!(C_RLIMIT_MEMLOCK, RLIMIT_MEMLOCK, i32);
CVAR!(C_RLIMIT_RSS, RLIMIT_RSS, i32);
CVAR!(C_RLIMIT_NOFILE, RLIMIT_NOFILE, i32);
CVAR!(C_RLIMIT_MSGQUEUE, RLIMIT_MSGQUEUE, i32);
CVAR!(C_RLIMIT_RTPRIO, RLIMIT_RTPRIO, i32);
CVAR!(C_RLIMIT_STACK, RLIMIT_STACK, i32);
CVAR!(C_RLIMIT_CPU, RLIMIT_CPU, i32);
CVAR!(C_RLIMIT_NPROC, RLIMIT_NPROC, i32);
CVAR!(C_RLIMIT_AS, RLIMIT_AS, i32);
CVAR!(C_RLIMIT_SWAP, RLIMIT_SWAP, i32);
CVAR!(C_RLIMIT_RTTIME, RLIMIT_RTTIME, i32);
CVAR!(C_RLIMIT_KQUEUES, RLIMIT_KQUEUES, i32);
CVAR!(C_RLIMIT_NPTS, RLIMIT_NPTS, i32);
CVAR!(C_RLIMIT_NTHR, RLIMIT_NTHR, i32);

pub(crate) fn readdir64(dirp: *mut libc::DIR) -> Option<(*const c_char, usize, u64, u8)> {
    let mut d_name = unsafe { std::mem::zeroed() };
    let mut d_name_len = unsafe { std::mem::zeroed() };
    let mut d_ino = unsafe { std::mem::zeroed() };
    let mut d_type = unsafe { std::mem::zeroed() };
    if !unsafe { C_readdir64(dirp, &mut d_name, &mut d_name_len, &mut d_ino, &mut d_type) } {
        return None;
    }
    Some((d_name, d_name_len, d_ino, d_type))
}
extern "C" {
    fn C_readdir64(
        dirp: *mut libc::DIR,
        d_name: *mut *const c_char,
        d_name_len: *mut usize,
        d_ino: *mut u64,
        d_type: *mut u8,
    ) -> bool;
}

pub(crate) fn fstatat64(
    dirfd: c_int,
    file: &CStr,
    flag: c_int,
) -> Option<(u64, u64, libc::mode_t)> {
    let mut st_dev = unsafe { std::mem::zeroed() };
    let mut st_ino = unsafe { std::mem::zeroed() };
    let mut st_mode = unsafe { std::mem::zeroed() };
    if !unsafe {
        C_fstatat64(
            dirfd,
            file.as_ptr(),
            flag,
            &mut st_dev,
            &mut st_ino,
            &mut st_mode,
        )
    } {
        return None;
    }
    Some((st_dev, st_ino, st_mode))
}
extern "C" {
    fn C_fstatat64(
        dirfd: c_int,
        file: *const c_char,
        flag: c_int,
        st_dev: *mut u64,
        st_ino: *mut u64,
        st_mode: *mut libc::mode_t,
    ) -> bool;
}

pub(crate) fn localtime64_r(timep: i64) -> Option<libc::tm> {
    let mut timestamp = unsafe { std::mem::zeroed() };
    if !unsafe { C_localtime64_r(timep, &mut timestamp) } {
        return None;
    }
    Some(timestamp)
}
extern "C" {
    fn C_localtime64_r(timep: i64, result: *mut libc::tm) -> bool;
}

#[repr(C)]
#[derive(Clone, Copy)]
pub struct timeval64 {
    pub tv_sec: i64,
    pub tv_usec: i64,
}

pub(crate) fn select64(
    nfds: c_int,
    readfds: *mut fd_set,
    writefds: *mut fd_set,
    errorfds: *mut fd_set,
    timeout: *mut timeval64,
) -> c_int {
    unsafe { C_select64(nfds, readfds, writefds, errorfds, timeout) }
}
extern "C" {
    fn C_select64(
        nfds: c_int,
        readfds: *mut fd_set,
        writefds: *mut fd_set,
        errorfds: *mut fd_set,
        timeout: *mut timeval64,
    ) -> c_int;
}

#[repr(C)]
#[derive(Clone, Copy)]
pub struct timespec64 {
    pub tv_sec: i64,
    pub tv_nsec: i64,
}

pub(crate) fn pselect64(
    nfds: c_int,
    readfds: *mut fd_set,
    writefds: *mut fd_set,
    errorfds: *mut fd_set,
    timeout: *const timespec64,
    sigmask: *const libc::sigset_t,
) -> c_int {
    unsafe { C_pselect64(nfds, readfds, writefds, errorfds, timeout, sigmask) }
}
extern "C" {
    fn C_pselect64(
        nfds: c_int,
        readfds: *mut fd_set,
        writefds: *mut fd_set,
        errorfds: *mut fd_set,
        timeout: *const timespec64,
        sigmask: *const libc::sigset_t,
    ) -> c_int;
}

#[repr(C)]
pub struct rusage64 {
    pub ru_utime: timeval64,
    pub ru_stime: timeval64,
    pub ru_maxrss: u64,
    pub ru_nsignals: u64,
}

pub fn getrusage64(resource: c_int) -> Option<rusage64> {
    let mut rusage = std::mem::MaybeUninit::uninit();
    let result = unsafe { C_getrusage64(resource, rusage.as_mut_ptr()) };
    // getrusage(2) says the syscall can only fail if the dest address is invalid (EFAULT) or if the
    // requested resource type is invalid. Since we're in control of both, we can assume it won't
    // fail. In case it does anyway (e.g. OS where the syscall isn't implemented), we can just
    // return an empty value.
    match result {
        0 => unsafe { Some(rusage.assume_init()) },
        _ => None,
    }
}
extern "C" {
    fn C_getrusage64(resource: c_int, usage: *mut rusage64) -> c_int;
}

pub(crate) fn clock_gettime64(clk_id: libc::clockid_t) -> Option<timespec64> {
    let mut tp = unsafe { std::mem::zeroed() };
    if !unsafe { C_clock_gettime64(clk_id, &mut tp) } {
        return None;
    }
    Some(tp)
}
extern "C" {
    fn C_clock_gettime64(clk_id: libc::clockid_t, tp: *mut timespec64) -> bool;
}

pub(crate) fn futimens64(fd: c_int, time0: timespec64, time1: timespec64) -> bool {
    unsafe { C_futimens64(fd, time0, time1) }
}
extern "C" {
    fn C_futimens64(fd: c_int, time0: timespec64, time1: timespec64) -> bool;
}
