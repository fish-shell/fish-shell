use libc::pid_t;

use super::prelude::*;
use crate::ffi::{job_t, parser_t, proc_wait_any};
use crate::signal::SigChecker;
use crate::wait_handle::{WaitHandleRef, WaitHandleStore};
use crate::wutil;

/// \return true if we can wait on a job.
fn can_wait_on_job(j: &cxx::SharedPtr<job_t>) -> bool {
    j.is_constructed() && !j.is_foreground() && !j.is_stopped()
}

/// \return true if a wait handle matches a pid or a process name.
fn wait_handle_matches(query: WaitHandleQuery, wh: &WaitHandleRef) -> bool {
    match query {
        WaitHandleQuery::Pid(pid) => wh.pid == pid,
        WaitHandleQuery::ProcName(proc_name) => proc_name == wh.base_name,
    }
}

/// \return true if all chars are numeric.
fn iswnumeric(s: &wstr) -> bool {
    s.chars().all(|c| c.is_ascii_digit())
}

#[derive(Copy, Clone)]
enum WaitHandleQuery<'a> {
    Pid(pid_t),
    ProcName(&'a wstr),
}

/// Walk the list of jobs, looking for a process with the given pid or proc name.
/// Append all matching wait handles to \p handles.
/// \return true if we found a matching job (even if not waitable), false if not.
fn find_wait_handles(
    query: WaitHandleQuery<'_>,
    parser: &mut parser_t,
    handles: &mut Vec<WaitHandleRef>,
) -> bool {
    // Has a job already completed?
    // TODO: we can avoid traversing this list if searching by pid.
    let mut matched = false;
    let wait_handles: &mut WaitHandleStore = parser.get_wait_handles_mut();
    for wh in wait_handles.iter() {
        if wait_handle_matches(query, wh) {
            handles.push(wh.clone());
            matched = true;
        }
    }

    // Is there a running job match?
    for j in parser.get_jobs() {
        // We want to set 'matched' to true if we could have matched, even if the job was stopped.
        let provide_handle = can_wait_on_job(j);
        for proc in j.get_procs() {
            let wh = proc
                .pin_mut()
                .unpin()
                .make_wait_handle(j.get_internal_job_id());
            let Some(wh) = wh else {
                continue;
            };
            if wait_handle_matches(query, &wh) {
                matched = true;
                if provide_handle {
                    handles.push(wh.clone());
                }
            }
        }
    }
    matched
}

fn get_all_wait_handles(parser: &parser_t) -> Vec<WaitHandleRef> {
    // Get wait handles for reaped jobs.
    let mut result = parser.get_wait_handles().get_list();

    // Get wait handles for running jobs.
    for j in parser.get_jobs() {
        if !can_wait_on_job(j) {
            continue;
        }
        for proc_ptr in j.get_procs().iter_mut() {
            let proc = proc_ptr.pin_mut().unpin();
            if let Some(wh) = proc.make_wait_handle(j.get_internal_job_id()) {
                result.push(wh);
            }
        }
    }
    result
}

fn is_completed(wh: &WaitHandleRef) -> bool {
    wh.is_completed()
}

/// Wait for the given wait handles to be marked as completed.
/// If \p any_flag is set, wait for the first one; otherwise wait for all.
/// \return a status code.
fn wait_for_completion(
    parser: &mut parser_t,
    whs: &[WaitHandleRef],
    any_flag: bool,
) -> Option<c_int> {
    if whs.is_empty() {
        return Some(0);
    }

    let mut sigint = SigChecker::new_sighupint();
    loop {
        let finished = if any_flag {
            whs.iter().any(is_completed)
        } else {
            whs.iter().all(is_completed)
        };

        if finished {
            // Remove completed wait handles (at most 1 if any_flag is set).
            for wh in whs {
                if is_completed(wh) {
                    parser.get_wait_handles_mut().remove(wh);
                    if any_flag {
                        break;
                    }
                }
            }
            return Some(0);
        }
        if sigint.check() {
            return Some(128 + libc::SIGINT);
        }
        proc_wait_any(parser.pin());
    }
}

#[widestrs]
pub fn wait(
    parser: &mut parser_t,
    streams: &mut io_streams_t,
    argv: &mut [&wstr],
) -> Option<c_int> {
    let cmd = argv[0];
    let argc = argv.len();
    let mut any_flag = false; // flag for -n option
    let mut print_help = false;
    let print_hints = false;

    const shortopts: &wstr = ":nh"L;
    const longopts: &[woption] = &[
        wopt("any"L, woption_argument_t::no_argument, 'n'),
        wopt("help"L, woption_argument_t::no_argument, 'h'),
    ];

    let mut w = wgetopter_t::new(shortopts, longopts, argv);
    while let Some(c) = w.wgetopt_long() {
        match c {
            'n' => {
                any_flag = true;
            }
            'h' => {
                print_help = true;
            }
            ':' => {
                builtin_missing_argument(parser, streams, cmd, argv[w.woptind - 1], print_hints);
                return STATUS_INVALID_ARGS;
            }
            '?' => {
                builtin_unknown_option(parser, streams, cmd, argv[w.woptind - 1], print_hints);
                return STATUS_INVALID_ARGS;
            }
            _ => {
                panic!("unexpected retval from wgeopter.next()");
            }
        }
    }

    if print_help {
        builtin_print_help(parser, streams, cmd);
        return STATUS_CMD_OK;
    }

    if w.woptind == argc {
        // No jobs specified.
        // Note this may succeed with an empty wait list.
        return wait_for_completion(parser, &get_all_wait_handles(parser), any_flag);
    }

    // Get the list of wait handles for our waiting.
    let mut wait_handles: Vec<WaitHandleRef> = Vec::new();
    for i in w.woptind..argc {
        if iswnumeric(argv[i]) {
            // argument is pid
            let mpid: Result<pid_t, wutil::Error> = fish_wcstoi(argv[i]);
            if mpid.is_err() || mpid.unwrap() <= 0 {
                streams.err.append(wgettext_fmt!(
                    "%ls: '%ls' is not a valid process id\n",
                    cmd,
                    argv[i],
                ));
                continue;
            }
            let pid = mpid.unwrap() as pid_t;
            if !find_wait_handles(WaitHandleQuery::Pid(pid), parser, &mut wait_handles) {
                streams.err.append(wgettext_fmt!(
                    "%ls: Could not find a job with process id '%d'\n",
                    cmd,
                    pid,
                ));
            }
        } else {
            // argument is process name
            if !find_wait_handles(
                WaitHandleQuery::ProcName(argv[i]),
                parser,
                &mut wait_handles,
            ) {
                streams.err.append(wgettext_fmt!(
                    "%ls: Could not find child processes with the name '%ls'\n",
                    cmd,
                    argv[i],
                ));
            }
        }
    }
    if wait_handles.is_empty() {
        return STATUS_INVALID_ARGS;
    }
    return wait_for_completion(parser, &wait_handles, any_flag);
}
