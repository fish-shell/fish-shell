// Implementation of the contains builtin.
use super::prelude::*;

#[derive(Debug, Clone, Copy, Default)]
struct Options {
    print_help: bool,
    print_index: bool,
}

fn parse_options(
    args: &mut [&wstr],
    parser: &Parser,
    streams: &mut IoStreams,
) -> Result<(Options, usize), ErrorCode> {
    let cmd = args[0];

    const SHORT_OPTS: &wstr = L!("+hi");
    const LONG_OPTS: &[WOption] = &[
        wopt(L!("help"), ArgType::NoArgument, 'h'),
        wopt(L!("index"), ArgType::NoArgument, 'i'),
    ];

    let mut opts = Options::default();

    let mut w = WGetopter::new(SHORT_OPTS, LONG_OPTS, args);
    while let Some(c) = w.next_opt() {
        match c {
            'h' => opts.print_help = true,
            'i' => opts.print_index = true,
            ':' => {
                builtin_missing_argument(parser, streams, cmd, args[w.wopt_index - 1], false);
                return Err(STATUS_INVALID_ARGS);
            }
            ';' => {
                builtin_unexpected_argument(parser, streams, cmd, args[w.wopt_index - 1], false);
                return Err(STATUS_INVALID_ARGS);
            }
            '?' => {
                builtin_unknown_option(parser, streams, cmd, args[w.wopt_index - 1], false);
                return Err(STATUS_INVALID_ARGS);
            }
            _ => {
                panic!("unexpected retval from WGetopter");
            }
        }
    }

    Ok((opts, w.wopt_index))
}

/// Implementation of the builtin contains command, used to check if a specified string is part of
/// a list.
pub fn contains(parser: &Parser, streams: &mut IoStreams, args: &mut [&wstr]) -> BuiltinResult {
    let cmd = args[0];

    let (opts, optind) = parse_options(args, parser, streams)?;

    if opts.print_help {
        builtin_print_help(parser, streams, cmd);
        return Ok(SUCCESS);
    }

    let needle = args.get(optind);
    if let Some(needle) = needle {
        for (i, arg) in args[optind..].iter().enumerate().skip(1) {
            if needle == arg {
                if opts.print_index {
                    streams.out.appendln(i.to_wstring());
                }
                return Ok(SUCCESS);
            }
        }
    } else {
        streams
            .err
            .append(wgettext_fmt!("%s: Key not specified\n", cmd));
    }

    Err(STATUS_CMD_ERROR)
}
