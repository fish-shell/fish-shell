// Implementation of the read builtin.
#include "config.h"  // IWYU pragma: keep

#include "read.h"

#include <unistd.h>

#include <algorithm>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cwchar>
#include <string>
#include <utility>

#include "../builtin.h"
#include "../common.h"
#include "../env.h"
#include "../fallback.h"  // IWYU pragma: keep
#include "../io.h"
#include "../maybe.h"
#include "../parser.h"
#include "../reader.h"
#include "../tokenizer.h"
#include "../wcstringutil.h"
#include "../wgetopt.h"
#include "../wutil.h"  // IWYU pragma: keep

namespace {
struct read_cmd_opts_t {
    bool print_help = false;
    int place = ENV_USER;
    wcstring prompt_cmd;
    const wchar_t *prompt = nullptr;
    const wchar_t *prompt_str = nullptr;
    const wchar_t *right_prompt = L"";
    const wchar_t *commandline = L"";
    // If a delimiter was given. Used to distinguish between the default
    // empty string and a given empty delimiter.
    bool have_delimiter = false;
    wcstring delimiter;
    bool tokenize = false;
    bool shell = false;
    bool array = false;
    bool silent = false;
    bool split_null = false;
    bool to_stdout = false;
    int nchars = 0;
    bool one_line = false;
};
}  // namespace

static const wchar_t *const short_options = L":ac:d:fghiLln:p:sStuxzP:UR:L";
static const struct woption long_options[] = {{L"array", no_argument, 'a'},
                                              {L"command", required_argument, 'c'},
                                              {L"delimiter", required_argument, 'd'},
                                              {L"export", no_argument, 'x'},
                                              {L"function", no_argument, 'f'},
                                              {L"global", no_argument, 'g'},
                                              {L"help", no_argument, 'h'},
                                              {L"line", no_argument, 'L'},
                                              {L"list", no_argument, 'a'},
                                              {L"local", no_argument, 'l'},
                                              {L"nchars", required_argument, 'n'},
                                              {L"null", no_argument, 'z'},
                                              {L"prompt", required_argument, 'p'},
                                              {L"prompt-str", required_argument, 'P'},
                                              {L"right-prompt", required_argument, 'R'},
                                              {L"shell", no_argument, 'S'},
                                              {L"silent", no_argument, 's'},
                                              {L"tokenize", no_argument, 't'},
                                              {L"unexport", no_argument, 'u'},
                                              {L"universal", no_argument, 'U'},
                                              {}};

