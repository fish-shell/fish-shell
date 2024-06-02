use std::sync::Mutex;

use crate::env::EnvVar;
use crate::flog::log_extra_to_flog_file;
#[allow(unused_imports)]
use crate::future::IsSomeAnd;
use crate::parser::Parser;
use crate::{common::escape, wchar::prelude::*};

static TRACE_VAR: Mutex<Option<EnvVar>> = Mutex::new(None);

pub fn trace_set_enabled(trace_var: Option<EnvVar>) {
    *TRACE_VAR.lock().unwrap() = trace_var;
}

pub(crate) enum TraceCategory {
    Bind,
    Event,
    Prompt,
    Title,
}

pub(crate) fn should_suppress_trace(category: TraceCategory) -> bool {
    let trace_var = TRACE_VAR.lock().unwrap();
    let Some(enabled_categories) = trace_var.as_ref() else {
        return false; // not tracing, no need to suppress anything
    };
    let enabled_categories = enabled_categories.as_list();

    let category = match category {
        TraceCategory::Bind => "bind",
        TraceCategory::Event => "event",
        TraceCategory::Prompt => "prompt",
        TraceCategory::Title => "title",
    };

    !enabled_categories
        .iter()
        .any(|s| s == "all" || s == category)
}

/// return whether tracing is enabled.
fn trace_enabled(parser: &Parser) -> bool {
    let ld = &parser.libdata().pods;
    if ld.suppress_fish_trace {
        return false;
    }
    TRACE_VAR.lock().unwrap().is_some()
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
