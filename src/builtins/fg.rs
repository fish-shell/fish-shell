//! Implementation of the fg builtin.

use crate::fds::make_fd_blocking;
use crate::proc::Pid;
use crate::reader::reader_write_title;
use crate::tokenizer::tok_command;
use crate::wutil::perror;
use crate::{env::EnvMode, proc::TtyTransfer};
use libc::{STDIN_FILENO, TCSADRAIN};

use super::prelude::*;

/// Builtin for putting a job in the foreground.
pub fn fg(
    parser: &Parser,
    streams: &mut IoStreams,
    argv: &mut [&wstr],
) -> Result<StatusOk, StatusError> {
    let opts = HelpOnlyCmdOpts::parse(argv, parser, streams)?;

    let cmd = match argv.get(0) {
        Some(cmd) => *cmd,
        None => return Err(StatusError::STATUS_INVALID_ARGS),
    };

    if opts.print_help {
        builtin_print_help(parser, streams, cmd);
        return Ok(StatusOk::OK);
    }

    let job;
    let job_pos;
    let optind = opts.optind;
    if optind == argv.len() {
        // Select last constructed job (i.e. first job in the job queue) that can be brought
        // to the foreground.
        let jobs = parser.jobs();
        match jobs.iter().enumerate().find(|(_pos, job)| {
            job.is_constructed()
                && !job.is_completed()
                && ((job.is_stopped() || !job.is_foreground()) && job.wants_job_control())
        }) {
            None => {
                streams
                    .err
                    .append(wgettext_fmt!("%ls: There are no suitable jobs\n", cmd));
                return Err(StatusError::STATUS_INVALID_ARGS);
            }
            Some((pos, j)) => {
                job_pos = Some(pos);
                job = Some(j.clone());
            }
        }
    } else if optind + 1 < argv.len() {
        // Specifying more than one job to put to the foreground is a syntax error, we still
        // try to locate the job $argv[1], since we need to determine which error message to
        // emit (ambigous job specification vs malformed job id).
        let mut found_job = false;
        if let Ok(Some(pid)) = fish_wcstoi(argv[optind]).map(Pid::new) {
            found_job = parser.job_get_from_pid(pid).is_some();
        };

        if found_job {
            streams
                .err
                .append(wgettext_fmt!("%ls: Ambiguous job\n", cmd));
        } else {
            streams.err.append(wgettext_fmt!(
                "%ls: '%ls' is not a job\n",
                cmd,
                argv[optind]
            ));
        }

        builtin_print_error_trailer(parser, streams.err, cmd);
        job_pos = None;
        job = None;
    } else {
        match fish_wcstoi(argv[optind]) {
            Err(_) => {
                streams
                    .err
                    .append(wgettext_fmt!(BUILTIN_ERR_NOT_NUMBER, cmd, argv[optind]));
                job_pos = None;
                job = None;
                builtin_print_error_trailer(parser, streams.err, cmd);
            }
            Ok(pid) => {
                let raw_pid = pid;
                let pid = Pid::new(pid.abs());
                let j = pid.and_then(|pid| parser.job_get_with_index_from_pid(pid));
                if j.as_ref()
                    .map_or(true, |(_pos, j)| !j.is_constructed() || j.is_completed())
                {
                    streams
                        .err
                        .append(wgettext_fmt!("%ls: No suitable job: %d\n", cmd, raw_pid));
                    job_pos = None;
                    job = None
                } else {
                    let (pos, j) = j.unwrap();
                    job_pos = Some(pos);
                    job = if !j.wants_job_control() {
                        streams.err.append(wgettext_fmt!(
                            concat!(
                                "%ls: Can't put job %d, '%ls' to foreground because ",
                                "it is not under job control\n"
                            ),
                            cmd,
                            raw_pid,
                            j.command()
                        ));
                        None
                    } else {
                        Some(j)
                    };
                }
            }
        }
    };

    let Some(job) = job else {
        return Err(StatusError::STATUS_INVALID_ARGS);
    };
    let job_pos = job_pos.unwrap();

    if streams.err_is_redirected {
        streams
            .err
            .append(wgettext_fmt!(FG_MSG, job.job_id(), job.command()));
    } else {
        // If we aren't redirecting, send output to real stderr, since stuff in sb_err won't get
        // printed until the command finishes.
        eprintf!("%s", wgettext_fmt!(FG_MSG, job.job_id(), job.command()));
    }

    let ft = tok_command(job.command());
    if !ft.is_empty() {
        // Provide value for `status current-command`
        parser.libdata_mut().status_vars.command = ft.clone();
        // Also provide a value for the deprecated fish 2.0 $_ variable
        parser.set_var_and_fire(L!("_"), EnvMode::EXPORT, vec![ft]);
        // Provide value for `status current-commandline`
        parser.libdata_mut().status_vars.commandline = job.command().to_owned();
    }
    reader_write_title(job.command(), parser, true);

    // Note if tty transfer fails, we still try running the job.
    parser.job_promote_at(job_pos);
    let _ = make_fd_blocking(STDIN_FILENO);
    {
        let job_group = job.group();
        job_group.set_is_foreground(true);
        if job.entitled_to_terminal() {
            crate::input_common::terminal_protocols_disable_ifn();
        }
        let tmodes = job_group.tmodes.borrow();
        if job_group.wants_terminal() && tmodes.is_some() {
            let termios = tmodes.as_ref().unwrap();
            let res = unsafe { libc::tcsetattr(STDIN_FILENO, TCSADRAIN, termios) };
            if res < 0 {
                perror("tcsetattr");
            }
        }
    }
    let mut transfer = TtyTransfer::new();
    transfer.to_job_group(job.group.as_ref().unwrap());
    let resumed = job.resume();
    if resumed {
        job.continue_job(parser);
    }
    if job.is_stopped() {
        transfer.save_tty_modes();
    }
    transfer.reclaim();
    if resumed {
        Ok(StatusOk::OK)
    } else {
        Err(StatusError::STATUS_CMD_ERROR)
    }
}
