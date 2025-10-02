//! Implementation of the history builtin.

use crate::history::in_private_mode;
use crate::history::{self, history_session_id, History};
use crate::reader::commandline_get_state;

use super::prelude::*;

#[derive(Default, Eq, PartialEq)]
enum HistCmd {
    Search,
    Delete,
    Clear,
    Merge,
    Save,
    #[default]
    None,
    ClearSession,
    Append,
}

impl HistCmd {
    fn to_wstr(&self) -> &'static wstr {
        match self {
            HistCmd::Search => L!("search"),
            HistCmd::Delete => L!("delete"),
            HistCmd::Clear => L!("clear"),
            HistCmd::Merge => L!("merge"),
            HistCmd::Save => L!("save"),
            HistCmd::None => panic!(),
            HistCmd::ClearSession => L!("clear-session"),
            HistCmd::Append => L!("append"),
        }
    }
}

impl TryFrom<&wstr> for HistCmd {
    type Error = ();
    fn try_from(val: &wstr) -> Result<Self, ()> {
        match val {
            _ if val == "search" => Ok(HistCmd::Search),
            _ if val == "delete" => Ok(HistCmd::Delete),
            _ if val == "clear" => Ok(HistCmd::Clear),
            _ if val == "merge" => Ok(HistCmd::Merge),
            _ if val == "save" => Ok(HistCmd::Save),
            _ if val == "clear-session" => Ok(HistCmd::ClearSession),
            _ if val == "append" => Ok(HistCmd::Append),
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
const short_options: &wstr = L!("CRcehmn:pt::z");
const longopts: &[WOption] = &[
    wopt(L!("prefix"), ArgType::NoArgument, 'p'),
    wopt(L!("contains"), ArgType::NoArgument, 'c'),
    wopt(L!("help"), ArgType::NoArgument, 'h'),
    wopt(L!("show-time"), ArgType::OptionalArgument, 't'),
    wopt(L!("exact"), ArgType::NoArgument, 'e'),
    wopt(L!("max"), ArgType::RequiredArgument, 'n'),
    wopt(L!("null"), ArgType::NoArgument, 'z'),
    wopt(L!("case-sensitive"), ArgType::NoArgument, 'C'),
    wopt(L!("delete"), ArgType::NoArgument, '\x01'),
    wopt(L!("search"), ArgType::NoArgument, '\x02'),
    wopt(L!("save"), ArgType::NoArgument, '\x03'),
    wopt(L!("clear"), ArgType::NoArgument, '\x04'),
    wopt(L!("merge"), ArgType::NoArgument, '\x05'),
    wopt(L!("reverse"), ArgType::NoArgument, 'R'),
];

/// Remember the history subcommand and disallow selecting more than one history subcommand.
fn set_hist_cmd(
    cmd: &wstr,
    hist_cmd: &mut HistCmd,
    sub_cmd: HistCmd,
    streams: &mut IoStreams,
) -> bool {
    if *hist_cmd != HistCmd::None {
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
            "%s: %s: subcommand takes no options\n",
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
) -> BuiltinResult {
    let Some(&cmd) = argv.get(0) else {
        return Err(STATUS_INVALID_ARGS);
    };

    let mut w = WGetopter::new(short_options, longopts, argv);
    while let Some(opt) = w.next_opt() {
        match opt {
            '\x01' => {
                if !set_hist_cmd(cmd, &mut opts.hist_cmd, HistCmd::Delete, streams) {
                    return Err(STATUS_CMD_ERROR);
                }
            }
            '\x02' => {
                if !set_hist_cmd(cmd, &mut opts.hist_cmd, HistCmd::Search, streams) {
                    return Err(STATUS_CMD_ERROR);
                }
            }
            '\x03' => {
                if !set_hist_cmd(cmd, &mut opts.hist_cmd, HistCmd::Save, streams) {
                    return Err(STATUS_CMD_ERROR);
                }
            }
            '\x04' => {
                if !set_hist_cmd(cmd, &mut opts.hist_cmd, HistCmd::Clear, streams) {
                    return Err(STATUS_CMD_ERROR);
                }
            }
            '\x05' => {
                if !set_hist_cmd(cmd, &mut opts.hist_cmd, HistCmd::Merge, streams) {
                    return Err(STATUS_CMD_ERROR);
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
                    return Err(STATUS_INVALID_ARGS);
                }
            },
            'z' => {
                opts.null_terminate = true;
            }
            'h' => {
                opts.print_help = true;
            }
            ':' => {
                builtin_missing_argument(parser, streams, cmd, argv[w.wopt_index - 1], true);
                return Err(STATUS_INVALID_ARGS);
            }
            ';' => {
                builtin_unexpected_argument(parser, streams, cmd, argv[w.wopt_index - 1], true);
                return Err(STATUS_INVALID_ARGS);
            }
            '?' => {
                // Try to parse it as a number; e.g., "-123".
                match fish_wcstol(&w.argv[w.wopt_index - 1][1..]) {
                    Ok(x) => opts.max_items = Some(x as _), // todo!("historical behavior is to cast")
                    Err(_) => {
                        builtin_unknown_option(parser, streams, cmd, argv[w.wopt_index - 1], true);
                        return Err(STATUS_INVALID_ARGS);
                    }
                }
                w.remaining_text = L!("");
            }
            _ => {
                panic!("unexpected retval from WGetopter");
            }
        }
    }

    *optind = w.wopt_index;
    Ok(SUCCESS)
}

/// Manipulate history of interactive commands executed by the user.
pub fn history(parser: &Parser, streams: &mut IoStreams, args: &mut [&wstr]) -> BuiltinResult {
    let mut opts = HistoryCmdOpts::default();
    let mut optind = 0;

    parse_cmd_opts(&mut opts, &mut optind, args, parser, streams)?;

    let Some(&cmd) = args.get(0) else {
        return Err(STATUS_INVALID_ARGS);
    };

    if opts.print_help {
        builtin_print_help(parser, streams, cmd);
        return Ok(SUCCESS);
    }

    // Use the default history if we have none (which happens if invoked non-interactively, e.g.
    // from webconfig.py.
    let history = commandline_get_state(true)
        .history
        .unwrap_or_else(|| History::with_name(&history_session_id(parser.vars())));

    // If a history command hasn't already been specified via a flag check the first word.
    // Note that this can be simplified after we eliminate allowing subcommands as flags.
    // See the TODO above regarding the `long_options` array.
    if optind < args.len() {
        if let Ok(subcmd) = HistCmd::try_from(args[optind]) {
            if !set_hist_cmd(cmd, &mut opts.hist_cmd, subcmd, streams) {
                return Err(STATUS_INVALID_ARGS);
            }
            optind += 1;
        }
    }

    // Every argument that we haven't consumed already is an argument for a subcommand (e.g., a
    // search term).
    let args = &args[optind..];

    let mut status = Ok(SUCCESS);
    match opts.hist_cmd {
        HistCmd::None | HistCmd::Search => {
            if !history.search(
                opts.search_type
                    .unwrap_or(history::SearchType::ContainsGlob),
                args,
                opts.show_time_format.as_deref(),
                opts.max_items.unwrap_or(usize::MAX),
                opts.case_sensitive,
                opts.null_terminate,
                opts.reverse,
                &parser.context().cancel_checker,
                streams,
            ) {
                status = Err(STATUS_CMD_ERROR);
            }
        }
        HistCmd::Delete => {
            // TODO: Move this code to the history module and support the other search types
            // including case-insensitive matches. At this time we expect the non-exact deletions to
            // be handled only by the history function's interactive delete feature.
            if opts.search_type.unwrap_or(history::SearchType::Exact) != history::SearchType::Exact
            {
                streams
                    .err
                    .append(wgettext!("builtin history delete only supports --exact\n"));
                return Err(STATUS_INVALID_ARGS);
            }
            if !opts.case_sensitive {
                streams.err.append(wgettext!(
                    "builtin history delete --exact requires --case-sensitive\n"
                ));
                return Err(STATUS_INVALID_ARGS);
            }

            for delete_string in args {
                history.remove(delete_string);
            }
        }
        HistCmd::Clear => {
            if check_for_unexpected_hist_args(&opts, cmd, args, streams) {
                return Err(STATUS_INVALID_ARGS);
            }
            history.clear();
            history.save();
        }
        HistCmd::ClearSession => {
            if check_for_unexpected_hist_args(&opts, cmd, args, streams) {
                return Err(STATUS_INVALID_ARGS);
            }
            history.clear_session();
            history.save();
        }
        HistCmd::Merge => {
            if check_for_unexpected_hist_args(&opts, cmd, args, streams) {
                return Err(STATUS_INVALID_ARGS);
            }

            if in_private_mode(parser.vars()) {
                streams.err.append(wgettext_fmt!(
                    "%s: can't merge history in private mode\n",
                    cmd
                ));
                return Err(STATUS_INVALID_ARGS);
            }
            history.incorporate_external_changes();
        }
        HistCmd::Save => {
            if check_for_unexpected_hist_args(&opts, cmd, args, streams) {
                return Err(STATUS_INVALID_ARGS);
            }
            history.save();
        }
        HistCmd::Append => {
            for &arg in args {
                history.add_commandline(arg.to_owned());
            }
        }
    }

    status
}
