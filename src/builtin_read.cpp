// Implementation of the read builtin.
#include "config.h"  // IWYU pragma: keep

#include <errno.h>
#include <limits.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <wchar.h>

#include <algorithm>
#include <memory>
#include <numeric>
#include <string>
#include <vector>

#include "builtin.h"
#include "builtin_read.h"
#include "common.h"
#include "complete.h"
#include "env.h"
#include "event.h"
#include "fallback.h"  // IWYU pragma: keep
#include "highlight.h"
#include "history.h"
#include "io.h"
#include "proc.h"
#include "reader.h"
#include "wcstringutil.h"
#include "wgetopt.h"
#include "wutil.h"  // IWYU pragma: keep

struct read_cmd_opts_t {
    bool print_help = false;
    int place = ENV_USER;
    wcstring prompt_cmd;
    const wchar_t *prompt = NULL;
    const wchar_t *prompt_str = NULL;
    const wchar_t *right_prompt = L"";
    const wchar_t *commandline = L"";
    // If a delimiter was given. Used to distinguish between the default
    // empty string and a given empty delimiter.
    bool have_delimiter = false;
    wcstring delimiter;
    bool shell = false;
    bool array = false;
    bool silent = false;
    bool split_null = false;
    bool to_stdout = false;
    int nchars = 0;
    bool all_lines = false;
    bool one_line = false;
};

static const wchar_t *const short_options = L":Aac:d:ghiLlm:n:p:suxzP:UR:LB";
static const struct woption long_options[] = {
    {L"array", no_argument, NULL, 'a'},
    {L"all-lines", no_argument, NULL, 'A'},
    {L"command", required_argument, NULL, 'c'},
    {L"delimiter", required_argument, NULL, 'd'},
    {L"export", no_argument, NULL, 'x'},
    {L"global", no_argument, NULL, 'g'},
    {L"help", no_argument, NULL, 'h'},
    {L"line", no_argument, NULL, 'L'},
    {L"local", no_argument, NULL, 'l'},
    {L"mode-name", required_argument, NULL, 'm'},
    {L"nchars", required_argument, NULL, 'n'},
    {L"null", no_argument, NULL, 'z'},
    {L"prompt", required_argument, NULL, 'p'},
    {L"prompt-str", required_argument, NULL, 'P'},
    {L"right-prompt", required_argument, NULL, 'R'},
    {L"shell", no_argument, NULL, 'S'},
    {L"silent", no_argument, NULL, 's'},
    {L"unexport", no_argument, NULL, 'u'},
    {L"universal", no_argument, NULL, 'U'},
    {NULL, 0, NULL, 0}
};

