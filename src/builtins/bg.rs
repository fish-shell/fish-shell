// Implementation of the bg builtin.

use std::collections::HashSet;

use crate::proc::Pid;

use super::prelude::*;

/// Helper function for builtin_bg().
fn send_to_bg(
    parser: &Parser,
    streams: &mut IoStreams,
    cmd: &wstr,
    job_pos: usize,
) -> BuiltinResult {
    {
        let jobs = parser.jobs();
        if !jobs[job_pos].wants_job_control() {
            let job = &jobs[job_pos];
            streams.err.append(wgettext_fmt!(
                "%s: Can't put job %s, '%s' to background because it is not under job control\n",
                cmd,
                job.job_id().to_wstring(),
                job.command()
            ));
            return Err(STATUS_CMD_ERROR);
        }

        let job = &jobs[job_pos];
        streams.err.append(wgettext_fmt!(
            "Send job %s '%s' to background\n",
            job.job_id().to_wstring(),
            job.command()
        ));

        job.group().set_is_foreground(false);

        if !job.resume() {
            return Err(STATUS_CMD_ERROR);
        }
    }
    parser.job_promote_at(job_pos);

    return Ok(SUCCESS);
}

/// Builtin for putting a job in the background.
pub fn bg(parser: &Parser, streams: &mut IoStreams, args: &mut [&wstr]) -> BuiltinResult {
    let opts = HelpOnlyCmdOpts::parse(args, parser, streams)?;

    let Some(&cmd) = args.first() else {
        return Err(STATUS_INVALID_ARGS);
    };

    if opts.print_help {
        builtin_print_help(parser, streams, cmd);
        return Ok(SUCCESS);
    }

    if opts.optind == args.len() {
        // No jobs were specified so use the most recent (i.e., last) job.
        let job_pos = {
            let jobs = parser.jobs();
            jobs.iter()
                .position(|job| job.is_stopped() && job.wants_job_control() && !job.is_completed())
        };

        let Some(job_pos) = job_pos else {
            streams
                .err
                .append(wgettext_fmt!("%s: There are no suitable jobs\n", cmd));
            return Err(STATUS_CMD_ERROR);
        };

        return send_to_bg(parser, streams, cmd, job_pos);
    }

    // The user specified at least one job to be backgrounded.
    // If one argument is not a valid pid (i.e. integer >= 0), fail without backgrounding anything,
    // but still print errors for all of them.
    let mut retval: BuiltinResult = Ok(SUCCESS);
    let pids: Vec<Pid> = args[opts.optind..]
        .iter()
        .filter_map(|arg| match parse_pid(streams, cmd, arg) {
            Ok(pid) => Some(pid),
            _ => {
                retval = Err(STATUS_INVALID_ARGS);
                None
            }
        })
        .collect();

    retval?;

    // Background all existing jobs that match the pids.
    // Non-existent jobs aren't an error, but information about them is useful.
    let mut seen = HashSet::new();
    for pid in pids {
        if let Some((job_pos, job)) = parser.job_get_with_index_from_pid(pid) {
            if seen.insert(&*job as *const _) {
                send_to_bg(parser, streams, cmd, job_pos)?;
            }
        } else {
            streams
                .err
                .append(wgettext_fmt!("%s: Could not find job '%d'\n", cmd, pid));
        }
    }

    return Ok(SUCCESS);
}
