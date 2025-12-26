use crate::flog::log_extra_to_flog_file;
use crate::parser::Parser;
use crate::{common::escape, global_safety::RelaxedAtomicBool, prelude::*};

static DO_TRACE: RelaxedAtomicBool = RelaxedAtomicBool::new(false);
static DO_TRACE_ALL: RelaxedAtomicBool = RelaxedAtomicBool::new(false);

pub fn trace_set_enabled(enable: Vec<WString>) {
    DO_TRACE.store(!enable.is_empty());
    DO_TRACE_ALL.store(enable.iter().any(|s| s == "all"));
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
