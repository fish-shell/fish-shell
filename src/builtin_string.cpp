// Implementation of the string builtin.
#include "config.h"  // IWYU pragma: keep

#define PCRE2_CODE_UNIT_WIDTH WCHAR_T_BITS
#ifdef _WIN32
#define PCRE2_STATIC
#endif
#include <algorithm>
#include <cerrno>
#include <climits>
#include <cstdarg>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cwchar>
#include <cwctype>
#include <iterator>
#include <memory>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "builtin.h"
#include "common.h"
#include "fallback.h"  // IWYU pragma: keep
#include "future_feature_flags.h"
#include "io.h"
#include "parse_util.h"
#include "pcre2.h"
#include "wcstringutil.h"
#include "wgetopt.h"
#include "wildcard.h"
#include "wutil.h"  // IWYU pragma: keep

class parser_t;

// How many bytes we read() at once.
// Bash uses 128 here, so we do too (see READ_CHUNK_SIZE).
// This should be about the size of a line.
#define STRING_CHUNK_SIZE 128

static void string_error(io_streams_t &streams, const wchar_t *fmt, ...) {
    streams.err.append(L"string ");
    va_list va;
    va_start(va, fmt);
    streams.err.append_formatv(fmt, va);
    va_end(va);
}

static void string_unknown_option(parser_t &parser, io_streams_t &streams, const wchar_t *subcmd,
                                  const wchar_t *opt) {
    string_error(streams, BUILTIN_ERR_UNKNOWN, subcmd, opt);
    builtin_print_error_trailer(parser, streams.err, L"string");
}

// We read from stdin if we are the second or later process in a pipeline.
static bool string_args_from_stdin(const io_streams_t &streams) {
    return streams.stdin_is_directly_redirected;
}

static const wchar_t *string_get_arg_argv(int *argidx, const wchar_t *const *argv) {
    return argv && argv[*argidx] ? argv[(*argidx)++] : nullptr;
}

// A helper type for extracting arguments from either argv or stdin.
namespace {
class arg_iterator_t {
    // The list of arguments passed to the string builtin.
    const wchar_t *const *argv_;
    // If using argv, index of the next argument to return.
    int argidx_;
    // If not using argv, a string to store bytes that have been read but not yet returned.
    std::string buffer_;
    // If set, when reading from a stream, split on newlines.
    const bool split_;
    // Backing storage for the next() string.
    wcstring storage_;
    const io_streams_t &streams_;

    /// Reads the next argument from stdin, returning true if an argument was produced and false if
    /// not. On true, the string is stored in storage_.
    bool get_arg_stdin() {
        assert(string_args_from_stdin(streams_) && "should not be reading from stdin");
        // Read in chunks from fd until buffer has a line (or the end if split_ is unset).
        size_t pos;
        while (!split_ || (pos = buffer_.find('\n')) == std::string::npos) {
            char buf[STRING_CHUNK_SIZE];
            long n = read_blocked(streams_.stdin_fd, buf, STRING_CHUNK_SIZE);
            if (n == 0) {
                // If we still have buffer contents, flush them,
                // in case there was no trailing sep.
                if (buffer_.empty()) return false;
                storage_ = str2wcstring(buffer_);
                buffer_.clear();
                return true;
            }
            if (n == -1) {
                // Some error happened. We can't do anything about it,
                // so ignore it.
                // (read_blocked already retries for EAGAIN and EINTR)
                storage_ = str2wcstring(buffer_);
                buffer_.clear();
                return false;
            }
            buffer_.append(buf, n);
        }

        // Split the buffer on the sep and return the first part.
        storage_ = str2wcstring(buffer_, pos);
        buffer_.erase(0, pos + 1);
        return true;
    }

   public:
    arg_iterator_t(const wchar_t *const *argv, int argidx, const io_streams_t &streams,
                   bool split = true)
        : argv_(argv), argidx_(argidx), split_(split), streams_(streams) {}

    const wcstring *nextstr() {
        if (string_args_from_stdin(streams_)) {
            return get_arg_stdin() ? &storage_ : nullptr;
        }
        if (auto arg = string_get_arg_argv(&argidx_, argv_)) {
            storage_ = arg;
            return &storage_;
        } else {
            return nullptr;
        }
    }
};
}  // namespace

// This is used by the string subcommands to communicate with the option parser which flags are
// valid and get the result of parsing the command for flags.
using options_t = struct options_t {  //!OCLINT(too many fields)
    bool all_valid = false;
    bool char_to_pad_valid = false;
    bool chars_to_trim_valid = false;
    bool count_valid = false;
    bool entire_valid = false;
    bool filter_valid = false;
    bool ignore_case_valid = false;
    bool index_valid = false;
    bool invert_valid = false;
    bool left_valid = false;
    bool length_valid = false;
    bool max_valid = false;
    bool no_newline_valid = false;
    bool no_quoted_valid = false;
    bool quiet_valid = false;
    bool regex_valid = false;
    bool right_valid = false;
    bool start_valid = false;
    bool end_valid = false;
    bool style_valid = false;
    bool no_empty_valid = false;
    bool no_trim_newlines_valid = false;
    bool fields_valid = false;
    bool allow_empty_valid = false;
    bool width_valid = false;

    bool all = false;
    bool entire = false;
    bool filter = false;
    bool ignore_case = false;
    bool index = false;
    bool invert_match = false;
    bool left = false;
    bool no_newline = false;
    bool no_quoted = false;
    bool quiet = false;
    bool regex = false;
    bool right = false;
    bool no_empty = false;
    bool no_trim_newlines = false;
    bool allow_empty = false;

    long count = 0;
    long length = 0;
    long max = 0;
    long start = 0;
    long end = 0;
    size_t width = 0;

    wchar_t char_to_pad = L' ';

    std::vector<int> fields;

    const wchar_t *chars_to_trim = L" \f\n\r\t\v";
    const wchar_t *arg1 = nullptr;
    const wchar_t *arg2 = nullptr;

    escape_string_style_t escape_style = STRING_STYLE_SCRIPT;
};

/// This handles the `--style=xxx` flag.
static int handle_flag_1(wchar_t **argv, parser_t &parser, io_streams_t &streams,
                         const wgetopter_t &w, options_t *opts) {
    const wchar_t *cmd = argv[0];

    if (opts->style_valid) {
        if (std::wcscmp(w.woptarg, L"script") == 0) {
            opts->escape_style = STRING_STYLE_SCRIPT;
        } else if (std::wcscmp(w.woptarg, L"url") == 0) {
            opts->escape_style = STRING_STYLE_URL;
        } else if (std::wcscmp(w.woptarg, L"var") == 0) {
            opts->escape_style = STRING_STYLE_VAR;
        } else if (std::wcscmp(w.woptarg, L"regex") == 0) {
            opts->escape_style = STRING_STYLE_REGEX;
        } else {
            string_error(streams, _(L"%ls: Invalid escape style '%ls'\n"), cmd, w.woptarg);
            return STATUS_INVALID_ARGS;
        }
        return STATUS_CMD_OK;
    }

    string_unknown_option(parser, streams, cmd, argv[w.woptind - 1]);
    return STATUS_INVALID_ARGS;
}

static int handle_flag_N(wchar_t **argv, parser_t &parser, io_streams_t &streams,
                         const wgetopter_t &w, options_t *opts) {
    if (opts->no_newline_valid) {
        opts->no_newline = true;
        return STATUS_CMD_OK;
    } else if (opts->no_trim_newlines_valid) {
        opts->no_trim_newlines = true;
        return STATUS_CMD_OK;
    }
    string_unknown_option(parser, streams, argv[0], argv[w.woptind - 1]);
    return STATUS_INVALID_ARGS;
}

