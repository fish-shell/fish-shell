use crate::parser::Parser;
use crate::{
    common::escape,
    ffi::{self, wcharz_t, wcstring_list_ffi_t},
    global_safety::RelaxedAtomicBool,
    wchar::prelude::*,
    wchar_ffi::{WCharFromFFI, WCharToFFI},
};

#[cxx::bridge]
mod trace_ffi {
    extern "C++" {
        include!("wutil.h");
        include!("parser.h");
        type wcstring_list_ffi_t = super::wcstring_list_ffi_t;
        type wcharz_t = super::wcharz_t;
        type Parser = crate::parser::Parser;
    }

    extern "Rust" {
        fn trace_set_enabled(do_enable: bool);
        fn trace_enabled(parser: &Parser) -> bool;
        #[cxx_name = "trace_argv"]
        fn trace_argv_ffi(parser: &Parser, command: wcharz_t, args: &wcstring_list_ffi_t);
    }
}

static DO_TRACE: RelaxedAtomicBool = RelaxedAtomicBool::new(false);

pub fn trace_set_enabled(do_enable: bool) {
    DO_TRACE.store(do_enable);
}

/// return whether tracing is enabled.
pub fn trace_enabled(parser: &Parser) -> bool {
    let ld = &parser.libdata().pods;
    if ld.suppress_fish_trace {
        return false;
    }
    DO_TRACE.load()
}

/// Trace an "argv": a list of arguments where the first is the command.
// Allow the `&Vec` parameter as this function only exists temporarily for the FFI
#[allow(clippy::ptr_arg)]
fn trace_argv_ffi(parser: &Parser, command: wcharz_t, args: &wcstring_list_ffi_t) {
    let command: WString = command.into();
    let args: Vec<WString> = args.from_ffi();
    let args_ref: Vec<&wstr> = args.iter().map(WString::as_utfstr).collect();
    trace_argv(parser, command.as_utfstr(), &args_ref);
}

pub fn trace_argv<S: AsRef<wstr>>(parser: &Parser, command: &wstr, args: &[S]) {
    // Format into a string to prevent interleaving with flog in other threads.
    // Add the + prefix.
    let mut trace_text = L!("-").repeat(parser.blocks_size() - 1);
    trace_text.push('>');

    if !command.is_empty() {
        trace_text.push(' ');
        trace_text.push_utfstr(command);
    }
    for arg in args {
        trace_text.push(' ');
        trace_text.push_utfstr(&escape(arg.as_ref()));
    }
    trace_text.push('\n');
    ffi::log_extra_to_flog_file(&trace_text.to_ffi());
}

pub fn trace_if_enabled_ffi<S: AsRef<wstr>>(parser: &Parser, command: &wstr, args: &[S]) {
    if trace_enabled(parser) {
        trace_argv(parser, command, args);
    }
}

/// Convenience helper to trace a single command if tracing is enabled.
pub fn trace_if_enabled(parser: &Parser, command: &wstr) {
    if trace_enabled(parser) {
        let argv: &[&'static wstr] = &[];
        trace_argv(parser, command, argv);
    }
}
/// Convenience helper to trace a single command and arguments if tracing is enabled.
pub fn trace_if_enabled_with_args<S: AsRef<wstr>>(parser: &Parser, command: &wstr, args: &[S]) {
    if trace_enabled(parser) {
        trace_argv(parser, command, args);
    }
}
