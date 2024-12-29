// Implementation of the return builtin.

use super::prelude::*;

#[derive(Debug, Clone, Copy, Default)]
struct Options {
    print_help: bool,
}

fn parse_options(
    args: &mut [&wstr],
    parser: &Parser,
    streams: &mut IoStreams,
) -> Result<(Options, usize), StatusError> {
    let cmd = args[0];

    const SHORT_OPTS: &wstr = L!(":h");
    const LONG_OPTS: &[WOption] = &[wopt(L!("help"), ArgType::NoArgument, 'h')];

    let mut opts = Options::default();

    let mut w = WGetopter::new(SHORT_OPTS, LONG_OPTS, args);

    while let Some(c) = w.next_opt() {
        match c {
            'h' => opts.print_help = true,
            ':' => {
                builtin_missing_argument(parser, streams, cmd, args[w.wopt_index - 1], true);
                return Err(StatusError::STATUS_INVALID_ARGS);
            }
            '?' => {
                // We would normally invoke builtin_unknown_option() and return an error.
                // But for this command we want to let it try and parse the value as a negative
                // return value.
                return Ok((opts, w.wopt_index - 1));
            }
            _ => {
                panic!("unexpected retval from next_opt");
            }
        }
    }

    Ok((opts, w.wopt_index))
}

/// Function for handling the return builtin.
pub fn r#return(
    parser: &Parser,
    streams: &mut IoStreams,
    args: &mut [&wstr],
) -> Result<StatusOk, StatusError> {
    let mut retval = parse_return_value(args, parser, streams);

    let has_function_block = parser.blocks_iter_rev().any(|b| b.is_function_call());

    // *nix does not support negative return values, but our `return` builtin happily accepts being
    // called with negative literals (e.g. `return -1`).
    // Map negative values to (256 - their absolute value). This prevents `return -1` from
    // evaluating to a `$status` of 0 and keeps us from running into undefined behavior by trying to
    // left shift a negative value in W_EXITCODE().
    // Note in Rust, dividend % divisor has the same sign as the dividend.
    if let Err(ref err) = retval {
        if err.get_code() < 0 {
            retval = Err(StatusError::from(256 - (err.get_code() % 256).abs()));
        }
    };

    // If we're not in a function, exit the current script (but not an interactive shell).
    if !has_function_block {
        let ld = &mut parser.libdata_mut();
        if !ld.is_interactive {
            ld.exit_current_script = true;
        }
        return retval;
    }

    // Mark a return in the libdata.
    parser.libdata_mut().returning = true;

    return retval;
}

pub fn parse_return_value(
    args: &mut [&wstr],
    parser: &Parser,
    streams: &mut IoStreams,
) -> Result<StatusOk, StatusError> {
    let cmd = args[0];
    let (opts, optind) = parse_options(args, parser, streams)?;

    if opts.print_help {
        builtin_print_help(parser, streams, cmd);
        return Ok(StatusOk::OK);
    }
    if optind + 1 < args.len() {
        streams
            .err
            .append(wgettext_fmt!(BUILTIN_ERR_TOO_MANY_ARGUMENTS, cmd));
        builtin_print_error_trailer(parser, streams.err, cmd);
        return Err(StatusError::STATUS_INVALID_ARGS);
    }
    if optind == args.len() {
        Err(StatusError::from(parser.get_last_status()))
    } else {
        match fish_wcstoi(args[optind]) {
            Ok(i) => match i {
                0 => Ok(StatusOk::OK),
                code => Err(StatusError::from(code)),
            },
            Err(_e) => {
                streams
                    .err
                    .append(wgettext_fmt!(BUILTIN_ERR_NOT_NUMBER, cmd, args[1]));
                builtin_print_error_trailer(parser, streams.err, cmd);
                return Err(StatusError::STATUS_INVALID_ARGS);
            }
        }
    }
}
