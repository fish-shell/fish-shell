// Functions used for implementing the commandline builtin.
#include "config.h"  // IWYU pragma: keep

#include <errno.h>
#include <stddef.h>
#include <stdlib.h>
#include <wchar.h>

#include "builtin.h"
#include "common.h"
#include "fallback.h"  // IWYU pragma: keep
#include "input.h"
#include "io.h"
#include "parse_util.h"
#include "proc.h"
#include "reader.h"
#include "tokenizer.h"
#include "util.h"
#include "wgetopt.h"
#include "wutil.h"  // IWYU pragma: keep

class parser_t;

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

static owning_lock<wcstring_list_t> &get_transient_stack() {
    ASSERT_IS_MAIN_THREAD();
    static owning_lock<wcstring_list_t> s_transient_stack;
    return s_transient_stack;
}

static bool get_top_transient(wcstring *out_result) {
    auto stack = get_transient_stack().acquire();
    if (stack->empty()) {
        return false;
    }
    out_result->assign(stack->back());
    return true;
}

builtin_commandline_scoped_transient_t::builtin_commandline_scoped_transient_t(
    const wcstring &cmd) {
    ASSERT_IS_MAIN_THREAD();
    auto stack = get_transient_stack().acquire();
    stack->push_back(cmd);
    this->token = stack->size();
}

builtin_commandline_scoped_transient_t::~builtin_commandline_scoped_transient_t() {
    ASSERT_IS_MAIN_THREAD();
    auto stack = get_transient_stack().acquire();
    assert(this->token == stack->size());
    stack->pop_back();
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
static void replace_part(const wchar_t *begin, const wchar_t *end, const wchar_t *insert,
                         int append_mode, const wchar_t *buff, size_t cursor_pos) {
    size_t out_pos = cursor_pos;

    wcstring out;

    out.append(buff, begin - buff);

    switch (append_mode) {
        case REPLACE_MODE: {
            out.append(insert);
            out_pos = wcslen(insert) + (begin - buff);
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
            out_pos += wcslen(insert);
            break;
        }
        default: {
            DIE("unexpected append_mode");
            break;
        }
    }
    out.append(end);
    reader_set_buffer(out, out_pos);
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
        // fwprintf( stderr, L"Subshell: %ls, end char %lc\n", buff, *end );
        wcstring out;
        wcstring buff(begin, end - begin);
        tokenizer_t tok(buff.c_str(), TOK_ACCEPT_UNFINISHED);
        tok_t token;
        while (tok.next(&token)) {
            if ((cut_at_cursor) && (token.offset + token.length >= pos)) break;

            if (token.type == TOK_STRING) {
                wcstring tmp = tok.text_of(token);
                unescape_string_in_place(&tmp, UNESCAPE_INCOMPLETE);
                out.append(tmp);
                out.push_back(L'\n');
            }
        }

        streams.out.append(out);
    } else {
        if (cut_at_cursor) {
            streams.out.append(begin, pos);
        } else {
            streams.out.append(begin, end - begin);
        }
        streams.out.push_back(L'\n');
    }
}

