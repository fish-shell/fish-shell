// Functions used for implementing the commandline builtin.
#include <assert.h>
#include <errno.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdlib.h>
#include <wchar.h>
#include <cstring>

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

/// Pointer to what the commandline builtin considers to be the current contents of the command line
/// buffer.
static const wchar_t *current_buffer = 0;
// What the commandline builtin considers to be the current cursor position.
static size_t current_cursor_pos = (size_t)(-1);

/// Returns the current commandline buffer.
static const wchar_t *get_buffer() { return current_buffer; }

/// Returns the position of the cursor.
static size_t get_cursor_pos() { return current_cursor_pos; }

static pthread_mutex_t transient_commandline_lock = PTHREAD_MUTEX_INITIALIZER;
static wcstring_list_t *get_transient_stack() {
    ASSERT_IS_MAIN_THREAD();
    ASSERT_IS_LOCKED(transient_commandline_lock);
    // A pointer is a little more efficient than an object as a static because we can elide the
    // thread-safe initialization.
    static wcstring_list_t *result = NULL;
    if (!result) {
        result = new wcstring_list_t();
    }
    return result;
}

static bool get_top_transient(wcstring *out_result) {
    ASSERT_IS_MAIN_THREAD();
    bool result = false;
    scoped_lock locker(transient_commandline_lock);
    const wcstring_list_t *stack = get_transient_stack();
    if (!stack->empty()) {
        out_result->assign(stack->back());
        result = true;
    }
    return result;
}

builtin_commandline_scoped_transient_t::builtin_commandline_scoped_transient_t(
    const wcstring &cmd) {
    ASSERT_IS_MAIN_THREAD();
    scoped_lock locker(transient_commandline_lock);
    wcstring_list_t *stack = get_transient_stack();
    stack->push_back(cmd);
    this->token = stack->size();
}

