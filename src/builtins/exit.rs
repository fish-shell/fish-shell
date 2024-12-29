use super::prelude::*;
use super::r#return::parse_return_value;

/// Function for handling the exit builtin.
pub fn exit(
    parser: &Parser,
    streams: &mut IoStreams,
    args: &mut [&wstr],
) -> Result<StatusOk, StatusError> {
    let result = parse_return_value(args, parser, streams);

    // Mark that we are exiting in the parser.
    // TODO: in concurrent mode this won't successfully exit a pipeline, as there are other parsers
    // involved. That is, `exit | sleep 1000` may not exit as hoped. Need to rationalize what
    // behavior we want here.
    parser.libdata_mut().exit_current_script = true;

    result
}
