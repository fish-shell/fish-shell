use libc::c_int;
use widestring_suffix::widestrs;

use super::shared::{builtin_print_help, HelpOnlyCmdOpts, STATUS_CMD_OK, STATUS_INVALID_ARGS};
use crate::event;
use crate::io::IoStreams;
use crate::parser::Parser;
use crate::wchar::{wstr, WString};
use crate::wutil::printf::sprintf;

#[widestrs]
pub fn emit(
    parser: & Parser,
    streams: &mut IoStreams<'_>,
    argv: &mut [WString],
) -> Option<c_int> {
    let opts = match HelpOnlyCmdOpts::parse(argv, parser, streams) {
        Ok(opts) => opts,
        Err(err @ Some(_)) if err != STATUS_CMD_OK => return err,
        Err(err) => panic!("Illogical exit code from parse_options(): {err:?}"),
    };

    let cmd = &argv[0];

    if opts.print_help {
        builtin_print_help(parser, streams, cmd);
        return STATUS_CMD_OK;
    }

    let Some(event_name) = argv.get(opts.optind) else {
        streams.err.append(&sprintf!("%ls: expected event name\n"L, cmd));
        return STATUS_INVALID_ARGS;
    };

    if true {
        todo!()
    }
    // event::fire_generic(
    //     parser,
    //     (*event_name).to_owned(),
    //     argv[opts.optind + 1..]
    //         .iter()
    //         .map(|&s| WString::from(s))
    //         .collect(),
    // );

    STATUS_CMD_OK
}
