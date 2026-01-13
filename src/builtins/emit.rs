use super::prelude::*;
use crate::{err_str, event};

pub fn emit(parser: &mut Parser, streams: &mut IoStreams, argv: &mut [&wstr]) -> BuiltinResult {
    let Some(&cmd) = argv.first() else {
        return Err(STATUS_INVALID_ARGS);
    };

    let opts = HelpOnlyCmdOpts::parse(argv, parser, streams)?;

    if opts.print_help {
        builtin_print_help(parser, streams, cmd);
        return Ok(SUCCESS);
    }

    let Some(event_name) = argv.get(opts.optind) else {
        err_str!("expected event name").cmd(cmd).finish(streams);
        return Err(STATUS_INVALID_ARGS);
    };

    event::fire_generic(
        parser,
        (*event_name).to_owned(),
        argv[opts.optind + 1..]
            .iter()
            .map(|&s| WString::from(s))
            .collect(),
    );

    Ok(SUCCESS)
}
