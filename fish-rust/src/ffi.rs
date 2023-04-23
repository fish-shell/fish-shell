use crate::wchar;
use crate::wchar_ffi::WCharToFFI;
#[rustfmt::skip]
use ::std::pin::Pin;
#[rustfmt::skip]
use ::std::slice;
use crate::env::EnvMode;
pub use crate::wait_handle::{
    WaitHandleRef, WaitHandleRefFFI, WaitHandleStore, WaitHandleStoreFFI,
};
use crate::wchar::{wstr, WString};
use crate::wchar_ffi::WCharFromFFI;
use autocxx::prelude::*;
use cxx::SharedPtr;
use libc::pid_t;

// autocxx has been hacked up to know about this.
pub type wchar_t = u32;

include_cpp! {
    #include "builtin.h"
    #include "common.h"
    #include "env.h"
    #include "env_dispatch.h"
    #include "env_universal_common.h"
    #include "event.h"
    #include "fallback.h"
    #include "fds.h"
    #include "fish_indent_common.h"
    #include "flog.h"
    #include "function.h"
    #include "highlight.h"
    #include "io.h"
    #include "kill.h"
    #include "parse_constants.h"
    #include "parser.h"
    #include "parse_util.h"
    #include "path.h"
    #include "proc.h"
    #include "reader.h"
    #include "tokenizer.h"
    #include "wildcard.h"
    #include "wutil.h"

    // We need to block these types so when exposing C++ to Rust.
    block!("WaitHandleStoreFFI")
    block!("WaitHandleRefFFI")

    safety!(unsafe_ffi)

    generate_pod!("wcharz_t")
    generate!("wcstring_list_ffi_t")
    generate!("make_wcharz_vec")
    generate!("make_fd_nonblocking")
    generate!("wperror")

    generate_pod!("pipes_ffi_t")
    generate!("environment_t")
    generate!("env_dispatch_var_change_ffi")
    generate!("env_stack_t")
    generate!("env_var_t")
    generate!("env_universal_t")
    generate!("env_universal_sync_result_t")
    generate!("callback_data_t")
    generate!("universal_notifier_t")
    generate!("var_table_ffi_t")

    generate!("event_list_ffi_t")

    generate!("make_pipes_ffi")

    generate!("get_flog_file_fd")
    generate!("log_extra_to_flog_file")

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

    generate!("highlighter_t")

    generate!("proc_wait_any")

    generate!("output_stream_t")
    generate!("io_streams_t")

    generate_pod!("RustFFIJobList")
    generate_pod!("RustFFIProcList")
    generate_pod!("RustBuiltin")

    generate!("builtin_exists")
    generate!("builtin_missing_argument")
    generate!("builtin_unknown_option")
    generate!("builtin_print_help")
    generate!("builtin_print_error_trailer")
    generate!("builtin_get_names_ffi")

    generate!("pretty_printer_t")

    generate!("escape_string")

    generate!("fd_event_signaller_t")

    generate!("block_t")
    generate!("block_type_t")
    generate!("statuses_t")
    generate!("io_chain_t")

    generate!("env_var_t")

    generate!("path_get_paths_ffi")

    generate!("colorize_shell")
    generate!("reader_status_count")
    generate!("kill_entries_ffi")

    generate!("get_history_variable_text_ffi")
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

    /// Helper to get a variable as a string, using the default flags.
    pub fn var_as_string(&mut self, name: &wstr) -> Option<WString> {
        self.pin().vars().unpin().get_as_string(name)
    }

    pub fn get_var_stack(&mut self) -> &mut env_stack_t {
        self.pin().vars().unpin()
    }

    pub fn get_var_stack_env(&mut self) -> &environment_t {
        self.vars_env_ffi()
    }

    pub fn set_var(&mut self, name: &wstr, value: &[&wstr], flags: EnvMode) -> libc::c_int {
        self.get_var_stack().set_var(name, value, flags)
    }
}

unsafe impl Send for env_universal_t {}

impl environment_t {
    /// Helper to get a variable as a string, using the default flags.
    pub fn get_as_string(&self, name: &wstr) -> Option<WString> {
        self.get_as_string_flags(name, EnvMode::DEFAULT)
    }

    /// Helper to get a variable as a string, using the given flags.
    pub fn get_as_string_flags(&self, name: &wstr, flags: EnvMode) -> Option<WString> {
        self.get_or_null(&name.to_ffi(), flags.bits())
            .as_ref()
            .map(|s| s.as_string().from_ffi())
    }
}

impl env_stack_t {
    /// Helper to get a variable as a string, using the default flags.
    pub fn get_as_string(&self, name: &wstr) -> Option<WString> {
        self.get_as_string_flags(name, EnvMode::DEFAULT)
    }

    /// Helper to get a variable as a string, using the given flags.
    pub fn get_as_string_flags(&self, name: &wstr, flags: EnvMode) -> Option<WString> {
        self.get_or_null(&name.to_ffi(), flags.bits())
            .as_ref()
            .map(|s| s.as_string().from_ffi())
    }

    /// Helper to set a value.
    pub fn set_var(&mut self, name: &wstr, value: &[&wstr], flags: EnvMode) -> libc::c_int {
        use crate::wchar_ffi::{wstr_to_u32string, W0String};
        let strings: Vec<W0String> = value.iter().map(wstr_to_u32string).collect();
        let ptrs: Vec<*const u32> = strings.iter().map(|s| s.as_ptr()).collect();
        self.pin()
            .set_ffi(
                &name.to_ffi(),
                flags.bits(),
                ptrs.as_ptr() as *const c_void,
                ptrs.len(),
            )
            .into()
    }
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

/// Allow wcstring_list_ffi_t to be "into" Vec<WString>.
impl From<&wcstring_list_ffi_t> for Vec<wchar::WString> {
    fn from(w: &wcstring_list_ffi_t) -> Self {
        let mut result = Vec::with_capacity(w.size());
        for i in 0..w.size() {
            result.push(w.at(i).from_ffi());
        }
        result
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
impl Repin for env_universal_t {}
impl Repin for io_streams_t {}
impl Repin for job_t {}
impl Repin for output_stream_t {}
impl Repin for parser_t {}
impl Repin for process_t {}
impl Repin for wcstring_list_ffi_t {}

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

impl core::convert::From<void_ptr> for *const u8 {
    fn from(value: void_ptr) -> Self {
        value.0 as *const _
    }
}

impl core::convert::From<void_ptr> for *const core::ffi::c_void {
    fn from(value: void_ptr) -> Self {
        value.0 as *const _
    }
}

impl core::convert::From<void_ptr> for *const autocxx::c_void {
    fn from(value: void_ptr) -> Self {
        value.0 as *const _
    }
}
