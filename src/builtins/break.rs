use super::prelude::*;

pub fn r#break(parser: &Parser, streams: &mut IoStreams, argv: &mut [&wstr]) -> BuiltinResult {
    builtin_break_continue(parser, streams, argv)
}
