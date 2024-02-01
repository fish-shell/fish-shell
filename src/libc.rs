use std::sync::atomic::AtomicPtr;

use libc::{c_char, c_int};
use once_cell::sync::Lazy;

pub fn MB_CUR_MAX() -> usize {
    unsafe { C_MB_CUR_MAX() }
}
extern "C" {
    fn C_MB_CUR_MAX() -> usize;
}

pub fn ST_LOCAL() -> u64 {
    unsafe { C_ST_LOCAL() }
}
extern "C" {
    fn C_ST_LOCAL() -> u64;
}

pub fn MNT_LOCAL() -> u64 {
    unsafe { C_MNT_LOCAL() }
}
extern "C" {
    fn C_MNT_LOCAL() -> u64;
}

pub fn _CS_PATH() -> i32 {
    unsafe { C_CS_PATH() }
}
extern "C" {
    fn C_CS_PATH() -> i32;
}

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
            unsafe { $cfn() }
        }
    };
}

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

extern "C" {
    fn C_RLIMIT_SBSIZE() -> i32; // ifndef: -1
    fn C_RLIMIT_CORE() -> i32;
    fn C_RLIMIT_DATA() -> i32;
    fn C_RLIMIT_NICE() -> i32; // ifndef: -1
    fn C_RLIMIT_FSIZE() -> i32;
    fn C_RLIMIT_SIGPENDING() -> i32; // ifndef: -1
    fn C_RLIMIT_MEMLOCK() -> i32; // ifndef: -1
    fn C_RLIMIT_RSS() -> i32; // ifndef: -1
    fn C_RLIMIT_NOFILE() -> i32;
    fn C_RLIMIT_MSGQUEUE() -> i32; // ifndef: -1
    fn C_RLIMIT_RTPRIO() -> i32; // ifndef: -1
    fn C_RLIMIT_STACK() -> i32;
    fn C_RLIMIT_CPU() -> i32;
    fn C_RLIMIT_NPROC() -> i32; // ifndef: -1
    fn C_RLIMIT_AS() -> i32; // ifndef: -1
    fn C_RLIMIT_SWAP() -> i32; // ifndef: -1
    fn C_RLIMIT_RTTIME() -> i32; // ifndef: -1
    fn C_RLIMIT_KQUEUES() -> i32; // ifndef: -1
    fn C_RLIMIT_NPTS() -> i32; // ifndef: -1
    fn C_RLIMIT_NTHR() -> i32; // ifndef: -1
}
