use libc::c_int;
use widestring_suffix::widestrs;

use super::shared::{
    builtin_print_help, io_streams_t, HelpOnlyCmdOpts, STATUS_CMD_OK, STATUS_INVALID_ARGS,
};
use crate::ffi::{self, parser_t, Repin};
use crate::wchar_ffi::{wstr, W0String, WCharToFFI};
use crate::wutil::format::printf::sprintf;

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
        streams.err.append(&sprintf!("%ls: expected event name\n"L, cmd));
        return STATUS_INVALID_ARGS;
    };

    let event_args: Vec<W0String> = argv[opts.optind + 1..]
        .iter()
        .map(|s| W0String::from_ustr(s).unwrap())
        .collect();
    let event_arg_ptrs: Vec<ffi::wcharz_t> = event_args
        .iter()
        .map(|s| ffi::wcharz_t { str_: s.as_ptr() })
        .collect();

    ffi::event_fire_generic(
        parser.pin(),
        event_name.to_ffi(),
        event_arg_ptrs.as_ptr(),
        c_int::try_from(event_arg_ptrs.len()).unwrap().into(),
    );

    STATUS_CMD_OK
}
