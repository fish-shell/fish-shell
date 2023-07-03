mod fd_monitor;

use std::ffi::CString;

use libc::setlocale;
use libc::LC_ALL;

pub fn make_locale_sensitive() {
    unsafe {
        let locale = CString::new("").unwrap();
        setlocale(LC_ALL, locale.as_ptr());
    }
}