static int parse_cmd_opts(read_cmd_opts_t &opts, int *optind,  //!OCLINT(high ncss method)
                          int argc, wchar_t **argv, parser_t &parser, io_streams_t &streams) {
    wchar_t *cmd = argv[0];
    int opt;
    wgetopter_t w;
    while ((opt = w.wgetopt_long(argc, argv, short_options, long_options, NULL)) != -1) {
        switch (opt) {
            case 'a': {
                opts.array = true;
                break;
            }
            case L'A': {
                opts.all_lines = true;
                break;
            }
            case L'c': {
                opts.commandline = w.woptarg;
                break;
            }
            case 'd': {
                opts.have_delimiter = true;
                opts.delimiter = w.woptarg;
                break;
            }
            case 'i': {
                streams.err.append_format(_(L"%ls: usage of -i for --silent is deprecated. Please use -s or --silent instead.\n"),
                        cmd);
                return STATUS_INVALID_ARGS;
            }
            case L'g': {
                opts.place |= ENV_GLOBAL;
                break;
            }
            case 'h': {
                opts.print_help = true;
                break;
            }
            case L'L': {
                opts.one_line = true;
                break;
            }
            case L'l': {
                opts.place |= ENV_LOCAL;
                break;
            }
            case L'm': {
                streams.err.append_format(_(L"%ls: flags '--mode-name' / '-m' are now ignored. "
                                            L"Set fish_history instead.\n"),
                                          cmd);
                break;
            }
            case L'n': {
                opts.nchars = fish_wcstoi(w.woptarg);
                if (errno) {
                    if (errno == ERANGE) {
                        streams.err.append_format(_(L"%ls: Argument '%ls' is out of range\n"), cmd,
                                                  w.woptarg);
                        builtin_print_help(parser, streams, cmd, streams.err);
                        return STATUS_INVALID_ARGS;
                    }

                    streams.err.append_format(_(L"%ls: Argument '%ls' must be an integer\n"), cmd,
                                              w.woptarg);
                    builtin_print_help(parser, streams, cmd, streams.err);
                    return STATUS_INVALID_ARGS;
                }
                break;
            }
            case L'P': {
                opts.prompt_str = w.woptarg;
                break;
            }
            case L'p': {
                opts.prompt = w.woptarg;
                break;
            }
            case L'R': {
                opts.right_prompt = w.woptarg;
                break;
            }
            case 's': {
                opts.silent = true;
                break;
            }
            case L'S': {
                opts.shell = true;
                break;
            }
            case L'U': {
                opts.place |= ENV_UNIVERSAL;
                break;
            }
            case L'u': {
                opts.place |= ENV_UNEXPORT;
                break;
            }
            case L'x': {
                opts.place |= ENV_EXPORT;
                break;
            }
            case L'z': {
                opts.split_null = true;
                break;
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

    *optind = w.woptind;
    return STATUS_CMD_OK;
}

/// Read from the tty. This is only valid when the stream is stdin and it is attached to a tty and
/// we weren't asked to split on null characters.
static int read_interactive(wcstring &buff, int nchars, bool shell, bool silent,
                            const wchar_t *prompt, const wchar_t *right_prompt,
                            const wchar_t *commandline) {
    int exit_res = STATUS_CMD_OK;
    const wchar_t *line;

    wcstring read_history_ID = history_session_id();
    if (!read_history_ID.empty()) read_history_ID += L"_read";
    reader_push(read_history_ID);

    reader_set_left_prompt(prompt);
    reader_set_right_prompt(right_prompt);
    if (shell) {
        reader_set_complete_function(&complete);
        reader_set_highlight_function(&highlight_shell);
        reader_set_test_function(&reader_shell_test);
    }
    // No autosuggestions or abbreviations in builtin_read.
    reader_set_allow_autosuggesting(false);
    reader_set_expand_abbreviations(false);
    reader_set_exit_on_interrupt(true);
    reader_set_silent_status(silent);

    reader_set_buffer(commandline, wcslen(commandline));
    proc_push_interactive(1);

    event_fire_generic(L"fish_prompt");
    line = reader_readline(nchars);
    proc_pop_interactive();
    if (line) {
        if (0 < nchars && (size_t)nchars < wcslen(line)) {
            // Line may be longer than nchars if a keybinding used `commandline -i`
            // note: we're deliberately throwing away the tail of the commandline.
            // It shouldn't be unread because it was produced with `commandline -i`,
            // not typed.
            buff = wcstring(line, nchars);
        } else {
            buff = wcstring(line);
        }
    } else {
        exit_res = STATUS_CMD_ERROR;
    }
    reader_pop();
    return exit_res;
}

/// Bash uses 128 bytes for its chunk size. Very informal testing I did suggested that a smaller
/// chunk size performed better. However, we're going to use the bash value under the assumption
/// they've done more extensive testing.
#define READ_CHUNK_SIZE 128

/// Read from the fd in chunks until we see newline or null, as requested, is seen. This is only
/// used when the fd is seekable (so not from a tty or pipe) and we're not reading a specific number
/// of chars.
///
/// Returns an exit status.
static int read_in_chunks(int fd, wcstring &buff, bool split_null) {
    int exit_res = STATUS_CMD_OK;
    std::string str;
    bool eof = false;
    bool finished = false;

    while (!finished) {
        char inbuf[READ_CHUNK_SIZE];
        long bytes_read = read_blocked(fd, inbuf, READ_CHUNK_SIZE);

        if (bytes_read <= 0) {
            eof = true;
            break;
        }

        const char *end = std::find(inbuf, inbuf + bytes_read, split_null ? L'\0' : L'\n');
        long bytes_consumed = end - inbuf;  // must be signed for use in lseek
        assert(bytes_consumed <= bytes_read);
        str.append(inbuf, bytes_consumed);
        if (bytes_consumed < bytes_read) {
            // We found a splitter. The +1 because we need to treat the splitter as consumed, but
            // not append it to the string.
            CHECK(lseek(fd, bytes_consumed - bytes_read + 1, SEEK_CUR) != -1, STATUS_CMD_ERROR)
            finished = true;
        } else if (str.size() > read_byte_limit) {
            exit_res = STATUS_READ_TOO_MUCH;
            finished = true;
        }
    }

    buff = str2wcstring(str);
    if (buff.empty() && eof) {
        exit_res = STATUS_CMD_ERROR;
    }

    return exit_res;
}

/// Read from the fd on char at a time until we've read the requested number of characters or a
/// newline or null, as appropriate, is seen. This is inefficient so should only be used when the
/// fd is not seekable.
static int read_one_char_at_a_time(int fd, wcstring &buff, int nchars, bool split_null) {
    int exit_res = STATUS_CMD_OK;
    bool eof = false;
    size_t nbytes = 0;

    while (true) {
        bool finished = false;
        wchar_t res = 0;
        mbstate_t state = {};

        while (!finished) {
            char b;
            if (read_blocked(fd, &b, 1) <= 0) {
                eof = true;
                break;
            }

            nbytes++;
            if (MB_CUR_MAX == 1) {
                res = (unsigned char)b;
                finished = true;
            } else {
                size_t sz = mbrtowc(&res, &b, 1, &state);
                if (sz == (size_t)-1) {
                    memset(&state, 0, sizeof(state));
                } else if (sz != (size_t)-2) {
                    finished = true;
                }
            }
        }

        if (nbytes > read_byte_limit) {
            exit_res = STATUS_READ_TOO_MUCH;
            break;
        }
        if (eof) break;
        if (!split_null && res == L'\n') break;
        if (split_null && res == L'\0') break;

        buff.push_back(res);
        if (nchars > 0 && (size_t)nchars <= buff.size()) {
            break;
        }
    }

    if (buff.empty() && eof) {
        exit_res = STATUS_CMD_ERROR;
    }

    return exit_res;
}

/// Validate the arguments given to `read` and provide defaults where needed.
static int validate_read_args(const wchar_t *cmd, read_cmd_opts_t &opts, int argc,
                              const wchar_t *const *argv, parser_t &parser, io_streams_t &streams) {
    if (opts.prompt && opts.prompt_str) {
        streams.err.append_format(_(L"%ls: Options %ls and %ls cannot be used together\n"), cmd, L"-p", L"-P");
        builtin_print_help(parser, streams, cmd, streams.err);
        return STATUS_INVALID_ARGS;
    }

    if (opts.have_delimiter && opts.all_lines) {
        streams.err.append_format(_(L"%ls: Options %ls and %ls cannot be used together\n"), cmd, L"--delimiter", L"--all-lines");
        return STATUS_INVALID_ARGS;
    }
    if (opts.have_delimiter && opts.one_line) {
        streams.err.append_format(_(L"%ls: Options %ls and %ls cannot be used together\n"), cmd, L"--delimiter", L"--line");
        return STATUS_INVALID_ARGS;
    }
    if (opts.one_line && opts.all_lines) {
        streams.err.append_format(_(L"%ls: Options %ls and %ls cannot be used together\n"), cmd, L"--all-lines", L"--line");
        return STATUS_INVALID_ARGS;
    }
    if (opts.one_line && opts.split_null) {
        streams.err.append_format(_(L"%ls: Options %ls and %ls cannot be used together\n"), cmd, L"-z", L"--line");
        return STATUS_INVALID_ARGS;
    }

    if (opts.prompt_str) {
        opts.prompt_cmd = L"echo " + escape_string(opts.prompt_str, ESCAPE_ALL);
        opts.prompt = opts.prompt_cmd.c_str();
    } else if (!opts.prompt) {
        opts.prompt = DEFAULT_READ_PROMPT;
    }

    if ((opts.place & ENV_UNEXPORT) && (opts.place & ENV_EXPORT)) {
        streams.err.append_format(BUILTIN_ERR_EXPUNEXP, cmd);
        builtin_print_help(parser, streams, cmd, streams.err);
        return STATUS_INVALID_ARGS;
    }

    if ((opts.place & ENV_LOCAL ? 1 : 0) + (opts.place & ENV_GLOBAL ? 1 : 0) +
            (opts.place & ENV_UNIVERSAL ? 1 : 0) >
        1) {
        streams.err.append_format(BUILTIN_ERR_GLOCAL, cmd);
        builtin_print_help(parser, streams, cmd, streams.err);
        return STATUS_INVALID_ARGS;
    }

    if (!opts.array && argc < 1 && !opts.to_stdout) {
        streams.err.append_format(BUILTIN_ERR_MIN_ARG_COUNT1, cmd, 1, argc);
        return STATUS_INVALID_ARGS;
    }

    if (opts.array && argc != 1) {
        streams.err.append_format(BUILTIN_ERR_ARG_COUNT1, cmd, 1, argc);
        return STATUS_INVALID_ARGS;
    }

    if (opts.to_stdout && argc > 0) {
        streams.err.append_format(BUILTIN_ERR_MAX_ARG_COUNT1, cmd, 0, argc);
        return STATUS_INVALID_ARGS;
    }

    // Verify all variable names.
    for (int i = 0; i < argc; i++) {
        if (!valid_var_name(argv[i])) {
            streams.err.append_format(BUILTIN_ERR_VARNAME, cmd, argv[i]);
            builtin_print_help(parser, streams, cmd, streams.err);
            return STATUS_INVALID_ARGS;
        }
    }

    return STATUS_CMD_OK;
}

/// The read builtin. Reads from stdin and stores the values in environment variables.
int builtin_read(parser_t &parser, io_streams_t &streams, wchar_t **argv) {
    wchar_t *cmd = argv[0];
    int argc = builtin_count_args(argv);
    wcstring buff;
    int exit_res = STATUS_CMD_OK;
    read_cmd_opts_t opts;

    int optind;
    int retval = parse_cmd_opts(opts, &optind, argc, argv, parser, streams);
    if (retval != STATUS_CMD_OK) return retval;
    if (!opts.to_stdout) {
        argc -= optind;
        argv += optind;
    }

    if (argc == 0) {
        opts.to_stdout = true;
    }

    if (opts.print_help) {
        builtin_print_help(parser, streams, cmd, streams.out);
        return STATUS_CMD_OK;
    }

    retval = validate_read_args(cmd, opts, argc, argv, parser, streams);
    if (retval != STATUS_CMD_OK) return retval;

    if (opts.all_lines) {
        // --all-lines is the same as read -d \n -z
        opts.have_delimiter = true;
        opts.delimiter = L"\n";
        opts.split_null = true;
        opts.shell = false;
    }
    else if (opts.one_line) {
        // --line is the same as read -d \n repeated N times
        opts.have_delimiter = true;
        opts.delimiter = L"\n";
        opts.split_null = false;
        opts.shell = false;
    }

    wchar_t * const *var_ptr = argv;
    auto vars_left = [&] () { return argv + argc - var_ptr; };
    auto clear_remaining_vars = [&] () {
        while (vars_left()) {
            env_set_empty(*var_ptr, opts.place);
            // env_set_one(*var_ptr, opts.place, L"");
            ++var_ptr;
        }
    };

    // Normally, we either consume a line of input or all available input. But if we are reading a line at
    // a time, we need a middle ground where we only consume as many lines as we need to fill the given vars.
    do {
        buff.clear();

        // TODO: Determine if the original set of conditions for interactive reads should be reinstated:
        // if (isatty(0) && streams.stdin_fd == STDIN_FILENO && !split_null) {
        int stream_stdin_is_a_tty = isatty(streams.stdin_fd);
        if (stream_stdin_is_a_tty && !opts.split_null) {
            // Read interactively using reader_readline(). This does not support splitting on null.
            exit_res = read_interactive(buff, opts.nchars, opts.shell, opts.silent, opts.prompt,
                                        opts.right_prompt, opts.commandline);
        } else if (!opts.nchars && !stream_stdin_is_a_tty &&
                   lseek(streams.stdin_fd, 0, SEEK_CUR) != -1) {
            exit_res = read_in_chunks(streams.stdin_fd, buff, opts.split_null);
        } else {
            exit_res = read_one_char_at_a_time(streams.stdin_fd, buff, opts.nchars, opts.split_null);
        }

        if (exit_res != STATUS_CMD_OK) {
            clear_remaining_vars();
            return exit_res;
        }

        if (opts.to_stdout) {
            streams.out.append(buff);
            return exit_res;
        }

        if (!opts.have_delimiter) {
            auto ifs = env_get(L"IFS");
            if (!ifs.missing_or_empty()) opts.delimiter = ifs->as_string();
        }

        if (opts.delimiter.empty()) {
            // Every character is a separate token with one wrinkle involving non-array mode where the
            // final var gets the remaining characters as a single string.
            size_t x = std::max(static_cast<size_t>(1), buff.size());
            size_t n_splits = (opts.array || static_cast<size_t>(vars_left()) > x) ? x : vars_left();
            wcstring_list_t chars;
            chars.reserve(n_splits);
            x = x - n_splits + 1;
            int i = 0;
            for (auto it = buff.begin(), end = buff.end(); it != end; ++i, ++it) {
                if (opts.array || i + 1 < vars_left()) {
                    chars.emplace_back(1, *it);
                } else {
                    chars.emplace_back(it, buff.end());
                    break;
                }
            }

            if (opts.array) {
                // Array mode: assign each char as a separate element of the sole var.
                env_set(*var_ptr++, opts.place, chars);
            } else {
                // Not array mode: assign each char to a separate var with the remainder being assigned
                // to the last var.
                auto c = chars.begin();
                for (; c != chars.end() && vars_left(); ++c) {
                    env_set_one(*var_ptr++, opts.place, *c);
                }
            }
        } else if (opts.array) {
            // The user has requested the input be split into a sequence of tokens and all the tokens
            // assigned to a single var. How we do the tokenizing depends on whether the user specified
            // the delimiter string or we're using IFS.
            if (!opts.have_delimiter) {
                // We're using IFS, so tokenize the buffer using each IFS char. This is for backward
                // compatibility with old versions of fish.
                wcstring_list_t tokens;

                for (wcstring_range loc = wcstring_tok(buff, opts.delimiter);
                     loc.first != wcstring::npos; loc = wcstring_tok(buff, opts.delimiter, loc)) {
                    tokens.emplace_back(wcstring(buff, loc.first, loc.second));
                }
                env_set(*var_ptr++, opts.place, tokens);
            } else {
                // We're using a delimiter provided by the user so use the `string split` behavior.
                wcstring_list_t splits;
                split_about(buff.begin(), buff.end(), opts.delimiter.begin(), opts.delimiter.end(),
                            &splits);

                env_set(*var_ptr++, opts.place, splits);
            }
        } else {
            // Not array mode. Split the input into tokens and assign each to the vars in sequence.
            if (!opts.have_delimiter) {
                // We're using IFS, so tokenize the buffer using each IFS char. This is for backward
                // compatibility with old versions of fish.
                wcstring_range loc = wcstring_range(0, 0);
                while (vars_left()) {
                    wcstring substr;
                    loc = wcstring_tok(buff, (vars_left() > 1) ? opts.delimiter : wcstring(), loc);
                    if (loc.first != wcstring::npos) {
                        substr = wcstring(buff, loc.first, loc.second);
                    }
                    env_set_one(*var_ptr++, opts.place, substr);
                }
            } else {
                // We're using a delimiter provided by the user so use the `string split` behavior.
                wcstring_list_t splits;
                // We're making at most argc - 1 splits so the last variable
                // is set to the remaining string.
                split_about(buff.begin(), buff.end(), opts.delimiter.begin(), opts.delimiter.end(),
                            &splits, argc - 1);
                assert(splits.size() <= (size_t) vars_left());
                for (const auto &split : splits) {
                    env_set_one(*var_ptr++, opts.place, split);
                }
            }
        }
    } while (opts.one_line && vars_left());

    if (!opts.array) {
        // In case there were more args than splits
        clear_remaining_vars();
    }

    return exit_res;
}
