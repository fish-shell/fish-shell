//! Implementation of the history builtin.

use crate::history::in_private_mode;
use crate::history::{self, history_session_id, History};
use crate::reader::commandline_get_state;

use super::prelude::*;

#[derive(Default, Eq, PartialEq)]
enum HistCmd {
    HIST_SEARCH = 1,
    HIST_DELETE,
    HIST_CLEAR,
    HIST_MERGE,
    HIST_SAVE,
    #[default]
    HIST_UNDEF,
    HIST_CLEAR_SESSION,
}

impl HistCmd {
    fn to_wstr(&self) -> &'static wstr {
        match self {
            HistCmd::HIST_SEARCH => L!("search"),
            HistCmd::HIST_DELETE => L!("delete"),
            HistCmd::HIST_CLEAR => L!("clear"),
            HistCmd::HIST_MERGE => L!("merge"),
            HistCmd::HIST_SAVE => L!("save"),
            HistCmd::HIST_UNDEF => panic!(),
            HistCmd::HIST_CLEAR_SESSION => L!("clear-session"),
        }
    }
}

impl TryFrom<&wstr> for HistCmd {
    type Error = ();
    fn try_from(val: &wstr) -> Result<Self, ()> {
        match val {
            _ if val == "search" => Ok(HistCmd::HIST_SEARCH),
            _ if val == "delete" => Ok(HistCmd::HIST_DELETE),
            _ if val == "clear" => Ok(HistCmd::HIST_CLEAR),
            _ if val == "merge" => Ok(HistCmd::HIST_MERGE),
            _ if val == "save" => Ok(HistCmd::HIST_SAVE),
            _ if val == "clear-session" => Ok(HistCmd::HIST_CLEAR_SESSION),
            _ => Err(()),
        }
    }
}

#[derive(Default)]
struct HistoryCmdOpts {
    hist_cmd: HistCmd,
    search_type: Option<history::SearchType>,
    show_time_format: Option<String>,
    max_items: Option<usize>,
    print_help: bool,
    case_sensitive: bool,
    null_terminate: bool,
    reverse: bool,
}

/// Note: Do not add new flags that represent subcommands. We're encouraging people to switch to
/// the non-flag subcommand form. While many of these flags are deprecated they must be
/// supported at least until fish 3.0 and possibly longer to avoid breaking everyones
/// config.fish and other scripts.
const short_options: &wstr = L!(":CRcehmn:pt::z");
const longopts: &[woption] = &[
    wopt(L!("prefix"), woption_argument_t::no_argument, 'p'),
    wopt(L!("contains"), woption_argument_t::no_argument, 'c'),
    wopt(L!("help"), woption_argument_t::no_argument, 'h'),
    wopt(L!("show-time"), woption_argument_t::optional_argument, 't'),
    wopt(L!("exact"), woption_argument_t::no_argument, 'e'),
    wopt(L!("max"), woption_argument_t::required_argument, 'n'),
    wopt(L!("null"), woption_argument_t::no_argument, 'z'),
    wopt(L!("case-sensitive"), woption_argument_t::no_argument, 'C'),
    wopt(L!("delete"), woption_argument_t::no_argument, '\x01'),
    wopt(L!("search"), woption_argument_t::no_argument, '\x02'),
    wopt(L!("save"), woption_argument_t::no_argument, '\x03'),
    wopt(L!("clear"), woption_argument_t::no_argument, '\x04'),
    wopt(L!("merge"), woption_argument_t::no_argument, '\x05'),
    wopt(L!("reverse"), woption_argument_t::no_argument, 'R'),
];

/// Remember the history subcommand and disallow selecting more than one history subcommand.
fn set_hist_cmd(
    cmd: &wstr,
    hist_cmd: &mut HistCmd,
    sub_cmd: HistCmd,
    streams: &mut IoStreams,
) -> bool {
    if *hist_cmd != HistCmd::HIST_UNDEF {
        streams.err.append(wgettext_fmt!(
            BUILTIN_ERR_COMBO2_EXCLUSIVE,
            cmd,
            hist_cmd.to_wstr(),
            sub_cmd.to_wstr()
        ));
        return false;
    }
    *hist_cmd = sub_cmd;
    true
}

