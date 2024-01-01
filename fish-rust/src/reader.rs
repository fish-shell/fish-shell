use std::sync::atomic::AtomicI32;

use crate::common::{escape_string, EscapeFlags, EscapeStringStyle};
use crate::complete::{CompleteFlags, CompletionList};
use crate::env::Environment;
use crate::expand::{expand_string, ExpandFlags, ExpandResultCode};
use crate::ffi;
use crate::global_safety::RelaxedAtomicBool;
use crate::operation_context::OperationContext;
use crate::parser::Parser;
use crate::signal::signal_check_cancel;
use crate::wchar::prelude::*;
use crate::wchar_ffi::{WCharFromFFI, WCharToFFI};
use crate::wcstringutil::count_preceding_backslashes;
use crate::wildcard::wildcard_has;
use cxx::{CxxWString, UniquePtr};
use libc::SIGINT;
use std::os::fd::RawFd;
use std::sync::atomic::Ordering;

#[derive(Default)]
pub struct ReaderConfig {
    /// Left prompt command, typically fish_prompt.
    pub left_prompt_cmd: WString,

    /// Right prompt command, typically fish_right_prompt.
    pub right_prompt_cmd: WString,

    /// Name of the event to trigger once we're set up.
    pub event: &'static wstr,

    /// Whether tab completion is OK.
    pub complete_ok: bool,

    /// Whether to perform syntax highlighting.
    pub highlight_ok: bool,

    /// Whether to perform syntax checking before returning.
    pub syntax_check_ok: bool,

    /// Whether to allow autosuggestions.
    pub autosuggest_ok: bool,

    /// Whether to expand abbreviations.
    pub expand_abbrev_ok: bool,

    /// Whether to exit on interrupt (^C).
    pub exit_on_interrupt: bool,

    /// If set, do not show what is typed.
    pub in_silent_mode: bool,

    /// The fd for stdin, default to actual stdin.
    pub inputfd: RawFd,
}

pub fn reader_push(parser: &Parser, history_name: &wstr, conf: ReaderConfig) {
    ffi::reader_push_ffi(
        parser as *const Parser as *const autocxx::c_void,
        &history_name.to_ffi(),
        &conf as *const ReaderConfig as *const autocxx::c_void,
    );
}

pub fn reader_readline(nchars: i32) -> Option<WString> {
    let mut line = L!("").to_ffi();
    if ffi::reader_readline_ffi(line.pin_mut(), autocxx::c_int(nchars)) {
        Some(line.from_ffi())
    } else {
        None
    }
}

pub fn reader_pop() {
    ffi::reader_pop()
}

pub fn reader_write_title(cmd: &wstr, parser: &Parser, reset_cursor_position: bool /*=true*/) {
    ffi::reader_write_title_ffi(
        &cmd.to_ffi(),
        parser as *const Parser as *const autocxx::c_void,
        reset_cursor_position,
    );
}

/// This variable is set to a signal by the signal handler when ^C is pressed.
static INTERRUPTED: AtomicI32 = AtomicI32::new(0);

/// The readers interrupt signal handler. Cancels all currently running blocks.
/// This is called from a signal handler!
pub fn reader_handle_sigint() {
    INTERRUPTED.store(SIGINT, Ordering::Relaxed);
}

/// Clear the interrupted flag unconditionally without handling anything. The flag could have been
/// set e.g. when an interrupt arrived just as we were ending an earlier \c reader_readline
/// invocation but before the \c is_interactive_read flag was cleared.
pub fn reader_reset_interrupted() {
    INTERRUPTED.store(0, Ordering::Relaxed);
}

/// Return the value of the interrupted flag, which is set by the sigint handler, and clear it if it
/// was set. In practice this will return 0 or SIGINT.
fn reader_test_and_clear_interrupted() -> i32 {
    let res = INTERRUPTED.load(Ordering::Relaxed);
    if res != 0 {
        INTERRUPTED.store(0, Ordering::Relaxed);
    };
    res
}

/// If set, SIGHUP has been received. This latches to true.
/// This is set from a signal handler.
static SIGHUP_RECEIVED: RelaxedAtomicBool = RelaxedAtomicBool::new(false);

