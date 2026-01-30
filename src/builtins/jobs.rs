// Functions for executing the jobs builtin.

use super::prelude::*;
use crate::common::{EscapeFlags, EscapeStringStyle, escape_string, timef};
use crate::io::IoStreams;
use crate::job_group::{JobId, MaybeJobId};
use crate::localization::{wgettext, wgettext_fmt};
use crate::parser::Parser;
use crate::proc::{HAVE_PROC_STAT, Job, clock_ticks_to_seconds, proc_get_jiffies};
use crate::wutil::fish_wcstoi;
use fish_wgetopt::{ArgType, WGetopter, WOption, wopt};
use fish_widestring::{L, WExt, WString, wstr};
use std::num::NonZeroU32;

/// Print modes for the jobs builtin.

#[derive(Clone, Copy, Eq, PartialEq)]
enum JobsPrintMode {
    Default,      // print lots of general info
    PrintPid,     // print pid of each process in job
    PrintCommand, // print command name of each process in job
    PrintGroup,   // print group ID of job
    PrintNothing, // print nothing (exit status only)
}

/// Calculates the cpu usage (as a fraction of 1) of the specified job.
/// This may exceed 1 if there are multiple CPUs!
fn cpu_use(j: &Job) -> f64 {
    let mut u = 0.0;
    for p in j.external_procs() {
        let now = timef();
        let jiffies = proc_get_jiffies(*p.pid.get().unwrap());
        let last_jiffies = p.last_times.get().jiffies;
        let since = now - last_jiffies as f64;
        if since > 0.0 && jiffies > last_jiffies {
            u += clock_ticks_to_seconds(jiffies - last_jiffies) / since;
        }
    }
    u
}

/// Print information about the specified job.
fn builtin_jobs_print(j: &Job, mode: JobsPrintMode, header: bool, streams: &mut IoStreams) {
    let pgid = match j.get_pgid() {
        Some(pgid) => pgid.to_string(),
        None => "-".to_string(),
    };

    let mut out = WString::new();
    match mode {
        JobsPrintMode::PrintNothing => (),
        JobsPrintMode::Default => {
            if header {
                // Print table header before first job.
                out += wgettext!("Job\tGroup\t");
                if *HAVE_PROC_STAT {
                    out += wgettext!("CPU\t");
                }
                out += wgettext!("State\tCommand");
                out.push('\n');
            }

            sprintf!(=> &mut out, "%d\t%s\t", j.job_id(), pgid);

            if *HAVE_PROC_STAT {
                sprintf!(=> &mut out, "%.0f%%\t", 100.0 * cpu_use(j));
            }

            out += if j.is_stopped() {
                wgettext!("stopped")
            } else {
                wgettext!("running")
            };
            out += "\t";

            let cmd = escape_string(
                j.command(),
                EscapeStringStyle::Script(EscapeFlags::NO_PRINTABLES),
            );
            out += &cmd[..];

            out += "\n";
            streams.out.append(&out);
        }
        JobsPrintMode::PrintGroup => {
            if header {
                // Print table header before first job.
                out += wgettext!("Group");
                out.push('\n');
            }
            out += &sprintf!("%s\n", pgid)[..];
            streams.out.append(&out);
        }
        JobsPrintMode::PrintPid => {
            if header {
                // Print table header before first job.
                out += wgettext!("Process");
                out.push('\n');
            }

            for p in j.external_procs() {
                out += &sprintf!("%d\n", *p.pid.get().unwrap())[..];
            }
            streams.out.append(&out);
        }
        JobsPrintMode::PrintCommand => {
            if header {
                // Print table header before first job.
                out += wgettext!("Command");
                out.push('\n');
            }

            for p in j.processes() {
                out += &sprintf!("%s\n", p.argv0().unwrap())[..];
            }
            streams.out.append(&out);
        }
    }
}

const SHORT_OPTIONS: &wstr = L!("cghlpq");
const LONG_OPTIONS: &[WOption] = &[
    wopt(L!("command"), ArgType::NoArgument, 'c'),
    wopt(L!("group"), ArgType::NoArgument, 'g'),
    wopt(L!("help"), ArgType::NoArgument, 'h'),
    wopt(L!("last"), ArgType::NoArgument, 'l'),
    wopt(L!("pid"), ArgType::NoArgument, 'p'),
    wopt(L!("quiet"), ArgType::NoArgument, 'q'),
    wopt(L!("query"), ArgType::NoArgument, 'q'),
];

