//! Implementation of the realpath builtin.

use errno::errno;

use super::prelude::*;
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
const long_options: &[woption] = &[
    wopt(L!("no-symlinks"), no_argument, 's'),
    wopt(L!("help"), no_argument, 'h'),
];

fn parse_options(
    args: &mut [&wstr],
    parser: &mut parser_t,
    streams: &mut io_streams_t,
) -> Result<(Options, usize), Option<c_int>> {
    let cmd = args[0];

    let mut opts = Options::default();

    let mut w = wgetopter_t::new(short_options, long_options, args);

    while let Some(c) = w.wgetopt_long() {
        match c {
            's' => opts.no_symlinks = true,
            'h' => opts.print_help = true,
            ':' => {
                builtin_missing_argument(parser, streams, cmd, args[w.woptind - 1], false);
                return Err(STATUS_INVALID_ARGS);
            }
            '?' => {
                builtin_unknown_option(parser, streams, cmd, args[w.woptind - 1], false);
                return Err(STATUS_INVALID_ARGS);
            }
            _ => panic!("unexpected retval from wgetopt_long"),
        }
    }

    Ok((opts, w.woptind))
}

/// An implementation of the external realpath command. Doesn't support any options.
/// In general scripts shouldn't invoke this directly. They should just use `realpath` which
/// will fallback to this builtin if an external command cannot be found.
pub fn realpath(
    parser: &mut parser_t,
    streams: &mut io_streams_t,
    args: &mut [&wstr],
) -> Option<c_int> {
    let cmd = args[0];
    let (opts, optind) = match parse_options(args, parser, streams) {
        Ok((opts, optind)) => (opts, optind),
        Err(err @ Some(_)) if err != STATUS_CMD_OK => return err,
        Err(err) => panic!("Illogical exit code from parse_options(): {err:?}"),
    };

    if opts.print_help {
        builtin_print_help(parser, streams, cmd);
        return STATUS_CMD_OK;
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
        return STATUS_INVALID_ARGS;
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

            return STATUS_CMD_ERROR;
        }
    } else {
        // We need to get the *physical* pwd here.
        let realpwd = wrealpath(parser.vars1().get_pwd_slash().as_wstr());

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
            return STATUS_CMD_ERROR;
        }
    }

    streams.out.append(L!("\n"));

    STATUS_CMD_OK
}
