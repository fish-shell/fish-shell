use super::prelude::*;

pub fn r#true(
    _parser: &mut Parser,
    _streams: &mut IoStreams,
    _argv: &mut [&wstr],
) -> BuiltinResult {
    Ok(SUCCESS)
}