static int handle_flag_a(wchar_t **argv, parser_t &parser, io_streams_t &streams,
                         const wgetopter_t &w, options_t *opts) {
    if (opts->all_valid) {
        opts->all = true;
        return STATUS_CMD_OK;
    } else if (opts->allow_empty_valid) {
        opts->allow_empty = true;
        return STATUS_CMD_OK;
    }
    string_unknown_option(parser, streams, argv[0], argv[w.woptind - 1]);
    return STATUS_INVALID_ARGS;
}

static int handle_flag_c(wchar_t **argv, parser_t &parser, io_streams_t &streams,
                         const wgetopter_t &w, options_t *opts) {
    if (opts->chars_to_trim_valid) {
        opts->chars_to_trim = w.woptarg;
        return STATUS_CMD_OK;
    } else if (opts->char_to_pad_valid) {
        if (wcslen(w.woptarg) != 1) {
            string_error(streams, _(L"%ls: Padding should be a character '%ls'\n"), argv[0],
                         w.woptarg);
            return STATUS_INVALID_ARGS;
        }
        opts->char_to_pad = w.woptarg[0];
        return STATUS_CMD_OK;
    }
    string_unknown_option(parser, streams, argv[0], argv[w.woptind - 1]);
    return STATUS_INVALID_ARGS;
}

static int handle_flag_e(wchar_t **argv, parser_t &parser, io_streams_t &streams,
                         const wgetopter_t &w, options_t *opts) {
    if (opts->end_valid) {
        opts->end = fish_wcstol(w.woptarg);
        if (opts->end == 0 || opts->end == LONG_MIN || errno == ERANGE) {
            string_error(streams, _(L"%ls: Invalid end value '%ls'\n"), argv[0], w.woptarg);
            return STATUS_INVALID_ARGS;
        } else if (errno) {
            string_error(streams, BUILTIN_ERR_NOT_NUMBER, argv[0], w.woptarg);
            return STATUS_INVALID_ARGS;
        }
        return STATUS_CMD_OK;
    } else if (opts->entire_valid) {
        opts->entire = true;
        return STATUS_CMD_OK;
    }
    string_unknown_option(parser, streams, argv[0], argv[w.woptind - 1]);
    return STATUS_INVALID_ARGS;
}

static int handle_flag_f(wchar_t **argv, parser_t &parser, io_streams_t &streams,
                         const wgetopter_t &w, options_t *opts) {
    if (opts->filter_valid) {
        opts->filter = true;
        return STATUS_CMD_OK;
    } else if (opts->fields_valid) {
        for (const wcstring &s : split_string(w.woptarg, L',')) {
            wcstring_list_t range = split_string(s, L'-');
            if (range.size() == 2) {
                int begin = fish_wcstoi(range.at(0).c_str());
                if (begin <= 0 || errno == ERANGE) {
                    string_error(streams, _(L"%ls: Invalid range value for field '%ls'\n"), argv[0],
                                 w.woptarg);
                    return STATUS_INVALID_ARGS;
                } else if (errno) {
                    string_error(streams, BUILTIN_ERR_NOT_NUMBER, argv[0], w.woptarg);
                    return STATUS_INVALID_ARGS;
                }
                int end = fish_wcstoi(range.at(1).c_str());
                if (end <= 0 || errno == ERANGE) {
                    string_error(streams, _(L"%ls: Invalid range value for field '%ls'\n"), argv[0],
                                 w.woptarg);
                    return STATUS_INVALID_ARGS;
                } else if (errno) {
                    string_error(streams, BUILTIN_ERR_NOT_NUMBER, argv[0], w.woptarg);
                    return STATUS_INVALID_ARGS;
                }
                if (begin <= end) {
                    for (int i = begin; i <= end; i++) {
                        opts->fields.push_back(i);
                    }
                } else {
                    for (int i = begin; i >= end; i--) {
                        opts->fields.push_back(i);
                    }
                }
            } else {
                int field = fish_wcstoi(s.c_str());
                if (field <= 0 || errno == ERANGE) {
                    string_error(streams, _(L"%ls: Invalid fields value '%ls'\n"), argv[0],
                                 w.woptarg);
                    return STATUS_INVALID_ARGS;
                } else if (errno) {
                    string_error(streams, BUILTIN_ERR_NOT_NUMBER, argv[0], w.woptarg);
                    return STATUS_INVALID_ARGS;
                }
                opts->fields.push_back(field);
            }
        }
        return STATUS_CMD_OK;
    }
    string_unknown_option(parser, streams, argv[0], argv[w.woptind - 1]);
    return STATUS_INVALID_ARGS;
}

static int handle_flag_i(wchar_t **argv, parser_t &parser, io_streams_t &streams,
                         const wgetopter_t &w, options_t *opts) {
    if (opts->ignore_case_valid) {
        opts->ignore_case = true;
        return STATUS_CMD_OK;
    } else if (opts->index_valid) {
        opts->index = true;
        return STATUS_CMD_OK;
    }
    string_unknown_option(parser, streams, argv[0], argv[w.woptind - 1]);
    return STATUS_INVALID_ARGS;
}

static int handle_flag_l(wchar_t **argv, parser_t &parser, io_streams_t &streams,
                         const wgetopter_t &w, options_t *opts) {
    if (opts->length_valid) {
        opts->length = fish_wcstol(w.woptarg);
        if (opts->length < 0 || opts->length == LONG_MIN || errno == ERANGE) {
            string_error(streams, _(L"%ls: Invalid length value '%ls'\n"), argv[0], w.woptarg);
            return STATUS_INVALID_ARGS;
        } else if (errno) {
            string_error(streams, BUILTIN_ERR_NOT_NUMBER, argv[0], w.woptarg);
            return STATUS_INVALID_ARGS;
        }
        return STATUS_CMD_OK;
    } else if (opts->left_valid) {
        opts->left = true;
        return STATUS_CMD_OK;
    }
    string_unknown_option(parser, streams, argv[0], argv[w.woptind - 1]);
    return STATUS_INVALID_ARGS;
}

static int handle_flag_m(wchar_t **argv, parser_t &parser, io_streams_t &streams,
                         const wgetopter_t &w, options_t *opts) {
    if (opts->max_valid) {
        opts->max = fish_wcstol(w.woptarg);
        if (opts->max < 0 || errno == ERANGE) {
            string_error(streams, _(L"%ls: Invalid max value '%ls'\n"), argv[0], w.woptarg);
            return STATUS_INVALID_ARGS;
        } else if (errno) {
            string_error(streams, BUILTIN_ERR_NOT_NUMBER, argv[0], w.woptarg);
            return STATUS_INVALID_ARGS;
        }
        return STATUS_CMD_OK;
    }
    string_unknown_option(parser, streams, argv[0], argv[w.woptind - 1]);
    return STATUS_INVALID_ARGS;
}

static int handle_flag_n(wchar_t **argv, parser_t &parser, io_streams_t &streams,
                         const wgetopter_t &w, options_t *opts) {
    if (opts->count_valid) {
        opts->count = fish_wcstol(w.woptarg);
        if (opts->count < 0 || errno == ERANGE) {
            string_error(streams, _(L"%ls: Invalid count value '%ls'\n"), argv[0], w.woptarg);
            return STATUS_INVALID_ARGS;
        } else if (errno) {
            string_error(streams, BUILTIN_ERR_NOT_NUMBER, argv[0], w.woptarg);
            return STATUS_INVALID_ARGS;
        }
        return STATUS_CMD_OK;
    } else if (opts->index_valid) {
        opts->index = true;
        return STATUS_CMD_OK;
    } else if (opts->no_quoted_valid) {
        opts->no_quoted = true;
        return STATUS_CMD_OK;
    } else if (opts->no_empty_valid) {
        opts->no_empty = true;
        return STATUS_CMD_OK;
    }
    string_unknown_option(parser, streams, argv[0], argv[w.woptind - 1]);
    return STATUS_INVALID_ARGS;
}

