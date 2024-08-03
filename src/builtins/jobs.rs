// Functions for executing the jobs builtin.

use super::shared::{
    builtin_missing_argument, builtin_print_help, builtin_unknown_option, STATUS_CMD_ERROR,
    STATUS_INVALID_ARGS,
};
use crate::common::{escape_string, timef, EscapeFlags, EscapeStringStyle};
use crate::io::IoStreams;
use crate::job_group::{JobId, MaybeJobId};
use crate::parser::Parser;
use crate::proc::{clock_ticks_to_seconds, have_proc_stat, proc_get_jiffies, Job, INVALID_PID};
use crate::wchar_ext::WExt;
use crate::wgetopt::{wopt, ArgType, WGetopter, WOption};
use crate::wutil::wgettext;
use crate::{
    builtins::shared::STATUS_CMD_OK,
    wchar::{wstr, WString, L},
    wutil::{fish_wcstoi, wgettext_fmt},
};
use fish_printf::sprintf;
use libc::c_int;
use std::num::NonZeroU32;
use std::sync::atomic::Ordering;

/// Print modes for the jobs builtin.

#[derive(Clone, Copy, Eq, PartialEq)]
enum JobsPrintMode {
    Default,      // print lots of general info
    PrintPid,     // print pid of each process in job
    PrintCommand, // print command name of each process in job
    PrintGroup,   // print group id of job
    PrintNothing, // print nothing (exit status only)
}

/// Calculates the cpu usage (as a fraction of 1) of the specified job.
/// This may exceed 1 if there are multiple CPUs!
fn cpu_use(j: &Job) -> f64 {
    let mut u = 0.0;
    for p in j.processes() {
        let now = timef();
        let jiffies = proc_get_jiffies(p.pid.load(Ordering::Relaxed));
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
    let mut pgid = INVALID_PID;
    {
        if let Some(job_pgid) = j.get_pgid() {
            pgid = job_pgid;
        }
    }

    let mut out = WString::new();
    match mode {
        JobsPrintMode::PrintNothing => (),
        JobsPrintMode::Default => {
            if header {
                // Print table header before first job.
                out += wgettext!("Job\tGroup\t");
                if have_proc_stat() {
                    out += wgettext!("CPU\t");
                }
                out += wgettext!("State\tCommand\n");
            }

            sprintf!(=> &mut out, "%d\t%d\t", j.job_id(), pgid);

            if have_proc_stat() {
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
            streams.out.append(out);
        }
        JobsPrintMode::PrintGroup => {
            if header {
                // Print table header before first job.
                out += wgettext!("Group\n");
            }
            out += &sprintf!("%d\n", pgid)[..];
            streams.out.append(out);
        }
        JobsPrintMode::PrintPid => {
            if header {
                // Print table header before first job.
                out += wgettext!("Process\n");
            }

            for p in j.processes() {
                out += &sprintf!("%d\n", p.pid.load(Ordering::Relaxed))[..];
            }
            streams.out.append(out);
        }
        JobsPrintMode::PrintCommand => {
            if header {
                // Print table header before first job.
                out += wgettext!("Command\n");
            }

            for p in j.processes() {
                out += &sprintf!("%ls\n", p.argv0().unwrap())[..];
            }
            streams.out.append(out);
        }
    };
}

const SHORT_OPTIONS: &wstr = L!(":cghlpq");
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
pub fn jobs(parser: &Parser, streams: &mut IoStreams, argv: &mut [&wstr]) -> Option<c_int> {
    let cmd = argv[0];
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
                return STATUS_CMD_OK;
            }
            ':' => {
                builtin_missing_argument(parser, streams, cmd, argv[w.wopt_index - 1], true);
                return STATUS_INVALID_ARGS;
            }
            '?' => {
                builtin_unknown_option(parser, streams, cmd, argv[w.wopt_index - 1], true);
                return STATUS_INVALID_ARGS;
            }
            _ => panic!("unexpected retval from WGetopter"),
        }
    }

    if print_last {
        // Ignore unconstructed jobs, i.e. ourself.
        for j in &parser.jobs()[..] {
            if j.is_visible() {
                builtin_jobs_print(j, mode, !streams.out_is_redirected, streams);
                return STATUS_CMD_OK;
            }
        }
        return STATUS_CMD_ERROR;
    }

    if w.wopt_index < argc {
        for arg in &w.argv[w.wopt_index..] {
            let j;
            if arg.char_at(0) == '%' {
                match fish_wcstoi(&arg[1..]).ok().filter(|&job_id| job_id >= 0) {
                    None => {
                        streams.err.append(wgettext_fmt!(
                            "%ls: '%ls' is not a valid job id\n",
                            cmd,
                            arg
                        ));
                        return STATUS_INVALID_ARGS;
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
                match fish_wcstoi(arg).ok().filter(|&pid| pid >= 0) {
                    None => {
                        streams.err.append(wgettext_fmt!(
                            "%ls: '%ls' is not a valid process id\n",
                            cmd,
                            arg
                        ));
                        return STATUS_INVALID_ARGS;
                    }
                    Some(job_id) => {
                        j = parser.job_get_from_pid(job_id);
                    }
                }
            }

            if let Some(j) = j.filter(|j| !j.is_completed() && j.is_constructed()) {
                builtin_jobs_print(&j, mode, false, streams);
                found = true;
            } else {
                if mode != JobsPrintMode::PrintNothing {
                    streams
                        .err
                        .append(wgettext_fmt!("%ls: No suitable job: %ls\n", cmd, arg));
                }
                return STATUS_CMD_ERROR;
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
                .append(wgettext_fmt!("%ls: There are no jobs\n", argv[0]));
        }
        return STATUS_CMD_ERROR;
    }

    STATUS_CMD_OK
}
