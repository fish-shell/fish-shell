// Implementation of the read builtin.
#include "config.h"  // IWYU pragma: keep

#include <errno.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <wchar.h>

#include <algorithm>
#include <memory>
#include <string>

#include "builtin.h"
#include "builtin_read.h"
#include "common.h"
#include "complete.h"
#include "env.h"
#include "event.h"
#include "expand.h"
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
    bool shell = false;
    bool array = false;
    bool silent = false;
    bool split_null = false;
    int nchars = 0;
};

static const wchar_t *short_options = L":ac:ghilm:n:p:suxzP:UR:";
static const struct woption long_options[] = {{L"export", no_argument, NULL, 'x'},
                                              {L"global", no_argument, NULL, 'g'},
                                              {L"local", no_argument, NULL, 'l'},
                                              {L"universal", no_argument, NULL, 'U'},
                                              {L"unexport", no_argument, NULL, 'u'},
                                              {L"prompt", required_argument, NULL, 'p'},
                                              {L"prompt-str", required_argument, NULL, 'P'},
                                              {L"right-prompt", required_argument, NULL, 'R'},
                                              {L"command", required_argument, NULL, 'c'},
                                              {L"mode-name", required_argument, NULL, 'm'},
                                              {L"silent", no_argument, NULL, 'i'},
                                              {L"nchars", required_argument, NULL, 'n'},
                                              {L"shell", no_argument, NULL, 's'},
                                              {L"array", no_argument, NULL, 'a'},
                                              {L"null", no_argument, NULL, 'z'},
                                              {L"help", no_argument, NULL, 'h'},
                                              {NULL, 0, NULL, 0}};

static int parse_cmd_opts(read_cmd_opts_t &opts, int *optind,  //!OCLINT(high ncss method)
                          int argc, wchar_t **argv, parser_t &parser, io_streams_t &streams) {
    wchar_t *cmd = argv[0];
    int opt;
    wgetopter_t w;
    while ((opt = w.wgetopt_long(argc, argv, short_options, long_options, NULL)) != -1) {
        switch (opt) {
            case L'x': {
                opts.place |= ENV_EXPORT;
                break;
            }
            case L'g': {
                opts.place |= ENV_GLOBAL;
                break;
            }
            case L'l': {
                opts.place |= ENV_LOCAL;
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
            case L'p': {
                opts.prompt = w.woptarg;
                break;
            }
            case L'P': {
                opts.prompt_str = w.woptarg;
                break;
            }
            case L'R': {
                opts.right_prompt = w.woptarg;
                break;
            }
            case L'c': {
                opts.commandline = w.woptarg;
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
            case 's': {
                opts.shell = true;
                break;
            }
            case 'a': {
                opts.array = true;
                break;
            }
            case L'i': {
                opts.silent = true;
                break;
            }
            case L'z': {
                opts.split_null = true;
                break;
            }
            case 'h': {
                opts.print_help = true;
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
    reader_push(read_history_ID.c_str());

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

    if (opts.print_help) {
        builtin_print_help(parser, streams, cmd, streams.out);
        return STATUS_CMD_OK;
    }

    if (opts.prompt && opts.prompt_str) {
        streams.err.append_format(_(L"%ls: You can't specify both -p and -P\n"), cmd);
        builtin_print_help(parser, streams, cmd, streams.err);
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

    if (opts.array && optind + 1 != argc) {
        streams.err.append_format(_(L"%ls: --array option requires a single variable name.\n"),
                                  cmd);
        builtin_print_help(parser, streams, cmd, streams.err);
        return STATUS_INVALID_ARGS;
    }

    // Verify all variable names.
    for (int i = optind; i < argc; i++) {
        if (!valid_var_name(argv[i])) {
            streams.err.append_format(BUILTIN_ERR_VARNAME, cmd, argv[i]);
            builtin_print_help(parser, streams, cmd, streams.err);
            return STATUS_INVALID_ARGS;
        }
    }

    // TODO: Determine if the original set of conditions for interactive reads should be reinstated:
    // if (isatty(0) && streams.stdin_fd == STDIN_FILENO && !split_null) {
    int stream_stdin_is_a_tty = isatty(streams.stdin_fd);
    if (stream_stdin_is_a_tty && !opts.split_null) {
        // We should read interactively using reader_readline(). This does not support splitting on
        // null.
        exit_res = read_interactive(buff, opts.nchars, opts.shell, opts.silent, opts.prompt,
                                    opts.right_prompt, opts.commandline);
    } else if (!opts.nchars && !stream_stdin_is_a_tty &&
               lseek(streams.stdin_fd, 0, SEEK_CUR) != -1) {
        exit_res = read_in_chunks(streams.stdin_fd, buff, opts.split_null);
    } else {
        exit_res = read_one_char_at_a_time(streams.stdin_fd, buff, opts.nchars, opts.split_null);
    }

    if (optind == argc || exit_res != STATUS_CMD_OK) {
        // Define the var without any data. We do this because when this happens we want the user to
        // be able to use the var but have it expand to nothing.
        //
        // TODO: For fish 3.0 we should mandate at least one var name.
        if (argv[optind]) env_set(argv[optind], NULL, opts.place);
        return exit_res;
    }

    env_var_t ifs = env_get_string(L"IFS");
    if (ifs.missing_or_empty()) {
        // Every character is a separate token.
        size_t bufflen = buff.size();
        if (opts.array) {
            if (bufflen > 0) {
                wcstring chars(bufflen + (bufflen - 1), ARRAY_SEP);
                wcstring::iterator out = chars.begin();
                for (wcstring::const_iterator it = buff.begin(), end = buff.end(); it != end;
                     ++it) {
                    *out = *it;
                    out += 2;
                }
                env_set(argv[optind], chars.c_str(), opts.place);
            } else {
                env_set(argv[optind], NULL, opts.place);
            }
        } else {  // not array
            size_t j = 0;
            for (; optind + 1 < argc; ++optind) {
                if (j < bufflen) {
                    wchar_t buffer[2] = {buff[j++], 0};
                    env_set(argv[optind], buffer, opts.place);
                } else {
                    env_set(argv[optind], L"", opts.place);
                }
            }
            if (optind < argc) env_set(argv[optind], &buff[j], opts.place);
        }
    } else if (opts.array) {
        wcstring tokens;
        tokens.reserve(buff.size());
        bool empty = true;

        for (wcstring_range loc = wcstring_tok(buff, ifs); loc.first != wcstring::npos;
             loc = wcstring_tok(buff, ifs, loc)) {
            if (!empty) tokens.push_back(ARRAY_SEP);
            tokens.append(buff, loc.first, loc.second);
            empty = false;
        }
        env_set(argv[optind], empty ? NULL : tokens.c_str(), opts.place);
    } else {  // not array
        wcstring_range loc = wcstring_range(0, 0);

        while (optind < argc) {
            loc = wcstring_tok(buff, (optind + 1 < argc) ? ifs : wcstring(), loc);
            env_set(argv[optind], loc.first == wcstring::npos ? L"" : &buff.c_str()[loc.first],
                    opts.place);
            ++optind;
        }
    }

    return exit_res;
}