static int handle_flag_q(wchar_t **argv, parser_t &parser, io_streams_t &streams,
                         const wgetopter_t &w, options_t *opts) {
    if (opts->quiet_valid) {
        opts->quiet = true;
        return STATUS_CMD_OK;
    }
    string_unknown_option(parser, streams, argv[0], argv[w.woptind - 1]);
    return STATUS_INVALID_ARGS;
}

static int handle_flag_r(wchar_t **argv, parser_t &parser, io_streams_t &streams,
                         const wgetopter_t &w, options_t *opts) {
    if (opts->regex_valid) {
        opts->regex = true;
        return STATUS_CMD_OK;
    } else if (opts->right_valid) {
        opts->right = true;
        return STATUS_CMD_OK;
    }
    string_unknown_option(parser, streams, argv[0], argv[w.woptind - 1]);
    return STATUS_INVALID_ARGS;
}

static int handle_flag_s(wchar_t **argv, parser_t &parser, io_streams_t &streams,
                         const wgetopter_t &w, options_t *opts) {
    if (opts->start_valid) {
        opts->start = fish_wcstol(w.woptarg);
        if (opts->start == 0 || opts->start == LONG_MIN || errno == ERANGE) {
            string_error(streams, _(L"%ls: Invalid start value '%ls'\n"), argv[0], w.woptarg);
            return STATUS_INVALID_ARGS;
        } else if (errno) {
            string_error(streams, BUILTIN_ERR_NOT_NUMBER, argv[0], w.woptarg);
            return STATUS_INVALID_ARGS;
        }
        return STATUS_CMD_OK;
    }
    string_unknown_option(parser, streams, argv[0], argv[w.woptind - 1]);
    return STATUS_INVALID_ARGS;
}

static int handle_flag_v(wchar_t **argv, parser_t &parser, io_streams_t &streams,
                         const wgetopter_t &w, options_t *opts) {
    if (opts->invert_valid) {
        opts->invert_match = true;
        return STATUS_CMD_OK;
    }
    string_unknown_option(parser, streams, argv[0], argv[w.woptind - 1]);
    return STATUS_INVALID_ARGS;
}

static int handle_flag_w(wchar_t **argv, parser_t &parser, io_streams_t &streams,
                         const wgetopter_t &w, options_t *opts) {
    long width = 0;
    if (opts->width_valid) {
        width = fish_wcstol(w.woptarg);
        if (width < 0) {
            string_error(streams, _(L"%ls: Invalid width value '%ls'\n"), argv[0], w.woptarg);
            return STATUS_INVALID_ARGS;
        } else if (errno) {
            string_error(streams, BUILTIN_ERR_NOT_NUMBER, argv[0], w.woptarg);
            return STATUS_INVALID_ARGS;
        }
        opts->width = static_cast<size_t>(width);
        return STATUS_CMD_OK;
    }
    string_unknown_option(parser, streams, argv[0], argv[w.woptind - 1]);
    return STATUS_INVALID_ARGS;
}

/// This constructs the wgetopt() short options string based on which arguments are valid for the
/// subcommand. We have to do this because many short flags have multiple meanings and may or may
/// not require an argument depending on the meaning.
static wcstring construct_short_opts(options_t *opts) {  //!OCLINT(high npath complexity)
    wcstring short_opts(L":");
    if (opts->all_valid) short_opts.append(L"a");
    if (opts->char_to_pad_valid) short_opts.append(L"c:");
    if (opts->chars_to_trim_valid) short_opts.append(L"c:");
    if (opts->count_valid) short_opts.append(L"n:");
    if (opts->entire_valid) short_opts.append(L"e");
    if (opts->filter_valid) short_opts.append(L"f");
    if (opts->ignore_case_valid) short_opts.append(L"i");
    if (opts->index_valid) short_opts.append(L"n");
    if (opts->invert_valid) short_opts.append(L"v");
    if (opts->left_valid) short_opts.append(L"l");
    if (opts->length_valid) short_opts.append(L"l:");
    if (opts->max_valid) short_opts.append(L"m:");
    if (opts->no_newline_valid) short_opts.append(L"N");
    if (opts->no_quoted_valid) short_opts.append(L"n");
    if (opts->quiet_valid) short_opts.append(L"q");
    if (opts->regex_valid) short_opts.append(L"r");
    if (opts->right_valid) short_opts.append(L"r");
    if (opts->start_valid) short_opts.append(L"s:");
    if (opts->end_valid) short_opts.append(L"e:");
    if (opts->no_empty_valid) short_opts.append(L"n");
    if (opts->no_trim_newlines_valid) short_opts.append(L"N");
    if (opts->fields_valid) short_opts.append(L"f:");
    if (opts->allow_empty_valid) short_opts.append(L"a");
    if (opts->width_valid) short_opts.append(L"w:");
    return short_opts;
}

// Note that several long flags share the same short flag. That is okay. The caller is expected
// to indicate that a max of one of the long flags sharing a short flag is valid.
// Remember: adjust share/completions/string.fish when `string` options change
static const struct woption long_options[] = {{L"all", no_argument, nullptr, 'a'},
                                              {L"chars", required_argument, nullptr, 'c'},
                                              {L"count", required_argument, nullptr, 'n'},
                                              {L"entire", no_argument, nullptr, 'e'},
                                              {L"end", required_argument, nullptr, 'e'},
                                              {L"filter", no_argument, nullptr, 'f'},
                                              {L"ignore-case", no_argument, nullptr, 'i'},
                                              {L"index", no_argument, nullptr, 'n'},
                                              {L"invert", no_argument, nullptr, 'v'},
                                              {L"left", no_argument, nullptr, 'l'},
                                              {L"length", required_argument, nullptr, 'l'},
                                              {L"max", required_argument, nullptr, 'm'},
                                              {L"no-empty", no_argument, nullptr, 'n'},
                                              {L"no-newline", no_argument, nullptr, 'N'},
                                              {L"no-quoted", no_argument, nullptr, 'n'},
                                              {L"quiet", no_argument, nullptr, 'q'},
                                              {L"regex", no_argument, nullptr, 'r'},
                                              {L"right", no_argument, nullptr, 'r'},
                                              {L"start", required_argument, nullptr, 's'},
                                              {L"style", required_argument, nullptr, 1},
                                              {L"no-trim-newlines", no_argument, nullptr, 'N'},
                                              {L"fields", required_argument, nullptr, 'f'},
                                              {L"allow-empty", no_argument, nullptr, 'a'},
                                              {L"width", required_argument, nullptr, 'w'},
                                              {nullptr, 0, nullptr, 0}};

static const std::unordered_map<char, decltype(*handle_flag_N)> flag_to_function = {
    {'N', handle_flag_N}, {'a', handle_flag_a}, {'c', handle_flag_c}, {'e', handle_flag_e},
    {'f', handle_flag_f}, {'i', handle_flag_i}, {'l', handle_flag_l}, {'m', handle_flag_m},
    {'n', handle_flag_n}, {'q', handle_flag_q}, {'r', handle_flag_r}, {'s', handle_flag_s},
    {'v', handle_flag_v}, {'w', handle_flag_w}, {1, handle_flag_1}};

