use super::prelude::*;

pub fn r#continue(
    parser: &mut Parser,
    streams: &mut IoStreams,
    argv: &mut [&wstr],
) -> BuiltinResult {
    builtin_break_continue(parser, streams, argv)
}
