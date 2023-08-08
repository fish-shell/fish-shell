use super::prelude::*;
use super::r#return::parse_return_value;

/// Function for handling the exit builtin.
pub fn exit(
    parser: &mut parser_t,
    streams: &mut io_streams_t,
    args: &mut [&wstr],
) -> Option<c_int> {
    let retval = match parse_return_value(args, parser, streams) {
        Ok(v) => v,
        Err(e) => return e,
    };

    // Mark that we are exiting in the parser.
    // TODO: in concurrent mode this won't successfully exit a pipeline, as there are other parsers
    // involved. That is, `exit | sleep 1000` may not exit as hoped. Need to rationalize what
    // behavior we want here.
    parser.libdata_pod().exit_current_script = true;

    return Some(retval);
}
