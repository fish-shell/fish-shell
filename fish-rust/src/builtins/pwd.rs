//! Implementation of the pwd builtin.
use errno::errno;
use libc::c_int;

use super::shared::{
    builtin_print_help, builtin_unknown_option, STATUS_CMD_ERROR, STATUS_CMD_OK,
    STATUS_INVALID_ARGS,
};
use crate::env::Environment;
use crate::io::IoStreams;
use crate::parser::Parser;
use crate::{
    builtins::shared::BUILTIN_ERR_ARG_COUNT1,
    env::EnvMode,
    wchar::{wstr, WString, L},
    wchar_ffi::{WCharFromFFI, WCharToFFI},
    wgetopt::{wgetopter_t, wopt, woption, woption_argument_t::no_argument},
    wutil::{wgettext_fmt, wrealpath},
};

// The pwd builtin. Respect -P to resolve symbolic links. Respect -L to not do that (the default).
const short_options: &wstr = L!("LPh");
const long_options: &[woption] = &[
    wopt(L!("help"), no_argument, 'h'),
    wopt(L!("logical"), no_argument, 'L'),
    wopt(L!("physical"), no_argument, 'P'),
];

pub fn pwd(
    parser: & Parser,
    streams: &mut IoStreams<'_>,
    argv: &mut [WString],
) -> Option<c_int> {
    let argc = argv.len();
    let mut resolve_symlinks = false;
    let mut w = wgetopter_t::new(short_options, long_options, argv);
    while let Some(opt) = w.wgetopt_long() {
        match opt {
            'L' => resolve_symlinks = false,
            'P' => resolve_symlinks = true,
            'h' => {
                builtin_print_help(parser, streams, w.cmd());
                return STATUS_CMD_OK;
            }
            '?' => {
                builtin_unknown_option(parser, streams, w.cmd(), &w.argv()[w.woptind - 1], false);
                return STATUS_INVALID_ARGS;
            }
            _ => panic!("unexpected retval from wgetopt_long"),
        }
    }

    if w.woptind != argc {
        streams
            .err
            .append(&wgettext_fmt!(BUILTIN_ERR_ARG_COUNT1, w.cmd(), 0, argc - 1));
        return STATUS_INVALID_ARGS;
    }

    let mut pwd = WString::new();
    if let Some(tmp) = parser.vars().get(L!("PWD")) {
        pwd = tmp.as_string();
    }
    if resolve_symlinks {
        if let Some(real_pwd) = wrealpath(&pwd) {
            pwd = real_pwd;
        } else {
            streams.err.append(&wgettext_fmt!(
                "%ls: realpath failed: %s\n",
                w.cmd(),
                errno().to_string()
            ));
            return STATUS_CMD_ERROR;
        }
    }
    if pwd.is_empty() {
        return STATUS_CMD_ERROR;
    }
    streams.out.append(&pwd);
    streams.out.push('\n');
    return STATUS_CMD_OK;
}
