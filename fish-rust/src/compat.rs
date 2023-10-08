use std::sync::atomic::AtomicPtr;

use libc::c_int;
use once_cell::sync::Lazy;

#[allow(non_snake_case)]
pub fn MB_CUR_MAX() -> usize {
    unsafe { C_MB_CUR_MAX() }
}

pub fn cur_term() -> bool {
    unsafe { has_cur_term() }
}

#[allow(non_snake_case)]
pub fn ST_LOCAL() -> u64 {
    unsafe { C_ST_LOCAL() }
}

#[allow(non_snake_case)]
pub fn MNT_LOCAL() -> u64 {
    unsafe { C_MNT_LOCAL() }
}

#[allow(non_snake_case)]
pub fn _CS_PATH() -> i32 {
    unsafe { C_CS_PATH() }
}

#[allow(non_snake_case)]
pub static _PATH_BSHELL: AtomicPtr<i8> = AtomicPtr::new(std::ptr::null_mut());

#[allow(non_snake_case)]
pub static _PC_CASE_SENSITIVE: Lazy<c_int> = Lazy::new(|| unsafe { C_PC_CASE_SENSITIVE() });

extern "C" {
    fn C_MB_CUR_MAX() -> usize;
    fn has_cur_term() -> bool;
    fn C_ST_LOCAL() -> u64;
    fn C_MNT_LOCAL() -> u64;
    fn C_CS_PATH() -> i32;
    pub(crate) fn confstr(
        name: libc::c_int,
        buf: *mut libc::c_char,
        len: libc::size_t,
    ) -> libc::size_t;
    pub fn C_PATH_BSHELL() -> *const i8;
    fn C_PC_CASE_SENSITIVE() -> c_int;
    pub fn C_O_EXLOCK() -> c_int;
    pub fn stdout_stream() -> *mut libc::FILE;
    pub fn UVAR_FILE_SET_MTIME_HACK() -> bool;
}
