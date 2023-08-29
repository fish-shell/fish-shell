//! Implementation of the pwd builtin.
use errno::errno;

use super::prelude::*;
use crate::{env::EnvMode, wutil::wrealpath};

// The pwd builtin. Respect -P to resolve symbolic links. Respect -L to not do that (the default).
const short_options: &wstr = L!("LPh");
const long_options: &[woption] = &[
    wopt(L!("help"), no_argument, 'h'),
    wopt(L!("logical"), no_argument, 'L'),
    wopt(L!("physical"), no_argument, 'P'),
];

pub fn pwd(parser: &mut parser_t, streams: &mut io_streams_t, argv: &mut [&wstr]) -> Option<c_int> {
    let cmd = argv[0];
    let argc = argv.len();
    let mut resolve_symlinks = false;
    let mut w = wgetopter_t::new(short_options, long_options, argv);
    while let Some(opt) = w.wgetopt_long() {
        match opt {
            'L' => resolve_symlinks = false,
            'P' => resolve_symlinks = true,
            'h' => {
                builtin_print_help(parser, streams, cmd);
                return STATUS_CMD_OK;
            }
            '?' => {
                builtin_unknown_option(parser, streams, cmd, argv[w.woptind - 1], false);
                return STATUS_INVALID_ARGS;
            }
            _ => panic!("unexpected retval from wgetopt_long"),
        }
    }

    if w.woptind != argc {
        streams
            .err
            .append(wgettext_fmt!(BUILTIN_ERR_ARG_COUNT1, cmd, 0, argc - 1));
        return STATUS_INVALID_ARGS;
    }

    let mut pwd = WString::new();
    let tmp = parser
        .vars1()
        .get_or_null(&L!("PWD").to_ffi(), EnvMode::default().bits());
    if !tmp.is_null() {
        pwd = tmp.as_string().from_ffi();
    }
    if resolve_symlinks {
        if let Some(real_pwd) = wrealpath(&pwd) {
            pwd = real_pwd;
        } else {
            streams.err.append(wgettext_fmt!(
                "%ls: realpath failed: %s\n",
                cmd,
                errno().to_string()
            ));
            return STATUS_CMD_ERROR;
        }
    }
    if pwd.is_empty() {
        return STATUS_CMD_ERROR;
    }
    streams.out.appendln(pwd);
    return STATUS_CMD_OK;
}
