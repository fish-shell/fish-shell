// Implementation of the contains builtin.
use super::prelude::*;

#[derive(Debug, Clone, Copy, Default)]
struct Options {
    print_help: bool,
    print_index: bool,
}

fn parse_options(
    args: &mut [WString],
    parser: &Parser,
    streams: &mut IoStreams<'_>,
) -> Result<(Options, usize), Option<c_int>> {
    const SHORT_OPTS: &wstr = L!("+:hi");
    const LONG_OPTS: &[woption] = &[
        wopt(L!("help"), woption_argument_t::no_argument, 'h'),
        wopt(L!("index"), woption_argument_t::no_argument, 'i'),
    ];

    let mut opts = Options::default();

    let mut w = wgetopter_t::new(SHORT_OPTS, LONG_OPTS, args);
    while let Some(c) = w.wgetopt_long() {
        match c {
            'h' => opts.print_help = true,
            'i' => opts.print_index = true,
            ':' => {
                builtin_missing_argument(parser, streams, w.cmd(), &w.argv()[w.woptind - 1], false);
                return Err(STATUS_INVALID_ARGS);
            }
            '?' => {
                builtin_unknown_option(parser, streams, w.cmd(), &w.argv()[w.woptind - 1], false);
                return Err(STATUS_INVALID_ARGS);
            }
            _ => {
                panic!("unexpected retval from wgetopt_long");
            }
        }
    }

    Ok((opts, w.woptind))
}

/// Implementation of the builtin contains command, used to check if a specified string is part of
/// a list.
pub fn contains(
    parser: &Parser,
    streams: &mut IoStreams<'_>,
    args: &mut [WString],
) -> Option<c_int> {
    let (opts, optind) = match parse_options(args, parser, streams) {
        Ok((opts, optind)) => (opts, optind),
        Err(err @ Some(_)) if err != STATUS_CMD_OK => return err,
        Err(err) => panic!("Illogical exit code from parse_options(): {err:?}"),
    };
    let cmd = &args[0];

    if opts.print_help {
        builtin_print_help(parser, streams, cmd);
        return STATUS_CMD_OK;
    }

    let needle = args.get(optind);
    if let Some(needle) = needle {
        for (i, arg) in args[optind..].iter().enumerate().skip(1) {
            if needle == arg {
                if opts.print_index {
                    streams.out.appendln_owned(i.to_wstring());
                }
                return STATUS_CMD_OK;
            }
        }
    } else {
        streams
            .err
            .append(&wgettext_fmt!("%ls: Key not specified\n", cmd));
    }

    return STATUS_CMD_ERROR;
}
