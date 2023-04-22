use once_cell::sync::Lazy;
use std::ffi::CStr;

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
pub static _PATH_BSHELL: Lazy<&'static CStr> =
    Lazy::new(|| unsafe { CStr::from_ptr(C_PATH_BSHELL()) });

extern "C" {
    fn C_MB_CUR_MAX() -> usize;
    fn has_cur_term() -> bool;
    fn C_ST_LOCAL() -> u64;
    fn C_MNT_LOCAL() -> u64;
    fn C_PATH_BSHELL() -> *const i8;
}
