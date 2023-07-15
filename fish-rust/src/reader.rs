use std::sync::atomic::AtomicI32;

use crate::common::{escape_string, EscapeFlags, EscapeStringStyle};
use crate::complete::{CompleteFlags, CompletionList};
use crate::env::{EnvDyn, EnvDynFFI};
use crate::env::{EnvStackRefFFI, Environment, EnvironmentRef};
use crate::expand::{expand_string, ExpandFlags, ExpandResultCode};
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
use std::sync::atomic::Ordering;

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
fn reader_reset_interrupted() {
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

pub fn reader_schedule_prompt_repaint() {
    todo!()
}

/// \return whether fish is currently unwinding the stack in preparation to exit.
pub fn fish_is_unwinding_for_exit() -> bool {
    todo!()
}

pub fn restore_term_mode() {
    todo!()
}

pub fn reader_run_count() -> u64 {
    todo!()
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
        &parser.variables,
        &|| signal_check_cancel() != 0,
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
    for match_ in expanded {
        if match_.flags.contains(CompleteFlags::DONT_ESCAPE) {
            joined.push_utfstr(&match_.completion);
        } else {
            let tildeflag = if match_.flags.contains(CompleteFlags::DONT_ESCAPE_TILDES) {
                EscapeFlags::NO_TILDE
            } else {
                EscapeFlags::default()
            };
            joined.push_utfstr(&escape_string(
                &match_.completion,
                EscapeStringStyle::Script(EscapeFlags::NO_QUOTED | tildeflag),
            ));
        }
        joined.push(' ');
    }

    *result = joined;
    ExpandResultCode::ok
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
