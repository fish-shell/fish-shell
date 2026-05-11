use super::prelude::*;

pub fn r#false(
    _parser: &mut Parser,
    _streams: &mut IoStreams,
    _argv: &mut [&wstr],
) -> BuiltinResult {
    Err(STATUS_CMD_ERROR)
}
