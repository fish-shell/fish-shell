//! Implementation of the bind builtin.

use super::prelude::*;

const BIND_INSERT: c_int = 0;
const BIND_ERASE: c_int = 1;
const BIND_KEY_NAMES: c_int = 2;
const BIND_FUNCTION_NAMES: c_int = 3;

struct Options {
    all: bool,
    bind_mode_given: bool,
    list_modes: bool,
    print_help: bool,
    silent: bool,
    use_terminfo: bool,
    have_user: bool,
    user: bool,
    have_preset: bool,
    preset: bool,
    mode: c_int,
    bind_mode: &'static wstr,
    sets_bind_mode: &'static wstr,
}

pub fn bind(parser: &Parser, streams: &mut IoStreams, args: &mut [&wstr]) -> Option<c_int> {
    run_builtin_ffi(crate::ffi::builtin_bind, parser, streams, args)
}