/// The jobs builtin. Used for printing running jobs. Defined in builtin_jobs.c.
pub fn jobs(parser: &Parser, streams: &mut IoStreams, argv: &mut [&wstr]) -> BuiltinResult {
    let cmd = match argv.first() {
        Some(cmd) => *cmd,
        None => return Err(STATUS_INVALID_ARGS),
    };

    let argc = argv.len();
    let mut found = false;
    let mut mode = JobsPrintMode::Default;
    let mut print_last = false;

    let mut w = WGetopter::new(SHORT_OPTIONS, LONG_OPTIONS, argv);
    while let Some(c) = w.next_opt() {
        match c {
            'p' => {
                mode = JobsPrintMode::PrintPid;
            }
            'q' => {
                mode = JobsPrintMode::PrintNothing;
            }
            'c' => {
                mode = JobsPrintMode::PrintCommand;
            }
            'g' => {
                mode = JobsPrintMode::PrintGroup;
            }
            'l' => {
                print_last = true;
            }
            'h' => {
                builtin_print_help(parser, streams, cmd);
                return Ok(SUCCESS);
            }
            ':' => {
                builtin_missing_argument(parser, streams, cmd, argv[w.wopt_index - 1], true);
                return Err(STATUS_INVALID_ARGS);
            }
            ';' => {
                builtin_unexpected_argument(parser, streams, cmd, argv[w.wopt_index - 1], true);
                return Err(STATUS_INVALID_ARGS);
            }
            '?' => {
                builtin_unknown_option(parser, streams, cmd, argv[w.wopt_index - 1], true);
                return Err(STATUS_INVALID_ARGS);
            }
            _ => panic!("unexpected retval from WGetopter"),
        }
    }

    if print_last {
        // Ignore unconstructed jobs, i.e. ourself.
        for j in &parser.jobs()[..] {
            if j.is_visible() {
                builtin_jobs_print(j, mode, !streams.out_is_redirected, streams);
                return Ok(SUCCESS);
            }
        }
        return Err(STATUS_CMD_ERROR);
    }

    if w.wopt_index < argc {
        for arg in &w.argv[w.wopt_index..] {
            let j;
            if arg.char_at(0) == '%' {
                match fish_wcstoi(&arg[1..]).ok().filter(|&job_id| job_id >= 0) {
                    None => {
                        streams.err.appendln(&wgettext_fmt!(
                            "%s: '%s' is not a valid job ID",
                            cmd,
                            arg
                        ));
                        return Err(STATUS_INVALID_ARGS);
                    }
                    Some(job_id) => {
                        let job_id = if job_id == 0 {
                            JobId::NONE
                        } else {
                            let job_id = u32::try_from(job_id).unwrap();
                            let job_id = NonZeroU32::try_from(job_id).unwrap();
                            MaybeJobId(Some(JobId::new(job_id)))
                        };
                        j = parser.job_with_id(job_id);
                    }
                }
            } else {
                let pid = parse_pid(streams, cmd, arg)?;
                j = parser.job_get_from_pid(pid);
            }

            if let Some(j) = j.filter(|j| !j.is_completed() && j.is_constructed()) {
                builtin_jobs_print(&j, mode, false, streams);
                found = true;
            } else {
                if mode != JobsPrintMode::PrintNothing {
                    streams
                        .err
                        .appendln(&wgettext_fmt!("%s: No suitable job: %s", cmd, arg));
                }
                return Err(STATUS_CMD_ERROR);
            }
        }
    } else {
        for j in &parser.jobs()[..] {
            // Ignore unconstructed jobs, i.e. ourself.
            if j.is_visible() {
                builtin_jobs_print(j, mode, !found && !streams.out_is_redirected, streams);
                found = true;
            }
        }
    }

    if !found {
        // Do not babble if not interactive.
        if !streams.out_is_redirected && mode != JobsPrintMode::PrintNothing {
            streams
                .out
                .appendln(&wgettext_fmt!("%s: There are no jobs", argv[0]));
        }
        return Err(STATUS_CMD_ERROR);
    }

    Ok(SUCCESS)
}
