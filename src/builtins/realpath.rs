//! Implementation of the realpath builtin.

use errno::errno;

use super::prelude::*;
use crate::env::Environment;
use crate::{
    path::path_apply_working_directory,
    wutil::{normalize_path, wrealpath},
};

#[derive(Default)]
struct Options {
    print_help: bool,
    no_symlinks: bool,
}

const short_options: &wstr = L!("+hs");
const long_options: &[WOption] = &[
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

    let mut w = WGetopter::new(short_options, long_options, args);

    while let Some(c) = w.next_opt() {
        match c {
            's' => opts.no_symlinks = true,
            'h' => opts.print_help = true,
            ':' => {
                builtin_missing_argument(parser, streams, cmd, args[w.wopt_index - 1], false);
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
pub fn realpath(parser: &Parser, streams: &mut IoStreams, args: &mut [&wstr]) -> BuiltinResult {
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
            if let Some(real_path) = wrealpath(arg) {
                streams.out.append(&real_path);
                streams.out.append(L!("\n"));
            } else {
                let errno = errno();
                if errno.0 != 0 {
                    streams.err.appendln(&wgettext_fmt!(
                        "builtin %s: %s: %s",
                        cmd,
                        arg,
                        errno.to_string()
                    ));
                } else {
                    streams
                        .err
                        .appendln(&wgettext_fmt!("builtin %s: Invalid arg: %s", cmd, arg));
                }
                had_error = true;
            }
        } else {
            let realpwd = wrealpath(&parser.vars().get_pwd_slash());

            if let Some(realpwd) = realpwd {
                let absolute_arg = if arg.starts_with(L!("/")) {
                    arg.to_owned()
                } else {
                    path_apply_working_directory(arg, &realpwd)
                };
                streams.out.append(&normalize_path(&absolute_arg, false));
                streams.out.append(L!("\n"));
            } else {
                streams.err.appendln(&wgettext_fmt!(
                    "builtin %s: realpath failed: %s",
                    cmd,
                    errno().to_string()
                ));
                had_error = true;
            }
        }
    }

    if had_error {
        Err(STATUS_CMD_ERROR)
    } else {
        Ok(SUCCESS)
    }
}
