use crate::wchar_ffi::WCharToFFI;
#[rustfmt::skip]
use ::std::pin::Pin;
#[rustfmt::skip]
use ::std::slice;
use crate::env::{EnvMode, EnvStackRef, EnvStackRefFFI};
use crate::job_group::JobGroup;
pub use crate::wait_handle::{
    WaitHandleRef, WaitHandleRefFFI, WaitHandleStore, WaitHandleStoreFFI,
};
use crate::wchar::prelude::*;
use crate::wchar_ffi::WCharFromFFI;
use autocxx::prelude::*;
use cxx::SharedPtr;
use libc::pid_t;

// autocxx has been hacked up to know about this.
pub type wchar_t = u32;

include_cpp! {
    #include "autoload.h"
    #include "builtin.h"
    #include "color.h"
    #include "common.h"
    #include "complete.h"
    #include "env.h"
    #include "env_universal_common.h"
    #include "event.h"
    #include "exec.h"
    #include "fallback.h"
    #include "fds.h"
    #include "fish_indent_common.h"
    #include "flog.h"
    #include "function.h"
    #include "highlight.h"
    #include "history.h"
    #include "io.h"
    #include "input_common.h"
    #include "input.h"
    #include "parse_constants.h"
    #include "parser.h"
    #include "parse_util.h"
    #include "path.h"
    #include "proc.h"
    #include "reader.h"
    #include "screen.h"
    #include "tokenizer.h"
    #include "wutil.h"

    // We need to block these types so when exposing C++ to Rust.
    block!("WaitHandleStoreFFI")
    block!("WaitHandleRefFFI")

    safety!(unsafe_ffi)

    generate_pod!("wcharz_t")
    generate!("wcstring_list_ffi_t")
    generate!("wperror")
    generate!("set_inheriteds_ffi")

    generate!("proc_init")
    generate!("misc_init")
    generate!("reader_init")
    generate!("term_copy_modes")
    generate!("set_profiling_active")
    generate!("reader_read_ffi")
    generate!("restore_term_mode")
    generate!("parse_util_detect_errors_ffi")
    generate!("set_flog_output_file_ffi")
    generate!("flog_setlinebuf_ffi")
    generate!("activate_flog_categories_by_pattern")
    generate!("save_term_foreground_process_group")
    generate!("restore_term_foreground_process_group_for_exit")
    generate!("set_cloexec")

    generate!("init_input")

    generate_pod!("pipes_ffi_t")
    generate!("environment_t")
    generate!("env_stack_t")
    generate!("env_var_t")
    generate!("env_universal_t")
    generate!("env_universal_sync_result_t")
    generate!("callback_data_t")
    generate!("universal_notifier_t")
    generate!("var_table_ffi_t")

    generate!("event_list_ffi_t")

    generate!("make_pipes_ffi")

    generate!("log_extra_to_flog_file")

    generate!("wgettext_ptr")

    generate!("block_t")
    generate!("parser_t")

    generate!("job_t")
    generate!("job_control_t")
    generate!("get_job_control_mode")
    generate!("set_job_control_mode")
    generate!("get_login")
    generate!("mark_login")
    generate!("mark_no_exec")
    generate!("process_t")
    generate!("library_data_t")
    generate_pod!("library_data_pod_t")

    generate!("highlighter_t")

    generate!("proc_wait_any")

    generate!("output_stream_t")
    generate!("io_streams_t")
    generate!("make_null_io_streams_ffi")
    generate!("make_test_io_streams_ffi")
    generate!("get_test_output_ffi")

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

    generate!("exec_subshell_ffi")

    generate!("rgb_color_t")
    generate_pod!("color24_t")
    generate!("colorize_shell")
    generate!("reader_status_count")

    generate!("get_history_variable_text_ffi")

    generate!("is_interactive_session")
    generate!("set_interactive_session")
    generate!("screen_set_midnight_commander_hack")
    generate!("screen_clear_layout_cache_ffi")
    generate!("escape_code_length_ffi")
    generate!("reader_schedule_prompt_repaint")
    generate!("reader_change_history")
    generate!("history_session_id")
    generate!("history_save_all")
    generate!("reader_change_cursor_selection_mode")
    generate!("reader_set_autosuggestion_enabled_ffi")
    generate!("complete_invalidate_path")
    generate!("complete_add_wrapper")
    generate!("update_wait_on_escape_ms_ffi")
    generate!("update_wait_on_sequence_key_ms_ffi")
    generate!("autoload_t")
    generate!("make_autoload_ffi")
    generate!("perform_autoload_ffi")
    generate!("complete_get_wrap_targets_ffi")

    generate!("is_thompson_shell_script")
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

    pub fn get_vars(&mut self) -> EnvStackRef {
        self.pin().vars().from_ffi()
    }

    pub fn get_func_name(&mut self, level: i32) -> Option<WString> {
        let name = self.pin().get_function_name_ffi(c_int(level));
        name.as_ref()
            .map(|s| s.from_ffi())
            .filter(|s| !s.is_empty())
    }
}

