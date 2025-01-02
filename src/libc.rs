use std::sync::atomic::AtomicPtr;

use libc::{c_char, c_int};
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
