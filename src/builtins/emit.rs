use super::prelude::*;
use crate::event;

pub fn emit(
    parser: &Parser,
    streams: &mut IoStreams,
    argv: &mut [&wstr],
) -> Result<StatusOk, StatusError> {
    let cmd = match argv.get(0) {
        Some(cmd) => *cmd,
        None => return Err(StatusError::STATUS_INVALID_ARGS),
    };

    let opts = HelpOnlyCmdOpts::parse(argv, parser, streams)?;

    if opts.print_help {
        builtin_print_help(parser, streams, cmd);
        return Ok(StatusOk::OK);
    }

    let Some(event_name) = argv.get(opts.optind) else {
        streams
            .err
            .append(sprintf!(L!("%ls: expected event name\n"), cmd));
        return Err(StatusError::STATUS_INVALID_ARGS);
    };

    event::fire_generic(
        parser,
        (*event_name).to_owned(),
        argv[opts.optind + 1..]
            .iter()
            .map(|&s| WString::from(s))
            .collect(),
    );

    Ok(StatusOk::OK)
}