/// The commandline builtin. It is used for specifying a new value for the commandline.
int builtin_commandline(parser_t &parser, io_streams_t &streams, wchar_t **argv) {
    // Pointer to what the commandline builtin considers to be the current contents of the command
    // line buffer.
    const wchar_t *current_buffer = 0;

    // What the commandline builtin considers to be the current cursor position.
    size_t current_cursor_pos = (size_t)(-1);

    wchar_t *cmd = argv[0];
    int buffer_part = 0;
    int cut_at_cursor = 0;

    int argc = builtin_count_args(argv);
    int append_mode = 0;

    int function_mode = 0;
    int selection_mode = 0;

    int tokenize = 0;

    int cursor_mode = 0;
    int line_mode = 0;
    int search_mode = 0;
    int paging_mode = 0;
    const wchar_t *begin = NULL, *end = NULL;

    wcstring transient_commandline;
    if (get_top_transient(&transient_commandline)) {
        current_buffer = transient_commandline.c_str();
        current_cursor_pos = transient_commandline.size();
    } else {
        current_buffer = reader_get_buffer();
        current_cursor_pos = reader_get_cursor_pos();
    }

    if (!current_buffer) {
        if (is_interactive_session) {
            // Prompt change requested while we don't have a prompt, most probably while reading the
            // init files. Just ignore it.
            return STATUS_CMD_ERROR;
        }

        streams.err.append(argv[0]);
        streams.err.append(L": Can not set commandline in non-interactive mode\n");
        builtin_print_help(parser, streams, cmd, streams.err);
        return STATUS_CMD_ERROR;
    }

    static const wchar_t *const short_options = L":abijpctwforhI:CLSsP";
    static const struct woption long_options[] = {{L"append", no_argument, NULL, 'a'},
                                                  {L"insert", no_argument, NULL, 'i'},
                                                  {L"replace", no_argument, NULL, 'r'},
                                                  {L"current-buffer", no_argument, NULL, 'b'},
                                                  {L"current-job", no_argument, NULL, 'j'},
                                                  {L"current-process", no_argument, NULL, 'p'},
                                                  {L"current-selection", no_argument, NULL, 's'},
                                                  {L"current-token", no_argument, NULL, 't'},
                                                  {L"cut-at-cursor", no_argument, NULL, 'c'},
                                                  {L"function", no_argument, NULL, 'f'},
                                                  {L"tokenize", no_argument, NULL, 'o'},
                                                  {L"help", no_argument, NULL, 'h'},
                                                  {L"input", required_argument, NULL, 'I'},
                                                  {L"cursor", no_argument, NULL, 'C'},
                                                  {L"line", no_argument, NULL, 'L'},
                                                  {L"search-mode", no_argument, NULL, 'S'},
                                                  {L"paging-mode", no_argument, NULL, 'P'},
                                                  {NULL, 0, NULL, 0}};

    int opt;
    wgetopter_t w;
    while ((opt = w.wgetopt_long(argc, argv, short_options, long_options, NULL)) != -1) {
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
                cut_at_cursor = 1;
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
                function_mode = 1;
                break;
            }
            case 'o': {
                tokenize = 1;
                break;
            }
            case 'I': {
                current_buffer = w.woptarg;
                current_cursor_pos = wcslen(w.woptarg);
                break;
            }
            case 'C': {
                cursor_mode = 1;
                break;
            }
            case 'L': {
                line_mode = 1;
                break;
            }
            case 'S': {
                search_mode = 1;
                break;
            }
            case 's': {
                selection_mode = 1;
                break;
            }
            case 'P': {
                paging_mode = 1;
                break;
            }
            case 'h': {
                builtin_print_help(parser, streams, cmd, streams.out);
                return STATUS_CMD_OK;
            }
            case ':': {
                builtin_missing_argument(parser, streams, cmd, argv[w.woptind - 1]);
                return STATUS_INVALID_ARGS;
            }
            case L'?': {
                builtin_unknown_option(parser, streams, cmd, argv[w.woptind - 1]);
                return STATUS_INVALID_ARGS;
            }
            default: {
                DIE("unexpected retval from wgetopt_long");
                break;
            }
        }
    }

    if (function_mode) {
        int i;

        // Check for invalid switch combinations.
        if (buffer_part || cut_at_cursor || append_mode || tokenize || cursor_mode || line_mode ||
            search_mode || paging_mode) {
            streams.err.append_format(BUILTIN_ERR_COMBO, argv[0]);
            builtin_print_help(parser, streams, cmd, streams.err);
            return STATUS_INVALID_ARGS;
        }

        if (argc == w.woptind) {
            builtin_missing_argument(parser, streams, cmd, argv[0]);
            return STATUS_INVALID_ARGS;
        }

        for (i = w.woptind; i < argc; i++) {
            wchar_t c = input_function_get_code(argv[i]);
            if (c != INPUT_CODE_NONE) {
                // input_unreadch inserts the specified keypress or readline function at the back of
                // the queue of unused keypresses.
                input_queue_ch(c);
            } else {
                streams.err.append_format(_(L"%ls: Unknown input function '%ls'"), cmd, argv[i]);
                builtin_print_help(parser, streams, cmd, streams.err);
                return STATUS_INVALID_ARGS;
            }
        }

        return STATUS_CMD_OK;
    }

    if (selection_mode) {
        size_t start, len;
        const wchar_t *buffer = reader_get_buffer();
        if (reader_get_selection(&start, &len)) {
            streams.out.append(buffer + start, len);
        }
        return STATUS_CMD_OK;
    }

    // Check for invalid switch combinations.
    if ((search_mode || line_mode || cursor_mode || paging_mode) && (argc - w.woptind > 1)) {
        streams.err.append_format(L"%ls: Too many arguments", argv[0]);
        builtin_print_help(parser, streams, cmd, streams.err);
        return STATUS_INVALID_ARGS;
    }

    if ((buffer_part || tokenize || cut_at_cursor) &&
        (cursor_mode || line_mode || search_mode || paging_mode)) {
        streams.err.append_format(BUILTIN_ERR_COMBO, argv[0]);
        builtin_print_help(parser, streams, cmd, streams.err);
        return STATUS_INVALID_ARGS;
    }

    if ((tokenize || cut_at_cursor) && (argc - w.woptind)) {
        streams.err.append_format(
            BUILTIN_ERR_COMBO2, cmd,
            L"--cut-at-cursor and --tokenize can not be used when setting the commandline");
        builtin_print_help(parser, streams, cmd, streams.err);
        return STATUS_INVALID_ARGS;
    }

    if (append_mode && !(argc - w.woptind)) {
        streams.err.append_format(
            BUILTIN_ERR_COMBO2, cmd,
            L"insertion mode switches can not be used when not in insertion mode");
        builtin_print_help(parser, streams, cmd, streams.err);
        return STATUS_INVALID_ARGS;
    }

    // Set default modes.
    if (!append_mode) {
        append_mode = REPLACE_MODE;
    }

    if (!buffer_part) {
        buffer_part = STRING_MODE;
    }

    if (cursor_mode) {
        if (argc - w.woptind) {
            long new_pos = fish_wcstol(argv[w.woptind]);
            if (errno) {
                streams.err.append_format(BUILTIN_ERR_NOT_NUMBER, cmd, argv[w.woptind]);
                builtin_print_help(parser, streams, cmd, streams.err);
            }

            current_buffer = reader_get_buffer();
            new_pos = maxi(0L, mini(new_pos, (long)wcslen(current_buffer)));
            reader_set_buffer(current_buffer, (size_t)new_pos);
        } else {
            streams.out.append_format(L"%lu\n", (unsigned long)reader_get_cursor_pos());
        }
        return STATUS_CMD_OK;
    }

    if (line_mode) {
        size_t pos = reader_get_cursor_pos();
        const wchar_t *buff = reader_get_buffer();
        streams.out.append_format(L"%lu\n", (unsigned long)parse_util_lineno(buff, pos));
        return STATUS_CMD_OK;
    }

    if (search_mode) {
        return reader_is_in_search_mode() ? 0 : 1;
    }

    if (paging_mode) {
        return reader_has_pager_contents() ? 0 : 1;
    }

    switch (buffer_part) {
        case STRING_MODE: {
            begin = current_buffer;
            end = begin + wcslen(begin);
            break;
        }
        case PROCESS_MODE: {
            parse_util_process_extent(current_buffer, current_cursor_pos, &begin, &end);
            break;
        }
        case JOB_MODE: {
            parse_util_job_extent(current_buffer, current_cursor_pos, &begin, &end);
            break;
        }
        case TOKEN_MODE: {
            parse_util_token_extent(current_buffer, current_cursor_pos, &begin, &end, 0, 0);
            break;
        }
        default: {
            DIE("unexpected buffer_part");
            break;
        }
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