unsafe impl Send for env_universal_t {}

impl env_stack_t {
    /// Access the underlying Rust environment stack.
    #[allow(clippy::borrowed_box)]
    pub fn from_ffi(&self) -> EnvStackRef {
        // Safety: get_impl_ffi returns a pointer to a Box<EnvStackRefFFI>.
        let envref = self.get_impl_ffi();
        assert!(!envref.is_null());
        let env: &Box<EnvStackRefFFI> = unsafe { &*(envref.cast()) };
        env.0.clone()
    }
}

impl environment_t {
    /// Helper to get a variable as a string, using the default flags.
    pub fn get_as_string(&self, name: &wstr) -> Option<WString> {
        self.get_as_string_flags(name, EnvMode::default())
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
        self.get_as_string_flags(name, EnvMode::default())
    }

    /// Helper to get a variable as a string, using the given flags.
    pub fn get_as_string_flags(&self, name: &wstr, flags: EnvMode) -> Option<WString> {
        self.get_or_null(&name.to_ffi(), flags.bits())
            .as_ref()
            .map(|s| s.as_string().from_ffi())
    }

    /// Helper to set a value.
    pub fn set_var<T: AsRef<wstr>, U: AsRef<wstr>>(
        &mut self,
        name: T,
        value: &[U],
        flags: EnvMode,
    ) -> libc::c_int {
        use crate::wchar_ffi::{wstr_to_u32string, W0String};
        let strings: Vec<W0String> = value.iter().map(wstr_to_u32string).collect();
        let ptrs: Vec<*const u32> = strings.iter().map(|s| s.as_ptr()).collect();
        self.pin()
            .set_ffi(
                &name.as_ref().to_ffi(),
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

    pub fn get_job_group(&self) -> &JobGroup {
        unsafe { ::std::mem::transmute::<&job_group_t, &JobGroup>(self.ffi_group()) }
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
impl From<wcharz_t> for &wstr {
    fn from(w: wcharz_t) -> Self {
        let len = w.length();
        #[allow(clippy::unnecessary_cast)]
        let v = unsafe { slice::from_raw_parts(w.str_ as *const u32, len) };
        wstr::from_slice(v).expect("Invalid UTF-32")
    }
}

/// Allow wcharz_t to be "into" WString.
impl From<wcharz_t> for WString {
    fn from(w: wcharz_t) -> Self {
        let w: &wstr = w.into();
        w.to_owned()
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
impl Repin for autoload_t {}
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

impl TryFrom<&wstr> for job_control_t {
    type Error = ();

    fn try_from(value: &wstr) -> Result<Self, Self::Error> {
        if value == "full" {
            Ok(job_control_t::all)
        } else if value == "interactive" {
            Ok(job_control_t::interactive)
        } else if value == "none" {
            Ok(job_control_t::none)
        } else {
            Err(())
        }
    }
}
