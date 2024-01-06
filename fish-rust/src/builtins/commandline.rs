use super::prelude::*;
use crate::common::{unescape_string, UnescapeFlags, UnescapeStringStyle};
use crate::input::input_function_get_code;
use crate::input_common::{CharEvent, ReadlineCmd};
use crate::parse_constants::ParserTestErrorBits;
use crate::parse_util::{
    parse_util_detect_errors, parse_util_job_extent, parse_util_lineno, parse_util_process_extent,
    parse_util_token_extent,
};
use crate::proc::is_interactive_session;
use crate::reader::{
    commandline_get_state, commandline_set_buffer, reader_handle_command, reader_queue_ch,
};
use crate::tokenizer::TokenType;
use crate::tokenizer::Tokenizer;
use crate::tokenizer::TOK_ACCEPT_UNFINISHED;
use crate::wchar::prelude::*;
use crate::wcstringutil::join_strings;
use crate::wgetopt::{wgetopter_t, wopt, woption, woption_argument_t};
use std::ops::Range;

/// Which part of the comandbuffer are we operating on.
enum TextScope {
    String,
    Job,
    Process,
    Token,
}

/// For text insertion, how should it be done.
enum AppendMode {
    // replace current text
    Replace,
    // insert at cursor position
    Insert,
    // insert at end of current token/command/buffer
    Append,
}

/// Replace/append/insert the selection with/at/after the specified string.
///
/// \param begin beginning of selection
/// \param end end of selection
/// \param insert the string to insert
/// \param append_mode can be one of REPLACE_MODE, INSERT_MODE or APPEND_MODE, affects the way the
/// test update is performed
/// \param buff the original command line buffer
/// \param cursor_pos the position of the cursor in the command line
fn replace_part(
    range: Range<usize>,
    insert: &wstr,
    insert_mode: AppendMode,
    buff: &wstr,
    cursor_pos: usize,
) {
    let mut out_pos = cursor_pos;

    let mut out = buff[..range.start].to_owned();

    match insert_mode {
        AppendMode::Replace => {
            out.push_utfstr(insert);
            out_pos = out.len();
        }
        AppendMode::Append => {
            out.push_utfstr(&buff[range.clone()]);
            out.push_utfstr(insert);
        }
        AppendMode::Insert => {
            let cursor = cursor_pos - range.start;
            assert!(range.start <= cursor);
            out.push_utfstr(&buff[range.start..cursor]);
            out.push_utfstr(&insert);
            out.push_utfstr(&buff[cursor..range.end]);
            out_pos += insert.len();
        }
    }

    out.push_utfstr(&buff[range.end..]);
    commandline_set_buffer(out, Some(out_pos));
}

/// Output the specified selection.
///
/// \param begin start of selection
/// \param end  end of selection
/// \param cut_at_cursor whether printing should stop at the surrent cursor position
/// \param tokenize whether the string should be tokenized, printing one string token on every line
/// and skipping non-string tokens
/// \param buffer the original command line buffer
/// \param cursor_pos the position of the cursor in the command line
fn write_part(
    range: Range<usize>,
    cut_at_cursor: bool,
    tokenize: bool,
    buffer: &wstr,
    cursor_pos: usize,
    streams: &mut IoStreams,
) {
    let pos = cursor_pos - range.start;

    if tokenize {
        let mut out = WString::new();
        let buff = &buffer[range];
        let mut tok = Tokenizer::new(buff, TOK_ACCEPT_UNFINISHED);
        while let Some(token) = tok.next() {
            if cut_at_cursor && token.end() >= pos {
                break;
            }

            if token.type_ == TokenType::string {
                let tmp = tok.text_of(&token);
                let unescaped =
                    unescape_string(tmp, UnescapeStringStyle::Script(UnescapeFlags::INCOMPLETE))
                        .unwrap();
                out.push_utfstr(&unescaped);
                out.push('\n');
            }
        }

        streams.out.append(out);
    } else {
        if cut_at_cursor {
            streams.out.append(&buffer[range.start..range.start + pos]);
        } else {
            streams.out.append(&buffer[range]);
        }
        streams.out.push('\n');
    }
}

