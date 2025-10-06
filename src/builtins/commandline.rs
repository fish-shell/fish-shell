use super::prelude::*;
use super::read::TokenOutputMode;
use crate::ast::{self, Kind, Leaf};
use crate::common::{unescape_string, UnescapeFlags, UnescapeStringStyle};
use crate::complete::Completion;
use crate::expand::{expand_string, ExpandFlags, ExpandResultCode};
use crate::input::input_function_get_code;
use crate::input_common::{CharEvent, ReadlineCmd};
use crate::operation_context::{no_cancel, OperationContext};
use crate::parse_constants::{ParseTreeFlags, ParserTestErrorBits};
use crate::parse_util::{
    parse_util_detect_errors, parse_util_get_offset_from_line, parse_util_job_extent,
    parse_util_lineno, parse_util_process_extent, parse_util_token_extent,
};
use crate::proc::is_interactive_session;
use crate::reader::{
    commandline_get_state, commandline_set_buffer, commandline_set_search_field,
    reader_execute_readline_cmd, reader_showing_suggestion,
};
use crate::tokenizer::TOK_ACCEPT_UNFINISHED;
use crate::tokenizer::{TokenType, Tokenizer};
use crate::wchar::prelude::*;
use crate::wcstringutil::join_strings;
use std::ops::Range;

/// Which part of the comandbuffer are we operating on.
#[derive(Eq, PartialEq)]
enum TextScope {
    String,
    Job,
    Process,
    Token,
}

/// For text insertion, how should it be done.
#[derive(Eq, PartialEq)]
enum AppendMode {
    // replace current text
    Replace,
    // insert at cursor position
    Insert,
    // insert at cursor position, DWIM style.
    InsertSmart,
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
    parser: &Parser,
    range: Range<usize>,
    insert: &wstr,
    insert_mode: AppendMode,
    buff: &wstr,
    cursor_pos: usize,
    search_field_mode: bool,
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
        AppendMode::Insert | AppendMode::InsertSmart => {
            assert!(cursor_pos >= range.start);
            assert!(cursor_pos <= range.end);
            let insert = strip_dollar_prefixes(insert_mode, &buff[range.start..cursor_pos], insert)
                .map(Cow::Owned)
                .unwrap_or(Cow::Borrowed(insert));
            out.push_utfstr(&buff[range.start..cursor_pos]);
            out.push_utfstr(&insert);
            out.push_utfstr(&buff[cursor_pos..range.end]);
            out_pos += insert.len();
        }
    }

    out.push_utfstr(&buff[range.end..]);
    if search_field_mode {
        commandline_set_search_field(parser, out, Some(out_pos));
    } else {
        commandline_set_buffer(parser, Some(out), Some(out_pos));
    }
}

// Prefix must be at the beginning of a process.
fn strip_dollar_prefixes(insert_mode: AppendMode, prefix: &wstr, insert: &wstr) -> Option<WString> {
    if insert_mode != AppendMode::InsertSmart {
        return None;
    }
    insert.find(L!("$ "))?; // Early return.
    let source = prefix.to_owned() + insert;
    let ast = ast::parse(
        &source,
        ParseTreeFlags::ACCEPT_INCOMPLETE_TOKENS | ParseTreeFlags::LEAVE_UNTERMINATED,
        None,
    );
    let mut stripped = WString::new();
    let mut have = prefix.len();
    for node in ast.walk() {
        let Kind::DecoratedStatement(ds) = node.kind() else {
            continue;
        };
        let Some(range) = ds.command.range() else {
            continue;
        };
        let pos = range.start();
        if pos < prefix.len() {
            continue;
        }
        if (pos == 0 || source.as_char_slice()[pos - 1] == '\n')
            && source.as_char_slice()[pos] == '$'
            && source.char_at(pos + 1) == ' '
        {
            stripped.push_utfstr(&source[have..pos]);
            have = pos + "$ ".len();
        }
    }
    stripped.push_utfstr(&source[have..]);
    return Some(stripped);
}