static int parse_cmd_opts(read_cmd_opts_t &opts, int *optind,  //!OCLINT(high ncss method)
                          int argc, const wchar_t **argv, parser_t &parser, io_streams_t &streams) {
    const wchar_t *cmd = argv[0];
    int opt;
    wgetopter_t w;
    while ((opt = w.wgetopt_long(argc, argv, short_options, long_options, nullptr)) != -1) {
        switch (opt) {
            case 'a': {
                opts.array = true;
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
                streams.err.append_format(_(L"%ls: usage of -i for --silent is deprecated. Please "
                                            L"use -s or --silent instead.\n"),
                                          cmd);
                return STATUS_INVALID_ARGS;
            }
            case L'f': {
                opts.place |= ENV_FUNCTION;
                break;
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
            case L'n': {
                opts.nchars = fish_wcstoi(w.woptarg);
                if (errno) {
                    if (errno == ERANGE) {
                        streams.err.append_format(_(L"%ls: Argument '%ls' is out of range\n"), cmd,
                                                  w.woptarg);
                        builtin_print_error_trailer(parser, streams.err, cmd);
                        return STATUS_INVALID_ARGS;
                    }

                    streams.err.append_format(BUILTIN_ERR_NOT_NUMBER, cmd, w.woptarg);
                    builtin_print_error_trailer(parser, streams.err, cmd);
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
            case L't': {
                opts.tokenize = true;
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
            }
        }
    }

    *optind = w.woptind;
    return STATUS_CMD_OK;
}

/// Read from the tty. This is only valid when the stream is stdin and it is attached to a tty and
/// we weren't asked to split on null characters.
static int read_interactive(parser_t &parser, wcstring &buff, int nchars, bool shell, bool silent,
                            const wchar_t *prompt, const wchar_t *right_prompt,
                            const wchar_t *commandline, int in) {
    int exit_res = STATUS_CMD_OK;

    // Construct a configuration.
    reader_config_t conf;
    conf.complete_ok = shell;
    conf.highlight_ok = shell;
    conf.syntax_check_ok = shell;

    // No autosuggestions or abbreviations in builtin_read.
    conf.autosuggest_ok = false;
    conf.expand_abbrev_ok = false;

    conf.exit_on_interrupt = true;
    conf.in_silent_mode = silent;

    conf.left_prompt_cmd = prompt;
    conf.right_prompt_cmd = right_prompt;
    conf.event = L"fish_read";

    conf.in = in;

    // Keep in-memory history only.
    reader_push(parser, wcstring{}, std::move(conf));

    commandline_set_buffer(commandline, std::wcslen(commandline));
    scoped_push<bool> interactive{&parser.libdata().is_interactive, true};

    auto mline = reader_readline(nchars);
    interactive.restore();
    if (mline) {
        buff = mline.acquire();
        if (nchars > 0 && static_cast<size_t>(nchars) < buff.size()) {
            // Line may be longer than nchars if a keybinding used `commandline -i`
            // note: we're deliberately throwing away the tail of the commandline.
            // It shouldn't be unread because it was produced with `commandline -i`,
            // not typed.
            buff.resize(nchars);
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
static int read_in_chunks(int fd, wcstring &buff, bool split_null, bool do_seek) {
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
            if (do_seek && lseek(fd, bytes_consumed - bytes_read + 1, SEEK_CUR) == -1) {
                wperror(L"lseek");
                return STATUS_CMD_ERROR;
            }
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
                res = static_cast<unsigned char>(b);
                finished = true;
            } else {
                size_t sz = std::mbrtowc(&res, &b, 1, &state);
                if (sz == static_cast<size_t>(-1)) {
                    std::memset(&state, 0, sizeof(state));
                } else if (sz != static_cast<size_t>(-2)) {
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
        if (nchars > 0 && static_cast<size_t>(nchars) <= buff.size()) {
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
        streams.err.append_format(_(L"%ls: Options %ls and %ls cannot be used together\n"), cmd,
                                  L"-p", L"-P");
        builtin_print_error_trailer(parser, streams.err, cmd);
        return STATUS_INVALID_ARGS;
    }

    if (opts.have_delimiter && opts.one_line) {
        streams.err.append_format(_(L"%ls: Options %ls and %ls cannot be used together\n"), cmd,
                                  L"--delimiter", L"--line");
        return STATUS_INVALID_ARGS;
    }
    if (opts.one_line && opts.split_null) {
        streams.err.append_format(_(L"%ls: Options %ls and %ls cannot be used together\n"), cmd,
                                  L"-z", L"--line");
        return STATUS_INVALID_ARGS;
    }

    if (opts.prompt_str) {
        opts.prompt_cmd = L"echo " + escape_string(opts.prompt_str);
        opts.prompt = opts.prompt_cmd.c_str();
    } else if (!opts.prompt) {
        opts.prompt = DEFAULT_READ_PROMPT;
    }

    if ((opts.place & ENV_UNEXPORT) && (opts.place & ENV_EXPORT)) {
        streams.err.append_format(BUILTIN_ERR_EXPUNEXP, cmd);
        builtin_print_error_trailer(parser, streams.err, cmd);
        return STATUS_INVALID_ARGS;
    }

    if ((opts.place & ENV_LOCAL ? 1 : 0) + (opts.place & ENV_FUNCTION ? 1 : 0) +
            (opts.place & ENV_GLOBAL ? 1 : 0) + (opts.place & ENV_UNIVERSAL ? 1 : 0) >
        1) {
        streams.err.append_format(BUILTIN_ERR_GLOCAL, cmd);
        builtin_print_error_trailer(parser, streams.err, cmd);
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

    if (opts.tokenize && opts.have_delimiter) {
        streams.err.append_format(BUILTIN_ERR_COMBO2_EXCLUSIVE, cmd, L"--delimiter", L"--tokenize");
        return STATUS_INVALID_ARGS;
    }

    if (opts.tokenize && opts.one_line) {
        streams.err.append_format(BUILTIN_ERR_COMBO2_EXCLUSIVE, cmd, L"--line", L"--tokenize");
        return STATUS_INVALID_ARGS;
    }

    // Verify all variable names.
    for (int i = 0; i < argc; i++) {
        if (!valid_var_name(argv[i])) {
            streams.err.append_format(BUILTIN_ERR_VARNAME, cmd, argv[i]);
            builtin_print_error_trailer(parser, streams.err, cmd);
            return STATUS_INVALID_ARGS;
        }
        if (env_var_t::flags_for(argv[i]) & env_var_t::flag_read_only) {
            streams.err.append_format(_(L"%ls: %ls: cannot overwrite read-only variable"), cmd,
                                      argv[i]);
            builtin_print_error_trailer(parser, streams.err, cmd);
            return STATUS_INVALID_ARGS;
        }
    }

    return STATUS_CMD_OK;
}

/// The read builtin. Reads from stdin and stores the values in environment variables.
maybe_t<int> builtin_read(parser_t &parser, io_streams_t &streams, const wchar_t **argv) {
    const wchar_t *cmd = argv[0];
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
        builtin_print_help(parser, streams, cmd);
        return STATUS_CMD_OK;
    }

    retval = validate_read_args(cmd, opts, argc, argv, parser, streams);
    if (retval != STATUS_CMD_OK) return retval;

    // stdin may have been explicitly closed
    if (streams.stdin_fd < 0) {
        streams.err.append_format(_(L"%ls: stdin is closed\n"), cmd);
        return STATUS_CMD_ERROR;
    }

    if (opts.one_line) {
        // --line is the same as read -d \n repeated N times
        opts.have_delimiter = true;
        opts.delimiter = L"\n";
        opts.split_null = false;
        opts.shell = false;
    }

    const wchar_t *const *var_ptr = argv;
    auto vars_left = [&]() { return argv + argc - var_ptr; };
    auto clear_remaining_vars = [&]() {
        while (vars_left()) {
            parser.vars().set_empty(*var_ptr, opts.place);
            ++var_ptr;
        }
    };

    // Normally, we either consume a line of input or all available input. But if we are reading a
    // line at a time, we need a middle ground where we only consume as many lines as we need to
    // fill the given vars.
    do {
        buff.clear();

        int stream_stdin_is_a_tty = isatty(streams.stdin_fd);
        if (stream_stdin_is_a_tty && !opts.split_null) {
            // Read interactively using reader_readline(). This does not support splitting on null.
            exit_res =
                read_interactive(parser, buff, opts.nchars, opts.shell, opts.silent, opts.prompt,
                                 opts.right_prompt, opts.commandline, streams.stdin_fd);
        } else if (!opts.nchars && !stream_stdin_is_a_tty &&
                   // "one_line" is implemented as reading n-times to a new line,
                   // if we're chunking we could get multiple lines so we would have to advance
                   // more than 1 per run through the loop. Let's skip that for now.
                   !opts.one_line &&
                   (streams.stdin_is_directly_redirected ||
                    lseek(streams.stdin_fd, 0, SEEK_CUR) != -1)) {
            // We read in chunks when we either can seek (so we put the bytes back),
            // or we have the bytes to ourselves (because it's directly redirected).
            //
            // Note we skip seeking back even if we're directly redirected to a seekable stream,
            // under the assumption that the stream will be closed soon anyway.
            // You don't rewind VHS tapes before throwing them in the trash.
            // TODO: Do this when nchars is set by seeking back.
            exit_res = read_in_chunks(streams.stdin_fd, buff, opts.split_null,
                                      !streams.stdin_is_directly_redirected);
        } else {
            exit_res =
                read_one_char_at_a_time(streams.stdin_fd, buff, opts.nchars, opts.split_null);
        }

        if (exit_res != STATUS_CMD_OK) {
            clear_remaining_vars();
            return exit_res;
        }

        if (opts.to_stdout) {
            streams.out.append(buff);
            return exit_res;
        }

        if (opts.tokenize) {
            auto tok = new_tokenizer(buff.c_str(), TOK_ACCEPT_UNFINISHED);
            if (opts.array) {
                // Array mode: assign each token as a separate element of the sole var.
                std::vector<wcstring> tokens;
                while (auto t = tok->next()) {
                    auto text = *tok->text_of(*t);
                    if (auto out = unescape_string(text, UNESCAPE_DEFAULT)) {
                        tokens.push_back(*out);
                    } else {
                        tokens.push_back(text);
                    }
                }

                parser.set_var_and_fire(*var_ptr++, opts.place, std::move(tokens));
            } else {
                std::unique_ptr<tok_t> t;
                while ((vars_left() - 1 > 0) && (t = tok->next())) {
                    auto text = *tok->text_of(*t);
                    if (auto out = unescape_string(text, UNESCAPE_DEFAULT)) {
                        parser.set_var_and_fire(*var_ptr++, opts.place, *out);
                    } else {
                        parser.set_var_and_fire(*var_ptr++, opts.place, text);
                    }
                }

                // If we still have tokens, set the last variable to them.
                if ((t = tok->next())) {
                    wcstring rest = wcstring(buff, t->offset);
                    parser.set_var_and_fire(*var_ptr++, opts.place, std::move(rest));
                }
            }
            // The rest of the loop is other split-modes, we don't care about those.
            continue;
        }

        if (!opts.have_delimiter) {
            auto ifs = parser.vars().get_unless_empty(L"IFS");
            if (ifs) opts.delimiter = ifs->as_string();
        }

        if (opts.delimiter.empty()) {
            // Every character is a separate token with one wrinkle involving non-array mode where
            // the final var gets the remaining characters as a single string.
            size_t x = std::max(static_cast<size_t>(1), buff.size());
            size_t n_splits =
                (opts.array || static_cast<size_t>(vars_left()) > x) ? x : vars_left();
            std::vector<wcstring> chars;
            chars.reserve(n_splits);

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
                parser.set_var_and_fire(*var_ptr++, opts.place, chars);
            } else {
                // Not array mode: assign each char to a separate var with the remainder being
                // assigned to the last var.
                for (const auto &c : chars) {
                    parser.set_var_and_fire(*var_ptr++, opts.place, c);
                }
            }
        } else if (opts.array) {
            // The user has requested the input be split into a sequence of tokens and all the
            // tokens assigned to a single var. How we do the tokenizing depends on whether the user
            // specified the delimiter string or we're using IFS.
            if (!opts.have_delimiter) {
                // We're using IFS, so tokenize the buffer using each IFS char. This is for backward
                // compatibility with old versions of fish.
                std::vector<wcstring> tokens = split_string_tok(buff, opts.delimiter);
                parser.set_var_and_fire(*var_ptr++, opts.place, std::move(tokens));
            } else {
                // We're using a delimiter provided by the user so use the `string split` behavior.
                std::vector<wcstring> splits;
                split_about(buff.begin(), buff.end(), opts.delimiter.begin(), opts.delimiter.end(),
                            &splits);

                parser.set_var_and_fire(*var_ptr++, opts.place, splits);
            }
        } else {
            // Not array mode. Split the input into tokens and assign each to the vars in sequence.
            if (!opts.have_delimiter) {
                // We're using IFS, so tokenize the buffer using each IFS char. This is for backward
                // compatibility with old versions of fish.
                // Note the final variable gets any remaining text.
                std::vector<wcstring> var_vals =
                    split_string_tok(buff, opts.delimiter, vars_left());
                size_t val_idx = 0;
                while (vars_left()) {
                    wcstring val;
                    if (val_idx < var_vals.size()) {
                        val = std::move(var_vals.at(val_idx++));
                    }
                    parser.set_var_and_fire(*var_ptr++, opts.place, std::move(val));
                }
            } else {
                // We're using a delimiter provided by the user so use the `string split` behavior.
                std::vector<wcstring> splits;
                // We're making at most argc - 1 splits so the last variable
                // is set to the remaining string.
                split_about(buff.begin(), buff.end(), opts.delimiter.begin(), opts.delimiter.end(),
                            &splits, argc - 1);
                assert(splits.size() <= static_cast<size_t>(vars_left()));
                for (const auto &split : splits) {
                    parser.set_var_and_fire(*var_ptr++, opts.place, split);
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
