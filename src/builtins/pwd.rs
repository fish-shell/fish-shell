//! Implementation of the pwd builtin.
use errno::errno;

use super::prelude::*;
use crate::{builtins::error::Error, env::Environment as _, err_fmt, wutil::wrealpath};

// The pwd builtin. Respect -P to resolve symbolic links. Respect -L to not do that (the default).
const SHORT_OPTIONS: &wstr = L!("LPh");
const LONG_OPTIONS: &[WOption] = &[
    wopt(L!("help"), NoArgument, 'h'),
    wopt(L!("logical"), NoArgument, 'L'),
    wopt(L!("physical"), NoArgument, 'P'),
];

pub fn pwd(parser: &mut Parser, streams: &mut IoStreams, argv: &mut [&wstr]) -> BuiltinResult {
    let cmd = argv[0];
    let argc = argv.len();
    let mut resolve_symlinks = false;
    let mut w = WGetopter::new(SHORT_OPTIONS, LONG_OPTIONS, argv);
    while let Some(opt) = w.next_opt() {
        match opt {
            'L' => resolve_symlinks = false,
            'P' => resolve_symlinks = true,
            'h' => {
                builtin_print_help(parser, streams, cmd);
                return Ok(SUCCESS);
            }
            ';' => {
                builtin_unexpected_argument(parser, streams, cmd, argv[w.wopt_index - 1], false);
                return Err(STATUS_INVALID_ARGS);
            }
            '?' => {
                builtin_unknown_option(parser, streams, cmd, argv[w.wopt_index - 1], false);
                return Err(STATUS_INVALID_ARGS);
            }
            _ => panic!("unexpected retval from WGetopter"),
        }
    }

    if w.wopt_index != argc {
        err_fmt!(Error::UNEXP_ARG_COUNT, 0, argc - 1)
            .cmd(cmd)
            .finish(streams);
        return Err(STATUS_INVALID_ARGS);
    }

    let mut pwd = WString::new();
    if let Some(tmp) = parser.vars().get(L!("PWD")) {
        pwd = tmp.as_string();
    }
    if resolve_symlinks {
        if let Some(real_pwd) = wrealpath(&pwd) {
            pwd = real_pwd;
        } else {
            err_fmt!("%s failed: %s", "realpath", errno().to_string())
                .cmd(cmd)
                .finish(streams);
            return Err(STATUS_CMD_ERROR);
        }
    }
    if pwd.is_empty() {
        return Err(STATUS_CMD_ERROR);
    }
    streams.out.appendln(&pwd);
    Ok(SUCCESS)
}
