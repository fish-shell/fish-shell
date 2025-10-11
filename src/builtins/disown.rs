// Implementation of the disown builtin.

use super::prelude::*;
use crate::io::IoStreams;
use crate::parser::Parser;
use crate::proc::{add_disowned_job, Job};
use crate::{builtins::shared::HelpOnlyCmdOpts, wchar::wstr, wutil::wgettext_fmt};
use libc::SIGCONT;

/// Helper for builtin_disown.
fn disown_job(cmd: &wstr, streams: &mut IoStreams, j: &Job) {
    // Nothing to do if already disowned.
    if j.flags().disown_requested {
        return;
    }

    // Stopped disowned jobs must be manually signaled; explain how to do so.
    let pgid = j.get_pgid();
    if j.is_stopped() {
        if let Some(pgid) = pgid {
            unsafe {
                libc::killpg(pgid.as_pid_t(), SIGCONT);
            }
        }
        streams.err.append(wgettext_fmt!(
            "%s: job %d ('%s') was stopped and has been signalled to continue.\n",
            cmd,
            j.job_id(),
            j.command()
        ));
    }

    // We cannot directly remove the job from the jobs() list as `disown` might be called
    // within the context of a subjob which will cause the parent job to crash in exec_job().
    // Instead, we set a flag and the parser removes the job from the jobs list later.
    j.mut_flags().disown_requested = true;
    add_disowned_job(j);
}

/// Builtin for removing jobs from the job list.
pub fn disown(parser: &Parser, streams: &mut IoStreams, args: &mut [&wstr]) -> BuiltinResult {
    let opts = HelpOnlyCmdOpts::parse(args, parser, streams)?;

    let cmd = args[0];
    if opts.print_help {
        builtin_print_help(parser, streams, cmd);
        return Ok(SUCCESS);
    }

    let mut retval: BuiltinResult;
    if opts.optind == args.len() {
        // Select last constructed job (ie first job in the job queue) that is possible to disown.
        // Stopped jobs can be disowned (they will be continued).
        // Foreground jobs can be disowned.
        // Even jobs that aren't under job control can be disowned!
        let mut job = None;
        for j in &parser.jobs()[..] {
            if j.is_constructed() && !j.is_completed() {
                job = Some(j.clone());
                break;
            }
        }

        if let Some(job) = job {
            disown_job(cmd, streams, &job);
            retval = Ok(SUCCESS);
        } else {
            streams
                .err
                .append(wgettext_fmt!("%s: There are no suitable jobs\n", cmd));
            retval = Err(STATUS_CMD_ERROR);
        }
    } else {
        retval = Ok(SUCCESS);

        // If one argument is not a valid pid (i.e. integer >= 0), fail without disowning anything,
        // but still print errors for all of them.
        // Non-existent jobs aren't an error, but information about them is useful.
        let mut jobs: Vec<_> = args[opts.optind..]
            .iter()
            .filter_map(|arg| {
                let pid = match parse_pid(streams, cmd, arg) {
                    Ok(pid) => pid,
                    Err(code) => {
                        retval = Err(code);
                        return None;
                    }
                };
                parser.job_get_from_pid(pid).or_else(|| {
                    // Valid identifier but no such job
                    streams
                        .err
                        .append(wgettext_fmt!("%s: Could not find job '%d'\n", cmd, pid));
                    None
                })
            })
            .collect();

        retval?;

        // One PID/JID may be repeated or multiple PIDs may refer to the same job;
        // include the job only once.
        jobs.sort_unstable_by_key(|job| job.job_id());
        jobs.dedup_by_key(|job| job.job_id());

        // Disown all target jobs.
        for j in jobs {
            disown_job(cmd, streams, &j);
        }
    }

    retval
}