/// Mark that we encountered SIGHUP and must (soon) exit. This is invoked from a signal handler.
pub fn reader_sighup() {
    // Beware, we may be in a signal handler.
    SIGHUP_RECEIVED.store(true);
}

fn reader_received_sighup() -> bool {
    SIGHUP_RECEIVED.load()
}

#[repr(u8)]
pub enum CursorSelectionMode {
    Exclusive = 0,
    Inclusive = 1,
}

pub fn check_autosuggestion_enabled(vars: &dyn Environment) -> bool {
    vars.get(L!("fish_autosuggestion_enabled"))
        .map(|v| v.as_string())
        .map(|v| v != L!("0"))
        .unwrap_or(true)
}

pub fn reader_reading_interrupted() -> i32 {
    crate::ffi::reader_reading_interrupted().0
}

pub fn reader_schedule_prompt_repaint() {
    crate::ffi::reader_schedule_prompt_repaint()
}

/// \return whether fish is currently unwinding the stack in preparation to exit.
pub fn fish_is_unwinding_for_exit() -> bool {
    crate::ffi::fish_is_unwinding_for_exit()
}

pub fn reader_run_count() -> u64 {
    crate::ffi::reader_run_count()
}

/// When tab-completing with a wildcard, we expand the wildcard up to this many results.
/// If expansion would exceed this many results, beep and do nothing.
const TAB_COMPLETE_WILDCARD_MAX_EXPANSION: usize = 256;

/// Given that the user is tab-completing a token \p wc whose cursor is at \p pos in the token,
/// try expanding it as a wildcard, populating \p result with the expanded string.
fn try_expand_wildcard(
    parser: &Parser,
    wc: WString,
    position: usize,
    result: &mut WString,
) -> ExpandResultCode {
    // Hacky from #8593: only expand if there are wildcards in the "current path component."
    // Find the "current path component" by looking for an unescaped slash before and after
    // our position.
    // This is quite naive; for example it mishandles brackets.
    let is_path_sep =
        |offset| wc.char_at(offset) == '/' && count_preceding_backslashes(&wc, offset) % 2 == 0;

    let mut comp_start = position;
    while comp_start > 0 && !is_path_sep(comp_start - 1) {
        comp_start -= 1;
    }
    let mut comp_end = position;
    while comp_end < wc.len() && !is_path_sep(comp_end) {
        comp_end += 1;
    }
    if !wildcard_has(&wc[comp_start..comp_end]) {
        return ExpandResultCode::wildcard_no_match;
    }
    result.clear();
    // Have a low limit on the number of matches, otherwise we will overwhelm the command line.

    let ctx = OperationContext::background_with_cancel_checker(
        &*parser.variables,
        Box::new(|| signal_check_cancel() != 0),
        TAB_COMPLETE_WILDCARD_MAX_EXPANSION,
    );

    // We do wildcards only.

    let flags = ExpandFlags::SKIP_CMDSUBST
        | ExpandFlags::SKIP_VARIABLES
        | ExpandFlags::PRESERVE_HOME_TILDES;
    let mut expanded = CompletionList::new();
    let ret = expand_string(wc, &mut expanded, flags, &ctx, None);
    if ret.result != ExpandResultCode::ok {
        return ret.result;
    }

    // Insert all matches (escaped) and a trailing space.
    let mut joined = WString::new();
    for r#match in expanded {
        if r#match.flags.contains(CompleteFlags::DONT_ESCAPE) {
            joined.push_utfstr(&r#match.completion);
        } else {
            let tildeflag = if r#match.flags.contains(CompleteFlags::DONT_ESCAPE_TILDES) {
                EscapeFlags::NO_TILDE
            } else {
                EscapeFlags::default()
            };
            joined.push_utfstr(&escape_string(
                &r#match.completion,
                EscapeStringStyle::Script(EscapeFlags::NO_QUOTED | tildeflag),
            ));
        }
        joined.push(' ');
    }

    *result = joined;
    ExpandResultCode::ok
}

