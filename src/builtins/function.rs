use super::prelude::*;
use crate::ast::BlockStatement;
use crate::common::{valid_func_name, valid_var_name};
use crate::complete::complete_add_wrapper;
use crate::env::environment::Environment;
use crate::env::is_read_only;
use crate::event::{self, EventDescription, EventHandler};
use crate::function;
use crate::global_safety::RelaxedAtomicBool;
use crate::nix::getpid;
use crate::parse_tree::NodeRef;
use crate::parser_keywords::parser_keywords_is_reserved;
use crate::proc::Pid;
use crate::signal::Signal;
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
const SHORT_OPTIONS: &wstr = L!("-a:d:e:hj:p:s:v:w:SV:");
#[rustfmt::skip]
const LONG_OPTIONS: &[WOption] = &[
    wopt(L!("description"), ArgType::RequiredArgument, 'd'),
    wopt(L!("on-signal"), ArgType::RequiredArgument, 's'),
    wopt(L!("on-job-exit"), ArgType::RequiredArgument, 'j'),
    wopt(L!("on-process-exit"), ArgType::RequiredArgument, 'p'),
    wopt(L!("on-variable"), ArgType::RequiredArgument, 'v'),
    wopt(L!("on-event"), ArgType::RequiredArgument, 'e'),
    wopt(L!("wraps"), ArgType::RequiredArgument, 'w'),
    wopt(L!("help"), ArgType::NoArgument, 'h'),
    wopt(L!("argument-names"), ArgType::RequiredArgument, 'a'),
    wopt(L!("no-scope-shadowing"), ArgType::NoArgument, 'S'),
    wopt(L!("inherit-variable"), ArgType::RequiredArgument, 'V'),
];

/// Return the internal_job_id for a pid, or None if none.
/// This looks through both active and finished jobs.
fn job_id_for_pid(pid: Pid, parser: &Parser) -> Option<u64> {
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
    argv: &mut [&wstr],
    parser: &Parser,
    streams: &mut IoStreams,
) -> c_int {
    let cmd = L!("function");
    let print_hints = false;
    let mut handling_named_arguments = false;
    let mut w = WGetopter::new(SHORT_OPTIONS, LONG_OPTIONS, argv);
    while let Some(opt) = w.next_opt() {
        // NON_OPTION_CHAR is returned when we reach a non-permuted non-option.
        if opt != 'a' && opt != NON_OPTION_CHAR {
            handling_named_arguments = false;
        }
        match opt {
            NON_OPTION_CHAR => {
                // A positional argument we got because we use RETURN_IN_ORDER.
                let woptarg = w.woptarg.unwrap().to_owned();
                if handling_named_arguments {
                    if is_read_only(&woptarg) {
                        streams.err.append(wgettext_fmt!(
                            "%s: variable '%s' is read-only\n",
                            cmd,
                            woptarg
                        ));
                        return STATUS_INVALID_ARGS;
                    }
                    opts.named_arguments.push(woptarg);
                } else {
                    streams.err.append(wgettext_fmt!(
                        "%s: %s: unexpected positional argument",
                        cmd,
                        woptarg
                    ));
                    return STATUS_INVALID_ARGS;
                }
            }
            'd' => {
                opts.description = w.woptarg.unwrap().to_owned();
            }
            's' => {
                let Some(signal) = Signal::parse(w.woptarg.unwrap()) else {
                    streams.err.append(wgettext_fmt!(
                        "%s: Unknown signal '%s'",
                        cmd,
                        w.woptarg.unwrap()
                    ));
                    return STATUS_INVALID_ARGS;
                };
                opts.events.push(EventDescription::Signal { signal });
            }
            'v' => {
                let name = w.woptarg.unwrap().to_owned();
                if !valid_var_name(&name) {
                    streams
                        .err
                        .append(wgettext_fmt!(BUILTIN_ERR_VARNAME, cmd, name));
                    return STATUS_INVALID_ARGS;
                }
                opts.events.push(EventDescription::Variable { name });
            }
            'e' => {
                let param = w.woptarg.unwrap().to_owned();
                opts.events.push(EventDescription::Generic { param });
            }
            'j' | 'p' => {
                let woptarg = w.woptarg.unwrap();
                let e: EventDescription;
                if opt == 'j' && woptarg == "caller" {
                    let caller_id = if parser.scope().is_subshell {
                        parser.scope().caller_id
                    } else {
                        0
                    };
                    if caller_id == 0 {
                        streams.err.append(wgettext_fmt!(
                            "%s: calling job for event handler not found",
                            cmd
                        ));
                        return STATUS_INVALID_ARGS;
                    }
                    e = EventDescription::CallerExit { caller_id };
                } else if opt == 'p' && woptarg == "%self" {
                    let pid = Pid::new(getpid());
                    e = EventDescription::ProcessExit { pid };
                } else {
                    let Ok(pid @ 0..) = fish_wcstoi(woptarg) else {
                        streams.err.append(wgettext_fmt!(
                            "%s: %s: invalid process id",
                            cmd,
                            woptarg
                        ));
                        return STATUS_INVALID_ARGS;
                    };
                    if opt == 'p' {
                        e = EventDescription::ProcessExit { pid: Pid::new(pid) };
                    } else {
                        // TODO: rationalize why a default of 0 is sensible.
                        let internal_job_id = match Pid::new(pid) {
                            Some(pid) => job_id_for_pid(pid, parser).unwrap_or(0),
                            None => 0,
                        };
                        e = EventDescription::JobExit {
                            pid: Pid::new(pid),
                            internal_job_id,
                        };
                    }
                }
                opts.events.push(e);
            }
            'a' => {
                let name = w.woptarg.unwrap().to_owned();
                if is_read_only(&name) {
                    streams.err.append(wgettext_fmt!(
                        "%s: variable '%s' is read-only\n",
                        cmd,
                        name
                    ));
                    return STATUS_INVALID_ARGS;
                }
                handling_named_arguments = true;
                opts.named_arguments.push(name);
            }
            'S' => {
                opts.shadow_scope = false;
            }
            'w' => {
                opts.wrap_targets.push(w.woptarg.unwrap().to_owned());
            }
            'V' => {
                let woptarg = w.woptarg.unwrap();
                if !valid_var_name(woptarg) {
                    streams
                        .err
                        .append(wgettext_fmt!(BUILTIN_ERR_VARNAME, cmd, woptarg));
                    return STATUS_INVALID_ARGS;
                }
                opts.inherit_vars.push(woptarg.to_owned());
            }
            'h' => {
                opts.print_help = true;
            }
            ':' => {
                builtin_missing_argument(parser, streams, cmd, argv[w.wopt_index - 1], print_hints);
                return STATUS_INVALID_ARGS;
            }
            ';' => {
                builtin_unexpected_argument(
                    parser,
                    streams,
                    cmd,
                    argv[w.wopt_index - 1],
                    print_hints,
                );
                return STATUS_INVALID_ARGS;
            }
            '?' => {
                builtin_unknown_option(parser, streams, cmd, argv[w.wopt_index - 1], print_hints);
                return STATUS_INVALID_ARGS;
            }
            other => {
                panic!("Unexpected retval from WGetopter: {}", other);
            }
        }
    }

    *optind = w.wopt_index;
    STATUS_CMD_OK
}

