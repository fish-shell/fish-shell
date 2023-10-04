use super::prelude::*;
use crate::event;

#[widestrs]
pub fn emit(
    parser: &mut parser_t,
    streams: &mut io_streams_t,
    argv: &mut [&wstr],
) -> Option<c_int> {
    let cmd = argv[0];

    let opts = match HelpOnlyCmdOpts::parse(argv, parser, streams) {
        Ok(opts) => opts,
        Err(err @ Some(_)) if err != STATUS_CMD_OK => return err,
        Err(err) => panic!("Illogical exit code from parse_options(): {err:?}"),
    };

    if opts.print_help {
        builtin_print_help(parser, streams, cmd);
        return STATUS_CMD_OK;
    }

    let Some(event_name) = argv.get(opts.optind) else {
        streams
            .err
            .append(sprintf!("%ls: expected event name\n"L, cmd));
        return STATUS_INVALID_ARGS;
    };

    event::fire_generic(
        parser,
        (*event_name).to_owned(),
        argv[opts.optind + 1..]
            .iter()
            .map(|&s| WString::from(s))
            .collect(),
    );

    STATUS_CMD_OK
}