/// Parse the arguments for flags recognized by a specific string subcommand.
static int parse_opts(options_t *opts, int *optind, int n_req_args, int argc, wchar_t **argv,
                      parser_t &parser, io_streams_t &streams) {
    const wchar_t *cmd = argv[0];
    wcstring short_opts = construct_short_opts(opts);
    const wchar_t *short_options = short_opts.c_str();
    int opt;
    wgetopter_t w;
    while ((opt = w.wgetopt_long(argc, argv, short_options, long_options, nullptr)) != -1) {
        auto fn = flag_to_function.find(opt);
        if (fn != flag_to_function.end()) {
            int retval = fn->second(argv, parser, streams, w, opts);
            if (retval != STATUS_CMD_OK) return retval;
        } else if (opt == ':') {
            streams.err.append(L"string ");  // clone of string_error
            builtin_missing_argument(parser, streams, cmd, argv[w.woptind - 1],
                                     false /* print_hints */);
            return STATUS_INVALID_ARGS;
        } else if (opt == '?') {
            string_unknown_option(parser, streams, cmd, argv[w.woptind - 1]);
            return STATUS_INVALID_ARGS;
        } else {
            DIE("unexpected retval from wgetopt_long");
        }
    }

    *optind = w.woptind;

    // If the caller requires one or two mandatory args deal with that here.
    if (n_req_args) {
        opts->arg1 = string_get_arg_argv(optind, argv);
        if (!opts->arg1 && n_req_args == 1) {
            string_error(streams, BUILTIN_ERR_ARG_COUNT0, cmd);
            return STATUS_INVALID_ARGS;
        }
    }
    if (n_req_args > 1) {
        opts->arg2 = string_get_arg_argv(optind, argv);
        if (!opts->arg2) {
            string_error(streams, BUILTIN_ERR_MIN_ARG_COUNT1, cmd, n_req_args,
                         !!opts->arg2 + !!opts->arg1);
            return STATUS_INVALID_ARGS;
        }
    }

    // At this point we should not have optional args and be reading args from stdin.
    if (string_args_from_stdin(streams) && argc > *optind) {
        string_error(streams, BUILTIN_ERR_TOO_MANY_ARGUMENTS, cmd);
        return STATUS_INVALID_ARGS;
    }

    return STATUS_CMD_OK;
}

static int string_escape(parser_t &parser, io_streams_t &streams, int argc, wchar_t **argv) {
    options_t opts;
    opts.no_quoted_valid = true;
    opts.style_valid = true;
    int optind;
    int retval = parse_opts(&opts, &optind, 0, argc, argv, parser, streams);
    if (retval != STATUS_CMD_OK) return retval;

    // Currently, only the script style supports options.
    // Ignore them for other styles for now.
    escape_flags_t flags = 0;
    if (opts.escape_style == STRING_STYLE_SCRIPT) {
        flags = ESCAPE_ALL;
        if (opts.no_quoted) flags |= ESCAPE_NO_QUOTED;
    }

    int nesc = 0;
    arg_iterator_t aiter(argv, optind, streams);
    while (const wcstring *arg = aiter.nextstr()) {
        streams.out.append(escape_string(*arg, flags, opts.escape_style));
        streams.out.append(L'\n');
        nesc++;
    }

    return nesc > 0 ? STATUS_CMD_OK : STATUS_CMD_ERROR;
    DIE("should never reach this statement");
}

static int string_unescape(parser_t &parser, io_streams_t &streams, int argc, wchar_t **argv) {
    options_t opts;
    opts.no_quoted_valid = true;
    opts.style_valid = true;
    int optind;
    int retval = parse_opts(&opts, &optind, 0, argc, argv, parser, streams);
    int nesc = 0;
    unescape_flags_t flags = 0;

    if (retval != STATUS_CMD_OK) return retval;

    arg_iterator_t aiter(argv, optind, streams);
    while (const wcstring *arg = aiter.nextstr()) {
        wcstring result;
        if (unescape_string(*arg, &result, flags, opts.escape_style)) {
            streams.out.append(result);
            streams.out.append(L'\n');
            nesc++;
        }
    }

    return nesc > 0 ? STATUS_CMD_OK : STATUS_CMD_ERROR;
    DIE("should never reach this statement");
}

static int string_join_maybe0(parser_t &parser, io_streams_t &streams, int argc, wchar_t **argv,
                              bool is_join0) {
    options_t opts;
    opts.quiet_valid = true;
    int optind;
    int retval = parse_opts(&opts, &optind, is_join0 ? 0 : 1, argc, argv, parser, streams);
    if (retval != STATUS_CMD_OK) return retval;

    const wcstring sep = is_join0 ? wcstring(1, L'\0') : wcstring(opts.arg1);
    int nargs = 0;
    arg_iterator_t aiter(argv, optind, streams);
    while (const wcstring *arg = aiter.nextstr()) {
        if (!opts.quiet) {
            if (nargs > 0) {
                streams.out.append(sep);
            }
            streams.out.append(*arg);
        }
        nargs++;
    }
    if (nargs > 0 && !opts.quiet) {
        streams.out.push_back(is_join0 ? L'\0' : L'\n');
    }

    return nargs > 1 ? STATUS_CMD_OK : STATUS_CMD_ERROR;
}

static int string_join(parser_t &parser, io_streams_t &streams, int argc, wchar_t **argv) {
    return string_join_maybe0(parser, streams, argc, argv, false /* is_join0 */);
}

static int string_join0(parser_t &parser, io_streams_t &streams, int argc, wchar_t **argv) {
    return string_join_maybe0(parser, streams, argc, argv, true /* is_join0 */);
}

static int string_length(parser_t &parser, io_streams_t &streams, int argc, wchar_t **argv) {
    options_t opts;
    opts.quiet_valid = true;
    int optind;
    int retval = parse_opts(&opts, &optind, 0, argc, argv, parser, streams);
    if (retval != STATUS_CMD_OK) return retval;

    int nnonempty = 0;
    arg_iterator_t aiter(argv, optind, streams);
    while (const wcstring *arg = aiter.nextstr()) {
        size_t n = arg->length();
        if (n > 0) {
            nnonempty++;
        }
        if (!opts.quiet) {
            streams.out.append(to_string(n));
            streams.out.append(L'\n');
        }
    }

    return nnonempty > 0 ? STATUS_CMD_OK : STATUS_CMD_ERROR;
}

class string_matcher_t {
   protected:
    options_t opts;
    io_streams_t &streams;
    int total_matched;

   public:
    string_matcher_t(options_t opts_, io_streams_t &streams_)
        : opts(std::move(opts_)), streams(streams_), total_matched(0) {}

    virtual ~string_matcher_t() = default;
    virtual bool report_matches(const wcstring &arg) = 0;
    int match_count() const { return total_matched; }
};

class wildcard_matcher_t : public string_matcher_t {
   private:
    wcstring wcpattern;

   public:
    wildcard_matcher_t(const wchar_t * /*argv0*/, const wcstring &pattern, const options_t &opts,
                       io_streams_t &streams)
        : string_matcher_t(opts, streams), wcpattern(parse_util_unescape_wildcards(pattern)) {
        if (opts.ignore_case) {
            wcpattern = wcstolower(std::move(wcpattern));
        }
        if (opts.entire) {
            if (!wcpattern.empty()) {
                if (wcpattern.front() != ANY_STRING) wcpattern.insert(0, 1, ANY_STRING);
                if (wcpattern.back() != ANY_STRING) wcpattern.push_back(ANY_STRING);
            } else {
                // If the pattern is empty, this becomes one ANY_STRING that matches everything.
                wcpattern.push_back(ANY_STRING);
            }
        }
    }

    ~wildcard_matcher_t() override = default;