/// Output the specified selection.
///
/// \param begin start of selection
/// \param end  end of selection
/// \param cut_at_cursor whether printing should stop at the surrent cursor position
/// and skipping non-string tokens
/// \param buffer the original command line buffer
/// \param cursor_pos the position of the cursor in the command line
fn write_part(
    parser: &Parser,
    range: Range<usize>,
    range_is_single_token: bool,
    cut_at_cursor: bool,
    token_mode: Option<TokenOutputMode>,
    buffer: &wstr,
    cursor_pos: usize,
    streams: &mut IoStreams,
) {
    let pos = cursor_pos - range.start;

    let Some(token_mode) = token_mode else {
        if cut_at_cursor {
            streams.out.append(&buffer[range.start..range.start + pos]);
        } else {
            streams.out.append(&buffer[range]);
        }
        streams.out.push('\n');
        return;
    };

    let mut args = vec![];
    let mut add_token = |token_text: &wstr| {
        match token_mode {
            TokenOutputMode::Expanded => {
                const COMMANDLINE_TOKENS_MAX_EXPANSION: usize = 512;

                match expand_string(
                    token_text.to_owned(),
                    &mut args,
                    ExpandFlags::SKIP_CMDSUBST,
                    &OperationContext::foreground(
                        parser,
                        Box::new(no_cancel),
                        COMMANDLINE_TOKENS_MAX_EXPANSION,
                    ),
                    None,
                )
                .result
                {
                    ExpandResultCode::error
                    | ExpandResultCode::overflow
                    | ExpandResultCode::wildcard_no_match => {
                        // Maybe hit expansion limit, forward the unexpanded string.
                        args.push(Completion::from_completion(token_text.to_owned()));
                    }
                    ExpandResultCode::cancel => {
                        return;
                    }
                    ExpandResultCode::ok => (),
                };
            }
            TokenOutputMode::Raw => {
                args.push(Completion::from_completion(token_text.to_owned()));
            }
            TokenOutputMode::Unescaped => {
                let unescaped = unescape_string(
                    token_text,
                    UnescapeStringStyle::Script(UnescapeFlags::INCOMPLETE),
                )
                .unwrap();
                args.push(Completion::from_completion(unescaped));
            }
        }
    };

    let buff = &buffer[range];
    if range_is_single_token {
        add_token(buff);
    } else {
        let mut tok = Tokenizer::new(buff, TOK_ACCEPT_UNFINISHED);
        let mut in_redirection = false;
        while let Some(token) = tok.next() {
            if cut_at_cursor && token.end() >= pos {
                break;
            }
            let is_redirection_target = in_redirection;
            in_redirection = token.type_ == TokenType::redirect;
            if is_redirection_target && token.type_ == TokenType::string {
                continue;
            }
            if token.type_ != TokenType::string {
                continue;
            }

            let token_text = tok.text_of(&token);
            add_token(token_text);
        }
    };

    for arg in args {
        streams.out.appendln(arg.completion);
    }
}