fn check_for_unexpected_hist_args(
    opts: &HistoryCmdOpts,
    cmd: &wstr,
    args: &[&wstr],
    streams: &mut IoStreams,
) -> bool {
    if opts.search_type.is_some() || opts.show_time_format.is_some() || opts.null_terminate {
        let subcmd_str = opts.hist_cmd.to_wstr();
        streams.err.append(wgettext_fmt!(
            "%ls: %ls: subcommand takes no options\n",
            cmd,
            subcmd_str
        ));
        return true;
    }
    if !args.is_empty() {
        let subcmd_str = opts.hist_cmd.to_wstr();
        streams.err.append(wgettext_fmt!(
            BUILTIN_ERR_ARG_COUNT2,
            cmd,
            subcmd_str,
            0,
            args.len()
        ));
        return true;
    }
    false
}

fn parse_cmd_opts(
    opts: &mut HistoryCmdOpts,
    optind: &mut usize,
    argv: &mut [&wstr],
    parser: &Parser,
    streams: &mut IoStreams,
) -> Option<c_int> {
    let cmd = argv[0];
    let mut w = wgetopter_t::new(short_options, longopts, argv);
    while let Some(opt) = w.wgetopt_long() {
        match opt {
            '\x01' => {
                if !set_hist_cmd(cmd, &mut opts.hist_cmd, HistCmd::HIST_DELETE, streams) {
                    return STATUS_CMD_ERROR;
                }
            }
            '\x02' => {
                if !set_hist_cmd(cmd, &mut opts.hist_cmd, HistCmd::HIST_SEARCH, streams) {
                    return STATUS_CMD_ERROR;
                }
            }
            '\x03' => {
                if !set_hist_cmd(cmd, &mut opts.hist_cmd, HistCmd::HIST_SAVE, streams) {
                    return STATUS_CMD_ERROR;
                }
            }
            '\x04' => {
                if !set_hist_cmd(cmd, &mut opts.hist_cmd, HistCmd::HIST_CLEAR, streams) {
                    return STATUS_CMD_ERROR;
                }
            }
            '\x05' => {
                if !set_hist_cmd(cmd, &mut opts.hist_cmd, HistCmd::HIST_MERGE, streams) {
                    return STATUS_CMD_ERROR;
                }
            }
            'C' => {
                opts.case_sensitive = true;
            }
            'R' => {
                opts.reverse = true;
            }
            'p' => {
                opts.search_type = Some(history::SearchType::PrefixGlob);
            }
            'c' => {
                opts.search_type = Some(history::SearchType::ContainsGlob);
            }
            'e' => {
                opts.search_type = Some(history::SearchType::Exact);
            }
            't' => {
                opts.show_time_format = Some(w.woptarg.unwrap_or(L!("# %c%n")).to_string());
            }
            'n' => match fish_wcstol(w.woptarg.unwrap()) {
                Ok(x) => opts.max_items = Some(x as _), // todo!("historical behavior is to cast")
                Err(_) => {
                    streams.err.append(wgettext_fmt!(
                        BUILTIN_ERR_NOT_NUMBER,
                        cmd,
                        w.woptarg.unwrap()
                    ));
                    return STATUS_INVALID_ARGS;
                }
            },
            'z' => {
                opts.null_terminate = true;
            }
            'h' => {
                opts.print_help = true;
            }
            ':' => {
                builtin_missing_argument(parser, streams, cmd, argv[w.woptind - 1], true);
                return STATUS_INVALID_ARGS;
            }
            '?' => {
                // Try to parse it as a number; e.g., "-123".
                match fish_wcstol(&w.argv[w.woptind - 1][1..]) {
                    Ok(x) => opts.max_items = Some(x as _), // todo!("historical behavior is to cast")
                    Err(_) => {
                        builtin_unknown_option(parser, streams, cmd, argv[w.woptind - 1], true);
                        return STATUS_INVALID_ARGS;
                    }
                }
                w.nextchar = L!("");
            }
            _ => {
                panic!("unexpected retval from wgetopt_long");
            }
        }
    }

    *optind = w.woptind;
    STATUS_CMD_OK
}

