use super::prelude::*;
use crate::ast::BlockStatement;
use crate::common::{valid_func_name, valid_var_name};
use crate::complete::complete_add_wrapper;
use crate::env::environment::Environment;
use crate::event::{self, EventDescription, EventHandler};
use crate::function;
use crate::global_safety::RelaxedAtomicBool;
use crate::io::IoStreams;
use crate::parse_tree::NodeRef;
use crate::parse_tree::ParsedSourceRefFFI;
use crate::parser::Parser;
use crate::parser_keywords::parser_keywords_is_reserved;
use crate::signal::Signal;
use crate::wchar_ffi::{wcstring_list_ffi_t, WCharFromFFI, WCharToFFI};
use std::pin::Pin;
use std::sync::Arc;

struct FunctionCmdOpts {
    print_help: bool,
    shadow_scope: bool,
    description: WString,
    events: Vec<EventDescription>,
    named_arguments: Vec<WString>,
    inherit_vars: Vec<WString>,
    wrap_targets: Vec<WString>,
}

impl Default for FunctionCmdOpts {
    fn default() -> Self {
        Self {
            print_help: false,
            shadow_scope: true,
            description: WString::new(),
            events: Vec::new(),
            named_arguments: Vec::new(),
            inherit_vars: Vec::new(),
            wrap_targets: Vec::new(),
        }
    }
}

// This command is atypical in using the "-" (RETURN_IN_ORDER) option for flag parsing.
// This is needed due to the semantics of the -a/--argument-names flag.
const SHORT_OPTIONS: &wstr = L!("-:a:d:e:hj:p:s:v:w:SV:");
#[rustfmt::skip]
const LONG_OPTIONS: &[woption] = &[
    wopt(L!("description"), woption_argument_t::required_argument, 'd'),
    wopt(L!("on-signal"), woption_argument_t::required_argument, 's'),
    wopt(L!("on-job-exit"), woption_argument_t::required_argument, 'j'),
    wopt(L!("on-process-exit"), woption_argument_t::required_argument, 'p'),
    wopt(L!("on-variable"), woption_argument_t::required_argument, 'v'),
    wopt(L!("on-event"), woption_argument_t::required_argument, 'e'),
    wopt(L!("wraps"), woption_argument_t::required_argument, 'w'),
    wopt(L!("help"), woption_argument_t::no_argument, 'h'),
    wopt(L!("argument-names"), woption_argument_t::required_argument, 'a'),
    wopt(L!("no-scope-shadowing"), woption_argument_t::no_argument, 'S'),
    wopt(L!("inherit-variable"), woption_argument_t::required_argument, 'V'),
];

/// \return the internal_job_id for a pid, or None if none.
/// This looks through both active and finished jobs.
fn job_id_for_pid(pid: i32, parser: &Parser) -> Option<u64> {
    if let Some(job) = parser.job_get_from_pid(pid) {
        Some(job.internal_job_id)
    } else {
        parser
            .get_wait_handles()
            .get_by_pid(pid)
            .map(|h| h.internal_job_id)
    }
}

