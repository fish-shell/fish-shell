use crate::wchar;
#[rustfmt::skip]
use ::std::pin::Pin;
#[rustfmt::skip]
use ::std::slice;
use autocxx::prelude::*;
use cxx::SharedPtr;

// autocxx has been hacked up to know about this.
pub type wchar_t = u32;

include_cpp! {
    #include "fds.h"
    #include "wutil.h"
    #include "flog.h"
    #include "io.h"
    #include "parse_util.h"
    #include "wildcard.h"
    #include "tokenizer.h"
    #include "parser.h"
    #include "proc.h"
    #include "common.h"
    #include "builtin.h"
    #include "fallback.h"
    #include "event.h"

    safety!(unsafe_ffi)

    generate_pod!("wcharz_t")
    generate!("make_fd_nonblocking")
    generate!("wperror")

    generate_pod!("pipes_ffi_t")
    generate!("make_pipes_ffi")

    generate!("valid_var_name_char")

    generate!("get_flog_file_fd")

    generate!("parse_util_unescape_wildcards")

    generate!("fish_wcwidth")
    generate!("fish_wcswidth")

    generate!("wildcard_match")
    generate!("wgettext_ptr")

    generate!("parser_t")
    generate!("job_t")
    generate!("process_t")
    generate!("library_data_t")

    generate!("proc_wait_any")

    generate!("output_stream_t")
    generate!("io_streams_t")

    generate_pod!("RustFFIJobList")
    generate_pod!("RustFFIProcList")
    generate_pod!("RustBuiltin")

    generate!("builtin_missing_argument")
    generate!("builtin_unknown_option")
    generate!("builtin_print_help")
    generate!("builtin_print_error_trailer")

    generate!("wait_handle_t")
    generate!("wait_handle_store_t")

    generate!("event_fire_generic")

    generate!("escape_string")
    generate!("sig2wcs")
    generate!("wcs2sig")
    generate!("signal_get_desc")
}

impl parser_t {
    pub fn get_jobs(&self) -> &[SharedPtr<job_t>] {
        let ffi_jobs = self.ffi_jobs();
        unsafe { slice::from_raw_parts(ffi_jobs.jobs, ffi_jobs.count) }
    }
}

impl job_t {
    #[allow(clippy::mut_from_ref)]
    pub fn get_procs(&self) -> &mut [UniquePtr<process_t>] {
        let ffi_procs = self.ffi_processes();
        unsafe { slice::from_raw_parts_mut(ffi_procs.procs, ffi_procs.count) }
    }
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

/// A bogus trait for turning &mut Foo into Pin<&mut Foo>.
/// autocxx enforces that non-const methods must be called through Pin,
/// but this means we can't pass around mutable references to types like parser_t.
/// We also don't want to assert that parser_t is Unpin.
/// So we just allow constructing a pin from a mutable reference; none of the C++ code.
/// It's worth considering disabling this in cxx; for now we use this trait.
/// Eventually parser_t and io_streams_t will not require Pin so we just unsafe-it away.
pub trait Repin {
    fn pin(&mut self) -> Pin<&mut Self> {
        unsafe { Pin::new_unchecked(self) }
    }

    fn unpin(self: Pin<&mut Self>) -> &mut Self {
        unsafe { self.get_unchecked_mut() }
    }
}

// Implement Repin for our types.
impl Repin for parser_t {}
impl Repin for job_t {}
impl Repin for process_t {}
impl Repin for io_streams_t {}
impl Repin for output_stream_t {}

pub use autocxx::c_int;
pub use ffi::*;
pub use libc::c_char;
