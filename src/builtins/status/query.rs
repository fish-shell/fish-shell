use std::ops::ControlFlow;
use std::rc::Rc;

use crate::builtins::prelude::*;

use crate::common::wcs2string;
use crate::global_safety::RelaxedAtomicBool;
use crate::input_common::{
    terminal_protocols_disable_ifn, InputEventQueuer, TerminalQuery, XtgettcapQuery,
};
use crate::nix::isatty;
use crate::reader::{
    query_xtgettcap, querying_allowed, reader_pop, reader_push, reader_readline, ReaderConfig,
    UserQuery,
};
use crate::terminal::TerminalCommand::QueryPrimaryDeviceAttribute;
use crate::terminal::{Output, Outputter, XTVERSION};
use libc::STDOUT_FILENO;

use super::StatusCmd;

pub(crate) fn status_xtversion(
    parser: &Parser,
    streams: &mut IoStreams,
    cmd: &wstr,
    args: &[&wstr],
) -> BuiltinResult {
    use super::StatusCmd::STATUS_XTVERSION;
    if !args.is_empty() {
        streams.err.append(wgettext_fmt!(
            BUILTIN_ERR_ARG_COUNT2,
            cmd,
            STATUS_XTVERSION.to_wstr(),
            0,
            args.len()
        ));
        return Err(STATUS_INVALID_ARGS);
    }

    let run_query = { move |_query: &mut Option<TerminalQuery>| ControlFlow::Break(()) };
    synchronous_query(parser, streams, cmd, &STATUS_XTVERSION, Box::new(run_query))?;

    let Some(xtversion) = XTVERSION.get() else {
        return Err(STATUS_CMD_ERROR);
    };

    streams.out.appendln(xtversion);

    Ok(SUCCESS)
}

pub(crate) fn status_xtgettcap(
    parser: &Parser,
    streams: &mut IoStreams,
    cmd: &wstr,
    args: &[&wstr],
) -> BuiltinResult {
    use super::StatusCmd::STATUS_XTGETTCAP;
    if !querying_allowed() {
        return Err(STATUS_CMD_ERROR);
    }
    if args.len() != 1 {
        streams.err.append(wgettext_fmt!(
            BUILTIN_ERR_ARG_COUNT2,
            cmd,
            STATUS_XTGETTCAP.to_wstr(),
            1,
            args.len()
        ));
        return Err(STATUS_INVALID_ARGS);
    }
    let result = Rc::new(RelaxedAtomicBool::new(false));
    let run_query = {
        let terminfo_code = wcs2string(args[0]);
        let result = Rc::clone(&result);
        move |query: &mut Option<TerminalQuery>| {
            assert!(matches!(
                *query,
                None | Some(TerminalQuery::PrimaryDeviceAttribute(None))
            ));
            let mut output = Outputter::stdoutput().borrow_mut();
            output.begin_buffering();
            query_xtgettcap(output.by_ref(), &terminfo_code);
            output.write_command(QueryPrimaryDeviceAttribute);
            output.end_buffering();
            *query = Some(TerminalQuery::PrimaryDeviceAttribute(Some(
                XtgettcapQuery {
                    terminfo_code,
                    result,
                },
            )));
            ControlFlow::Continue(())
        }
    };
    synchronous_query(parser, streams, cmd, &STATUS_XTGETTCAP, Box::new(run_query))?;
    if !result.load() {
        return Err(STATUS_CMD_ERROR);
    }
    Ok(SUCCESS)
}

fn synchronous_query(
    parser: &Parser,
    streams: &mut IoStreams,
    cmd: &wstr,
    subcmd: &StatusCmd,
    run_query: UserQuery,
) -> Result<(), ErrorCode> {
    if !isatty(streams.stdin_fd) {
        streams.err.append(wgettext_fmt!(
            "%s %s: stdin is not a TTY",
            cmd,
            subcmd.to_wstr(),
        ));
        return Err(STATUS_INVALID_ARGS);
    };
    let out_fd = STDOUT_FILENO;
    if !isatty(out_fd) {
        streams.err.append(wgettext_fmt!(
            "%s %s: stdout is not a TTY",
            cmd,
            subcmd.to_wstr(),
        ));
        return Err(STATUS_INVALID_ARGS);
    };

    if let Some(query_state) = parser.blocking_query.get() {
        if (run_query)(&mut query_state.borrow_mut()).is_break() {
            return Ok(());
        }
    } else {
        // We are the first reader.
        let empty_spot = parser.pending_user_query.replace(Some(run_query));
        assert!(empty_spot.is_none());
    }

    let mut conf = ReaderConfig::default();
    conf.inputfd = streams.stdin_fd;
    conf.prompt_ok = false;
    conf.exit_on_interrupt = true;

    let pending_keys = {
        let mut reader = reader_push(parser, L!(""), conf);
        {
            let _interactive = parser.push_scope(|s| s.is_interactive = true);
            let no_line = reader_readline(parser, None);
            assert!(no_line.is_none());
        }
        terminal_protocols_disable_ifn();
        let input_data = reader.get_input_data_mut();
        let pending_keys = std::mem::take(&mut input_data.queue);
        // We blocked code and mapping execution so input function args must be empty.
        assert!(input_data.input_function_args.is_empty());
        if input_data.paste_buffer.is_some() {
            FLOG!(
                reader,
                "Bracketed paste was interrupted; dropping uncommitted paste buffer"
            )
        }
        reader_pop();
        pending_keys
    };
    FLOGF!(reader, "Adding %lu pending keys", pending_keys.len());
    parser.pending_keys.borrow_mut().extend(pending_keys);
    Ok(())
}
