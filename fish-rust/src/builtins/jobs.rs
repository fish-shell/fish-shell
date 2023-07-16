//! Implementation of the jobs builtin.

use super::shared::{
    builtin_missing_argument, builtin_print_help, builtin_unknown_option, io_streams_t,
    STATUS_CMD_OK, STATUS_INVALID_ARGS, BUILTIN_ERR_ARG_COUNT1
};
use crate::ffi::parser_t;
use crate::wchar::{encode_byte_to_char, wstr, WString, L};
use crate::wgetopt::{wgetopter_t, wopt, woption, woption_argument_t::no_argument};
use crate::wutil::wgettext_fmt;
use libc::c_int;

#[derive(Debug, Clone, Copy)]
enum JobsMode {
    Default,
    PrintPid,
    PrintCommand,
    PrintGroup,
    PrintNothing,
}

#[derive(Debug, Clone, Copy)]
struct Options {
    mode: JobsMode,
    print_last: bool,
}

impl Default for Options {
    fn default() -> Self {
        Self {
            mode: JobsMode::Default,
            print_last: false,
        }
    }
}

fn parse_options(
    args: &mut [&wstr],
    parser: &mut parser_t,
    streams: &mut io_streams_t,
) -> Result<Options, Option<c_int>> {
    let cmd = args[0];
    let argc = args.len();

    let mut opts = Options::default();

    const SHORT_OPTS: &wstr = L!(":cghlpq");
    const LONG_OPTS: &[woption] = &[
        wopt(L!("command"), no_argument, 'c'),
        wopt(L!("group"), no_argument, 'g'),
        wopt(L!("help"), no_argument, 'h'),
        wopt(L!("last"), no_argument, 'l'),
        wopt(L!("pid"), no_argument, 'p'),
        wopt(L!("quiet"), no_argument, 'q'),
        wopt(L!("query"), no_argument, 'q'),
    ];

    let mut w = wgetopter_t::new(SHORT_OPTS, LONG_OPTS, args);

    while let Some(opt) = w.wgetopt_long() {
        match opt {
            'p' => opts.mode = JobsMode::PrintPid,
            'q' => opts.mode = JobsMode::PrintNothing,
            'c' => opts.mode = JobsMode::PrintCommand,
            'g' => opts.mode = JobsMode::PrintGroup,
            'l' => opts.print_last = true,
            'h' => {
                builtin_print_help(parser, streams, cmd);
                return Err(STATUS_CMD_OK);
            }
            ':' => {
                builtin_missing_argument(parser, streams, cmd, args[w.woptind - 1], true);
                return Err(STATUS_INVALID_ARGS);
            }
            '?' => {
                builtin_unknown_option(parser, streams, cmd, args[w.woptind - 1], false);
                return Err(STATUS_INVALID_ARGS);
            }
            _ => panic!("unexpected retval from wgetopt_long"),
        }
    }

    if w.woptind != argc {
        streams
            .err
            .append(wgettext_fmt!(BUILTIN_ERR_ARG_COUNT1, cmd, 0, argc - 1));
        return Err(STATUS_INVALID_ARGS);
    }

    Ok(opts)
}

pub fn jobs(
    parser: &mut parser_t,
    streams: &mut io_streams_t,
    args: &mut [&wstr],
) -> Option<c_int> {
    let opts = match parse_options(args, parser, streams) {
        Ok(opts) => opts,
        Err(err) => return err,
    };

    Some(1)
}
