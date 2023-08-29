// Implementation of the bg builtin.

use std::pin::Pin;

use super::prelude::*;

/// Helper function for builtin_bg().
fn send_to_bg(
    parser: &mut parser_t,
    streams: &mut io_streams_t,
    cmd: &wstr,
    job_pos: usize,
) -> Option<c_int> {
    let job = parser.get_jobs()[job_pos]
        .as_ref()
        .expect("job_pos must be valid");
    if !job.wants_job_control() {
        let err = wgettext_fmt!(
            "%ls: Can't put job %d, '%ls' to background because it is not under job control\n",
            cmd,
            job.job_id().0,
            job.command().from_ffi()
        );
        ffi::builtin_print_help(
            parser.pin(),
            streams.ffi_ref(),
            c_str!(cmd),
            err.to_ffi().as_ref()?,
        );
        return STATUS_CMD_ERROR;
    }

    streams.err.append(wgettext_fmt!(
        "Send job %d '%ls' to background\n",
        job.job_id().0,
        job.command().from_ffi()
    ));

    job.get_job_group().set_is_foreground(false);

    if !job.ffi_resume() {
        return STATUS_CMD_ERROR;
    }
    parser.pin().job_promote_at(job_pos);

    return STATUS_CMD_OK;
}

/// Builtin for putting a job in the background.
pub fn bg(parser: &mut parser_t, streams: &mut io_streams_t, args: &mut [&wstr]) -> Option<c_int> {
    let opts = match HelpOnlyCmdOpts::parse(args, parser, streams) {
        Ok(opts) => opts,
        Err(err @ Some(_)) if err != STATUS_CMD_OK => return err,
        Err(err) => panic!("Illogical exit code from parse_options(): {err:?}"),
    };

    let cmd = args[0];
    if opts.print_help {
        builtin_print_help(parser, streams, args.get(0)?);
        return STATUS_CMD_OK;
    }

    if opts.optind == args.len() {
        // No jobs were specified so use the most recent (i.e., last) job.
        let jobs = parser.get_jobs();
        let job_pos = jobs.iter().position(|job| {
            if let Some(job) = job.as_ref() {
                return job.is_stopped() && job.wants_job_control() && !job.is_completed();
            }

            false
        });

        let Some(job_pos) = job_pos else {
            streams
                .err
                .append(wgettext_fmt!("%ls: There are no suitable jobs\n", cmd));
            return STATUS_CMD_ERROR;
        };

        return send_to_bg(parser, streams, cmd, job_pos);
    }

    // The user specified at least one job to be backgrounded.
    let mut pids: Vec<libc::pid_t> = Vec::new();

    // If one argument is not a valid pid (i.e. integer >= 0), fail without backgrounding anything,
    // but still print errors for all of them.
    let mut retval: Option<i32> = STATUS_CMD_OK;
    for arg in &args[opts.optind..] {
        let pid = fish_wcstoi(arg);
        #[allow(clippy::unnecessary_unwrap)]
        if pid.is_err() || pid.unwrap() < 0 {
            streams.err.append(wgettext_fmt!(
                "%ls: '%ls' is not a valid job specifier\n",
                cmd,
                arg
            ));
            retval = STATUS_INVALID_ARGS;
        } else {
            pids.push(pid.unwrap());
        }
    }

    if retval != STATUS_CMD_OK {
        return retval;
    }

    // Background all existing jobs that match the pids.
    // Non-existent jobs aren't an error, but information about them is useful.
    for pid in pids {
        let mut job_pos = 0;
        let job = unsafe {
            parser
                .job_get_from_pid1(autocxx::c_int(pid), Pin::new(&mut job_pos))
                .as_ref()
        };

        if job.is_some() {
            send_to_bg(parser, streams, cmd, job_pos);
        } else {
            streams
                .err
                .append(wgettext_fmt!("%ls: Could not find job '%d'\n", cmd, pid));
        }
    }

    return STATUS_CMD_OK;
}
