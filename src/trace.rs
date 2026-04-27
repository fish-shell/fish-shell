use crate::{
    flog::log_extra_to_flog_file,
    global_safety::{RelaxedAtomicBool, RelaxedAtomicUsize},
    parser::Parser,
    prelude::*,
};
use fish_common::escape;

static DO_TRACE: RelaxedAtomicBool = RelaxedAtomicBool::new(false);
static DO_TRACE_ALL: RelaxedAtomicBool = RelaxedAtomicBool::new(false);

const TRACE_DEPTH_DEFAULT: usize = usize::MAX;
static TRACE_DEPTH: RelaxedAtomicUsize = RelaxedAtomicUsize::new(TRACE_DEPTH_DEFAULT);

pub fn trace_set_enabled(enable: Vec<WString>) {
    DO_TRACE.store(!enable.is_empty());
    DO_TRACE_ALL.store(enable.iter().any(|s| s == "all"));
}

pub fn trace_set_depth(depth: usize) {
    TRACE_DEPTH.store(depth);
}

pub fn trace_set_depth_default() {
    TRACE_DEPTH.store(TRACE_DEPTH_DEFAULT);
}

/// return whether tracing is enabled.
pub fn trace_enabled(parser: &Parser) -> bool {
    if DO_TRACE_ALL.load() {
        return true;
    }
    if parser.scope().suppress_fish_trace {
        return false;
    }
    DO_TRACE.load()
}

/// Trace an "argv": a list of arguments where the first is the command.
// Allow the `&Vec` parameter as this function only exists temporarily for the FFI
pub fn trace_argv<S: AsRef<wstr>>(parser: &Parser, command: &wstr, args: &[S]) {
    let current_depth = parser.blocks_size() - 1;

    // If the user requested traces only to a given depth, we may stop early.
    if current_depth > TRACE_DEPTH.load() {
        return;
    }

    // Format into a string to prevent interleaving with flog in other threads.
    // Add the + prefix.
    let mut trace_text = L!("-").repeat(current_depth);
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
    log_extra_to_flog_file(&trace_text);
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
