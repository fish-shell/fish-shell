// Implementation of the bg builtin.

use libc::pid_t;
use std::pin::Pin;
use std::sync::RwLockWriteGuard;

use super::prelude::*;

/// Helper function for builtin_bg().
fn send_to_bg(
    parser: &Parser,
    streams: &mut IoStreams<'_>,
    cmd: &wstr,
    job_pos: usize,
) -> Option<c_int> {
    let jobs = parser.jobs();
    if !jobs[job_pos].wants_job_control() {
        let err = {
            let job = &jobs[job_pos];
            wgettext_fmt!(
                "%ls: Can't put job %d, '%ls' to background because it is not under job control\n",
                cmd,
                job.job_id().to_wstring(),
                job.command()
            )
        };
        builtin_print_help_error(parser, streams, cmd, &err);
        return STATUS_CMD_ERROR;
    }

    {
        let job = &jobs[job_pos];
        streams.err.append(&wgettext_fmt!(
            "Send job %d '%ls' to background\n",
            job.job_id().to_wstring(),
            job.command()
        ));

        job.group_mut().set_is_foreground(false);

        if !job.resume() {
            return STATUS_CMD_ERROR;
        }
    }
    parser.job_promote_at(job_pos);

    return STATUS_CMD_OK;
}

/// Builtin for putting a job in the background.
pub fn bg(parser: &Parser, streams: &mut IoStreams<'_>, args: &mut [WString]) -> Option<c_int> {
    let opts = match HelpOnlyCmdOpts::parse(args, parser, streams) {
        Ok(opts) => opts,
        Err(err @ Some(_)) if err != STATUS_CMD_OK => return err,
        Err(err) => panic!("Illogical exit code from parse_options(): {err:?}"),
    };

    let cmd = &args[0];
    if opts.print_help {
        builtin_print_help(parser, streams, cmd);
        return STATUS_CMD_OK;
    }

    if opts.optind == args.len() {
        // No jobs were specified so use the most recent (i.e., last) job.
        let jobs = parser.jobs();
        let job_pos = jobs
            .iter()
            .position(|job| job.is_stopped() && job.wants_job_control() && !job.is_completed());

        let Some(job_pos) = job_pos else {
            streams
                    .err
                    .append(&wgettext_fmt!("%ls: There are no suitable jobs\n", cmd));
                return STATUS_CMD_ERROR;
        };

        return send_to_bg(parser, streams, cmd, job_pos);
    }

    // The user specified at least one job to be backgrounded.
    let mut pids: Vec<pid_t> = Vec::new();

    // If one argument is not a valid pid (i.e. integer >= 0), fail without backgrounding anything,
    // but still print errors for all of them.
    let mut retval = STATUS_CMD_OK;
    let pids: Vec<pid_t> = args[opts.optind..]
        .iter()
        .map(|arg| {
            fish_wcstoi(arg).unwrap_or_else(|_| {
                streams.err.append(&wgettext_fmt!(
                    "%ls: '%ls' is not a valid job specifier\n",
                    cmd,
                    arg
                ));
                retval = STATUS_INVALID_ARGS;
                0
            })
        })
        .collect();

    if retval != STATUS_CMD_OK {
        return retval;
    }

    // Background all existing jobs that match the pids.
    // Non-existent jobs aren't an error, but information about them is useful.
    for pid in pids {
        if let Some((job_pos, _job)) = parser.job_get_from_pid_at(pid) {
            send_to_bg(parser, streams, cmd, job_pos);
        } else {
            streams
                .err
                .append(&wgettext_fmt!("%ls: Could not find job '%d'\n", cmd, pid));
        }
    }

    return STATUS_CMD_OK;
}