/// Parses options to builtin function, populating opts.
/// Returns an exit status.
fn parse_cmd_opts(
    opts: &mut FunctionCmdOpts,
    optind: &mut usize,
    argv: &mut [WString],
    parser: &Parser,
    streams: &mut IoStreams<'_>,
) -> Option<c_int> {
    let cmd = L!("function");
    let print_hints = false;
    let mut handling_named_arguments = false;
    let mut w = wgetopter_t::new(SHORT_OPTIONS, LONG_OPTIONS, argv);
    while let Some(opt) = w.wgetopt_long() {
        // NONOPTION_CHAR_CODE is returned when we reach a non-permuted non-option.
        if opt != 'a' && opt != NONOPTION_CHAR_CODE {
            handling_named_arguments = false;
        }
        match opt {
            NONOPTION_CHAR_CODE => {
                // A positional argument we got because we use RETURN_IN_ORDER.
                let woptarg = w.woptarg().unwrap().to_owned();
                if handling_named_arguments {
                    opts.named_arguments.push(woptarg);
                } else {
                    streams.err.append(&wgettext_fmt!(
                        "%ls: %ls: unexpected positional argument",
                        cmd,
                        woptarg
                    ));
                    return STATUS_INVALID_ARGS;
                }
            }
            'd' => {
                opts.description = w.woptarg().unwrap().to_owned();
            }
            's' => {
                let Some(signal) = Signal::parse(w.woptarg().unwrap()) else {
                    streams.err.append(&wgettext_fmt!("%ls: Unknown signal '%ls'", cmd, w.woptarg().unwrap()));
                    return STATUS_INVALID_ARGS;
                };
                opts.events.push(EventDescription::Signal { signal });
            }
            'v' => {
                let name = w.woptarg().unwrap().to_owned();
                if !valid_var_name(&name) {
                    streams
                        .err
                        .append(&wgettext_fmt!(BUILTIN_ERR_VARNAME, cmd, name));
                    return STATUS_INVALID_ARGS;
                }
                opts.events.push(EventDescription::Variable { name });
            }
            'e' => {
                let param = w.woptarg().unwrap().to_owned();
                opts.events.push(EventDescription::Generic { param });
            }
            'j' | 'p' => {
                let woptarg = w.woptarg().unwrap();
                let e: EventDescription;
                if opt == 'j' && woptarg == "caller" {
                    let libdata = parser.libdata();
                    let caller_id = if libdata.pods.is_subshell {
                        libdata.pods.caller_id
                    } else {
                        0
                    };
                    if caller_id == 0 {
                        streams.err.append(&wgettext_fmt!(
                            "%ls: calling job for event handler not found",
                            cmd
                        ));
                        return STATUS_INVALID_ARGS;
                    }
                    e = EventDescription::CallerExit { caller_id };
                } else if opt == 'p' && woptarg == "%self" {
                    // Safety: getpid() is always successful.
                    let pid = unsafe { libc::getpid() };
                    e = EventDescription::ProcessExit { pid };
                } else {
                    let pid = fish_wcstoi(woptarg);
                    if pid.is_err() || pid.unwrap() < 0 {
                        streams
                            .err
                            .append(&wgettext_fmt!("%ls: %ls: invalid process id", cmd));
                        return STATUS_INVALID_ARGS;
                    }
                    let pid = pid.unwrap();
                    if opt == 'p' {
                        e = EventDescription::ProcessExit { pid };
                    } else {
                        // TODO: rationalize why a default of 0 is sensible.
                        let internal_job_id = job_id_for_pid(pid, parser).unwrap_or(0);
                        e = EventDescription::JobExit {
                            pid,
                            internal_job_id,
                        };
                    }
                }
                opts.events.push(e);
            }
            'a' => {
                handling_named_arguments = true;
                opts.named_arguments.push(w.woptarg().unwrap().to_owned());
            }
            'S' => {
                opts.shadow_scope = false;
            }
            'w' => {
                opts.wrap_targets.push(w.woptarg().unwrap().to_owned());
            }
            'V' => {
                let woptarg = w.woptarg().unwrap();
                if !valid_var_name(woptarg) {
                    streams
                        .err
                        .append(&wgettext_fmt!(BUILTIN_ERR_VARNAME, cmd, woptarg));
                    return STATUS_INVALID_ARGS;
                }
                opts.inherit_vars.push(woptarg.to_owned());
            }
            'h' => {
                opts.print_help = true;
            }
            ':' => {
                builtin_missing_argument(parser, streams, cmd, &argv[w.woptind - 1], print_hints);
                return STATUS_INVALID_ARGS;
            }
            '?' => {
                builtin_unknown_option(parser, streams, cmd, &argv[w.woptind - 1], print_hints);
                return STATUS_INVALID_ARGS;
            }
            other => {
                panic!("Unexpected retval from wgetopt_long: {}", other);
            }
        }
    }

    *optind = w.woptind;
    STATUS_CMD_OK
}

fn validate_function_name(
    argv: &[WString],
    function_name: &mut WString,
    cmd: &wstr,
    streams: &mut IoStreams<'_>,
) -> Option<c_int> {
    if argv.len() < 2 {
        // This is currently impossible but let's be paranoid.
        streams
            .err
            .append(&wgettext_fmt!("%ls: function name required", cmd));
        return STATUS_INVALID_ARGS;
    }
    *function_name = argv[1].to_owned();
    if !valid_func_name(function_name) {
        streams.err.append(&wgettext_fmt!(
            "%ls: %ls: invalid function name",
            cmd,
            function_name,
        ));
        return STATUS_INVALID_ARGS;
    }
    if parser_keywords_is_reserved(function_name) {
        streams.err.append(&wgettext_fmt!(
            "%ls: %ls: cannot use reserved keyword as function name",
            cmd,
            function_name
        ));
        return STATUS_INVALID_ARGS;
    }
    STATUS_CMD_OK
}

