use libc::c_int;

use crate::builtins::shared::{
    builtin_missing_argument, builtin_print_help, builtin_unknown_option, io_streams_t,
    STATUS_CMD_OK, STATUS_CMD_UNKNOWN, STATUS_INVALID_ARGS,
};
use crate::ffi::parser_t;
use crate::ffi::path_get_paths_ffi;
use crate::wchar::{wstr, WString, L};
use crate::wchar_ffi::WCharFromFFI;
use crate::wchar_ffi::WCharToFFI;
use crate::wgetopt::{wgetopter_t, wopt, woption, woption_argument_t};
use crate::wutil::sprintf;

#[derive(Default)]
struct command_cmd_opts_t {
    all: bool,
    quiet: bool,
    find_path: bool,
}

pub fn r#command(
    parser: &mut parser_t,
    streams: &mut io_streams_t,
    argv: &mut [&wstr],
) -> Option<c_int> {
    let cmd = argv[0];
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
                builtin_print_help(parser, streams, cmd);
                return STATUS_CMD_OK;
            }
            ':' => {
                builtin_missing_argument(parser, streams, cmd, argv[w.woptind - 1], print_hints);
                return STATUS_INVALID_ARGS;
            }
            '?' => {
                builtin_unknown_option(parser, streams, cmd, argv[w.woptind - 1], print_hints);
                return STATUS_INVALID_ARGS;
            }
            _ => {
                panic!("unexpected retval from wgeopter.next()");
            }
        }
    }

    // Quiet implies find_path.
    if !opts.find_path && !opts.all && !opts.quiet {
        builtin_print_help(parser, streams, cmd);
        return STATUS_INVALID_ARGS;
    }

    let mut res = false;
    let optind = w.woptind;
    for arg in argv.iter().take(argc).skip(optind) {
        // TODO: This always gets all paths, and then skips a bunch.
        // For the common case, we want to get just the one path.
        // Port this over once path.cpp is.
        let paths: Vec<WString> = path_get_paths_ffi(&arg.to_ffi(), parser).from_ffi();

        for path in paths.iter() {
            res = true;
            if opts.quiet {
                return STATUS_CMD_OK;
            }

            streams.out.append(sprintf!("%ls\n", path));
            if !opts.all {
                break;
            }
        }
    }

    if res {
        STATUS_CMD_OK
    } else {
        STATUS_CMD_UNKNOWN
    }
}
