//! Implementation of the pwd builtin.
use errno::errno;

use super::prelude::*;
use crate::{env::Environment, wutil::wrealpath};

// The pwd builtin. Respect -P to resolve symbolic links. Respect -L to not do that (the default).
const short_options: &wstr = L!("LPh");
const long_options: &[WOption] = &[
    wopt(L!("help"), NoArgument, 'h'),
    wopt(L!("logical"), NoArgument, 'L'),
    wopt(L!("physical"), NoArgument, 'P'),
];

pub fn pwd(parser: &Parser, streams: &mut IoStreams, argv: &mut [&wstr]) -> Option<c_int> {
    let cmd = argv[0];
    let argc = argv.len();
    let mut resolve_symlinks = false;
    let mut w = WGetopter::new(short_options, long_options, argv);
    while let Some(opt) = w.next_opt() {
        match opt {
            'L' => resolve_symlinks = false,
            'P' => resolve_symlinks = true,
            'h' => {
                builtin_print_help(parser, streams, cmd);
                return STATUS_CMD_OK;
            }
            '?' => {
                builtin_unknown_option(parser, streams, cmd, argv[w.wopt_index - 1], false);
                return STATUS_INVALID_ARGS;
            }
            _ => panic!("unexpected retval from WGetopter"),
        }
    }

    if w.wopt_index != argc {
        streams
            .err
            .append(wgettext_fmt!(BUILTIN_ERR_ARG_COUNT1, cmd, 0, argc - 1));
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