    bool report_matches(const wcstring &arg) override {
        // Note: --all is a no-op for glob matching since the pattern is always matched
        // against the entire argument.
        bool match;

        if (opts.ignore_case) {
            match = wildcard_match(wcstolower(arg), wcpattern, false);
        } else {
            match = wildcard_match(arg, wcpattern, false);
        }
        if (match ^ opts.invert_match) {
            total_matched++;

            if (!opts.quiet) {
                if (opts.index) {
                    streams.out.append_format(L"1 %lu\n", arg.length());
                } else {
                    streams.out.append(arg);
                    streams.out.append(L'\n');
                }
            }
        }
        return true;
    }
};

static wcstring pcre2_strerror(int err_code) {
    wchar_t buf[128];
    pcre2_get_error_message(err_code, reinterpret_cast<PCRE2_UCHAR *>(buf),
                            sizeof(buf) / sizeof(wchar_t));
    return buf;
}

struct compiled_regex_t {
    pcre2_code *code;
    pcre2_match_data *match;

    compiled_regex_t(const wchar_t *argv0, const wcstring &pattern, bool ignore_case,
                     io_streams_t &streams)
        : code(nullptr), match(nullptr) {
        // Disable some sequences that can lead to security problems.
        uint32_t options = PCRE2_NEVER_UTF;
#if PCRE2_CODE_UNIT_WIDTH < 32
        options |= PCRE2_NEVER_BACKSLASH_C;
#endif

        int err_code = 0;
        PCRE2_SIZE err_offset = 0;

        code = pcre2_compile(PCRE2_SPTR(pattern.c_str()), pattern.length(),
                             options | (ignore_case ? PCRE2_CASELESS : 0), &err_code, &err_offset,
                             nullptr);
        if (code == nullptr) {
            string_error(streams, _(L"%ls: Regular expression compile error: %ls\n"), argv0,
                         pcre2_strerror(err_code).c_str());
            string_error(streams, L"%ls: %ls\n", argv0, pattern.c_str());
            string_error(streams, L"%ls: %*ls\n", argv0, err_offset, L"^");
            return;
        }

        match = pcre2_match_data_create_from_pattern(code, nullptr);
        assert(match);
    }

    ~compiled_regex_t() {
        pcre2_match_data_free(match);
        pcre2_code_free(code);
    }
};

class pcre2_matcher_t : public string_matcher_t {
    const wchar_t *argv0;
    compiled_regex_t regex;

    int report_match(const wcstring &arg, int pcre2_rc) {
        // Return values: -1 = error, 0 = no match, 1 = match.
        if (pcre2_rc == PCRE2_ERROR_NOMATCH) {
            if (opts.invert_match && !opts.quiet) {
                if (opts.index) {
                    streams.out.append_format(L"1 %lu\n", arg.length());
                } else {
                    streams.out.append(arg);
                    streams.out.push_back(L'\n');
                }
            }

            return opts.invert_match ? 1 : 0;
        } else if (pcre2_rc < 0) {
            string_error(streams, _(L"%ls: Regular expression match error: %ls\n"), argv0,
                         pcre2_strerror(pcre2_rc).c_str());
            return -1;
        } else if (pcre2_rc == 0) {
            // The output vector wasn't big enough. Should not happen.
            string_error(streams, _(L"%ls: Regular expression internal error\n"), argv0);
            return -1;
        } else if (opts.invert_match) {
            return 0;
        }

        if (opts.entire && !opts.quiet) {
            streams.out.append(arg);
            streams.out.push_back(L'\n');
        }

        PCRE2_SIZE *ovector = pcre2_get_ovector_pointer(regex.match);
        for (int j = (opts.entire ? 1 : 0); j < pcre2_rc; j++) {
            PCRE2_SIZE begin = ovector[2 * j];
            PCRE2_SIZE end = ovector[2 * j + 1];

            if (begin != PCRE2_UNSET && end != PCRE2_UNSET && !opts.quiet) {
                if (opts.index) {
                    streams.out.append_format(L"%lu %lu", (begin + 1), (end - begin));
                } else if (end > begin) {
                    // May have end < begin if \K is used.
                    streams.out.append(arg.substr(begin, end - begin));
                }
                streams.out.push_back(L'\n');
            }
        }

        return opts.invert_match ? 0 : 1;
    }

   public:
    pcre2_matcher_t(const wchar_t *argv0_, const wcstring &pattern, const options_t &opts,
                    io_streams_t &streams)
        : string_matcher_t(opts, streams),
          argv0(argv0_),
          regex(argv0_, pattern, opts.ignore_case, streams) {}

    ~pcre2_matcher_t() override = default;

    bool report_matches(const wcstring &arg) override {
        // A return value of true means all is well (even if no matches were found), false indicates
        // an unrecoverable error.
        if (regex.code == nullptr) {
            // pcre2_compile() failed.
            return false;
        }

        // See pcre2demo.c for an explanation of this logic.
        PCRE2_SIZE arglen = arg.length();
        int rc = report_match(arg, pcre2_match(regex.code, PCRE2_SPTR(arg.c_str()), arglen, 0, 0,
                                               regex.match, nullptr));

        if (rc < 0 /* pcre2 error */)
            return false;
        else if (rc == 0 /* no match */)
            return true;
        else
            total_matched++;

        if (opts.invert_match) return true;

        // Report any additional matches.
        for (auto ovector = pcre2_get_ovector_pointer(regex.match); opts.all; total_matched++) {
            uint32_t options = 0;
            PCRE2_SIZE offset = ovector[1];  // start at end of previous match

            if (ovector[0] == ovector[1]) {
                if (ovector[0] == arglen) break;
                options = PCRE2_NOTEMPTY_ATSTART | PCRE2_ANCHORED;
            }

            rc = report_match(arg, pcre2_match(regex.code, PCRE2_SPTR(arg.c_str()), arglen, offset,
                                               options, regex.match, nullptr));

            if (rc < 0 /* pcre2 error */)
                return false;
            else if (rc == 0 /* no matches */) {
                if (options == 0 /* all matches found now */) break;
                ovector[1] = offset + 1;
                continue;
            }
        }
        return true;
    }
};

static int string_match(parser_t &parser, io_streams_t &streams, int argc, wchar_t **argv) {
    wchar_t *cmd = argv[0];

    options_t opts;
    opts.all_valid = true;
    opts.entire_valid = true;
    opts.ignore_case_valid = true;
    opts.invert_valid = true;
    opts.quiet_valid = true;
    opts.regex_valid = true;
    opts.index_valid = true;
    int optind;
    int retval = parse_opts(&opts, &optind, 1, argc, argv, parser, streams);
    if (retval != STATUS_CMD_OK) return retval;
    const wchar_t *pattern = opts.arg1;

    if (opts.entire && opts.index) {
        streams.err.append_format(BUILTIN_ERR_COMBO2, cmd,
                                  _(L"--entire and --index are mutually exclusive"));
        return STATUS_INVALID_ARGS;
    }

    std::unique_ptr<string_matcher_t> matcher;
    if (opts.regex) {
        matcher = make_unique<pcre2_matcher_t>(cmd, pattern, opts, streams);
    } else {
        matcher = make_unique<wildcard_matcher_t>(cmd, pattern, opts, streams);
    }

    arg_iterator_t aiter(argv, optind, streams);
    while (const wcstring *arg = aiter.nextstr()) {
        if (!matcher->report_matches(*arg)) {
            return STATUS_INVALID_ARGS;
        }
    }

    return matcher->match_count() > 0 ? STATUS_CMD_OK : STATUS_CMD_ERROR;
}