/// The commandline builtin. It is used for specifying a new value for the commandline.
pub fn commandline(parser: &Parser, streams: &mut IoStreams, args: &mut [&wstr]) -> Option<c_int> {
    let rstate = commandline_get_state();

    let mut buffer_part = None;
    let mut cut_at_cursor = false;
    let mut append_mode = None;

    let mut function_mode = false;
    let mut selection_mode = false;

    let mut tokenize = false;

    let mut cursor_mode = false;
    let mut selection_start_mode = false;
    let mut selection_end_mode = false;
    let mut line_mode = false;
    let mut search_mode = false;
    let mut paging_mode = false;
    let mut paging_full_mode = false;
    let mut is_valid = false;

    let mut range = 0..0;
    let mut override_buffer = None;

    let ld = parser.libdata();

    const short_options: &wstr = L!(":abijpctforhI:CBELSsP");
    let long_options: &[woption] = &[
        wopt(L!("append"), woption_argument_t::no_argument, 'a'),
        wopt(L!("insert"), woption_argument_t::no_argument, 'i'),
        wopt(L!("replace"), woption_argument_t::no_argument, 'r'),
        wopt(L!("current-buffer"), woption_argument_t::no_argument, 'b'),
        wopt(L!("current-job"), woption_argument_t::no_argument, 'j'),
        wopt(L!("current-process"), woption_argument_t::no_argument, 'p'),
        wopt(
            L!("current-selection"),
            woption_argument_t::no_argument,
            's',
        ),
        wopt(L!("current-token"), woption_argument_t::no_argument, 't'),
        wopt(L!("cut-at-cursor"), woption_argument_t::no_argument, 'c'),
        wopt(L!("function"), woption_argument_t::no_argument, 'f'),
        wopt(L!("tokenize"), woption_argument_t::no_argument, 'o'),
        wopt(L!("help"), woption_argument_t::no_argument, 'h'),
        wopt(L!("input"), woption_argument_t::required_argument, 'I'),
        wopt(L!("cursor"), woption_argument_t::no_argument, 'C'),
        wopt(L!("selection-start"), woption_argument_t::no_argument, 'B'),
        wopt(L!("selection-end"), woption_argument_t::no_argument, 'E'),
        wopt(L!("line"), woption_argument_t::no_argument, 'L'),
        wopt(L!("search-mode"), woption_argument_t::no_argument, 'S'),
        wopt(L!("paging-mode"), woption_argument_t::no_argument, 'P'),
        wopt(L!("paging-full-mode"), woption_argument_t::no_argument, 'F'),
        wopt(L!("is-valid"), woption_argument_t::no_argument, '\x01'),
    ];

    let mut w = wgetopter_t::new(short_options, long_options, args);
    let cmd = w.argv[0];
    while let Some(c) = w.wgetopt_long() {
        match c {
            'a' => append_mode = Some(AppendMode::Append),
            'b' => buffer_part = Some(TextScope::String),
            'i' => append_mode = Some(AppendMode::Insert),
            'r' => append_mode = Some(AppendMode::Replace),
            'c' => cut_at_cursor = true,
            't' => buffer_part = Some(TextScope::Token),
            'j' => buffer_part = Some(TextScope::Job),
            'p' => buffer_part = Some(TextScope::Process),
            'f' => function_mode = true,
            'o' => tokenize = true,
            'I' => {
                // A historical, undocumented feature. TODO: consider removing this.
                override_buffer = Some(w.woptarg.unwrap().to_owned());
            }
            'C' => cursor_mode = true,
            'B' => selection_start_mode = true,
            'E' => selection_end_mode = true,
            'L' => line_mode = true,
            'S' => search_mode = true,
            's' => selection_mode = true,
            'P' => paging_mode = true,
            'F' => paging_full_mode = true,
            '\x01' => is_valid = true,
            'h' => {
                builtin_print_help(parser, streams, cmd);
                return STATUS_CMD_OK;
            }
            ':' => {
                builtin_missing_argument(parser, streams, cmd, w.argv[w.woptind - 1], true);
                return STATUS_INVALID_ARGS;
            }
            '?' => {
                builtin_unknown_option(parser, streams, cmd, w.argv[w.woptind - 1], true);
                return STATUS_INVALID_ARGS;
            }
            _ => panic!(),
        }
    }

    let positional_args = w.argv.len() - w.woptind;

    if function_mode {
        // Check for invalid switch combinations.
        if buffer_part.is_some()
            || cut_at_cursor
            || append_mode.is_some()
            || tokenize
            || cursor_mode
            || line_mode
            || search_mode
            || paging_mode
            || selection_start_mode
            || selection_end_mode
        {
            streams.err.append(wgettext_fmt!(BUILTIN_ERR_COMBO, cmd));
            builtin_print_error_trailer(parser, streams.err, cmd);
            return STATUS_INVALID_ARGS;
        }

        if positional_args == 0 {
            builtin_missing_argument(parser, streams, cmd, L!("--function"), true);
            return STATUS_INVALID_ARGS;
        }

        type rl = ReadlineCmd;
        for arg in &w.argv[w.woptind..] {
            let Some(cmd) = input_function_get_code(arg) else {
                streams
                    .err
                    .append(wgettext_fmt!("%ls: Unknown input function '%ls'", cmd, arg));
                builtin_print_error_trailer(parser, streams.err, cmd);
                return STATUS_INVALID_ARGS;
            };
            // Don't enqueue a repaint if we're currently in the middle of one,
            // because that's an infinite loop.
            if matches!(cmd, rl::RepaintMode | rl::ForceRepaint | rl::Repaint) {
                if ld.pods.is_repaint {
                    continue;
                }
            }

            // HACK: Execute these right here and now so they can affect any insertions/changes
            // made via bindings. The correct solution is to change all `commandline`
            // insert/replace operations into readline functions with associated data, so that
            // all queued `commandline` operations - including buffer modifications - are
            // executed in order
            match cmd {
                rl::BeginUndoGroup | rl::EndUndoGroup => reader_handle_command(cmd),
                _ => {
                    // Inserts the readline function at the back of the queue.
                    reader_queue_ch(CharEvent::from_readline(cmd));
                }
            }
        }

        return STATUS_CMD_OK;
    }

    if selection_mode {
        if let Some(selection) = rstate.selection {
            streams.out.append(&rstate.text[selection]);
        }
        return STATUS_CMD_OK;
    }

    // Check for invalid switch combinations.
    if (selection_start_mode || selection_end_mode) && positional_args != 0 {
        streams
            .err
            .append(wgettext_fmt!(BUILTIN_ERR_TOO_MANY_ARGUMENTS, cmd));
        builtin_print_error_trailer(parser, streams.err, cmd);
        return STATUS_INVALID_ARGS;
    }

    if (search_mode || line_mode || cursor_mode || paging_mode) && positional_args > 1 {
        streams
            .err
            .append(wgettext_fmt!(BUILTIN_ERR_TOO_MANY_ARGUMENTS, cmd));
        builtin_print_error_trailer(parser, streams.err, cmd);
        return STATUS_INVALID_ARGS;
    }

    if (buffer_part.is_some() || tokenize || cut_at_cursor)
        && (cursor_mode || line_mode || search_mode || paging_mode || paging_full_mode)
        // Special case - we allow to get/set cursor position relative to the process/job/token.
        && (buffer_part.is_none() || !cursor_mode)
    {
        streams.err.append(wgettext_fmt!(BUILTIN_ERR_COMBO, cmd));
        builtin_print_error_trailer(parser, streams.err, cmd);
        return STATUS_INVALID_ARGS;
    }

    if (tokenize || cut_at_cursor) && positional_args != 0 {
        streams.err.append(wgettext_fmt!(
            BUILTIN_ERR_COMBO2,
            cmd,
            "--cut-at-cursor and --tokenize can not be used when setting the commandline"
        ));
        builtin_print_error_trailer(parser, streams.err, cmd);
        return STATUS_INVALID_ARGS;
    }

    if append_mode.is_some() && positional_args == 0 {
        // No tokens in insert mode just means we do nothing.
        return STATUS_CMD_ERROR;
    }

    // Set default modes.
    let append_mode = append_mode.unwrap_or(AppendMode::Replace);

    let buffer_part = buffer_part.unwrap_or(TextScope::String);

    if line_mode {
        streams.out.append(sprintf!(
            "%d\n",
            parse_util_lineno(&rstate.text, rstate.cursor_pos)
        ));
        return STATUS_CMD_OK;
    }

    if search_mode {
        return if commandline_get_state().search_mode {
            STATUS_CMD_OK
        } else {
            STATUS_CMD_ERROR
        };
    }

    if paging_mode {
        return if commandline_get_state().pager_mode {
            STATUS_CMD_OK
        } else {
            STATUS_CMD_ERROR
        };
    }

    if paging_full_mode {
        let state = commandline_get_state();
        return if state.pager_mode && state.pager_fully_disclosed {
            STATUS_CMD_OK
        } else {
            STATUS_CMD_ERROR
        };
    }

    if selection_start_mode {
        let Some(selection) = rstate.selection else {
            return STATUS_CMD_ERROR;
        };
        streams.out.append(sprintf!("%lu\n", selection.start));
        return STATUS_CMD_OK;
    }

    if selection_end_mode {
        let Some(selection) = rstate.selection else {
            return STATUS_CMD_ERROR;
        };
        streams.out.append(sprintf!("%lu\n", selection.end));
        return STATUS_CMD_OK;
    }

    // At this point we have (nearly) exhausted the options which always operate on the true command
    // line. Now we respect the possibility of a transient command line due to evaluating a wrapped
    // completion. Don't do this in cursor_mode: it makes no sense to move the cursor based on a
    // transient commandline.
    let current_buffer;
    let current_cursor_pos;
    let transient;
    if let Some(override_buffer) = &override_buffer {
        current_buffer = override_buffer;
        current_cursor_pos = current_buffer.len();
    } else if !ld.transient_commandlines.is_empty() && !cursor_mode {
        transient = ld.transient_commandlines.last().unwrap().clone();
        current_buffer = &transient;
        current_cursor_pos = transient.len();
    } else if rstate.initialized {
        current_buffer = &rstate.text;
        current_cursor_pos = rstate.cursor_pos;
    } else {
        // There is no command line, either because we are not interactive, or because we are
        // interactive and are still reading init files (in which case we silently ignore this).
        if !is_interactive_session() {
            streams.err.append(cmd);
            streams
                .err
                .append(L!(": Can not set commandline in non-interactive mode\n"));
            builtin_print_error_trailer(parser, streams.err, cmd);
        }
        return STATUS_CMD_ERROR;
    }

    if is_valid {
        if current_buffer.is_empty() {
            return Some(1);
        }
        let res = parse_util_detect_errors(current_buffer, None, /*accept_incomplete=*/ true);
        return match res {
            Ok(()) => STATUS_CMD_OK,
            Err(err) => {
                if err.contains(ParserTestErrorBits::INCOMPLETE) {
                    Some(2)
                } else {
                    STATUS_CMD_ERROR
                }
            }
        };
    }

    match buffer_part {
        TextScope::String => {
            range = 0..current_buffer.len();
        }
        TextScope::Job => {
            range = parse_util_job_extent(current_buffer, current_cursor_pos, None);
        }
        TextScope::Process => {
            range = parse_util_process_extent(current_buffer, current_cursor_pos, None);
        }
        TextScope::Token => {
            parse_util_token_extent(current_buffer, current_cursor_pos, &mut range, None);
        }
    }

    if cursor_mode {
        if positional_args != 0 {
            let arg = w.argv[w.woptind];
            let new_pos = match fish_wcstol(&arg[range.start..]) {
                Err(_) => {
                    streams
                        .err
                        .append(wgettext_fmt!(BUILTIN_ERR_NOT_NUMBER, cmd, arg));
                    builtin_print_error_trailer(parser, streams.err, cmd);
                    0
                }
                Ok(num) => num,
            };

            let new_pos = std::cmp::min(new_pos.max(0) as usize, current_buffer.len());
            commandline_set_buffer(current_buffer.to_owned(), Some(new_pos));
        } else {
            streams.out.append(sprintf!("%lu\n", current_cursor_pos));
        }
        return STATUS_CMD_OK;
    }

    if positional_args == 0 {
        write_part(
            range,
            cut_at_cursor,
            tokenize,
            current_buffer,
            current_cursor_pos,
            streams,
        );
    } else if positional_args == 1 {
        replace_part(
            range,
            args[w.woptind],
            append_mode,
            current_buffer,
            current_cursor_pos,
        );
    } else {
        let sb = join_strings(&w.argv[w.woptind..], '\n');
        replace_part(range, &sb, append_mode, current_buffer, current_cursor_pos);
    }

    STATUS_CMD_OK
}
