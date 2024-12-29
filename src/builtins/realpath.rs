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

const short_options: &wstr = L!("+:hs");
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

    // TODO: allow arbitrary args. `realpath *` should print many paths
    if optind + 1 != args.len() {
        streams.err.append(wgettext_fmt!(
            BUILTIN_ERR_ARG_COUNT1,
            cmd,
            0,
            args.len() - 1
        ));
        builtin_print_help(parser, streams, cmd);
        return Err(STATUS_INVALID_ARGS);
    }

    let arg = args[optind];

    if !opts.no_symlinks {
        if let Some(real_path) = wrealpath(arg) {
            streams.out.append(real_path);
        } else {
            let errno = errno();
            if errno.0 != 0 {
                // realpath() just couldn't do it. Report the error and make it clear
                // this is an error from our builtin, not the system's realpath.
                streams.err.append(wgettext_fmt!(
                    "builtin %ls: %ls: %s\n",
                    cmd,
                    arg,
                    errno.to_string()
                ));
            } else {
                // Who knows. Probably a bug in our wrealpath() implementation.
                streams
                    .err
                    .append(wgettext_fmt!("builtin %ls: Invalid arg: %ls\n", cmd, arg));
            }

            return Err(STATUS_CMD_ERROR);
        }
    } else {
        // We need to get the *physical* pwd here.
        let realpwd = wrealpath(&parser.vars().get_pwd_slash());

        if let Some(realpwd) = realpwd {
            let absolute_arg = if arg.starts_with(L!("/")) {
                arg.to_owned()
            } else {
                path_apply_working_directory(arg, &realpwd)
            };
            streams.out.append(normalize_path(&absolute_arg, false));
        } else {
            streams.err.append(wgettext_fmt!(
                "builtin %ls: realpath failed: %s\n",
                cmd,
                errno().to_string()
            ));
            return Err(STATUS_CMD_ERROR);
        }
    }

    streams.out.append(L!("\n"));

    Ok(SUCCESS)
}
