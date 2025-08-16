use std::sync::atomic::AtomicPtr;

use libc::c_char;

pub static _PATH_BSHELL: AtomicPtr<c_char> = AtomicPtr::new(std::ptr::null_mut());
extern "C" {
    pub fn C_PATH_BSHELL() -> *const c_char;
}
