use crate::io::{IoStreams, OutputStreamFfi};
use crate::wchar;
#[rustfmt::skip]
use ::std::pin::Pin;
#[rustfmt::skip]
use ::std::slice;
pub use crate::wait_handle::{WaitHandleRef, WaitHandleStore};
use crate::wchar::prelude::*;
use crate::wchar_ffi::WCharFromFFI;
use autocxx::prelude::*;

// autocxx has been hacked up to know about this.
pub type wchar_t = u32;

include_cpp! {
    #include "autoload.h"
    #include "color.h"
    #include "common.h"
    #include "complete.h"
    #include "env.h"
    #include "env_dispatch.h"
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

    #include "builtins/bind.h"
    #include "builtins/commandline.h"
    #include "builtins/ulimit.h"

    safety!(unsafe_ffi)

    generate_pod!("wcharz_t")
    generate!("wcstring_list_ffi_t")
    generate!("wperror")
    generate!("set_inheriteds_ffi")

    generate!("reader_init")
    generate!("reader_run_count")
    generate!("term_copy_modes")
    generate!("set_profiling_active")
    generate!("reader_read_ffi")
    generate!("fish_is_unwinding_for_exit")
    generate!("restore_term_mode")
    generate!("update_wait_on_escape_ms_ffi")
    generate!("read_generation_count")
    generate!("set_flog_output_file_ffi")
    generate!("flog_setlinebuf_ffi")
    generate!("activate_flog_categories_by_pattern")
    generate!("save_term_foreground_process_group")
    generate!("restore_term_foreground_process_group_for_exit")
    generate!("set_cloexec")
    generate!("env_universal_notifier_t_default_notifier_post_notification_ffi")

    generate!("builtin_bind")
    generate!("builtin_commandline")
    generate!("builtin_ulimit")

    generate!("init_input")

    generate_pod!("pipes_ffi_t")

    generate!("make_pipes_ffi")

    generate!("log_extra_to_flog_file")

    generate!("wgettext_ptr")

    generate!("pretty_printer_t")

    generate!("fd_event_signaller_t")

    generate!("highlight_role_t")
    generate!("highlight_spec_t")

    generate!("rgb_color_t")
    generate_pod!("color24_t")
    generate!("reader_status_count")
    generate!("reader_write_title_ffi")
    generate!("reader_push_ffi")
    generate!("reader_readline_ffi")
    generate!("reader_pop")
    generate!("commandline_get_state_history_ffi")
    generate!("commandline_set_buffer_ffi")
    generate!("commandline_get_state_initialized_ffi")
    generate!("commandline_get_state_text_ffi")
    generate!("completion_apply_to_command_line")

    generate!("get_history_variable_text_ffi")

    generate_pod!("escape_string_style_t")


    generate!("screen_set_midnight_commander_hack")
    generate!("screen_clear_layout_cache_ffi")
    generate!("escape_code_length_ffi")
    generate!("reader_schedule_prompt_repaint")
    generate!("reader_change_history")
    generate!("reader_change_cursor_selection_mode")
    generate!("reader_set_autosuggestion_enabled_ffi")
    generate!("update_wait_on_sequence_key_ms_ffi")
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
/// but this means we can't pass around mutable references to types like Parser.
/// We also don't want to assert that Parser is Unpin.
/// So we just allow constructing a pin from a mutable reference; none of the C++ code.
/// It's worth considering disabling this in cxx; for now we use this trait.
/// Eventually Parser and IoStreams will not require Pin so we just unsafe-it away.
pub trait Repin {
    fn pin(&mut self) -> Pin<&mut Self> {
        unsafe { Pin::new_unchecked(self) }
    }

    fn unpin(self: Pin<&mut Self>) -> &mut Self {
        unsafe { self.get_unchecked_mut() }
    }
}

// Implement Repin for our types.
impl Repin for IoStreams<'_> {}
impl Repin for wcstring_list_ffi_t {}
impl Repin for rgb_color_t {}
impl Repin for OutputStreamFfi<'_> {}

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