builtin_commandline_scoped_transient_t::~builtin_commandline_scoped_transient_t() {
    ASSERT_IS_MAIN_THREAD();
    scoped_lock locker(transient_commandline_lock);
    wcstring_list_t *stack = get_transient_stack();
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
static void replace_part(const wchar_t *begin, const wchar_t *end, const wchar_t *insert,
                         int append_mode) {
    const wchar_t *buff = get_buffer();
    size_t out_pos = get_cursor_pos();

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
            long cursor = get_cursor_pos() - (begin - buff);
            out.append(begin, cursor);
            out.append(insert);
            out.append(begin + cursor, end - begin - cursor);
            out_pos += wcslen(insert);
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
static void write_part(const wchar_t *begin, const wchar_t *end, int cut_at_cursor, int tokenize,
                       io_streams_t &streams) {
    size_t pos = get_cursor_pos() - (begin - get_buffer());

    if (tokenize) {
        wchar_t *buff = wcsndup(begin, end - begin);
        // fwprintf( stderr, L"Subshell: %ls, end char %lc\n", buff, *end );
        wcstring out;
        tokenizer_t tok(buff, TOK_ACCEPT_UNFINISHED);
        tok_t token;
        while (tok.next(&token)) {
            if ((cut_at_cursor) && (token.offset + token.text.size() >= pos)) break;

            switch (token.type) {
                case TOK_STRING: {
                    wcstring tmp = token.text;
                    unescape_string_in_place(&tmp, UNESCAPE_INCOMPLETE);
                    out.append(tmp);
                    out.push_back(L'\n');
                    break;
                }
                default: { break; }
            }
        }

        streams.out.append(out);

        free(buff);
    } else {
        if (cut_at_cursor) {
            end = begin + pos;
        }

        // debug( 0, L"woot2 %ls -> %ls", buff, esc );
        streams.out.append(begin, end - begin);
        streams.out.append(L"\n");
    }
}

/// The commandline builtin. It is used for specifying a new value for the commandline.
int builtin_commandline(parser_t &parser, io_streams_t &streams, wchar_t **argv) {
    wgetopter_t w;
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

    scoped_push<const wchar_t *> saved_current_buffer(&current_buffer);
    scoped_push<size_t> saved_current_cursor_pos(&current_cursor_pos);

    wcstring transient_commandline;
    if (get_top_transient(&transient_commandline)) {
        current_buffer = transient_commandline.c_str();
        current_cursor_pos = transient_commandline.size();
    } else {
        current_buffer = reader_get_buffer();
        current_cursor_pos = reader_get_cursor_pos();
    }

    if (!get_buffer()) {
        if (is_interactive_session) {
            // Prompt change requested while we don't have a prompt, most probably while reading the
            // init files. Just ignore it.
            return 1;
        }

        streams.err.append(argv[0]);
        streams.err.append(L": Can not set commandline in non-interactive mode\n");
        builtin_print_help(parser, streams, argv[0], streams.err);
        return 1;
    }

    w.woptind = 0;

    while (1) {
        static const struct woption long_options[] = {{L"append", no_argument, 0, 'a'},
                                                      {L"insert", no_argument, 0, 'i'},
                                                      {L"replace", no_argument, 0, 'r'},
                                                      {L"current-job", no_argument, 0, 'j'},
                                                      {L"current-process", no_argument, 0, 'p'},
                                                      {L"current-token", no_argument, 0, 't'},
                                                      {L"current-buffer", no_argument, 0, 'b'},
                                                      {L"cut-at-cursor", no_argument, 0, 'c'},
                                                      {L"function", no_argument, 0, 'f'},
                                                      {L"tokenize", no_argument, 0, 'o'},
                                                      {L"help", no_argument, 0, 'h'},
                                                      {L"input", required_argument, 0, 'I'},
                                                      {L"cursor", no_argument, 0, 'C'},
                                                      {L"line", no_argument, 0, 'L'},
                                                      {L"search-mode", no_argument, 0, 'S'},
                                                      {L"selection", no_argument, 0, 's'},
                                                      {L"paging-mode", no_argument, 0, 'P'},
                                                      {0, 0, 0, 0}};

        int opt_index = 0;

        int opt = w.wgetopt_long(argc, argv, L"abijpctwforhI:CLSsP", long_options, &opt_index);
        if (opt == -1) break;

        switch (opt) {
            case 0: {
                if (long_options[opt_index].flag != 0) break;
                streams.err.append_format(BUILTIN_ERR_UNKNOWN, argv[0],
                                          long_options[opt_index].name);
                builtin_print_help(parser, streams, argv[0], streams.err);

                return 1;
            }
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
                builtin_print_help(parser, streams, argv[0], streams.out);
                return 0;
            }
            case L'?': {
                builtin_unknown_option(parser, streams, argv[0], argv[w.woptind - 1]);
                return 1;
            }
        }
    }

    if (function_mode) {
        int i;

        // Check for invalid switch combinations.
        if (buffer_part || cut_at_cursor || append_mode || tokenize || cursor_mode || line_mode ||
            search_mode || paging_mode) {
            streams.err.append_format(BUILTIN_ERR_COMBO, argv[0]);

            builtin_print_help(parser, streams, argv[0], streams.err);
            return 1;
        }

        if (argc == w.woptind) {
            streams.err.append_format(BUILTIN_ERR_MISSING, argv[0]);

            builtin_print_help(parser, streams, argv[0], streams.err);
            return 1;
        }

        for (i = w.woptind; i < argc; i++) {
            wchar_t c = input_function_get_code(argv[i]);
            if (c != INPUT_CODE_NONE) {
                // input_unreadch inserts the specified keypress or readline function at the back of
                // the queue of unused keypresses.
                input_queue_ch(c);
            } else {
                streams.err.append_format(_(L"%ls: Unknown input function '%ls'\n"), argv[0],
                                          argv[i]);
                builtin_print_help(parser, streams, argv[0], streams.err);
                return 1;
            }
        }

        return 0;
    }

    if (selection_mode) {
        size_t start, len;
        const wchar_t *buffer = reader_get_buffer();
        if (reader_get_selection(&start, &len)) {
            streams.out.append(buffer + start, len);
        }
        return 0;
    }

    // Check for invalid switch combinations.
    if ((search_mode || line_mode || cursor_mode || paging_mode) && (argc - w.woptind > 1)) {
        streams.err.append_format(argv[0], L": Too many arguments\n", NULL);
        builtin_print_help(parser, streams, argv[0], streams.err);
        return 1;
    }

    if ((buffer_part || tokenize || cut_at_cursor) &&
        (cursor_mode || line_mode || search_mode || paging_mode)) {
        streams.err.append_format(BUILTIN_ERR_COMBO, argv[0]);

        builtin_print_help(parser, streams, argv[0], streams.err);
        return 1;
    }

    if ((tokenize || cut_at_cursor) && (argc - w.woptind)) {
        streams.err.append_format(
            BUILTIN_ERR_COMBO2, argv[0],
            L"--cut-at-cursor and --tokenize can not be used when setting the commandline");

        builtin_print_help(parser, streams, argv[0], streams.err);
        return 1;
    }

    if (append_mode && !(argc - w.woptind)) {
        streams.err.append_format(
            BUILTIN_ERR_COMBO2, argv[0],
            L"insertion mode switches can not be used when not in insertion mode");

        builtin_print_help(parser, streams, argv[0], streams.err);
        return 1;
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
            wchar_t *endptr;
            long new_pos;
            errno = 0;

            new_pos = wcstol(argv[w.woptind], &endptr, 10);
            if (*endptr || errno) {
                streams.err.append_format(BUILTIN_ERR_NOT_NUMBER, argv[0], argv[w.woptind]);
                builtin_print_help(parser, streams, argv[0], streams.err);
            }

            current_buffer = reader_get_buffer();
            new_pos = maxi(0L, mini(new_pos, (long)wcslen(current_buffer)));
            reader_set_buffer(current_buffer, (size_t)new_pos);
            return 0;
        } else {
            streams.out.append_format(L"%lu\n", (unsigned long)reader_get_cursor_pos());
            return 0;
        }
    }

    if (line_mode) {
        size_t pos = reader_get_cursor_pos();
        const wchar_t *buff = reader_get_buffer();
        streams.out.append_format(L"%lu\n", (unsigned long)parse_util_lineno(buff, pos));
        return 0;
    }

    if (search_mode) {
        return !reader_search_mode();
    }

    if (paging_mode) {
        return !reader_has_pager_contents();
    }

    switch (buffer_part) {
        case STRING_MODE: {
            begin = get_buffer();
            end = begin + wcslen(begin);
            break;
        }
        case PROCESS_MODE: {
            parse_util_process_extent(get_buffer(), get_cursor_pos(), &begin, &end);
            break;
        }
        case JOB_MODE: {
            parse_util_job_extent(get_buffer(), get_cursor_pos(), &begin, &end);
            break;
        }
        case TOKEN_MODE: {
            parse_util_token_extent(get_buffer(), get_cursor_pos(), &begin, &end, 0, 0);
            break;
        }
    }

    switch (argc - w.woptind) {
        case 0: {
            write_part(begin, end, cut_at_cursor, tokenize, streams);
            break;
        }
        case 1: {
            replace_part(begin, end, argv[w.woptind], append_mode);
            break;
        }
        default: {
            wcstring sb = argv[w.woptind];
            for (int i = w.woptind + 1; i < argc; i++) {
                sb.push_back(L'\n');
                sb.append(argv[i]);
            }
            replace_part(begin, end, sb.c_str(), append_mode);
            break;
        }
    }

    return 0;
}