static int string_pad(parser_t &parser, io_streams_t &streams, int argc, wchar_t **argv) {
    options_t opts;
    opts.char_to_pad_valid = true;
    opts.right_valid = true;
    opts.width_valid = true;
    int optind;
    int retval = parse_opts(&opts, &optind, 0, argc, argv, parser, streams);
    if (retval != STATUS_CMD_OK) return retval;

    size_t pad_char_width = fish_wcwidth(opts.char_to_pad);
    if (pad_char_width == 0) {
        string_error(streams, _(L"%ls: Invalid padding character of width zero\n"), argv[0]);
        return STATUS_INVALID_ARGS;
    }

    // Pad left by default
    if (!opts.right) {
        opts.left = true;
    }

    // Find max width of strings and keep the inputs
    size_t max_width = 0;
    std::vector<wcstring> inputs;

    arg_iterator_t aiter_width(argv, optind, streams);
    while (const wcstring *arg = aiter_width.nextstr()) {
        wcstring input_string = *arg;
        size_t width = fish_wcswidth(input_string);
        if (width > max_width) max_width = width;
        inputs.push_back(std::move(input_string));
    }

    size_t pad_width = max_width > opts.width ? max_width : opts.width;
    for (auto &input : inputs) {
        wcstring padded;
        size_t padded_width = fish_wcswidth(input);
        if (pad_width >= padded_width) {
            size_t pad = (pad_width - padded_width) / pad_char_width;
            size_t remaining_width = (pad_width - padded_width) % pad_char_width;
            if (opts.left) {
                padded.append(pad, opts.char_to_pad);
                padded.append(remaining_width, L' ');
                padded.append(input);
            }
            if (opts.right) {
                padded.append(input);
                padded.append(remaining_width, L' ');
                padded.append(pad, opts.char_to_pad);
            }
        }
        padded.push_back(L'\n');
        streams.out.append(padded);
    }

    return STATUS_CMD_OK;
}

class string_replacer_t {
   protected:
    const wchar_t *argv0;
    options_t opts;
    int total_replaced;
    io_streams_t &streams;

   public:
    string_replacer_t(const wchar_t *argv0_, options_t opts_, io_streams_t &streams_)
        : argv0(argv0_), opts(std::move(opts_)), total_replaced(0), streams(streams_) {}

    virtual ~string_replacer_t() = default;
    int replace_count() const { return total_replaced; }
    virtual bool replace_matches(const wcstring &arg) = 0;
};

class literal_replacer_t : public string_replacer_t {
    const wcstring pattern;
    const wcstring replacement;
    size_t patlen;

   public:
    literal_replacer_t(const wchar_t *argv0, wcstring pattern_, const wchar_t *replacement_,
                       const options_t &opts, io_streams_t &streams)
        : string_replacer_t(argv0, opts, streams),
          pattern(std::move(pattern_)),
          replacement(replacement_),
          patlen(pattern.length()) {}

    ~literal_replacer_t() override = default;
    bool replace_matches(const wcstring &arg) override;
};

static maybe_t<wcstring> interpret_escapes(const wcstring &arg) {
    wcstring result;
    result.reserve(arg.size());
    const wchar_t *cursor = arg.c_str();
    const wchar_t *end = cursor + arg.size();
    while (cursor < end) {
        if (*cursor == L'\\') {
            if (auto escape_len = read_unquoted_escape(cursor, &result, true, false)) {
                cursor += *escape_len;
            } else {
                // Invalid escape.
                return none();
            }
        } else {
            result.push_back(*cursor);
            cursor++;
        }
    }
    return result;
}

class regex_replacer_t : public string_replacer_t {
    compiled_regex_t regex;
    maybe_t<wcstring> replacement;

   public:
    regex_replacer_t(const wchar_t *argv0, const wcstring &pattern, const wcstring &replacement_,
                     const options_t &opts, io_streams_t &streams)
        : string_replacer_t(argv0, opts, streams),
          regex(argv0, pattern, opts.ignore_case, streams) {
        if (feature_test(features_t::string_replace_backslash)) {
            replacement = replacement_;
        } else {
            replacement = interpret_escapes(replacement_);
        }
    }

    bool replace_matches(const wcstring &arg) override;
};

/// A return value of true means all is well (even if no replacements were performed), false
/// indicates an unrecoverable error.
bool literal_replacer_t::replace_matches(const wcstring &arg) {
    wcstring result;
    bool replacement_occurred = false;

    if (patlen == 0) {
        replacement_occurred = true;
        result = arg;
    } else {
        auto &cmp_func = opts.ignore_case ? wcsncasecmp : std::wcsncmp;
        const wchar_t *cur = arg.c_str();
        const wchar_t *end = cur + arg.size();
        while (cur < end) {
            if ((opts.all || !replacement_occurred) &&
                cmp_func(cur, pattern.c_str(), patlen) == 0) {
                result += replacement;
                cur += patlen;
                replacement_occurred = true;
                total_replaced++;
            } else {
                result.push_back(*cur);
                cur++;
            }
        }
    }

    if (!opts.quiet && (!opts.filter || replacement_occurred)) {
        streams.out.append(result);
        streams.out.append(L'\n');
    }

    return true;
}

/// A return value of true means all is well (even if no replacements were performed), false
/// indicates an unrecoverable error.
bool regex_replacer_t::replace_matches(const wcstring &arg) {
    if (!regex.code) return false;   // pcre2_compile() failed
    if (!replacement) return false;  // replacement was an invalid string

    // clang-format off
    // SUBSTITUTE_OVERFLOW_LENGTH causes pcre to return the needed buffer length if the passed one is to small
    // SUBSTITUTE_EXTENDED changes how substitution expressions are interpreted (`$` as the special character)
    // SUBSTITUTE_UNSET_EMPTY treats unmatched capturing groups as empty instead of erroring.
    // SUBSTITUTE_GLOBAL means more than one substitution happens.
    // clang-format on
    uint32_t options = PCRE2_SUBSTITUTE_OVERFLOW_LENGTH | PCRE2_SUBSTITUTE_EXTENDED |
                       PCRE2_SUBSTITUTE_UNSET_EMPTY | (opts.all ? PCRE2_SUBSTITUTE_GLOBAL : 0);
    size_t arglen = arg.length();
    PCRE2_SIZE bufsize = (arglen == 0) ? 16 : 2 * arglen;
    auto output = static_cast<wchar_t *>(malloc(sizeof(wchar_t) * bufsize));
    int pcre2_rc;
    PCRE2_SIZE outlen = bufsize;

    bool done = false;
    while (!done) {
        assert(output);

        pcre2_rc = pcre2_substitute(regex.code, PCRE2_SPTR(arg.c_str()), arglen,
                                    0,  // start offset
                                    options, regex.match,
                                    nullptr,  // match_data
                                    PCRE2_SPTR(replacement->c_str()), replacement->length(),
                                    reinterpret_cast<PCRE2_UCHAR *>(output), &outlen);

        if (pcre2_rc != PCRE2_ERROR_NOMEMORY || bufsize >= outlen) {
            done = true;
        } else {
            bufsize = outlen;
            auto new_output = static_cast<wchar_t *>(realloc(output, sizeof(wchar_t) * bufsize));
            if (new_output) output = new_output;
        }
    }

    bool rc = true;
    if (pcre2_rc < 0) {
        string_error(streams, _(L"%ls: Regular expression substitute error: %ls\n"), argv0,
                     pcre2_strerror(pcre2_rc).c_str());
        rc = false;
    } else {
        wcstring outstr(output, outlen);
        bool replacement_occurred = pcre2_rc > 0;
        if (!opts.quiet && (!opts.filter || replacement_occurred)) {
            streams.out.append(outstr);
            streams.out.append(L'\n');
        }
        total_replaced += pcre2_rc;
    }

    free(output);
    return rc;
}

