use autocxx::PinMut;
use libc::c_int;

use crate::builtins::shared::io_streams_t;
use crate::ffi::{parser_t, Repin};
use crate::wchar::{wstr, L, WString};
use crate::wgetopt::{wgetopter_t, wopt, woption, woption_argument_t};
use crate::wutil::wgettext_fmt;

use super::shared::{builtin_print_help, builtin_unknown_option, STATUS_CMD_OK, STATUS_INVALID_ARGS};

const short_options: &wstr = L!("LPh");
const long_options: &[woption] = &[
    wopt(L!("help"), woption_argument_t::no_argument, 'h'),
    wopt(L!("logical"), woption_argument_t::no_argument, 'L'),
    wopt(L!("physical"), woption_argument_t::no_argument, 'P'),
];

pub fn pwd(parser: &mut parser_t, streams: &mut io_streams_t, argv: &mut [&wstr]) -> Option<c_int> {
    let cmd = argv[0];
    let argc = argv.len();
    let mut resolve_symlinks = false;
    let print_hints = false;

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
                builtin_unknown_option(parser, streams, cmd, argv[w.woptind - 1], print_hints);
            }
            _ => panic!("unexpected retval from wgetopt_long"),
        }
    }

    if w.woptind != argc {
        streams.err.append(wgettext_fmt!(
            "%ls: expected %d arguments; got %d\n",
            cmd,
            0,
            argc - 1
        ));
        return STATUS_INVALID_ARGS;
    }

    let mut pwd = WString::new();
    if resolve_symlinks {
        
    }

    return STATUS_CMD_OK;
}
