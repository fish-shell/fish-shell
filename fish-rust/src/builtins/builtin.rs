use libc::c_int;

use crate::builtins::shared::{
    builtin_exists, builtin_get_desc, builtin_get_names, builtin_missing_argument,
    builtin_print_help, builtin_unknown_option, BUILTIN_ERR_COMBO2, STATUS_CMD_ERROR,
    STATUS_CMD_OK, STATUS_INVALID_ARGS,
};
use crate::io::IoStreams;
use crate::parser::Parser;
use crate::wchar::{wstr, WString, L};
use crate::wchar_ffi::WCharFromFFI;
use crate::wchar_ffi::WCharToFFI;
use crate::wgetopt::{wgetopter_t, wopt, woption, woption_argument_t};
use crate::wutil::{wgettext, wgettext_fmt};

#[derive(Default)]
struct builtin_cmd_opts_t {
    query: bool,
    list_names: bool,
}

pub fn r#builtin(
    parser: &mut Parser,
    streams: &mut IoStreams<'_>,
    argv: &mut [WString],
) -> Option<c_int> {
    let argc = argv.len();
    let print_hints = false;
    let mut opts: builtin_cmd_opts_t = Default::default();

    const shortopts: &wstr = L!(":hnq");
    const longopts: &[woption] = &[
        wopt(L!("help"), woption_argument_t::no_argument, 'h'),
        wopt(L!("names"), woption_argument_t::no_argument, 'n'),
        wopt(L!("query"), woption_argument_t::no_argument, 'q'),
    ];

    let mut w = wgetopter_t::new(shortopts, longopts, argv);
    while let Some(c) = w.wgetopt_long() {
        match c {
            'q' => opts.query = true,
            'n' => opts.list_names = true,
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

    if opts.query && opts.list_names {
        streams.err.append(&wgettext_fmt!(
            BUILTIN_ERR_COMBO2,
            w.cmd(),
            wgettext!("--query and --names are mutually exclusive")
        ));
        return STATUS_INVALID_ARGS;
    }

    if opts.query {
        let optind = w.woptind;
        for arg in argv.iter().take(argc).skip(optind) {
            if builtin_exists(arg) {
                return STATUS_CMD_OK;
            }
        }
        return STATUS_CMD_ERROR;
    }

    if opts.list_names {
        // List is guaranteed to be sorted by name.
        let names = builtin_get_names();
        for name in names {
            streams.out.append(&name);
            streams.out.push('\n');
        }
    }

    STATUS_CMD_OK
}
