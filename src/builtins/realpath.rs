//! Implementation of the realpath builtin.

use super::prelude::*;
use crate::env::Environment as _;
use crate::err_fmt;
use crate::{
    path::path_apply_working_directory,
    wutil::{normalize_path, wrealpath},
};

#[derive(Default)]
struct Options {
    print_help: bool,
    no_symlinks: bool,
}

const SHORT_OPTIONS: &wstr = L!("+hs");
const LONG_OPTIONS: &[WOption] = &[
    wopt(L!("no-symlinks"), NoArgument, 's'),
    wopt(L!("help"), NoArgument, 'h'),
];

fn parse_options(
    args: &mut [&wstr],
    parser: &Parser,
    streams: &mut IoStreams,
) -> Result<(Options, usize), ErrorCode> {
    let cmd = args[0];

    let mut opts = Options::default();

    let mut w = WGetopter::new(SHORT_OPTIONS, LONG_OPTIONS, args);

    while let Some(c) = w.next_opt() {
        match c {
            's' => opts.no_symlinks = true,
            'h' => opts.print_help = true,
            ':' => {
                builtin_missing_argument(parser, streams, cmd, None, args[w.wopt_index - 1], false);
                return Err(STATUS_INVALID_ARGS);
            }
            ';' => {
                builtin_unexpected_argument(parser, streams, cmd, args[w.wopt_index - 1], false);
                return Err(STATUS_INVALID_ARGS);
            }
            '?' => {
                builtin_unknown_option(parser, streams, cmd, args[w.wopt_index - 1], false);
                return Err(STATUS_INVALID_ARGS);
            }
            _ => panic!("unexpected retval from WGetopter"),
        }
    }

    Ok((opts, w.wopt_index))
}

/// An implementation of the external realpath command. Doesn't support any options.
/// In general scripts shouldn't invoke this directly. They should just use `realpath` which
/// will fallback to this builtin if an external command cannot be found.
pub fn realpath(parser: &mut Parser, streams: &mut IoStreams, args: &mut [&wstr]) -> BuiltinResult {
    let cmd = args[0];
    let (opts, optind) = parse_options(args, parser, streams)?;

    if opts.print_help {
        builtin_print_help(parser, streams, cmd);
        return Ok(SUCCESS);
    }

    if optind == args.len() {
        builtin_print_help(parser, streams, cmd);
        return Err(STATUS_INVALID_ARGS);
    }

    let mut had_error = false;

    for &arg in &args[optind..] {
        if !opts.no_symlinks {
            match wrealpath(arg) {
                Ok(real_path) => {
                    streams.out.append(&real_path);
                    streams.out.append(L!("\n"));
                }
                Err(error) => {
                    err_fmt!("%s: %s", arg, error.to_string())
                        .cmd(cmd)
                        .finish(streams);
                    had_error = true;
                }
            }
        } else {
            let realpwd = wrealpath(&parser.vars().get_pwd_slash());

            match realpwd {
                Ok(realpwd) => {
                    let absolute_arg = if arg.starts_with(L!("/")) {
                        arg.to_owned()
                    } else {
                        path_apply_working_directory(arg, &realpwd)
                    };
                    streams.out.appendln(&normalize_path(&absolute_arg, false));
                }
                Err(error) => {
                    err_fmt!("%s failed: %s", "realpath", error.to_string())
                        .cmd(cmd)
                        .finish(streams);
                    had_error = true;
                }
            }
        }
    }

    if had_error {
        Err(STATUS_CMD_ERROR)
    } else {
        Ok(SUCCESS)
    }
}