static int string_replace(parser_t &parser, io_streams_t &streams, int argc, wchar_t **argv) {
    options_t opts;
    opts.all_valid = true;
    opts.filter_valid = true;
    opts.ignore_case_valid = true;
    opts.quiet_valid = true;
    opts.regex_valid = true;
    int optind;
    int retval = parse_opts(&opts, &optind, 2, argc, argv, parser, streams);
    if (retval != STATUS_CMD_OK) return retval;

    const wchar_t *pattern = opts.arg1;
    const wchar_t *replacement = opts.arg2;

    std::unique_ptr<string_replacer_t> replacer;
    if (opts.regex) {
        replacer = make_unique<regex_replacer_t>(argv[0], pattern, replacement, opts, streams);
    } else {
        replacer = make_unique<literal_replacer_t>(argv[0], pattern, replacement, opts, streams);
    }

    arg_iterator_t aiter(argv, optind, streams);
    while (const wcstring *arg = aiter.nextstr()) {
        if (!replacer->replace_matches(*arg)) return STATUS_INVALID_ARGS;
    }

    return replacer->replace_count() > 0 ? STATUS_CMD_OK : STATUS_CMD_ERROR;
}

static int string_split_maybe0(parser_t &parser, io_streams_t &streams, int argc, wchar_t **argv,
                               bool is_split0) {
    wchar_t *cmd = argv[0];
    options_t opts;
    opts.quiet_valid = true;
    opts.right_valid = true;
    opts.max_valid = true;
    opts.max = LONG_MAX;
    opts.no_empty_valid = true;
    opts.fields_valid = true;
    opts.allow_empty_valid = true;
    int optind;
    int retval = parse_opts(&opts, &optind, is_split0 ? 0 : 1, argc, argv, parser, streams);
    if (retval != STATUS_CMD_OK) return retval;

    if (opts.fields.size() < 1 && opts.allow_empty) {
        streams.err.append_format(BUILTIN_ERR_COMBO2, cmd,
                                  _(L"--allow-empty is only valid with --fields"));
        return STATUS_INVALID_ARGS;
    }

    const wcstring sep = is_split0 ? wcstring(1, L'\0') : wcstring(opts.arg1);

    std::vector<wcstring_list_t> all_splits;
    size_t split_count = 0;
    size_t arg_count = 0;
    arg_iterator_t aiter(argv, optind, streams, !is_split0);
    while (const wcstring *arg = aiter.nextstr()) {
        wcstring_list_t splits;
        if (opts.right) {
            split_about(arg->rbegin(), arg->rend(), sep.rbegin(), sep.rend(), &splits, opts.max,
                        opts.no_empty);
        } else {
            split_about(arg->begin(), arg->end(), sep.begin(), sep.end(), &splits, opts.max,
                        opts.no_empty);
        }
        all_splits.push_back(splits);
        split_count += splits.size();
        arg_count++;
    }

    for (auto &splits : all_splits) {
        // If we are from the right, split_about gave us reversed strings, in reversed order!
        if (opts.right) {
            for (auto &split : splits) {
                std::reverse(split.begin(), split.end());
            }
            std::reverse(splits.begin(), splits.end());
        }

        if (!opts.quiet) {
            if (is_split0 && !splits.empty()) {
                // split0 ignores a trailing \0, so a\0b\0 is two elements.
                // In contrast to split, where a\nb\n is three - "a", "b" and "".
                //
                // Remove the last element if it is empty.
                if (splits.back().empty()) splits.pop_back();
            }
            if (opts.fields.size() > 0) {
                // Print nothing and return error if any of the supplied
                // fields do not exist, unless `--allow-empty` is used.
                if (!opts.allow_empty) {
                    for (const auto &field : opts.fields) {
                        // field indexing starts from 1
                        if (field - 1 >= (long)splits.size()) {
                            return STATUS_CMD_ERROR;
                        }
                    }
                }
                for (const auto &field : opts.fields) {
                    if (field - 1 < (long)splits.size()) {
                        streams.out.append_with_separation(splits.at(field - 1),
                                                           separation_type_t::explicitly);
                    }
                }
            } else {
                for (const wcstring &split : splits) {
                    streams.out.append_with_separation(split, separation_type_t::explicitly);
                }
            }
        }
    }
    // We split something if we have more split values than args.
    return split_count > arg_count ? STATUS_CMD_OK : STATUS_CMD_ERROR;
}

static int string_split(parser_t &parser, io_streams_t &streams, int argc, wchar_t **argv) {
    return string_split_maybe0(parser, streams, argc, argv, false /* is_split0 */);
}

static int string_split0(parser_t &parser, io_streams_t &streams, int argc, wchar_t **argv) {
    return string_split_maybe0(parser, streams, argc, argv, true /* is_split0 */);
}

static int string_collect(parser_t &parser, io_streams_t &streams, int argc, wchar_t **argv) {
    options_t opts;
    opts.no_trim_newlines_valid = true;
    int optind;
    int retval = parse_opts(&opts, &optind, 0, argc, argv, parser, streams);
    if (retval != STATUS_CMD_OK) return retval;

    arg_iterator_t aiter(argv, optind, streams, /* don't split */ false);
    size_t appended = 0;
    while (const wcstring *arg = aiter.nextstr()) {
        const wchar_t *s = arg->c_str();
        size_t len = arg->size();
        if (!opts.no_trim_newlines) {
            while (len > 0 && s[len - 1] == L'\n') {
                len -= 1;
            }
        }
        streams.out.append_with_separation(s, len, separation_type_t::explicitly);
        appended += len;
    }

    return appended > 0 ? STATUS_CMD_OK : STATUS_CMD_ERROR;
}

// Helper function to abstract the repeat logic from string_repeat
// returns the to_repeat string, repeated count times.
static wcstring wcsrepeat(const wcstring &to_repeat, size_t count) {
    wcstring repeated;
    repeated.reserve(to_repeat.length() * count);

    for (size_t j = 0; j < count; j++) {
        repeated += to_repeat;
    }

    return repeated;
}

// Helper function to abstract the repeat until logic from string_repeat
// returns the to_repeat string, repeated until max char has been reached.
static wcstring wcsrepeat_until(const wcstring &to_repeat, size_t max) {
    if (to_repeat.length() == 0) return wcstring();
    size_t count = max / to_repeat.length();
    size_t mod = max % to_repeat.length();

    return wcsrepeat(to_repeat, count) + to_repeat.substr(0, mod);
}

static int string_repeat(parser_t &parser, io_streams_t &streams, int argc, wchar_t **argv) {
    options_t opts;
    opts.count_valid = true;
    opts.max_valid = true;
    opts.quiet_valid = true;
    opts.no_newline_valid = true;
    int optind;
    int retval = parse_opts(&opts, &optind, 0, argc, argv, parser, streams);
    if (retval != STATUS_CMD_OK) return retval;

    bool is_empty = true;

    arg_iterator_t aiter(argv, optind, streams);
    if (const wcstring *word = aiter.nextstr()) {
        const bool limit_repeat =
            (opts.max > 0 && word->length() * opts.count > static_cast<size_t>(opts.max)) ||
            !opts.count;
        const wcstring repeated =
            limit_repeat ? wcsrepeat_until(*word, opts.max) : wcsrepeat(*word, opts.count);
        is_empty = repeated.empty();

        if (!opts.quiet && !is_empty) {
            streams.out.append(repeated);
            if (!opts.no_newline) streams.out.append(L"\n");
        }
    }

    return !is_empty ? STATUS_CMD_OK : STATUS_CMD_ERROR;
}

