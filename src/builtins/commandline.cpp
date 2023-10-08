// Functions used for implementing the commandline builtin.
#include "config.h"  // IWYU pragma: keep

#include "commandline.h"

#include <algorithm>
#include <cerrno>
#include <cwchar>
#include <string>

#include "../builtin.h"
#include "../common.h"
#include "../fallback.h"  // IWYU pragma: keep
#include "../input.h"
#include "../input_common.h"
#include "../io.h"
#include "../maybe.h"
#include "../parse_constants.h"
#include "../parse_util.h"
#include "../parser.h"
#include "../proc.h"
#include "../reader.h"
#include "../tokenizer.h"
#include "../wgetopt.h"
#include "../wutil.h"  // IWYU pragma: keep
#include "builtins/shared.rs.h"

/// Which part of the comandbuffer are we operating on.
enum {
    STRING_MODE = 1,  // operate on entire buffer
    JOB_MODE,         // operate on job under cursor
    PROCESS_MODE,     // operate on process under cursor
    TOKEN_MODE        // operate on token under cursor
};

/// For text insertion, how should it be done.
enum {
    REPLACE_MODE = 1,  // replace current text
    INSERT_MODE,       // insert at cursor position
    APPEND_MODE        // insert at end of current token/command/buffer
};

/// Handle a single readline_cmd_t command out-of-band.
void reader_handle_command(readline_cmd_t cmd);

