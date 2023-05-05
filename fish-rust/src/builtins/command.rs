use libc::c_int;

use crate::builtins::shared::{
    builtin_missing_argument, builtin_print_help, builtin_unknown_option, STATUS_CMD_OK,
    STATUS_CMD_UNKNOWN, STATUS_INVALID_ARGS,
};
use crate::io::IoStreams;
use crate::parser::Parser;
use crate::path::{path_get_path, path_get_paths};
use crate::wchar::{wstr, WString, L};
use crate::wchar_ffi::{WCharFromFFI, WCharToFFI};
use crate::wgetopt::{wgetopter_t, wopt, woption, woption_argument_t};
use crate::wutil::sprintf;

#[derive(Default)]
struct command_cmd_opts_t {
    all: bool,
    quiet: bool,
    find_path: bool,
}

pub fn r#command(
    parser: & Parser,
    streams: &mut IoStreams<'_>,
    argv: &mut [WString],
) -> Option<c_int> {
    let argc = argv.len();
    let print_hints = false;
    let mut opts: command_cmd_opts_t = Default::default();

    const shortopts: &wstr = L!(":hasqv");
    const longopts: &[woption] = &[
        wopt(L!("help"), woption_argument_t::no_argument, 'h'),
        wopt(L!("all"), woption_argument_t::no_argument, 'a'),
        wopt(L!("query"), woption_argument_t::no_argument, 'q'),
        wopt(L!("quiet"), woption_argument_t::no_argument, 'q'),
        wopt(L!("search"), woption_argument_t::no_argument, 's'),
    ];

    let mut w = wgetopter_t::new(shortopts, longopts, argv);
    while let Some(c) = w.wgetopt_long() {
        match c {
            'a' => opts.all = true,
            'q' => opts.quiet = true,
            's' => opts.find_path = true,
            // -s and -v are aliases
            'v' => opts.find_path = true,
            'h' => {
                builtin_print_help(parser, streams, w.cmd());
                return STATUS_CMD_OK;
            }
            ':' => {
                builtin_missing_argument(
                    parser,
                    streams,
                    w.cmd(),
                    &w.argv()[w.woptind - 1],
                    print_hints,
                );
                return STATUS_INVALID_ARGS;
            }
            '?' => {
                builtin_unknown_option(
                    parser,
                    streams,
                    w.cmd(),
                    &w.argv()[w.woptind - 1],
                    print_hints,
                );
                return STATUS_INVALID_ARGS;
            }
            _ => {
                panic!("unexpected retval from wgeopter.next()");
            }
        }
    }

    // Quiet implies find_path.
    if !opts.find_path && !opts.all && !opts.quiet {
        builtin_print_help(parser, streams, w.cmd());
        return STATUS_INVALID_ARGS;
    }

    let mut res = false;
    let optind = w.woptind;
    for arg in argv.iter().take(argc).skip(optind) {
        if opts.all {
            let paths = path_get_paths(arg, parser.vars());
            for path in paths {
                res = true;
                if opts.quiet {
                    return STATUS_CMD_OK;
                }

                streams.out.append(&sprintf!("%ls\n", path));
            }
        } else {
            // Either find_path explicitly or just quiet.
            if let Some(path) = path_get_path(arg, parser.vars()) {
                if opts.quiet {
                    return STATUS_CMD_OK;
                }
                res = true;
                streams.out.append(&sprintf!("%ls\n", path));
            }
        }
    }

    if res {
        STATUS_CMD_OK
    } else {
        STATUS_CMD_UNKNOWN
    }
}