/// Manipulate history of interactive commands executed by the user.
pub fn history(parser: &Parser, streams: &mut IoStreams, args: &mut [&wstr]) -> Option<c_int> {
    let mut opts = HistoryCmdOpts::default();
    let mut optind = 0;
    let retval = parse_cmd_opts(&mut opts, &mut optind, args, parser, streams);
    if retval != STATUS_CMD_OK {
        return retval;
    }
    let cmd = &args[0];

    if opts.print_help {
        builtin_print_help(parser, streams, cmd);
        return STATUS_CMD_OK;
    }

    // Use the default history if we have none (which happens if invoked non-interactively, e.g.
    // from webconfig.py.
    let history = commandline_get_state()
        .history
        .unwrap_or_else(|| History::with_name(&history_session_id(parser.vars())));

    // If a history command hasn't already been specified via a flag check the first word.
    // Note that this can be simplified after we eliminate allowing subcommands as flags.
    // See the TODO above regarding the `long_options` array.
    if optind < args.len() {
        if let Ok(subcmd) = HistCmd::try_from(args[optind]) {
            if !set_hist_cmd(cmd, &mut opts.hist_cmd, subcmd, streams) {
                return STATUS_INVALID_ARGS;
            }
            optind += 1;
        }
    }

    // Every argument that we haven't consumed already is an argument for a subcommand (e.g., a
    // search term).
    let args = &args[optind..];

    // Establish appropriate defaults.
    if opts.hist_cmd == HistCmd::HIST_UNDEF {
        opts.hist_cmd = HistCmd::HIST_SEARCH;
    }
    if opts.search_type.is_none() {
        if opts.hist_cmd == HistCmd::HIST_SEARCH {
            opts.search_type = Some(history::SearchType::ContainsGlob);
        }
        if opts.hist_cmd == HistCmd::HIST_DELETE {
            opts.search_type = Some(history::SearchType::Exact);
        }
    }

    let mut status = STATUS_CMD_OK;
    match opts.hist_cmd {
        HistCmd::HIST_SEARCH => {
            if !history.search(
                opts.search_type.unwrap(),
                args,
                opts.show_time_format.as_deref(),
                opts.max_items.unwrap_or(usize::MAX),
                opts.case_sensitive,
                opts.null_terminate,
                opts.reverse,
                &parser.context().cancel_checker,
                streams,
            ) {
                status = STATUS_CMD_ERROR;
            }
        }
        HistCmd::HIST_DELETE => {
            // TODO: Move this code to the history module and support the other search types
            // including case-insensitive matches. At this time we expect the non-exact deletions to
            // be handled only by the history function's interactive delete feature.
            if opts.search_type.unwrap() != history::SearchType::Exact {
                streams
                    .err
                    .append(wgettext!("builtin history delete only supports --exact\n"));
                return STATUS_INVALID_ARGS;
            }
            if !opts.case_sensitive {
                streams.err.append(wgettext!(
                    "builtin history delete --exact requires --case-sensitive\n"
                ));
                return STATUS_INVALID_ARGS;
            }
            for delete_string in args.iter().copied() {
                history.remove(delete_string.to_owned());
            }
        }
        HistCmd::HIST_CLEAR => {
            if check_for_unexpected_hist_args(&opts, cmd, args, streams) {
                return STATUS_INVALID_ARGS;
            }
            history.clear();
            history.save();
        }
        HistCmd::HIST_CLEAR_SESSION => {
            if check_for_unexpected_hist_args(&opts, cmd, args, streams) {
                return STATUS_INVALID_ARGS;
            }
            history.clear_session();
            history.save();
        }
        HistCmd::HIST_MERGE => {
            if check_for_unexpected_hist_args(&opts, cmd, args, streams) {
                return STATUS_INVALID_ARGS;
            }

            if in_private_mode(parser.vars()) {
                streams.err.append(wgettext_fmt!(
                    "%ls: can't merge history in private mode\n",
                    cmd
                ));
                return STATUS_INVALID_ARGS;
            }
            history.incorporate_external_changes();
        }
        HistCmd::HIST_SAVE => {
            if check_for_unexpected_hist_args(&opts, cmd, args, streams) {
                return STATUS_INVALID_ARGS;
            }
            history.save();
        }
        HistCmd::HIST_UNDEF => panic!("Unexpected HIST_UNDEF seen"),
    }

    status
}
