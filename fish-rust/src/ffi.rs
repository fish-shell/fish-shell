use crate::wchar;
use crate::wchar_ffi::WCharToFFI;
#[rustfmt::skip]
use ::std::fmt::{self, Debug, Formatter};
#[rustfmt::skip]
use ::std::pin::Pin;
#[rustfmt::skip]
use ::std::slice;
pub use crate::wait_handle::{
    WaitHandleRef, WaitHandleRefFFI, WaitHandleStore, WaitHandleStoreFFI,
};
use crate::wchar::wstr;
use autocxx::prelude::*;
use cxx::SharedPtr;
use libc::pid_t;

// autocxx has been hacked up to know about this.
pub type wchar_t = u32;

include_cpp! {
    #include "builtin.h"
    #include "common.h"
    #include "env.h"
    #include "event.h"
    #include "fallback.h"
    #include "fds.h"
    #include "flog.h"
    #include "io.h"
    #include "parse_constants.h"
    #include "parser.h"
    #include "parse_util.h"
    #include "proc.h"
    #include "re.h"
    #include "tokenizer.h"
    #include "wildcard.h"
    #include "wutil.h"
    #include "termsize.h"

    // We need to block these types so when exposing C++ to Rust.
    block!("WaitHandleStoreFFI")
    block!("WaitHandleRefFFI")

    safety!(unsafe_ffi)

    generate_pod!("wcharz_t")
    generate!("make_fd_nonblocking")
    generate!("wperror")

    generate_pod!("pipes_ffi_t")
    generate!("env_stack_t")
    generate!("make_pipes_ffi")

    generate!("valid_var_name_char")

    generate!("get_flog_file_fd")

    generate!("parse_util_unescape_wildcards")

    generate!("fish_wcwidth")
    generate!("fish_wcswidth")

    generate!("wildcard_match")
    generate!("wgettext_ptr")

    generate!("block_t")
    generate!("parser_t")

    generate!("job_t")
    generate!("process_t")
    generate!("library_data_t")
    generate_pod!("library_data_pod_t")

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

    generate!("escape_string")
    generate!("sig2wcs")
    generate!("wcs2sig")
    generate!("signal_get_desc")

    generate!("fd_event_signaller_t")

    generate_pod!("re::flags_t")
    generate_pod!("re::re_error_t")
    generate!("re::regex_t")
    generate!("re::regex_result_ffi")
    generate!("re::try_compile_ffi")
    generate!("wcs2string")
    generate!("str2wcstring")

    generate!("signal_handle")
    generate!("signal_check_cancel")

    generate!("block_t")
    generate!("block_type_t")
    generate!("statuses_t")
    generate!("io_chain_t")

    generate!("termsize_container_t")
    generate!("env_var_t")
}

impl parser_t {
    pub fn get_wait_handles_mut(&mut self) -> &mut WaitHandleStore {
        let ptr = self.get_wait_handles_void() as *mut Box<WaitHandleStoreFFI>;
        assert!(!ptr.is_null());
        unsafe { (*ptr).from_ffi_mut() }
    }

    pub fn get_wait_handles(&self) -> &WaitHandleStore {
        let ptr = self.get_wait_handles_void() as *const Box<WaitHandleStoreFFI>;
        assert!(!ptr.is_null());
        unsafe { (*ptr).from_ffi() }
    }

    pub fn get_block_at_index(&self, i: usize) -> Option<&block_t> {
        let b = self.block_at_index(i);
        unsafe { b.as_ref() }
    }

    pub fn get_jobs(&self) -> &[SharedPtr<job_t>] {
        let ffi_jobs = self.ffi_jobs();
        unsafe { slice::from_raw_parts(ffi_jobs.jobs, ffi_jobs.count) }
    }

    pub fn libdata_pod(&mut self) -> &mut library_data_pod_t {
        let libdata = self.pin().ffi_libdata_pod();

        unsafe { &mut *libdata }
    }

    pub fn remove_var(&mut self, var: &wstr, flags: c_int) -> c_int {
        self.pin().remove_var_ffi(&var.to_ffi(), flags)
    }

    pub fn job_get_from_pid(&self, pid: pid_t) -> Option<&job_t> {
        let job = self.ffi_job_get_from_pid(pid.into());
        unsafe { job.as_ref() }
    }
}

pub fn try_compile(anchored: &wstr, flags: &re::flags_t) -> Pin<Box<re::regex_result_ffi>> {
    re::try_compile_ffi(&anchored.to_ffi(), flags).within_box()
}

impl job_t {
    #[allow(clippy::mut_from_ref)]
    pub fn get_procs(&self) -> &mut [UniquePtr<process_t>] {
        let ffi_procs = self.ffi_processes();
        unsafe { slice::from_raw_parts_mut(ffi_procs.procs, ffi_procs.count) }
    }
}

impl process_t {
    /// \return the wait handle for the process, if it exists.
    pub fn get_wait_handle(&self) -> Option<WaitHandleRef> {
        let handle_ptr = self.get_wait_handle_void() as *const Box<WaitHandleRefFFI>;
        if handle_ptr.is_null() {
            None
        } else {
            let handle: &WaitHandleRefFFI = unsafe { &*handle_ptr };
            Some(handle.from_ffi().clone())
        }
    }

    /// \return the wait handle for the process, creating it if necessary.
    pub fn make_wait_handle(&mut self, jid: u64) -> Option<WaitHandleRef> {
        let handle_ref = self.pin().make_wait_handle_void(jid) as *const Box<WaitHandleRefFFI>;
        if handle_ref.is_null() {
            None
        } else {
            let handle: &WaitHandleRefFFI = unsafe { &*handle_ref };
            Some(handle.from_ffi().clone())
        }
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

impl Debug for re::regex_t {
    fn fmt(&self, f: &mut Formatter<'_>) -> fmt::Result {
        f.write_str("regex_t")
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
impl Repin for block_t {}
impl Repin for env_stack_t {}
impl Repin for io_streams_t {}
impl Repin for job_t {}
impl Repin for output_stream_t {}
impl Repin for parser_t {}
impl Repin for process_t {}
impl Repin for re::regex_result_ffi {}

unsafe impl Send for re::regex_t {}

pub use autocxx::c_int;
pub use ffi::*;
pub use libc::c_char;

/// A version of [`* const core::ffi::c_void`] (or [`* const libc::c_void`], if you prefer) that
/// implements `Copy` and `Clone`, because those two don't. Used to represent a `void *` ptr for ffi
/// purposes.
#[repr(transparent)]
#[derive(Copy, Clone)]
pub struct void_ptr(pub *const core::ffi::c_void);

impl core::fmt::Debug for void_ptr {
    fn fmt(&self, f: &mut core::fmt::Formatter<'_>) -> core::fmt::Result {
        write!(f, "{:p}", &self.0)
    }
}

unsafe impl Send for void_ptr {}
unsafe impl Sync for void_ptr {}

impl core::convert::From<*const core::ffi::c_void> for void_ptr {
    fn from(value: *const core::ffi::c_void) -> Self {
        Self(value as *const _)
    }
}

impl core::convert::From<*const u8> for void_ptr {
    fn from(value: *const u8) -> Self {
        Self(value as *const _)
    }
}

impl core::convert::From<*const autocxx::c_void> for void_ptr {
    fn from(value: *const autocxx::c_void) -> Self {
        Self(value as *const _)
    }
}
