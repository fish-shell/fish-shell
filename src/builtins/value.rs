use super::prelude::*;

pub fn value(_parser: &mut Parser, streams: &mut IoStreams, argv: &mut [&wstr]) -> BuiltinResult {
    for arg in &argv[1..] {
        streams
            .out
            .append_with_separation(*arg, SeparationType::Explicitly, true);
    }
    Ok(SUCCESS)
}