pub fn builtin_function(
    parser: &Parser,
    streams: &mut IoStreams<'_>,
    argv: &mut [WString],
) -> Option<c_int> {
    todo!()
}

/// Define a function. Calls into `function.rs` to perform the heavy lifting of defining a
/// function. Note this isn't strictly a "builtin": it is called directly from parse_execution.
/// That is why its signature is different from the other builtins.
pub fn function(
    parser: &Parser,
    streams: &mut IoStreams<'_>,
    c_args: &mut [WString],
    func_node: NodeRef<BlockStatement>,
) -> Option<c_int> {
    // The wgetopt function expects 'function' as the first argument. Make a new vec with
    // that property. This is needed because this builtin has a different signature than the other
    // builtins.
    let mut args = vec![L!("function").to_owned()];
    args.extend_from_slice(c_args);
    let cmd = L!("function");

    // A valid function name has to be the first argument.
    let mut function_name = WString::new();
    let mut retval = validate_function_name(&args, &mut function_name, cmd, streams);
    if retval != STATUS_CMD_OK {
        return retval;
    }
    let args = &mut args[1..];

    let mut opts = FunctionCmdOpts::default();
    let mut optind = 0;
    retval = parse_cmd_opts(&mut opts, &mut optind, args, parser, streams);
    if retval != STATUS_CMD_OK {
        return retval;
    }

    if opts.print_help {
        builtin_print_error_trailer(parser, streams.err, cmd);
        return STATUS_CMD_OK;
    }

    if args.len() != optind {
        if !opts.named_arguments.is_empty() {
            // Remaining arguments are named arguments.
            for arg in &args[optind..] {
                if !valid_var_name(arg) {
                    streams
                        .err
                        .append(&wgettext_fmt!(BUILTIN_ERR_VARNAME, cmd, arg));
                    return STATUS_INVALID_ARGS;
                }
                opts.named_arguments.push(arg.to_owned());
            }
        } else {
            streams.err.append(&wgettext_fmt!(
                "%ls: %ls: unexpected positional argument",
                cmd,
                args[optind],
            ));
            return STATUS_INVALID_ARGS;
        }
    }

    // Extract the current filename.
    let definition_file = parser.libdata().current_filename.clone();

    // Ensure inherit_vars is unique and then populate it.
    opts.inherit_vars.sort_unstable();
    opts.inherit_vars.dedup();

    let inherit_vars: Vec<(WString, Vec<WString>)> = opts
        .inherit_vars
        .into_iter()
        .filter_map(|name| {
            let vals = parser.vars().get(&name)?.as_list().to_vec();
            Some((name, vals))
        })
        .collect();

    // We have what we need to actually define the function.
    let props = function::FunctionProperties {
        func_node,
        named_arguments: opts.named_arguments,
        description: opts.description,
        inherit_vars: inherit_vars.into_boxed_slice(),
        shadow_scope: opts.shadow_scope,
        is_autoload: RelaxedAtomicBool::new(false),
        definition_file,
        is_copy: false,
        copy_definition_file: None,
        copy_definition_lineno: 0,
    };

    // Add the function itself.
    function::add(function_name.clone(), Arc::new(props));

    // Handle wrap targets by creating the appropriate completions.
    for wt in opts.wrap_targets.into_iter() {
        complete_add_wrapper(function_name.clone(), wt.clone());
    }

    // Add any event handlers.
    for ed in &opts.events {
        event::add_handler(EventHandler::new(ed.clone(), Some(function_name.clone())));
    }

    // If there is an --on-process-exit or --on-job-exit event handler for some pid, and that
    // process has already exited, run it immediately (#7210).
    for ed in &opts.events {
        match *ed {
            EventDescription::ProcessExit { pid } if pid != event::ANY_PID => {
                if let Some(status) = parser
                    .get_wait_handles()
                    .get_by_pid(pid)
                    .and_then(|wh| wh.status())
                {
                    event::fire(parser, event::Event::process_exit(pid, status));
                }
            }
            EventDescription::JobExit { pid, .. } if pid != event::ANY_PID => {
                if let Some(wh) = parser.get_wait_handles().get_by_pid(pid) {
                    if wh.is_completed() {
                        event::fire(parser, event::Event::job_exit(pid, wh.internal_job_id));
                    }
                }
            }
            _ => (),
        }
    }

    STATUS_CMD_OK
}
