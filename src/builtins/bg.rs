// Implementation of the bg builtin.

use crate::proc::Pid;

use super::prelude::*;

/// Helper function for builtin_bg().
fn send_to_bg(
    parser: &Parser,
    streams: &mut IoStreams,
    cmd: &wstr,
    job_pos: usize,
) -> Result<StatusOk, StatusError> {
    {
        let jobs = parser.jobs();
        if !jobs[job_pos].wants_job_control() {
            let err = {
                let job = &jobs[job_pos];
                wgettext_fmt!(
                "%ls: Can't put job %s, '%ls' to background because it is not under job control\n",
                cmd,
                job.job_id().to_wstring(),
                job.command()
            )
            };
            builtin_print_help_error(parser, streams, cmd, &err);
            return Err(StatusError::STATUS_CMD_ERROR);
        }

        let job = &jobs[job_pos];
        streams.err.append(wgettext_fmt!(
            "Send job %s '%ls' to background\n",
            job.job_id().to_wstring(),
            job.command()
        ));

        job.group().set_is_foreground(false);

        if !job.resume() {
            return Err(StatusError::STATUS_CMD_ERROR);
        }
    }
    parser.job_promote_at(job_pos);

    return Ok(StatusOk::OK);
}

/// Builtin for putting a job in the background.
pub fn bg(
    parser: &Parser,
    streams: &mut IoStreams,
    args: &mut [&wstr],
) -> Result<StatusOk, StatusError> {
    let opts = HelpOnlyCmdOpts::parse(args, parser, streams)?;

    let cmd = match args.get(0) {
        Some(cmd) => *cmd,
        None => return Err(StatusError::STATUS_INVALID_ARGS),
    };

    if opts.print_help {
        builtin_print_help(parser, streams, cmd);
        return Ok(StatusOk::OK);
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
                .append(wgettext_fmt!("%ls: There are no suitable jobs\n", cmd));
            return Err(StatusError::STATUS_CMD_ERROR);
        };

        return send_to_bg(parser, streams, cmd, job_pos);
    }

    // The user specified at least one job to be backgrounded.
    // If one argument is not a valid pid (i.e. integer >= 0), fail without backgrounding anything,
    // but still print errors for all of them.
    let mut retval: Result<StatusOk, StatusError> = Ok(StatusOk::OK);
    let pids: Vec<Pid> = args[opts.optind..]
        .iter()
        .filter_map(|arg| match fish_wcstoi(arg).map(Pid::new) {
            Ok(Some(pid)) => Some(pid),
            _ => {
                streams.err.append(wgettext_fmt!(
                    "%ls: '%ls' is not a valid job specifier\n",
                    cmd,
                    arg
                ));
                retval = Err(StatusError::STATUS_INVALID_ARGS);
                None
            }
        })
        .collect();

    // clippy wants to use `?` here by using .as_ref() which doesn't exist for this Result type.
    #[allow(clippy::question_mark)]
    if retval.is_err() {
        return retval;
    }

    // Background all existing jobs that match the pids.
    // Non-existent jobs aren't an error, but information about them is useful.
    for pid in pids {
        if let Some((job_pos, _job)) = parser.job_get_with_index_from_pid(pid) {
            send_to_bg(parser, streams, cmd, job_pos)?;
        } else {
            streams
                .err
                .append(wgettext_fmt!("%ls: Could not find job '%d'\n", cmd, pid));
        }
    }

    return Ok(StatusOk::OK);
}
