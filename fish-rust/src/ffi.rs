use crate::wchar::{self};
use ::std::slice;
use autocxx::prelude::*;

// autocxx has been hacked up to know about this.
pub type wchar_t = u32;

include_cpp! {
    #include "fds.h"
    #include "wutil.h"
    #include "flog.h"
    #include "io.h"
    #include "parse_util.h"
    #include "wildcard.h"
    #include "parser.h"
    #include "proc.h"
    #include "common.h"
    #include "builtin.h"

    safety!(unsafe_ffi)

    generate_pod!("wcharz_t")
    generate!("make_fd_nonblocking")
    generate!("wperror")

    generate_pod!("pipes_ffi_t")
    generate!("make_pipes_ffi")

    generate!("get_flog_file_fd")

    generate!("parse_util_unescape_wildcards")

    generate!("wildcard_match")

}

/// Allow wcharz_t to be "into" wstr.
impl From<wcharz_t> for &wchar::wstr {
    fn from(w: wcharz_t) -> Self {
        let len = w.length();
        let v = unsafe { slice::from_raw_parts(w.str_ as *const u32, len) };
        wchar::wstr::from_slice(v).expect("Invalid UTF-32")
    }
}

/// Allow wcharz_t to be "into" WString.
impl From<wcharz_t> for wchar::WString {
    fn from(w: wcharz_t) -> Self {
        let len = w.length();
        let v = unsafe { slice::from_raw_parts(w.str_ as *const u32, len).to_vec() };
        Self::from_vec(v).expect("Invalid UTF-32")
    }
}

pub use autocxx::c_int;
pub use ffi::*;
pub use libc::c_char;
