//! The eval builtin.

use super::prelude::*;
use crate::io::IoBufferfill;
use crate::parser::BlockType;
use crate::wcstringutil::join_strings;
use libc::{STDERR_FILENO, STDOUT_FILENO};

pub fn eval(parser: &Parser, streams: &mut IoStreams, args: &mut [&wstr]) -> Option<c_int> {
    let argc = args.len();
    if argc <= 1 {
        return STATUS_CMD_OK;
    }

    let new_cmd = join_strings(&args[1..], ' ');

    // Copy the full io chain; we may append bufferfills.
    let mut ios = streams.io_chain.clone();

    // If stdout is piped, then its output must go to the streams, not to the io_chain in our
    // streams, because the pipe may be intended to be consumed by a process which
    // is not yet launched (#6806). If stdout is NOT redirected, it must see the tty (#6955). So
    // create a bufferfill for stdout if and only if stdout is piped.
    // Note do not do this if stdout is merely redirected (say, to a file); we don't want to
    // buffer in that case.
    let mut stdout_fill = None;
    if streams.out_is_piped {
        match IoBufferfill::create_opts(parser.libdata().read_limit, STDOUT_FILENO) {
            Err(_) => {
                // We were unable to create a pipe, probably fd exhaustion.
                return STATUS_CMD_ERROR;
            }
            Ok(buffer) => {
                stdout_fill = Some(buffer.clone());
                ios.push(buffer);
            }
        }
    }

    // Of course the same applies to stderr.
    let mut stderr_fill = None;
    if streams.err_is_piped {
        match IoBufferfill::create_opts(parser.libdata().read_limit, STDERR_FILENO) {
            Err(_) => {
                // We were unable to create a pipe, probably fd exhaustion.
                return STATUS_CMD_ERROR;
            }
            Ok(buffer) => {
                stderr_fill = Some(buffer.clone());
                ios.push(buffer);
            }
        }
    }

    let res = parser.eval_with(&new_cmd, &ios, streams.job_group.as_ref(), BlockType::top);
    let status = if res.was_empty {
        // Issue #5692, in particular, to catch `eval ""`, `eval "begin; end;"`, etc.
        // where we have an argument but nothing is executed.
        STATUS_CMD_OK
    } else {
        Some(res.status.status_value())
    };

    // Finish the bufferfills - exhaust and close our pipes.
    // Copy the output from the bufferfill back to the streams.
    // Note it is important that we hold no other references to the bufferfills here - they need to
    // deallocate to close.
    ios.clear();
    if let Some(stdout) = stdout_fill {
        let output = IoBufferfill::finish(stdout);
        streams.out.append_narrow_buffer(&output);
    }
    if let Some(stderr) = stderr_fill {
        let errput = IoBufferfill::finish(stderr);
        streams.err.append_narrow_buffer(&errput);
    }

    status
}