static int string_sub(parser_t &parser, io_streams_t &streams, int argc, wchar_t **argv) {
    wchar_t *cmd = argv[0];

    options_t opts;
    opts.length_valid = true;
    opts.quiet_valid = true;
    opts.start_valid = true;
    opts.end_valid = true;
    opts.length = -1;
    int optind;
    int retval = parse_opts(&opts, &optind, 0, argc, argv, parser, streams);
    if (retval != STATUS_CMD_OK) return retval;

    if (opts.length != -1 && opts.end != 0) {
        streams.err.append_format(BUILTIN_ERR_COMBO2, cmd,
                                  _(L"--end and --length are mutually exclusive"));
        return STATUS_INVALID_ARGS;
    }

    int nsub = 0;
    arg_iterator_t aiter(argv, optind, streams);
    while (const wcstring *s = aiter.nextstr()) {
        using size_type = wcstring::size_type;
        size_type pos = 0;
        size_type count = wcstring::npos;

        if (opts.start > 0) {
            pos = static_cast<size_type>(opts.start - 1);
        } else if (opts.start < 0) {
            assert(opts.start != LONG_MIN);  // checked above
            auto n = static_cast<size_type>(-opts.start);
            pos = n > s->length() ? 0 : s->length() - n;
        }

        if (pos > s->length()) {
            pos = s->length();
        }

        if (opts.length >= 0) {
            count = static_cast<size_type>(opts.length);
        } else if (opts.end != 0) {
            size_type n;
            if (opts.end > 0) {
                n = static_cast<size_type>(opts.end);
            } else {
                assert(opts.end != LONG_MIN);  // checked above
                n = static_cast<size_type>(-opts.end);
                n = n > s->length() ? 0 : s->length() - n;
            }
            count = n < pos ? 0 : n - pos;
        }

        // Note that std::string permits count to extend past end of string.
        if (!opts.quiet) {
            streams.out.append(s->substr(pos, count));
            streams.out.append(L'\n');
        }
        nsub++;
    }

    return nsub > 0 ? STATUS_CMD_OK : STATUS_CMD_ERROR;
}

static int string_trim(parser_t &parser, io_streams_t &streams, int argc, wchar_t **argv) {
    options_t opts;
    opts.chars_to_trim_valid = true;
    opts.left_valid = true;
    opts.right_valid = true;
    opts.quiet_valid = true;
    int optind;
    int retval = parse_opts(&opts, &optind, 0, argc, argv, parser, streams);
    if (retval != STATUS_CMD_OK) return retval;

    // If neither left or right is specified, we do both.
    if (!opts.left && !opts.right) {
        opts.left = opts.right = true;
    }

    size_t ntrim = 0;

    arg_iterator_t aiter(argv, optind, streams);
    while (const wcstring *arg = aiter.nextstr()) {
        // Begin and end are respectively the first character to keep on the left, and first
        // character to trim on the right. The length is thus end - start.
        size_t begin = 0, end = arg->size();
        if (opts.right) {
            size_t last_to_keep = arg->find_last_not_of(opts.chars_to_trim);
            end = (last_to_keep == wcstring::npos) ? 0 : last_to_keep + 1;
        }
        if (opts.left) {
            size_t first_to_keep = arg->find_first_not_of(opts.chars_to_trim);
            begin = (first_to_keep == wcstring::npos ? end : first_to_keep);
        }
        assert(begin <= end && end <= arg->size());
        ntrim += arg->size() - (end - begin);
        if (!opts.quiet) {
            streams.out.append(wcstring(*arg, begin, end - begin));
            streams.out.append(L'\n');
        }
    }

    return ntrim > 0 ? STATUS_CMD_OK : STATUS_CMD_ERROR;
}

// A helper function for lower and upper.
static int string_transform(parser_t &parser, io_streams_t &streams, int argc, wchar_t **argv,
                            std::wint_t (*func)(std::wint_t)) {
    options_t opts;
    opts.quiet_valid = true;
    int optind;
    int retval = parse_opts(&opts, &optind, 0, argc, argv, parser, streams);
    if (retval != STATUS_CMD_OK) return retval;

    int n_transformed = 0;
    arg_iterator_t aiter(argv, optind, streams);
    while (const wcstring *arg = aiter.nextstr()) {
        wcstring transformed(*arg);
        std::transform(transformed.begin(), transformed.end(), transformed.begin(), func);
        if (transformed != *arg) n_transformed++;
        if (!opts.quiet) {
            streams.out.append(transformed);
            streams.out.append(L'\n');
        }
    }

    return n_transformed > 0 ? STATUS_CMD_OK : STATUS_CMD_ERROR;
}

/// Implementation of `string lower`.
static int string_lower(parser_t &parser, io_streams_t &streams, int argc, wchar_t **argv) {
    return string_transform(parser, streams, argc, argv, std::towlower);
}

/// Implementation of `string upper`.
static int string_upper(parser_t &parser, io_streams_t &streams, int argc, wchar_t **argv) {
    return string_transform(parser, streams, argc, argv, std::towupper);
}

// Keep sorted alphabetically
static const struct string_subcommand {
    const wchar_t *name;
    int (*handler)(parser_t &, io_streams_t &, int argc,  //!OCLINT(unused param)
                   wchar_t **argv);                       //!OCLINT(unused param)
} string_subcommands[] = {
    {L"collect", &string_collect}, {L"escape", &string_escape}, {L"join", &string_join},
    {L"join0", &string_join0},     {L"length", &string_length}, {L"lower", &string_lower},
    {L"match", &string_match},     {L"pad", &string_pad},       {L"repeat", &string_repeat},
    {L"replace", &string_replace}, {L"split", &string_split},   {L"split0", &string_split0},
    {L"sub", &string_sub},         {L"trim", &string_trim},     {L"unescape", &string_unescape},
    {L"upper", &string_upper},
};

/// The string builtin, for manipulating strings.
maybe_t<int> builtin_string(parser_t &parser, io_streams_t &streams, wchar_t **argv) {
    wchar_t *cmd = argv[0];
    int argc = builtin_count_args(argv);
    if (argc <= 1) {
        streams.err.append_format(BUILTIN_ERR_MISSING_SUBCMD, cmd);
        builtin_print_error_trailer(parser, streams.err, L"string");
        return STATUS_INVALID_ARGS;
    }

    if (std::wcscmp(argv[1], L"-h") == 0 || std::wcscmp(argv[1], L"--help") == 0) {
        builtin_print_help(parser, streams, L"string");
        return STATUS_CMD_OK;
    }

    const wchar_t *subcmd_name = argv[1];

    static auto begin = std::begin(string_subcommands);
    static auto end = std::end(string_subcommands);
    string_subcommand search{subcmd_name, 0};
    auto binsearch = std::lower_bound(
        begin, end, search, [&](const string_subcommand &cmd1, const string_subcommand &cmd2) {
            return wcscmp(cmd1.name, cmd2.name) < 0;
        });
    const string_subcommand *subcmd = nullptr;
    if (binsearch != end && wcscmp(subcmd_name, binsearch->name) == 0) subcmd = &*binsearch;

    if (subcmd == nullptr) {
        streams.err.append_format(BUILTIN_ERR_INVALID_SUBCMD, cmd, subcmd_name);
        builtin_print_error_trailer(parser, streams.err, L"string");
        return STATUS_INVALID_ARGS;
    }

    if (argc >= 3 && (std::wcscmp(argv[2], L"-h") == 0 || std::wcscmp(argv[2], L"--help") == 0)) {
        wcstring string_dash_subcommand = wcstring(argv[0]) + L"-" + subcmd_name;
        builtin_print_help(parser, streams, string_dash_subcommand.c_str());
        return STATUS_CMD_OK;
    }
    argc--;
    argv++;
    return subcmd->handler(parser, streams, argc, argv);
}