fn validate_function_name(function_name: &wstr, cmd: &wstr, streams: &mut IoStreams) -> c_int {
    if !valid_func_name(function_name) {
        streams.err.append(wgettext_fmt!(
            "%s: %s: invalid function name",
            cmd,
            function_name,
        ));
        return STATUS_INVALID_ARGS;
    }
    if parser_keywords_is_reserved(function_name) {
        streams.err.append(wgettext_fmt!(
            "%s: %s: cannot use reserved keyword as function name",
            cmd,
            function_name
        ));
        return STATUS_INVALID_ARGS;
    }
    STATUS_CMD_OK
}

/// Define a function. Calls into `function.rs` to perform the heavy lifting of defining a
/// function. Note this isn't strictly a "builtin": it is called directly from parse_execution.
/// That is why its signature is different from the other builtins.
pub fn function(
    parser: &Parser,
    streams: &mut IoStreams,
    c_args: &mut [&wstr],
    func_node: NodeRef<BlockStatement>,
) -> c_int {
    // The wgetopt function expects 'function' as the first argument. Make a new vec with
    // that property. This is needed because this builtin has a different signature than the other
    // builtins.
    let mut args = vec![L!("function")];
    args.extend_from_slice(c_args);
    let argv: &mut [&wstr] = &mut args;
    let cmd = argv[0];

    // A valid function name has to be the first argument.
    let function_name = argv[1].to_owned();
    let mut retval = validate_function_name(&function_name, cmd, streams);
    if retval != STATUS_CMD_OK {
        return retval;
    }
    let argv = &mut argv[1..];

    let mut opts = FunctionCmdOpts::default();
    let mut optind = 0;
    retval = parse_cmd_opts(&mut opts, &mut optind, argv, parser, streams);
    if retval != STATUS_CMD_OK {
        return retval;
    }

    if opts.print_help {
        builtin_print_error_trailer(parser, streams.err, cmd);
        return STATUS_CMD_OK;
    }

    if argv.len() != optind {
        if !opts.named_arguments.is_empty() {
            // Remaining arguments are named arguments.
            for &arg in argv[optind..].iter() {
                if !valid_var_name(arg) {
                    streams
                        .err
                        .append(wgettext_fmt!(BUILTIN_ERR_VARNAME, cmd, arg));
                    return STATUS_INVALID_ARGS;
                }
                opts.named_arguments.push(arg.to_owned());
            }
        } else {
            streams.err.append(wgettext_fmt!(
                "%s: %s: unexpected positional argument",
                cmd,
                argv[optind],
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

    for named in &opts.named_arguments {
        if !valid_var_name(named) {
            streams
                .err
                .append(wgettext_fmt!(BUILTIN_ERR_VARNAME, cmd, named));
            return STATUS_INVALID_ARGS;
        }
    }

    // We have what we need to actually define the function.
    let props = function::FunctionProperties {
        func_node,
        named_arguments: opts.named_arguments,
        // Function descriptions are extracted from scripts in `share` via
        // `build_tools/fish_xgettext.fish`.
        description: LocalizableString::from_external_source(opts.description),
        inherit_vars: inherit_vars.into_boxed_slice(),
        shadow_scope: opts.shadow_scope,
        is_autoload: RelaxedAtomicBool::new(false),
        definition_file,
        is_copy: false,
        copy_definition_file: None,
        copy_definition_lineno: None,
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
            EventDescription::ProcessExit { pid: Some(pid) } => {
                let wh = parser.get_wait_handles().get_by_pid(pid);
                if let Some(status) = wh.and_then(|wh| wh.status()) {
                    event::fire(parser, event::Event::process_exit(pid, status));
                }
            }
            EventDescription::JobExit { pid: Some(pid), .. } => {
                let wh = parser.get_wait_handles().get_by_pid(pid);
                if let Some(wh) = wh {
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
