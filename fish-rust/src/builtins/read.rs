//! Implementation of the read builtin.

use super::prelude::*;

pub fn read(parser: &Parser, streams: &mut IoStreams, args: &mut [&wstr]) -> Option<c_int> {
    run_builtin_ffi(crate::ffi::builtin_read, parser, streams, args)
}
