use crate::{
    common::{escape_string, EscapeStringStyle},
    ffi::{self, parser_t, wcharz_t},
    global_safety::RelaxedAtomicBool,
    wchar::{self, wstr, L},
    wchar_ffi::WCharToFFI,
};

#[cxx::bridge]
mod trace_ffi {
    extern "C++" {
        include!("wutil.h");
        include!("parser.h");
        type wcharz_t = super::wcharz_t;
        type parser_t = super::parser_t;
    }

    extern "Rust" {
        fn trace_set_enabled(do_enable: bool);
        fn trace_enabled(parser: &parser_t) -> bool;
        #[cxx_name = "trace_argv"]
        fn trace_argv_ffi(parser: &parser_t, command: wcharz_t, args: &Vec<wcharz_t>);
    }
}

static DO_TRACE: RelaxedAtomicBool = RelaxedAtomicBool::new(false);

pub fn trace_set_enabled(do_enable: bool) {
    DO_TRACE.store(do_enable);
}

/// return whether tracing is enabled.
pub fn trace_enabled(parser: &parser_t) -> bool {
    let ld = parser.ffi_libdata_pod_const();
    if ld.suppress_fish_trace {
        return false;
    }
    DO_TRACE.load()
}

/// Trace an "argv": a list of arguments where the first is the command.
// Allow the `&Vec` parameter as this function only exists temporarily for the FFI
#[allow(clippy::ptr_arg)]
fn trace_argv_ffi(parser: &parser_t, command: wcharz_t, args: &Vec<wcharz_t>) {
    let command: wchar::WString = command.into();
    let args: Vec<wchar::WString> = args.iter().map(Into::into).collect();
    let args_ref: Vec<&wstr> = args.iter().map(wchar::WString::as_utfstr).collect();
    trace_argv(parser, command.as_utfstr(), &args_ref);
}

pub fn trace_argv(parser: &parser_t, command: &wstr, args: &[&wstr]) {
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
        trace_text.push_utfstr(&escape_string(arg, EscapeStringStyle::default()));
    }
    trace_text.push('\n');
    ffi::log_extra_to_flog_file(&trace_text.to_ffi());
}

/// Convenience helper to trace a single string if tracing is enabled.
pub fn trace_if_enabled(parser: &parser_t, command: &wstr, args: &[&wstr]) {
    if trace_enabled(parser) {
        trace_argv(parser, command, args);
    }
}