/// The commandline builtin. It is used for specifying a new value for the commandline.
pub fn commandline(parser: &Parser, streams: &mut IoStreams, args: &mut [&wstr]) -> BuiltinResult {
    let rstate = commandline_get_state(true);

    let mut buffer_part = None;
    let mut cut_at_cursor = false;
    let mut append_mode = None;

    let mut function_mode = false;
    let mut selection_mode = false;

    let mut token_mode = None;

    let mut cursor_mode = false;
    let mut selection_start_mode = false;
    let mut selection_end_mode = false;
    let mut line_mode = false;
    let mut column_mode = false;
    let mut search_mode = false;
    let mut paging_mode = false;
    let mut paging_full_mode = false;
    let mut search_field_mode = false;
    let mut is_valid = false;
    let mut showing_suggestion = false;

    let mut override_buffer = None;

    const short_options: &wstr = L!("abijpctfxorhI:CBELSsP");
    let long_options: &[WOption] = &[
        wopt(L!("append"), ArgType::NoArgument, 'a'),
        wopt(L!("insert"), ArgType::NoArgument, 'i'),
        wopt(L!("insert-smart"), ArgType::NoArgument, '\x06'),
        wopt(L!("replace"), ArgType::NoArgument, 'r'),
        wopt(L!("current-buffer"), ArgType::NoArgument, 'b'),
        wopt(L!("current-job"), ArgType::NoArgument, 'j'),
        wopt(L!("current-process"), ArgType::NoArgument, 'p'),
        wopt(L!("current-selection"), ArgType::NoArgument, 's'),
        wopt(L!("current-token"), ArgType::NoArgument, 't'),
        wopt(L!("cut-at-cursor"), ArgType::NoArgument, 'c'),
        wopt(L!("function"), ArgType::NoArgument, 'f'),
        wopt(L!("tokens-expanded"), ArgType::NoArgument, 'x'),
        wopt(L!("tokens-raw"), ArgType::NoArgument, '\x02'),
        wopt(L!("tokenize"), ArgType::NoArgument, 'o'),
        wopt(L!("help"), ArgType::NoArgument, 'h'),
        wopt(L!("input"), ArgType::RequiredArgument, 'I'),
        wopt(L!("cursor"), ArgType::NoArgument, 'C'),
        wopt(L!("selection-start"), ArgType::NoArgument, 'B'),
        wopt(L!("selection-end"), ArgType::NoArgument, 'E'),
        wopt(L!("line"), ArgType::NoArgument, 'L'),
        wopt(L!("column"), ArgType::NoArgument, '\x05'),
        wopt(L!("search-mode"), ArgType::NoArgument, 'S'),
        wopt(L!("paging-mode"), ArgType::NoArgument, 'P'),
        wopt(L!("paging-full-mode"), ArgType::NoArgument, 'F'),
        wopt(L!("search-field"), ArgType::NoArgument, '\x03'),
        wopt(L!("is-valid"), ArgType::NoArgument, '\x01'),
        wopt(L!("showing-suggestion"), ArgType::NoArgument, '\x04'),
    ];

    let mut w = WGetopter::new(short_options, long_options, args);
    let cmd = w.argv[0];
    while let Some(c) = w.next_opt() {
        match c {
            'a' => append_mode = Some(AppendMode::Append),
            'b' => buffer_part = Some(TextScope::String),
            'i' => append_mode = Some(AppendMode::Insert),
            '\x06' => append_mode = Some(AppendMode::InsertSmart),
            'r' => append_mode = Some(AppendMode::Replace),
            'c' => cut_at_cursor = true,
            't' => buffer_part = Some(TextScope::Token),
            'j' => buffer_part = Some(TextScope::Job),
            'p' => buffer_part = Some(TextScope::Process),
            'f' => function_mode = true,
            'x' | '\x02' | 'o' => {
                if token_mode.is_some() {
                    streams.err.append(wgettext_fmt!(
                        BUILTIN_ERR_COMBO2,
                        cmd,
                        wgettext!("--tokens options are mutually exclusive")
                    ));
                    builtin_print_error_trailer(parser, streams.err, cmd);
                    return Err(STATUS_INVALID_ARGS);
                }
                token_mode = Some(match c {
                    'x' => TokenOutputMode::Expanded,
                    '\x02' => TokenOutputMode::Raw,
                    'o' => TokenOutputMode::Unescaped,
                    _ => unreachable!(),
                })
            }
            'I' => {
                // A historical, undocumented feature. TODO: consider removing this.
                override_buffer = Some(w.woptarg.unwrap().to_owned());
            }
            'C' => cursor_mode = true,
            'B' => selection_start_mode = true,
            'E' => selection_end_mode = true,
            'L' => line_mode = true,
            '\x05' => column_mode = true,
            'S' => search_mode = true,
            's' => selection_mode = true,
            'P' => paging_mode = true,
            'F' => paging_full_mode = true,
            '\x03' => search_field_mode = true,
            '\x01' => is_valid = true,
            '\x04' => showing_suggestion = true,
            'h' => {
                builtin_print_help(parser, streams, cmd);
                return Ok(SUCCESS);
            }
            ':' => {
                builtin_missing_argument(parser, streams, cmd, w.argv[w.wopt_index - 1], true);
                return Err(STATUS_INVALID_ARGS);
            }
            ';' => {
                builtin_unexpected_argument(parser, streams, cmd, w.argv[w.wopt_index - 1], true);
                return Err(STATUS_INVALID_ARGS);
            }
            '?' => {
                builtin_unknown_option(parser, streams, cmd, w.argv[w.wopt_index - 1], true);
                return Err(STATUS_INVALID_ARGS);
            }
            _ => panic!(),
        }
    }

    let positional_args = w.argv.len() - w.wopt_index;

    if function_mode {
        // Check for invalid switch combinations.
        if buffer_part.is_some()
            || cut_at_cursor
            || append_mode.is_some()
            || token_mode.is_some()
            || cursor_mode
            || line_mode
            || column_mode
            || search_mode
            || paging_mode
            || selection_start_mode
            || selection_end_mode
        {
            streams.err.append(wgettext_fmt!(BUILTIN_ERR_COMBO, cmd));
            builtin_print_error_trailer(parser, streams.err, cmd);
            return Err(STATUS_INVALID_ARGS);
        }

        if positional_args == 0 {
            builtin_missing_argument(parser, streams, cmd, L!("--function"), true);
            return Err(STATUS_INVALID_ARGS);
        }

        type RL = ReadlineCmd;
        for arg in &w.argv[w.wopt_index..] {
            let Some(cmd) = input_function_get_code(arg) else {
                streams
                    .err
                    .append(wgettext_fmt!("%s: Unknown input function '%s'", cmd, arg));
                builtin_print_error_trailer(parser, streams.err, cmd);
                return Err(STATUS_INVALID_ARGS);
            };
            // Don't enqueue a repaint if we're currently in the middle of one,
            // because that's an infinite loop.
            if matches!(cmd, RL::RepaintMode | RL::ForceRepaint | RL::Repaint) {
                if parser.libdata().is_repaint {
                    continue;
                }
            }

            // Inserts the readline function at the back of the queue.
            reader_execute_readline_cmd(parser, CharEvent::from_readline(cmd));
        }

        return Ok(SUCCESS);
    }

    if selection_mode {
        if let Some(selection) = rstate.selection {
            streams.out.append(&rstate.text[selection]);
        }
        return Ok(SUCCESS);
    }

    // Check for invalid switch combinations.
    if (selection_start_mode || selection_end_mode) && positional_args != 0 {
        streams
            .err
            .append(wgettext_fmt!(BUILTIN_ERR_TOO_MANY_ARGUMENTS, cmd));
        builtin_print_error_trailer(parser, streams.err, cmd);
        return Err(STATUS_INVALID_ARGS);
    }

    if (search_mode || line_mode || column_mode || cursor_mode || paging_mode)
        && positional_args > 1
    {
        streams
            .err
            .append(wgettext_fmt!(BUILTIN_ERR_TOO_MANY_ARGUMENTS, cmd));
        builtin_print_error_trailer(parser, streams.err, cmd);
        return Err(STATUS_INVALID_ARGS);
    }

    if (buffer_part.is_some() || token_mode.is_some() || cut_at_cursor || search_field_mode)
        && (cursor_mode || line_mode||column_mode || search_mode || paging_mode || paging_full_mode)
        // Special case - we allow to get/set cursor position relative to the process/job/token.
        && ((buffer_part.is_none() && !search_field_mode) || !cursor_mode)
    {
        streams.err.append(wgettext_fmt!(BUILTIN_ERR_COMBO, cmd));
        builtin_print_error_trailer(parser, streams.err, cmd);
        return Err(STATUS_INVALID_ARGS);
    }

    if (token_mode.is_some() || cut_at_cursor) && positional_args != 0 {
        streams.err.append(wgettext_fmt!(
            BUILTIN_ERR_COMBO2,
            cmd,
            "--cut-at-cursor and token options can not be used when setting the commandline"
        ));
        builtin_print_error_trailer(parser, streams.err, cmd);
        return Err(STATUS_INVALID_ARGS);
    }

    if search_field_mode && (buffer_part.is_some() || token_mode.is_some()) {
        streams.err.append(wgettext_fmt!(BUILTIN_ERR_COMBO, cmd));
        builtin_print_error_trailer(parser, streams.err, cmd);
        return Err(STATUS_INVALID_ARGS);
    }

    if append_mode.is_some() && positional_args == 0 {
        // No tokens in insert mode just means we do nothing.
        return Err(STATUS_CMD_ERROR);
    }

    // Set default modes.
    let append_mode = append_mode.unwrap_or(AppendMode::Replace);

    let buffer_part = buffer_part.unwrap_or(TextScope::String);

    if append_mode == AppendMode::InsertSmart {
        if search_field_mode {
            streams.err.append(wgettext_fmt!(
                BUILTIN_ERR_COMBO2_EXCLUSIVE,
                cmd,
                "--insert-smart",
                "--search-field"
            ));
            builtin_print_error_trailer(parser, streams.err, cmd);
            return Err(STATUS_INVALID_ARGS);
        }
        match buffer_part {
            TextScope::String | TextScope::Job | TextScope::Process => (),
            TextScope::Token => {
                // To-do: we can support it in command position.
                streams.err.append(wgettext_fmt!(
                    BUILTIN_ERR_COMBO2_EXCLUSIVE,
                    cmd,
                    "--insert-smart",
                    "--current-token"
                ));
                builtin_print_error_trailer(parser, streams.err, cmd);
                return Err(STATUS_INVALID_ARGS);
            }
        }
    }

    if line_mode || column_mode {
        if positional_args != 0 {
            let arg = w.argv[w.wopt_index];
            let new_coord = match fish_wcstol(arg) {
                Err(_) => {
                    streams
                        .err
                        .append(wgettext_fmt!(BUILTIN_ERR_NOT_NUMBER, cmd, arg));
                    builtin_print_error_trailer(parser, streams.err, cmd);
                    0
                }
                Ok(num) => num - 1,
            };
            let Ok(new_coord) = usize::try_from(new_coord) else {
                streams
                    .err
                    .append(wgettext_fmt!("%s: line/column index starts at 1", cmd));
                builtin_print_error_trailer(parser, streams.err, cmd);
                return Err(STATUS_INVALID_ARGS);
            };

            let new_pos = if line_mode {
                let Some(offset) = parse_util_get_offset_from_line(
                    &rstate.text,
                    i32::try_from(new_coord).unwrap(),
                ) else {
                    streams
                        .err
                        .append(wgettext_fmt!("%s: there is no line %s\n", cmd, arg));
                    builtin_print_error_trailer(parser, streams.err, cmd);
                    return Err(STATUS_INVALID_ARGS);
                };
                offset
            } else {
                let line_index =
                    i32::try_from(parse_util_lineno(&rstate.text, rstate.cursor_pos)).unwrap() - 1;
                let line_offset =
                    parse_util_get_offset_from_line(&rstate.text, line_index).unwrap_or_default();
                let next_line_offset =
                    parse_util_get_offset_from_line(&rstate.text, line_index + 1)
                        .unwrap_or(rstate.text.len());
                if line_offset + new_coord > next_line_offset {
                    streams.err.append(wgettext_fmt!(
                        "%s: column %s exceeds line length\n",
                        cmd,
                        arg
                    ));
                    builtin_print_error_trailer(parser, streams.err, cmd);
                    return Err(STATUS_INVALID_ARGS);
                }
                line_offset + new_coord
            };
            commandline_set_buffer(parser, None, Some(new_pos));
        } else {
            streams.out.append(sprintf!(
                "%d\n",
                if line_mode {
                    parse_util_lineno(&rstate.text, rstate.cursor_pos)
                } else {
                    rstate.cursor_pos + 1
                        - parse_util_get_offset_from_line(
                            &rstate.text,
                            i32::try_from(parse_util_lineno(&rstate.text, rstate.cursor_pos) - 1)
                                .unwrap(),
                        )
                        .unwrap_or_default()
                }
            ));
        }
        return Ok(SUCCESS);
    }

    if search_mode {
        return if rstate.search_mode {
            Ok(SUCCESS)
        } else {
            Err(STATUS_CMD_ERROR)
        };
    }

    if paging_mode {
        return if rstate.pager_mode {
            Ok(SUCCESS)
        } else {
            Err(STATUS_CMD_ERROR)
        };
    }

    if paging_full_mode {
        return if rstate.pager_mode && rstate.pager_fully_disclosed {
            Ok(SUCCESS)
        } else {
            Err(STATUS_CMD_ERROR)
        };
    }

    if selection_start_mode {
        let Some(selection) = rstate.selection else {
            return Err(STATUS_CMD_ERROR);
        };
        streams.out.append(sprintf!("%u\n", selection.start));
        return Ok(SUCCESS);
    }

    if selection_end_mode {
        let Some(selection) = rstate.selection else {
            return Err(STATUS_CMD_ERROR);
        };
        streams.out.append(sprintf!("%u\n", selection.end));
        return Ok(SUCCESS);
    }

    // At this point we have (nearly) exhausted the options which always operate on the true command
    // line. Now we respect the possibility of a transient command line due to evaluating a wrapped
    // completion. Don't do this in cursor_mode: it makes no sense to move the cursor based on a
    // transient commandline.
    let current_buffer;
    let current_cursor_pos;
    let transient;

    if search_field_mode {
        let Some((search_field_text, cursor_pos)) = rstate.search_field else {
            return Err(STATUS_CMD_ERROR);
        };
        transient = search_field_text;
        current_buffer = &transient;
        current_cursor_pos = cursor_pos;
    } else if let Some(override_buffer) = &override_buffer {
        current_buffer = override_buffer;
        current_cursor_pos = current_buffer.len();
    } else if parser.libdata().transient_commandline.is_some() && !cursor_mode {
        transient = parser.libdata().transient_commandline.clone().unwrap();
        current_buffer = &transient;
        current_cursor_pos = transient.len();
    } else if parser.interactive_initialized.load() || is_interactive_session() {
        current_buffer = &rstate.text;
        current_cursor_pos = rstate.cursor_pos;
    } else {
        // There is no command line because we are not interactive.
        streams.err.append(cmd);
        streams
            .err
            .append(L!(": Can not set commandline in non-interactive mode\n"));
        builtin_print_error_trailer(parser, streams.err, cmd);
        return Err(STATUS_CMD_ERROR);
    }

    if is_valid {
        if current_buffer.is_empty() {
            return Err(STATUS_CMD_ERROR);
        }
        let res = parse_util_detect_errors(current_buffer, None, /*accept_incomplete=*/ true);
        return match res {
            Ok(()) => Ok(SUCCESS),
            Err(err) => {
                if err.contains(ParserTestErrorBits::INCOMPLETE) {
                    Err(STATUS_INVALID_ARGS)
                } else {
                    Err(STATUS_CMD_ERROR)
                }
            }
        };
    }

    if showing_suggestion {
        if reader_showing_suggestion(parser) {
            return Ok(SUCCESS);
        }
        return Err(STATUS_CMD_ERROR);
    }

    let range;
    if search_field_mode {
        range = 0..current_buffer.len();
    } else {
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
                (range, _) = parse_util_token_extent(current_buffer, current_cursor_pos);
            }
        }
    }

    if cursor_mode {
        if positional_args != 0 {
            let arg = w.argv[w.wopt_index];
            let new_pos = match fish_wcstol(arg) {
                Err(_) => {
                    streams
                        .err
                        .append(wgettext_fmt!(BUILTIN_ERR_NOT_NUMBER, cmd, arg));
                    builtin_print_error_trailer(parser, streams.err, cmd);
                    0
                }
                Ok(num) => num,
            };

            let new_pos = std::cmp::min(
                range
                    .start
                    .saturating_add_signed(isize::try_from(new_pos).unwrap()),
                current_buffer.len(),
            );
            commandline_set_buffer(parser, None, Some(new_pos));
        } else {
            streams
                .out
                .append(sprintf!("%u\n", current_cursor_pos - range.start));
        }
        return Ok(SUCCESS);
    }

    if positional_args == 0 {
        write_part(
            parser,
            range,
            buffer_part == TextScope::Token,
            cut_at_cursor,
            token_mode,
            current_buffer,
            current_cursor_pos,
            streams,
        );
    } else {
        let replacement = if positional_args == 1 {
            Cow::Borrowed(args[w.wopt_index])
        } else {
            Cow::Owned(join_strings(&w.argv[w.wopt_index..], '\n'))
        };
        replace_part(
            parser,
            range,
            &replacement,
            append_mode,
            current_buffer,
            current_cursor_pos,
            search_field_mode,
        );
    }

    Ok(SUCCESS)
}