/// Replace/append/insert the selection with/at/after the specified string.
///
/// \param begin beginning of selection
/// \param end end of selection
/// \param insert the string to insert
/// \param append_mode can be one of REPLACE_MODE, INSERT_MODE or APPEND_MODE, affects the way the
/// test update is performed
/// \param buff the original command line buffer
/// \param cursor_pos the position of the cursor in the command line
static void replace_part(const wchar_t *begin, const wchar_t *end, const wchar_t *insert,
                         int append_mode, const wchar_t *buff, size_t cursor_pos) {
    size_t out_pos = cursor_pos;

    wcstring out;

    out.append(buff, begin - buff);

    switch (append_mode) {
        case REPLACE_MODE: {
            out.append(insert);
            out_pos = out.size();
            break;
        }
        case APPEND_MODE: {
            out.append(begin, end - begin);
            out.append(insert);
            break;
        }
        case INSERT_MODE: {
            long cursor = cursor_pos - (begin - buff);
            out.append(begin, cursor);
            out.append(insert);
            out.append(begin + cursor, end - begin - cursor);
            out_pos += std::wcslen(insert);
            break;
        }
        default: {
            DIE("unexpected append_mode");
        }
    }
    out.append(end);
    commandline_set_buffer(out, out_pos);
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
static void write_part(const wchar_t *begin, const wchar_t *end, int cut_at_cursor, int tokenize,
                       const wchar_t *buffer, size_t cursor_pos, io_streams_t &streams) {
    size_t pos = cursor_pos - (begin - buffer);

    if (tokenize) {
        // std::fwprintf( stderr, L"Subshell: %ls, end char %lc\n", buff, *end );
        wcstring out;
        wcstring buff(begin, end - begin);
        auto tok = new_tokenizer(buff.c_str(), TOK_ACCEPT_UNFINISHED);
        while (auto token = tok->next()) {
            if ((cut_at_cursor) && (token->offset + token->length >= pos)) break;

            if (token->type_ == token_type_t::string) {
                wcstring tmp = *tok->text_of(*token);
                auto maybe_unescaped = unescape_string(tmp.c_str(), tmp.size(), UNESCAPE_INCOMPLETE,
                                                       STRING_STYLE_SCRIPT);
                assert(maybe_unescaped);
                out.append(*maybe_unescaped);
                out.push_back(L'\n');
            }
        }

        streams.out()->append(out);
    } else {
        if (cut_at_cursor) {
            streams.out()->append(wcstring{begin, pos});
        } else {
            streams.out()->append(wcstring{begin, end});
        }
        streams.out()->push(L'\n');
    }
}

/// The commandline builtin. It is used for specifying a new value for the commandline.
int builtin_commandline(const void *_parser, void *_streams, void *_argv) {
    const auto &parser = *static_cast<const parser_t *>(_parser);
    auto &streams = *static_cast<io_streams_t *>(_streams);
    auto argv = static_cast<const wchar_t **>(_argv);
    const commandline_state_t rstate = commandline_get_state();
    const wchar_t *cmd = argv[0];
    int buffer_part = 0;
    bool cut_at_cursor = false;

    int argc = builtin_count_args(argv);
    int append_mode = 0;

    bool function_mode = false;
    bool selection_mode = false;

    bool tokenize = false;

    bool cursor_mode = false;
    bool selection_start_mode = false;
    bool selection_end_mode = false;
    bool line_mode = false;
    bool search_mode = false;
    bool paging_mode = false;
    bool paging_full_mode = false;
    bool is_valid = false;
    const wchar_t *begin = nullptr, *end = nullptr;
    const wchar_t *override_buffer = nullptr;

    const auto &ld = parser.libdata();

    static const wchar_t *const short_options = L":abijpctforhI:CBELSsP";
    static const struct woption long_options[] = {{L"append", no_argument, 'a'},
                                                  {L"insert", no_argument, 'i'},
                                                  {L"replace", no_argument, 'r'},
                                                  {L"current-buffer", no_argument, 'b'},
                                                  {L"current-job", no_argument, 'j'},
                                                  {L"current-process", no_argument, 'p'},
                                                  {L"current-selection", no_argument, 's'},
                                                  {L"current-token", no_argument, 't'},
                                                  {L"cut-at-cursor", no_argument, 'c'},
                                                  {L"function", no_argument, 'f'},
                                                  {L"tokenize", no_argument, 'o'},
                                                  {L"help", no_argument, 'h'},
                                                  {L"input", required_argument, 'I'},
                                                  {L"cursor", no_argument, 'C'},
                                                  {L"selection-start", no_argument, 'B'},
                                                  {L"selection-end", no_argument, 'E'},
                                                  {L"line", no_argument, 'L'},
                                                  {L"search-mode", no_argument, 'S'},
                                                  {L"paging-mode", no_argument, 'P'},
                                                  {L"paging-full-mode", no_argument, 'F'},
                                                  {L"is-valid", no_argument, 1},
                                                  {}};

    int opt;
    wgetopter_t w;
    while ((opt = w.wgetopt_long(argc, argv, short_options, long_options, nullptr)) != -1) {
        switch (opt) {
            case L'a': {
                append_mode = APPEND_MODE;
                break;
            }
            case L'b': {
                buffer_part = STRING_MODE;
                break;
            }
            case L'i': {
                append_mode = INSERT_MODE;
                break;
            }
            case L'r': {
                append_mode = REPLACE_MODE;
                break;
            }
            case 'c': {
                cut_at_cursor = true;
                break;
            }
            case 't': {
                buffer_part = TOKEN_MODE;
                break;
            }
            case 'j': {
                buffer_part = JOB_MODE;
                break;
            }
            case 'p': {
                buffer_part = PROCESS_MODE;
                break;
            }
            case 'f': {
                function_mode = true;
                break;
            }
            case 'o': {
                tokenize = true;
                break;
            }
            case 'I': {
                // A historical, undocumented feature. TODO: consider removing this.
                override_buffer = w.woptarg;
                break;
            }
            case 'C': {
                cursor_mode = true;
                break;
            }
            case 'B': {
                selection_start_mode = true;
                break;
            }
            case 'E': {
                selection_end_mode = true;
                break;
            }
            case 'L': {
                line_mode = true;
                break;
            }
            case 'S': {
                search_mode = true;
                break;
            }
            case 's': {
                selection_mode = true;
                break;
            }
            case 'P': {
                paging_mode = true;
                break;
            }
            case 'F': {
                paging_full_mode = true;
                break;
            }
            case 1: {
                is_valid = true;
                break;
            }
            case 'h': {
                builtin_print_help(parser, streams, cmd);
                return STATUS_CMD_OK;
            }
            case ':': {
                builtin_missing_argument(parser, streams, cmd, argv[w.woptind - 1], true);
                return STATUS_INVALID_ARGS;
            }
            case L'?': {
                builtin_unknown_option(parser, streams, cmd, argv[w.woptind - 1], true);
                return STATUS_INVALID_ARGS;
            }
            default: {
                DIE("unexpected retval from wgetopt_long");
            }
        }
    }

    if (function_mode) {
        int i;

        // Check for invalid switch combinations.
        if (buffer_part || cut_at_cursor || append_mode || tokenize || cursor_mode || line_mode ||
            search_mode || paging_mode || selection_start_mode || selection_end_mode) {
            streams.err()->append(format_string(BUILTIN_ERR_COMBO, argv[0]));
            builtin_print_error_trailer(parser, *streams.err(), cmd);
            return STATUS_INVALID_ARGS;
        }

        if (argc == w.woptind) {
            builtin_missing_argument(parser, streams, cmd, argv[0], true);
            return STATUS_INVALID_ARGS;
        }

        using rl = readline_cmd_t;
        for (i = w.woptind; i < argc; i++) {
            if (auto mc = input_function_get_code(argv[i])) {
                // Don't enqueue a repaint if we're currently in the middle of one,
                // because that's an infinite loop.
                if (mc == rl::repaint_mode || mc == rl::force_repaint || mc == rl::repaint) {
                    if (ld.is_repaint()) continue;
                }

                // HACK: Execute these right here and now so they can affect any insertions/changes
                // made via bindings. The correct solution is to change all `commandline`
                // insert/replace operations into readline functions with associated data, so that
                // all queued `commandline` operations - including buffer modifications - are
                // executed in order
                if (mc == rl::begin_undo_group || mc == rl::end_undo_group) {
                    reader_handle_command(*mc);
                } else {
                    // Inserts the readline function at the back of the queue.
                    reader_queue_ch(*mc);
                }
            } else {
                streams.err()->append(
                    format_string(_(L"%ls: Unknown input function '%ls'"), cmd, argv[i]));
                builtin_print_error_trailer(parser, *streams.err(), cmd);
                return STATUS_INVALID_ARGS;
            }
        }

        return STATUS_CMD_OK;
    }

    if (selection_mode) {
        if (rstate.selection) {
            streams.out()->append(
                {rstate.text.c_str() + rstate.selection->start, rstate.selection->length});
        }
        return STATUS_CMD_OK;
    }

    // Check for invalid switch combinations.
    if ((selection_start_mode || selection_end_mode) && (argc - w.woptind)) {
        streams.err()->append(format_string(BUILTIN_ERR_TOO_MANY_ARGUMENTS, argv[0]));
        builtin_print_error_trailer(parser, *streams.err(), cmd);
        return STATUS_INVALID_ARGS;
    }

    if ((search_mode || line_mode || cursor_mode || paging_mode) && (argc - w.woptind > 1)) {
        streams.err()->append(format_string(BUILTIN_ERR_TOO_MANY_ARGUMENTS, argv[0]));
        builtin_print_error_trailer(parser, *streams.err(), cmd);
        return STATUS_INVALID_ARGS;
    }

    if ((buffer_part || tokenize || cut_at_cursor) &&
        (cursor_mode || line_mode || search_mode || paging_mode || paging_full_mode) &&
        // Special case - we allow to get/set cursor position relative to the process/job/token.
        !(buffer_part && cursor_mode)) {
        streams.err()->append(format_string(BUILTIN_ERR_COMBO, argv[0]));
        builtin_print_error_trailer(parser, *streams.err(), cmd);
        return STATUS_INVALID_ARGS;
    }

    if ((tokenize || cut_at_cursor) && (argc - w.woptind)) {
        streams.err()->append(format_string(
            BUILTIN_ERR_COMBO2, cmd,
            L"--cut-at-cursor and --tokenize can not be used when setting the commandline"));
        builtin_print_error_trailer(parser, *streams.err(), cmd);
        return STATUS_INVALID_ARGS;
    }

    if (append_mode && !(argc - w.woptind)) {
        // No tokens in insert mode just means we do nothing.
        return STATUS_CMD_ERROR;
    }

    // Set default modes.
    if (!append_mode) {
        append_mode = REPLACE_MODE;
    }

    if (!buffer_part) {
        buffer_part = STRING_MODE;
    }

    if (line_mode) {
        streams.out()->append(
            format_string(L"%d\n", parse_util_lineno(rstate.text, rstate.cursor_pos)));
        return STATUS_CMD_OK;
    }

    if (search_mode) {
        return commandline_get_state().search_mode ? 0 : 1;
    }

    if (paging_mode) {
        return commandline_get_state().pager_mode ? 0 : 1;
    }

    if (paging_full_mode) {
        auto state = commandline_get_state();
        return (state.pager_mode && state.pager_fully_disclosed) ? 0 : 1;
    }

    if (selection_start_mode) {
        if (!rstate.selection) {
            return STATUS_CMD_ERROR;
        }
        source_offset_t start = rstate.selection->start;
        streams.out()->append(format_string(L"%lu\n", static_cast<unsigned long>(start)));
        return STATUS_CMD_OK;
    }

    if (selection_end_mode) {
        if (!rstate.selection) {
            return STATUS_CMD_ERROR;
        }
        source_offset_t end = rstate.selection->end();
        streams.out()->append(format_string(L"%lu\n", static_cast<unsigned long>(end)));
        return STATUS_CMD_OK;
    }

    // At this point we have (nearly) exhausted the options which always operate on the true command
    // line. Now we respect the possibility of a transient command line due to evaluating a wrapped
    // completion. Don't do this in cursor_mode: it makes no sense to move the cursor based on a
    // transient commandline.
    const wchar_t *current_buffer = nullptr;
    size_t current_cursor_pos{0};
    wcstring transient;
    if (override_buffer) {
        current_buffer = override_buffer;
        current_cursor_pos = std::wcslen(current_buffer);
    } else if (!ld.transient_commandlines_empty() && !cursor_mode) {
        transient = *ld.transient_commandlines_back();
        current_buffer = transient.c_str();
        current_cursor_pos = transient.size();
    } else if (rstate.initialized) {
        current_buffer = rstate.text.c_str();
        current_cursor_pos = rstate.cursor_pos;
    } else {
        // There is no command line, either because we are not interactive, or because we are
        // interactive and are still reading init files (in which case we silently ignore this).
        if (!is_interactive_session()) {
            streams.err()->append(cmd);
            streams.err()->append(L": Can not set commandline in non-interactive mode\n");
            builtin_print_error_trailer(parser, *streams.err(), cmd);
        }
        return STATUS_CMD_ERROR;
    }

    if (is_valid) {
        if (!*current_buffer) return 1;
        parser_test_error_bits_t res = parse_util_detect_errors(
            current_buffer, nullptr, true /* accept incomplete so we can tell the difference */);
        if (res & PARSER_TEST_INCOMPLETE) {
            return 2;
        }
        return res & PARSER_TEST_ERROR ? STATUS_CMD_ERROR : STATUS_CMD_OK;
    }

    switch (buffer_part) {
        case STRING_MODE: {
            begin = current_buffer;
            end = begin + std::wcslen(begin);
            break;
        }
        case PROCESS_MODE: {
            parse_util_process_extent(current_buffer, current_cursor_pos, &begin, &end, nullptr);
            break;
        }
        case JOB_MODE: {
            parse_util_job_extent(current_buffer, current_cursor_pos, &begin, &end);
            break;
        }
        case TOKEN_MODE: {
            parse_util_token_extent(current_buffer, current_cursor_pos, &begin, &end, nullptr,
                                    nullptr);
            break;
        }
        default: {
            DIE("unexpected buffer_part");
        }
    }

    if (cursor_mode) {
        if (argc - w.woptind) {
            long new_pos = fish_wcstol(argv[w.woptind]) + (begin - current_buffer);
            if (errno) {
                streams.err()->append(format_string(BUILTIN_ERR_NOT_NUMBER, cmd, argv[w.woptind]));
                builtin_print_error_trailer(parser, *streams.err(), cmd);
            }

            new_pos =
                std::max(0L, std::min(new_pos, static_cast<long>(std::wcslen(current_buffer))));
            commandline_set_buffer(current_buffer, static_cast<size_t>(new_pos));
        } else {
            size_t pos = current_cursor_pos - (begin - current_buffer);
            streams.out()->append(format_string(L"%lu\n", static_cast<unsigned long>(pos)));
        }
        return STATUS_CMD_OK;
    }

    int arg_count = argc - w.woptind;
    if (arg_count == 0) {
        write_part(begin, end, cut_at_cursor, tokenize, current_buffer, current_cursor_pos,
                   streams);
    } else if (arg_count == 1) {
        replace_part(begin, end, argv[w.woptind], append_mode, current_buffer, current_cursor_pos);
    } else {
        wcstring sb = argv[w.woptind];
        for (int i = w.woptind + 1; i < argc; i++) {
            sb.push_back(L'\n');
            sb.append(argv[i]);
        }
        replace_part(begin, end, sb.c_str(), append_mode, current_buffer, current_cursor_pos);
    }

    return STATUS_CMD_OK;
}
