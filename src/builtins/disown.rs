// Implementation of the disown builtin.

use super::shared::{builtin_print_help, STATUS_CMD_ERROR, STATUS_INVALID_ARGS};
use crate::io::IoStreams;
use crate::parser::Parser;
use crate::proc::{add_disowned_job, Job, Pid};
use crate::{
    builtins::shared::{HelpOnlyCmdOpts, STATUS_CMD_OK},
    wchar::wstr,
    wutil::{fish_wcstoi, wgettext_fmt},
};
use libc::c_int;
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
            "%ls: job %d ('%ls') was stopped and has been signalled to continue.\n",
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
pub fn disown(parser: &Parser, streams: &mut IoStreams, args: &mut [&wstr]) -> Option<c_int> {
    let opts = match HelpOnlyCmdOpts::parse(args, parser, streams) {
        Ok(opts) => opts,
        Err(err @ Some(_)) if err != STATUS_CMD_OK => return err,
        Err(err) => panic!("Illogical exit code from parse_options(): {err:?}"),
    };

    let cmd = args[0];
    if opts.print_help {
        builtin_print_help(parser, streams, cmd);
        return STATUS_CMD_OK;
    }

    let mut retval;
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
            retval = STATUS_CMD_OK;
        } else {
            streams
                .err
                .append(wgettext_fmt!("%ls: There are no suitable jobs\n", cmd));
            retval = STATUS_CMD_ERROR;
        }
    } else {
        retval = STATUS_CMD_OK;

        // If one argument is not a valid pid (i.e. integer >= 0), fail without disowning anything,
        // but still print errors for all of them.
        // Non-existent jobs aren't an error, but information about them is useful.
        let mut jobs: Vec<_> = args[1..]
            .iter()
            .filter_map(|arg| {
                // Attempt to convert the argument to a PID.
                match fish_wcstoi(arg).ok().and_then(Pid::new) {
                    None => {
                        // Invalid identifier
                        streams.err.append(wgettext_fmt!(
                            "%ls: '%ls' is not a valid job specifier\n",
                            cmd,
                            arg
                        ));
                        retval = STATUS_INVALID_ARGS;
                        None
                    }
                    Some(pid) => parser.job_get_from_pid(pid).or_else(|| {
                        // Valid identifier but no such job
                        streams.err.append(wgettext_fmt!(
                            "%ls: Could not find job '%d'\n",
                            cmd,
                            pid
                        ));
                        None
                    }),
                }
            })
            .collect();

        if retval != STATUS_CMD_OK {
            return retval;
        }

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
