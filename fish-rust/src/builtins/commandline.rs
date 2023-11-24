use super::prelude::*;

pub fn commandline(parser: &Parser, streams: &mut IoStreams, args: &mut [&wstr]) -> Option<c_int> {
    run_builtin_ffi(crate::ffi::builtin_commandline, parser, streams, args)
}