pub fn completion_apply_to_command_line(
    val_str: &wstr,
    flags: CompleteFlags,
    command_line: &wstr,
    inout_cursor_pos: &mut usize,
    append_only: bool,
) -> WString {
    ffi::completion_apply_to_command_line(
        &val_str.to_ffi(),
        flags.bits(),
        &command_line.to_ffi(),
        inout_cursor_pos,
        append_only,
    )
    .from_ffi()
}

#[cxx::bridge]
mod reader_ffi {
    extern "C++" {
        include!("operation_context.h");
        include!("env.h");
        include!("parser.h");
        #[cxx_name = "EnvDyn"]
        type EnvDynFFI = crate::env::EnvDynFFI;
        type Parser = crate::parser::Parser;
    }
    extern "Rust" {
        fn reader_reset_interrupted();
        fn reader_handle_sigint();
        fn reader_test_and_clear_interrupted() -> i32;
        fn reader_sighup();
        fn reader_received_sighup() -> bool;
    }
    extern "Rust" {
        #[cxx_name = "try_expand_wildcard"]
        fn try_expand_wildcard_ffi(
            parser: &Parser,
            wc: &CxxWString,
            position: usize,
            result: &mut UniquePtr<CxxWString>,
        ) -> u8;
    }
    extern "Rust" {
        type ReaderConfig;
        #[cxx_name = "left_prompt_cmd"]
        fn left_prompt_cmd_ffi(&self) -> UniquePtr<CxxWString>;
        #[cxx_name = "right_prompt_cmd"]
        fn right_prompt_cmd_ffi(&self) -> UniquePtr<CxxWString>;
        #[cxx_name = "event"]
        fn event_ffi(&self) -> UniquePtr<CxxWString>;
        #[cxx_name = "complete_ok"]
        fn complete_ok_ffi(&self) -> bool;
        #[cxx_name = "highlight_ok"]
        fn highlight_ok_ffi(&self) -> bool;
        #[cxx_name = "syntax_check_ok"]
        fn syntax_check_ok_ffi(&self) -> bool;
        #[cxx_name = "autosuggest_ok"]
        fn autosuggest_ok_ffi(&self) -> bool;
        #[cxx_name = "expand_abbrev_ok"]
        fn expand_abbrev_ok_ffi(&self) -> bool;
        #[cxx_name = "exit_on_interrupt"]
        fn exit_on_interrupt_ffi(&self) -> bool;
        #[cxx_name = "in_silent_mode"]
        fn in_silent_mode_ffi(&self) -> bool;
        #[cxx_name = "inputfd"]
        fn inputfd_ffi(&self) -> i32;
    }
}

impl ReaderConfig {
    fn left_prompt_cmd_ffi(&self) -> UniquePtr<CxxWString> {
        self.left_prompt_cmd.to_ffi()
    }
    fn right_prompt_cmd_ffi(&self) -> UniquePtr<CxxWString> {
        self.right_prompt_cmd.to_ffi()
    }
    fn event_ffi(&self) -> UniquePtr<CxxWString> {
        self.event.to_ffi()
    }
    fn complete_ok_ffi(&self) -> bool {
        self.complete_ok
    }
    fn highlight_ok_ffi(&self) -> bool {
        self.highlight_ok
    }
    fn syntax_check_ok_ffi(&self) -> bool {
        self.syntax_check_ok
    }
    fn autosuggest_ok_ffi(&self) -> bool {
        self.autosuggest_ok
    }
    fn expand_abbrev_ok_ffi(&self) -> bool {
        self.expand_abbrev_ok
    }
    fn exit_on_interrupt_ffi(&self) -> bool {
        self.exit_on_interrupt
    }
    fn in_silent_mode_ffi(&self) -> bool {
        self.in_silent_mode
    }
    fn inputfd_ffi(&self) -> i32 {
        self.inputfd as _
    }
}
fn try_expand_wildcard_ffi(
    parser: &Parser,
    wc: &CxxWString,
    position: usize,
    result: &mut UniquePtr<CxxWString>,
) -> u8 {
    let mut rust_result = WString::new();
    let result_code = try_expand_wildcard(parser, wc.from_ffi(), position, &mut rust_result);
    *result = rust_result.to_ffi();
    unsafe { std::mem::transmute(result_code) }
}
