// Implementation of the string builtin.
#include "config.h"  // IWYU pragma: keep

#include <algorithm>
#include <cerrno>
#include <climits>
#include <cstdarg>
#include <cstddef>
#include <cstdint>
#include <cwchar>
#include <cwctype>
#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "../builtin.h"
#include "../common.h"
#include "../env.h"
#include "../fallback.h"  // IWYU pragma: keep
#include "../future_feature_flags.h"
#include "../io.h"
#include "../maybe.h"
#include "../parse_util.h"
#include "../parser.h"
#include "../re.h"
#include "../screen.h"
#include "../wcstringutil.h"
#include "../wgetopt.h"
#include "../wildcard.h"
#include "../wutil.h"  // IWYU pragma: keep

// Empirically determined.
// This is probably down to some pipe buffer or some such,
// but too small means we need to call `read(2)` and str2wcstring a lot.
#define STRING_CHUNK_SIZE 1024

namespace {

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
    // If set, we have consumed all of stdin and its last line is missing a newline character.
    // This is an edge case -- we expect text input, which is conventionally terminated by a
    // newline character. But if it isn't, we use this to avoid creating one out of thin air,
    // to not corrupt input data.
    bool missing_trailing_newline = false;

    /// Reads the next argument from stdin, returning true if an argument was produced and false if
    /// not. On true, the string is stored in storage_.
    bool get_arg_stdin() {
        assert(string_args_from_stdin(streams_) && "should not be reading from stdin");
        assert(streams_.stdin_fd >= 0 && "should have a valid fd");
        // Read in chunks from fd until buffer has a line (or the end if split_ is unset).
        size_t pos;
        while (!split_ || (pos = buffer_.find('\n')) == std::string::npos) {
            char buf[STRING_CHUNK_SIZE];
            long n = read_blocked(streams_.stdin_fd, buf, STRING_CHUNK_SIZE);
            if (n == 0) {
                // If we still have buffer contents, flush them,
                // in case there was no trailing sep.
                if (buffer_.empty()) return false;
                missing_trailing_newline = true;
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

    /// Returns true if we should add a newline after printing output for the current item.
    /// This is only ever false in an edge case, namely after we have consumed stdin and the
    /// last line is missing a trailing newline.
    bool want_newline() const { return !missing_trailing_newline; }
};

// This is used by the string subcommands to communicate with the option parser which flags are
// valid and get the result of parsing the command for flags.
struct options_t {  //!OCLINT(too many fields)
    bool all_valid = false;
    bool char_to_pad_valid = false;
    bool chars_to_trim_valid = false;
    bool chars_to_shorten_valid = false;
    bool count_valid = false;
    bool entire_valid = false;
    bool filter_valid = false;
    bool groups_only_valid = false;
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
    bool visible_valid = false;
    bool width_valid = false;

    bool all = false;
    bool entire = false;
    bool filter = false;
    bool groups_only = false;
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
    bool visible = false;

    long count = 0;
    long length = 0;
    long max = 0;
    long start = 0;
    long end = 0;
    ssize_t width = 0;

    wchar_t char_to_pad = L' ';

    std::vector<int> fields;

    const wchar_t *chars_to_trim = L" \f\n\r\t\v";
    const wchar_t *arg1 = nullptr;
    const wchar_t *arg2 = nullptr;

    escape_string_style_t escape_style = STRING_STYLE_SCRIPT;
};

static size_t width_without_escapes(const wcstring &ins, size_t start_pos = 0) {
    ssize_t width = 0;
    for (size_t i = start_pos; i < ins.size(); i++) {
        wchar_t c = ins[i];
        auto w = fish_wcwidth_visible(c);
        // We assume that this string is on its own line,
        // in which case a backslash can't bring us below 0.
        if (w > 0 || width > 0) {
            width += w;
        }
    }

    // ANSI escape sequences like \e\[31m contain printable characters. Subtract their width
    // because they are not rendered.
    size_t pos = start_pos;
    while ((pos = ins.find('\x1B', pos)) != std::string::npos) {
        auto len = escape_code_length(ins.c_str() + pos);
        if (len.has_value()) {
            auto sub = ins.substr(pos, *len);
            for (auto c : sub) {
                auto w = fish_wcwidth_visible(c);
                width -= w;
            }
            // Move us forward behind the escape code,
            // it might include a second escape!
            // E.g. SGR0 ("reset") is \e\(B\e\[m in xterm.
            pos += *len - 1;
        } else {
            pos++;
        }
    }
    return width;
}

/// This handles the `--style=xxx` flag.
static int handle_flag_1(const wchar_t **argv, parser_t &parser, io_streams_t &streams,
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

using flag_handler_t = int (*)(const wchar_t **argv, parser_t &parser, io_streams_t &streams,
                               const wgetopter_t &w, options_t *opts);

static int handle_flag_N(const wchar_t **argv, parser_t &parser, io_streams_t &streams,
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

static int handle_flag_a(const wchar_t **argv, parser_t &parser, io_streams_t &streams,
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

static int handle_flag_c(const wchar_t **argv, parser_t &parser, io_streams_t &streams,
                         const wgetopter_t &w, options_t *opts) {
    if (opts->chars_to_trim_valid || opts->chars_to_shorten_valid) {
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

static int handle_flag_e(const wchar_t **argv, parser_t &parser, io_streams_t &streams,
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

static int handle_flag_f(const wchar_t **argv, parser_t &parser, io_streams_t &streams,
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

static int handle_flag_g(const wchar_t **argv, parser_t &parser, io_streams_t &streams,
                         const wgetopter_t &w, options_t *opts) {
    if (opts->groups_only_valid) {
        opts->groups_only = true;
        return STATUS_CMD_OK;
    }
    string_unknown_option(parser, streams, argv[0], argv[w.woptind - 1]);
    return STATUS_INVALID_ARGS;
}

static int handle_flag_i(const wchar_t **argv, parser_t &parser, io_streams_t &streams,
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

static int handle_flag_l(const wchar_t **argv, parser_t &parser, io_streams_t &streams,
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

static int handle_flag_m(const wchar_t **argv, parser_t &parser, io_streams_t &streams,
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

static int handle_flag_n(const wchar_t **argv, parser_t &parser, io_streams_t &streams,
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

static int handle_flag_q(const wchar_t **argv, parser_t &parser, io_streams_t &streams,
                         const wgetopter_t &w, options_t *opts) {
    if (opts->quiet_valid) {
        opts->quiet = true;
        return STATUS_CMD_OK;
    }
    string_unknown_option(parser, streams, argv[0], argv[w.woptind - 1]);
    return STATUS_INVALID_ARGS;
}

static int handle_flag_r(const wchar_t **argv, parser_t &parser, io_streams_t &streams,
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

static int handle_flag_s(const wchar_t **argv, parser_t &parser, io_streams_t &streams,
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

static int handle_flag_v(const wchar_t **argv, parser_t &parser, io_streams_t &streams,
                         const wgetopter_t &w, options_t *opts) {
    if (opts->invert_valid) {
        opts->invert_match = true;
        return STATUS_CMD_OK;
    }
    string_unknown_option(parser, streams, argv[0], argv[w.woptind - 1]);
    return STATUS_INVALID_ARGS;
}

static int handle_flag_V(const wchar_t **argv, parser_t &parser, io_streams_t &streams,
                         const wgetopter_t &w, options_t *opts) {
    if (opts->visible_valid) {
        opts->visible = true;
        return STATUS_CMD_OK;
    }
    string_unknown_option(parser, streams, argv[0], argv[w.woptind - 1]);
    return STATUS_INVALID_ARGS;
}

static int handle_flag_w(const wchar_t **argv, parser_t &parser, io_streams_t &streams,
                         const wgetopter_t &w, options_t *opts) {
    if (opts->width_valid) {
        long width = fish_wcstol(w.woptarg);
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
    if (opts->chars_to_shorten_valid) short_opts.append(L"c:");
    if (opts->count_valid) short_opts.append(L"n:");
    if (opts->entire_valid) short_opts.append(L"e");
    if (opts->filter_valid) short_opts.append(L"f");
    if (opts->groups_only_valid) short_opts.append(L"g");
    if (opts->ignore_case_valid) short_opts.append(L"i");
    if (opts->index_valid) short_opts.append(L"n");
    if (opts->invert_valid) short_opts.append(L"v");
    if (opts->visible_valid) short_opts.append(L"V");
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
                                              {L"groups-only", no_argument, nullptr, 'g'},
                                              {L"ignore-case", no_argument, nullptr, 'i'},
                                              {L"index", no_argument, nullptr, 'n'},
                                              {L"invert", no_argument, nullptr, 'v'},
                                              {L"visible", no_argument, nullptr, 'V'},
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
                                              {}};

static flag_handler_t get_handler_for_flag(char c) {
    // clang-format off
    switch (c) {
        case 'N': return handle_flag_N;
        case 'a': return handle_flag_a;
        case 'c': return handle_flag_c;
        case 'e': return handle_flag_e;
        case 'f': return handle_flag_f;
        case 'g': return handle_flag_g;
        case 'i': return handle_flag_i;
        case 'l': return handle_flag_l;
        case 'm': return handle_flag_m;
        case 'n': return handle_flag_n;
        case 'q': return handle_flag_q;
        case 'r': return handle_flag_r;
        case 's': return handle_flag_s;
        case 'V': return handle_flag_V;
        case 'v': return handle_flag_v;
        case 'w': return handle_flag_w;
        case 1 : return handle_flag_1;
        default: return nullptr;
    }
    // clang-format on
}

/// Parse the arguments for flags recognized by a specific string subcommand.
static int parse_opts(options_t *opts, int *optind, int n_req_args, int argc, const wchar_t **argv,
                      parser_t &parser, io_streams_t &streams) {
    const wchar_t *cmd = argv[0];
    wcstring short_opts = construct_short_opts(opts);
    const wchar_t *short_options = short_opts.c_str();
    int opt;
    wgetopter_t w;
    while ((opt = w.wgetopt_long(argc, argv, short_options, long_options, nullptr)) != -1) {
        if (auto fn = get_handler_for_flag(opt)) {
            int retval = fn(argv, parser, streams, w, opts);
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

static int string_escape(parser_t &parser, io_streams_t &streams, int argc, const wchar_t **argv) {
    options_t opts;
    opts.no_quoted_valid = true;
    opts.style_valid = true;
    int optind;
    int retval = parse_opts(&opts, &optind, 0, argc, argv, parser, streams);
    if (retval != STATUS_CMD_OK) return retval;

    // Currently, only the script style supports options.
    // Ignore them for other styles for now.
    escape_flags_t flags = 0;
    if (opts.escape_style == STRING_STYLE_SCRIPT && opts.no_quoted) {
        flags |= ESCAPE_NO_QUOTED;
    }

    int nesc = 0;
    arg_iterator_t aiter(argv, optind, streams);
    while (const wcstring *arg = aiter.nextstr()) {
        wcstring sep = aiter.want_newline() ? L"\n" : L"";
        streams.out.append(escape_string(*arg, flags, opts.escape_style) + sep);
        nesc++;
    }

    return nesc > 0 ? STATUS_CMD_OK : STATUS_CMD_ERROR;
    DIE("should never reach this statement");
}

static int string_unescape(parser_t &parser, io_streams_t &streams, int argc,
                           const wchar_t **argv) {
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
        wcstring sep = aiter.want_newline() ? L"\n" : L"";
        if (unescape_string(*arg, &result, flags, opts.escape_style)) {
            streams.out.append(result + sep);
            nesc++;
        }
    }

    return nesc > 0 ? STATUS_CMD_OK : STATUS_CMD_ERROR;
    DIE("should never reach this statement");
}

static int string_join_maybe0(parser_t &parser, io_streams_t &streams, int argc,
                              const wchar_t **argv, bool is_join0) {
    options_t opts;
    opts.quiet_valid = true;
    opts.no_empty_valid = true;
    int optind;
    int retval = parse_opts(&opts, &optind, is_join0 ? 0 : 1, argc, argv, parser, streams);
    if (retval != STATUS_CMD_OK) return retval;

    const wcstring sep = is_join0 ? wcstring(1, L'\0') : wcstring(opts.arg1);
    int nargs = 0;
    arg_iterator_t aiter(argv, optind, streams);
    while (const wcstring *arg = aiter.nextstr()) {
        if (!opts.quiet) {
            if (opts.no_empty && arg->empty()) continue;

            if (nargs > 0) {
                streams.out.append(sep);
            }
            streams.out.append(*arg);
        } else if (nargs > 1) {
            return STATUS_CMD_OK;
        }
        nargs++;
    }
    if (nargs > 0 && !opts.quiet) {
        if (is_join0) {
            streams.out.push_back(L'\0');
        } else if (aiter.want_newline()) {
            streams.out.push_back(L'\n');
        }
    }

    return nargs > 1 ? STATUS_CMD_OK : STATUS_CMD_ERROR;
}

static int string_join(parser_t &parser, io_streams_t &streams, int argc, const wchar_t **argv) {
    return string_join_maybe0(parser, streams, argc, argv, false /* is_join0 */);
}

static int string_join0(parser_t &parser, io_streams_t &streams, int argc, const wchar_t **argv) {
    return string_join_maybe0(parser, streams, argc, argv, true /* is_join0 */);
}

static int string_length(parser_t &parser, io_streams_t &streams, int argc, const wchar_t **argv) {
    options_t opts;
    opts.quiet_valid = true;
    opts.visible_valid = true;
    int optind;
    int retval = parse_opts(&opts, &optind, 0, argc, argv, parser, streams);
    if (retval != STATUS_CMD_OK) return retval;

    int nnonempty = 0;
    arg_iterator_t aiter(argv, optind, streams);
    while (const wcstring *arg = aiter.nextstr()) {
        if (opts.visible) {
            // Visible length only makes sense line-wise.
            for (auto &line : split_string(*arg, L'\n')) {
                size_t max = 0;
                // Carriage-return returns us to the beginning. The longest substring without
                // carriage-return determines the overall width.
                for (auto &reset : split_string(line, L'\r')) {
                    size_t n = width_without_escapes(reset);
                    if (n > max) max = n;
                }
                if (max > 0) {
                    nnonempty++;
                }
                if (!opts.quiet) {
                    streams.out.append(to_string(max) + L"\n");
                } else if (nnonempty > 0) {
                    return STATUS_CMD_OK;
                }
            }
        } else {
            size_t n = arg->length();
            if (n > 0) {
                nnonempty++;
            }
            if (!opts.quiet) {
                streams.out.append(to_string(n) + L"\n");
            } else if (nnonempty > 0) {
                return STATUS_CMD_OK;
            }
        }
    }

    return nnonempty > 0 ? STATUS_CMD_OK : STATUS_CMD_ERROR;
}

namespace {
class string_matcher_t {
   protected:
    const options_t opts;
    int total_matched{0};

   public:
    explicit string_matcher_t(const options_t &opts_) : opts(opts_) {}

    virtual ~string_matcher_t() = default;
    virtual void report_matches(const wcstring &arg, io_streams_t &streams) = 0;
    int match_count() const { return total_matched; }

    virtual void import_captures(env_stack_t &) {}
};

class wildcard_matcher_t final : public string_matcher_t {
   private:
    wcstring wcpattern;

   public:
    wildcard_matcher_t(const wcstring &pattern, const options_t &opts)
        : string_matcher_t(opts), wcpattern(parse_util_unescape_wildcards(pattern)) {
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

    void report_matches(const wcstring &arg, io_streams_t &streams) override {
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
                    streams.out.append(arg + L"\n");
                }
            }
        }
    }
};

// Compile a regex, printing an error on failure.
static maybe_t<re::regex_t> try_compile_regex(const wcstring &pattern, const options_t &opts,
                                              const wchar_t *cmd, io_streams_t &streams) {
    re::re_error_t error{};
    re::flags_t flags{};
    flags.icase = opts.ignore_case;
    auto re = re::regex_t::try_compile(pattern, flags, &error);
    if (!re) {
        string_error(streams, _(L"%ls: Regular expression compile error: %ls\n"), cmd,
                     error.message().c_str());
        string_error(streams, L"%ls: %ls\n", cmd, pattern.c_str());
        string_error(streams, L"%ls: %*ls\n", cmd, static_cast<int>(error.offset), L"^");
    }
    return re;
}

/// Check if a list of capture group names is valid for variables. If any are invalid then report an
/// error to \p streams. \return true if all names are valid.
static bool validate_capture_group_names(const wcstring_list_t &capture_group_names,
                                         io_streams_t &streams) {
    for (const wcstring &name : capture_group_names) {
        if (env_var_t::flags_for(name.c_str()) & env_var_t::flag_read_only) {
            streams.err.append_format(
                L"Modification of read-only variable \"%ls\" is not allowed\n", name.c_str());
            return false;
        }
    }
    return true;
}

class regex_matcher_t final : public string_matcher_t {
    using regex_t = re::regex_t;
    using match_data_t = re::match_data_t;
    using match_range_t = re::match_range_t;

    // The regex to match against.
    const regex_t regex_;

    // Match data associated with the regex.
    match_data_t match_data_;

    // map from group name to matched substrings, for the first argument.
    std::map<wcstring, wcstring_list_t> first_match_captures_;

    void populate_captures_from_match(const wcstring &subject) {
        for (auto &kv : first_match_captures_) {
            const auto &name = kv.first;
            wcstring_list_t &vals = kv.second;

            // If there are multiple named groups and --all was used, we need to ensure that
            // the indexes are always in sync between the variables. If an optional named
            // group didn't match but its brethren did, we need to make sure to put
            // *something* in the resulting array, and unfortunately fish doesn't support
            // empty/null members so we're going to have to use an empty string as the
            // sentinel value.
            if (maybe_t<wcstring> capture =
                    regex_.substring_for_group(match_data_, name, subject)) {
                vals.push_back(capture.acquire());
            } else if (this->opts.all) {
                vals.emplace_back();
            }
        }
    }

    enum class match_result_t {
        no_match = 0,
        match = 1,
    };

    match_result_t report_match(const wcstring &arg, maybe_t<match_range_t> mrange,
                                io_streams_t &streams) const {
        if (!mrange.has_value()) {
            if (opts.invert_match && !opts.quiet) {
                if (opts.index) {
                    streams.out.append_format(L"1 %lu\n", arg.length());
                } else {
                    streams.out.append(arg + L"\n");
                }
            }

            return opts.invert_match ? match_result_t::match : match_result_t::no_match;
        } else if (opts.invert_match) {
            return match_result_t::no_match;
        }

        if (opts.entire && !opts.quiet) {
            streams.out.append(arg + L"\n");
        }

        // If we have groups-only, we skip the first match, which is the full one.
        size_t group_count = match_data_.matched_capture_group_count();
        for (size_t j = (opts.entire || opts.groups_only ? 1 : 0); j < group_count; j++) {
            maybe_t<match_range_t> cg = this->regex_.group(match_data_, j);
            if (cg.has_value() && !opts.quiet) {
                if (opts.index) {
                    streams.out.append_format(L"%lu %lu\n", cg->begin + 1, cg->end - cg->begin);
                } else {
                    streams.out.append(arg.substr(cg->begin, cg->end - cg->begin) + L"\n");
                }
            }
        }

        return opts.invert_match ? match_result_t::no_match : match_result_t::match;
    }

   public:
    regex_matcher_t(regex_t regex, const options_t &opts)
        : string_matcher_t(opts), regex_(std::move(regex)), match_data_(regex_.prepare()) {
        // Populate first_match_captures_ with the capture group names and empty lists.
        for (const wcstring &name : regex_.capture_group_names()) {
            first_match_captures_.emplace(name, wcstring_list_t{});
        }
    }

    ~regex_matcher_t() override = default;

    void report_matches(const wcstring &arg, io_streams_t &streams) override {
        using namespace re;

        match_data_.reset();
        auto rc = report_match(arg, this->regex_.match(match_data_, arg), streams);

        bool populate_captures = false;
        if (rc == match_result_t::match) {
            // We only populate captures for the *first matching argument*.
            populate_captures = (total_matched == 0);
            total_matched++;
        }

        if (populate_captures) {
            this->populate_captures_from_match(arg);
        }

        // Report any additional matches.
        if (!opts.invert_match && opts.all) {
            while (auto mr = this->regex_.match(match_data_, arg)) {
                auto rc = this->report_match(arg, mr, streams);
                if (rc == match_result_t::match && populate_captures) {
                    this->populate_captures_from_match(arg);
                }
            }
        }
    }

    void import_captures(env_stack_t &vars) override {
        for (auto &kv : first_match_captures_) {
            const wcstring &name = kv.first;
            vars.set(name, ENV_DEFAULT, std::move(kv.second));
        }
    }
};
}  // namespace

static int string_match(parser_t &parser, io_streams_t &streams, int argc, const wchar_t **argv) {
    const wchar_t *cmd = argv[0];

    options_t opts;
    opts.all_valid = true;
    opts.entire_valid = true;
    opts.groups_only_valid = true;
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

    if (opts.invert_match && opts.groups_only) {
        streams.err.append_format(BUILTIN_ERR_COMBO2, cmd,
                                  _(L"--invert and --groups-only are mutually exclusive"));
        return STATUS_INVALID_ARGS;
    }

    if (opts.entire && opts.groups_only) {
        streams.err.append_format(BUILTIN_ERR_COMBO2, cmd,
                                  _(L"--entire and --groups-only are mutually exclusive"));
        return STATUS_INVALID_ARGS;
    }

    std::unique_ptr<string_matcher_t> matcher;
    if (!opts.regex) {
        // Globs cannot fail.
        matcher = make_unique<wildcard_matcher_t>(pattern, opts);
    } else {
        // Compile the pattern as regex and validate capture group names as variables; both may
        // fail. Note both try_compile_regex and validate_capture_group_names print an error on
        // failure.
        auto re = try_compile_regex(pattern, opts, cmd, streams);
        if (!re || !validate_capture_group_names(re->capture_group_names(), streams)) {
            return STATUS_INVALID_ARGS;
        }
        matcher = make_unique<regex_matcher_t>(re.acquire(), opts);
    }

    assert(matcher && "Should have a matcher");
    arg_iterator_t aiter(argv, optind, streams);
    while (const wcstring *arg = aiter.nextstr()) {
        matcher->report_matches(*arg, streams);
        if (opts.quiet && matcher->match_count() > 0) {
            break;
        }
    }
    matcher->import_captures(parser.vars());

    return matcher->match_count() > 0 ? STATUS_CMD_OK : STATUS_CMD_ERROR;
}

static int string_pad(parser_t &parser, io_streams_t &streams, int argc, const wchar_t **argv) {
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
    ssize_t max_width = 0;
    std::vector<wcstring> inputs;

    arg_iterator_t aiter_width(argv, optind, streams);
    while (const wcstring *arg = aiter_width.nextstr()) {
        wcstring input_string = *arg;
        ssize_t width = width_without_escapes(input_string);
        if (width > max_width) max_width = width;
        inputs.push_back(std::move(input_string));
    }

    ssize_t pad_width = max_width > opts.width ? max_width : opts.width;
    for (auto &input : inputs) {
        wcstring padded;
        ssize_t padded_width = width_without_escapes(input);
        if (pad_width >= padded_width) {
            ssize_t pad = (pad_width - padded_width) / pad_char_width;
            ssize_t remaining_width = (pad_width - padded_width) % pad_char_width;
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
        if (aiter_width.want_newline()) {
            padded.push_back(L'\n');
        }
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
    virtual bool replace_matches(const wcstring &arg, bool want_newline) = 0;
};

class literal_replacer_t final : public string_replacer_t {
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
    bool replace_matches(const wcstring &arg, bool want_newline) override;
};

static maybe_t<wcstring> interpret_escapes(const wcstring &arg) {
    wcstring result;
    result.reserve(arg.size());
    const wchar_t *cursor = arg.c_str();
    const wchar_t *end = cursor + arg.size();
    while (cursor < end) {
        if (*cursor == L'\\') {
            auto escape_len = read_unquoted_escape(cursor, &result, true, false);
            if (escape_len.has_value()) {
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

class regex_replacer_t final : public string_replacer_t {
    re::regex_t regex;
    maybe_t<wcstring> replacement;

   public:
    regex_replacer_t(const wchar_t *argv0, re::regex_t regex, const wcstring &replacement_,
                     const options_t &opts, io_streams_t &streams)
        : string_replacer_t(argv0, opts, streams), regex(std::move(regex)) {
        if (feature_test(features_t::string_replace_backslash)) {
            replacement = replacement_;
        } else {
            replacement = interpret_escapes(replacement_);
        }
    }

    bool replace_matches(const wcstring &arg, bool want_newline) override;
};

/// A return value of true means all is well (even if no replacements were performed), false
/// indicates an unrecoverable error.
bool literal_replacer_t::replace_matches(const wcstring &arg, bool want_newline) {
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
        wcstring sep = want_newline ? L"\n" : L"";
        streams.out.append(result + sep);
    }

    return true;
}

/// A return value of true means all is well (even if no replacements were performed), false
/// indicates an unrecoverable error.
bool regex_replacer_t::replace_matches(const wcstring &arg, bool want_newline) {
    using namespace re;
    if (!replacement) return false;  // replacement was an invalid string

    sub_flags_t sflags{};
    sflags.global = opts.all;
    sflags.extended = true;

    re_error_t error{};
    int repl_count{};
    maybe_t<wcstring> result =
        this->regex.substitute(arg, *replacement, sflags, 0, &error, &repl_count);

    if (!result) {
        string_error(streams, _(L"%ls: Regular expression substitute error: %ls\n"), argv0,
                     error.message().c_str());
    } else {
        bool replacement_occurred = repl_count > 0;
        if (!opts.quiet && (!opts.filter || replacement_occurred)) {
            wcstring sep = want_newline ? L"\n" : L"";
            streams.out.append(*result + sep);
        }
        total_replaced += repl_count;
    }
    return result.has_value();
}

static int string_replace(parser_t &parser, io_streams_t &streams, int argc, const wchar_t **argv) {
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
        if (auto re = try_compile_regex(pattern, opts, argv[0], streams)) {
            replacer =
                make_unique<regex_replacer_t>(argv[0], re.acquire(), replacement, opts, streams);
        } else {
            // try_compile_regex prints an error.
            return STATUS_INVALID_ARGS;
        }
    } else {
        replacer = make_unique<literal_replacer_t>(argv[0], pattern, replacement, opts, streams);
    }

    arg_iterator_t aiter(argv, optind, streams);
    while (const wcstring *arg = aiter.nextstr()) {
        if (!replacer->replace_matches(*arg, aiter.want_newline())) return STATUS_INVALID_ARGS;
        if (opts.quiet && replacer->replace_count() > 0) return STATUS_CMD_OK;
    }

    return replacer->replace_count() > 0 ? STATUS_CMD_OK : STATUS_CMD_ERROR;
}

static int string_split_maybe0(parser_t &parser, io_streams_t &streams, int argc,
                               const wchar_t **argv, bool is_split0) {
    const wchar_t *cmd = argv[0];
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

    if (opts.fields.empty() && opts.allow_empty) {
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
        // If we're quiet, we return early if we've found something to split.
        if (opts.quiet && splits.size() > 1) return STATUS_CMD_OK;
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
            if (!opts.fields.empty()) {
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
                                                           separation_type_t::explicitly, true);
                    }
                }
            } else {
                for (const wcstring &split : splits) {
                    streams.out.append_with_separation(split, separation_type_t::explicitly, true);
                }
            }
        }
    }
    // We split something if we have more split values than args.
    return split_count > arg_count ? STATUS_CMD_OK : STATUS_CMD_ERROR;
}

static int string_split(parser_t &parser, io_streams_t &streams, int argc, const wchar_t **argv) {
    return string_split_maybe0(parser, streams, argc, argv, false /* is_split0 */);
}

static int string_split0(parser_t &parser, io_streams_t &streams, int argc, const wchar_t **argv) {
    return string_split_maybe0(parser, streams, argc, argv, true /* is_split0 */);
}

static int string_collect(parser_t &parser, io_streams_t &streams, int argc, const wchar_t **argv) {
    options_t opts;
    opts.allow_empty_valid = true;
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
        streams.out.append_with_separation(s, len, separation_type_t::explicitly,
                                           aiter.want_newline());
        appended += len;
    }

    // If we haven't printed anything and "no_empty" is set,
    // print something empty. Helps with empty ellision:
    // echo (true | string collect --allow-empty)"bar"
    // prints "bar".
    if (opts.allow_empty && appended == 0) {
        streams.out.append_with_separation(
            L"", 0, separation_type_t::explicitly,
            true /* historical behavior is to always print a newline */);
    }

    return appended > 0 ? STATUS_CMD_OK : STATUS_CMD_ERROR;
}

static int string_repeat(parser_t &parser, io_streams_t &streams, int argc, const wchar_t **argv) {
    options_t opts;
    opts.count_valid = true;
    opts.max_valid = true;
    opts.quiet_valid = true;
    opts.no_newline_valid = true;
    int optind;
    int retval = parse_opts(&opts, &optind, 0, argc, argv, parser, streams);
    if (retval != STATUS_CMD_OK) return retval;
    if (opts.max == 0 && opts.count == 0) {
        // XXX: This used to be allowed, but returned 1.
        // Keep it that way for now instead of adding an error.
        // streams.err.append(L"Count or max must be greater than zero");
        return STATUS_CMD_ERROR;
    }

    bool all_empty = true;
    bool first = true;

    arg_iterator_t aiter(argv, optind, streams);
    while (const wcstring *word = aiter.nextstr()) {
        // If the string is empty, there is nothing to repeat.
        if (word->empty()) {
            continue;
        }

        all_empty = false;
        if (opts.quiet) {
            // Early out if we can - see #7495.
            return STATUS_CMD_OK;
        }

        if (!first && !opts.quiet) {
            streams.out.append(L'\n');
        }
        first = false;

        auto &w = *word;

        // The maximum size of the string is either the "max" characters,
        // or it's the "count" repetitions, whichever ends up lower.
        size_t max = opts.max;
        if (max == 0 || (opts.count > 0 && w.length() * opts.count < max)) {
            max = w.length() * opts.count;
        }

        // Reserve a string to avoid writing constantly.
        // The 1500 here is a total gluteal extraction, but 500 seems to perform slightly worse.
        const size_t chunk_size = 1500;
        // The + word length is so we don't have to hit the chunk size exactly,
        // which would require us to restart in the middle of the string.
        // E.g. imagine repeating "12345678". The first chunk is hit after a last "1234",
        // so we would then have to restart by appending "5678", which requires a substring.
        // So let's not bother.
        //
        // Unless of course we don't even print the entire word, in which case we just need max.
        wcstring chunk;
        chunk.reserve(std::min(chunk_size + w.length(), max));

        for (size_t i = max; i > 0; i -= w.length()) {
            // Build up the chunk.
            if (i >= w.length()) {
                chunk.append(w);
            } else {
                chunk.append(w.substr(0, i));
                break;
            }
            if (chunk.length() >= chunk_size) {
                // We hit the chunk size, write it repeatedly until we can't anymore.
                streams.out.append(chunk);
                while (i >= chunk.length()) {
                    streams.out.append(chunk);
                    // We can easily be asked to write *a lot* of data,
                    // so we need to check every so often if the pipe has been closed.
                    // If we didn't, running `string repeat -n LARGENUMBER foo | pv`
                    // and pressing ctrl-c seems to hang.
                    if (streams.out.flush_and_check_error() != STATUS_CMD_OK) {
                        return STATUS_CMD_ERROR;
                    }
                    i -= chunk.length();
                }
                chunk.clear();
            }
        }
        // Flush the remainder.
        if (!chunk.empty()) {
            streams.out.append(chunk);
        }
    }

    // Historical behavior is to never append a newline if all strings were empty.
    if (!opts.quiet && !opts.no_newline && !all_empty && aiter.want_newline()) {
        streams.out.append(L'\n');
    }

    return all_empty ? STATUS_CMD_ERROR : STATUS_CMD_OK;
}

static int string_sub(parser_t &parser, io_streams_t &streams, int argc, const wchar_t **argv) {
    const wchar_t *cmd = argv[0];

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
        wcstring sep = aiter.want_newline() ? L"\n" : L"";

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
            streams.out.append(s->substr(pos, count) + sep);
        }
        nsub++;
        if (opts.quiet) return STATUS_CMD_OK;
    }

    return nsub > 0 ? STATUS_CMD_OK : STATUS_CMD_ERROR;
}

static int string_shorten(parser_t &parser, io_streams_t &streams, int argc, const wchar_t **argv) {
    options_t opts;
    opts.chars_to_shorten_valid = true;
    opts.chars_to_trim = get_ellipsis_str();
    opts.max_valid = true;
    opts.no_newline_valid = true;
    opts.quiet_valid = true;
    opts.max = -1;
    opts.left_valid = true;
    int optind;
    int retval = parse_opts(&opts, &optind, 0, argc, argv, parser, streams);
    if (retval != STATUS_CMD_OK) return retval;

    // Find max width of strings and keep the inputs
    size_t min_width = SIZE_MAX;
    std::vector<wcstring> inputs;
    wcstring ell = opts.chars_to_trim;

    auto ell_width = fish_wcswidth(ell);

    arg_iterator_t aiter_width(argv, optind, streams);
    while (const wcstring *arg = aiter_width.nextstr()) {
        // Visible width only makes sense line-wise.
        // So either we have no-newlines (which means we shorten on the first newline),
        // or we handle the lines separately.
        auto splits = split_string(*arg, L'\n');
        if (opts.no_newline && splits.size() > 1) {
            wcstring str = !opts.left ? splits[0] : splits[splits.size() - 1];
            str.append(ell);
            ssize_t width = width_without_escapes(str);
            if (width > 0 && (size_t)width < min_width) min_width = width;
            inputs.push_back(str);
        } else {
            for (auto &input_string : splits) {
                ssize_t width = width_without_escapes(input_string);
                if (width > 0 && (size_t)width < min_width) min_width = width;
                inputs.push_back(std::move(input_string));
            }
        }
    }

    // opts.max is signed for other subcommands,
    // but we compare against .size() a bunch,
    // this shuts the compiler up.
    size_t ourmax = min_width;
    if (opts.max > 0) {
        ourmax = opts.max;
    }

    if (ell_width > (ssize_t)ourmax) {
        // If we can't even print our ellipsis, we substitute nothing,
        // truncating instead.
        ell = L"";
        ell_width = 0;
    }

    int nsub = 0;
    // We could also error out here if the width of our ellipsis is larger
    // than the target width.
    // That seems excessive - specifically because the ellipsis on LANG=C
    // is "..." (width 3!).

    auto skip_escapes = [&](const wcstring &l, size_t pos) {
        size_t totallen = 0;
        while (l[pos + totallen] == L'\x1B') {
            auto len = escape_code_length(l.c_str() + pos + totallen);
            if (!len.has_value()) break;
            totallen += *len;
        }
        return totallen;
    };

    for (auto &line : inputs) {
        size_t pos = 0;
        size_t max = 0;
        // Collect how much of the string we can use without going over the maximum.
        if (opts.left) {
            // Our strategy for keeping from the end.
            // This is rather unoptimized - actually going *backwards*
            // is extremely tricky because we would have to subtract escapes again.
            // Also we need to avoid hacking combiners into bits.
            // This should work for most cases considering the combiners typically have width 0.
            wcstring out;
            while (pos < line.size()) {
                auto w = width_without_escapes(line, pos);
                // If we're at the beginning and it fits, we sits.
                //
                // Otherwise we require it to fit the ellipsis
                if ((w <= ourmax && pos == 0) || w + ell_width <= ourmax) {
                    out = line.substr(pos);
                    break;
                }

                auto skip = skip_escapes(line, pos);
                pos += skip > 0 ? skip : 1;
            }
            if (opts.quiet && pos != 0) {
                return STATUS_CMD_OK;
            }

            if (pos == 0) {
                streams.out.append(line + L"\n");
            } else {
                // We have an ellipsis, construct our string and print it.
                nsub++;
                out = ell + out + L'\n';
                streams.out.append(out);
            }
            continue;
        } else {
            // Going from the left.
            // This is somewhat easier.
            while (max <= ourmax && pos < line.size()) {
                pos += skip_escapes(line, pos);
                auto w = fish_wcwidth(line[pos]);
                if (w <= 0 || max + w + ell_width <= ourmax) {
                    // If it still fits, even if it is the last, we add it.
                    max += w;
                    pos++;
                } else {
                    // We're at the limit, so see if the entire string fits.
                    auto max2 = max + w;
                    auto pos2 = pos + 1;
                    while (pos2 < line.size()) {
                        pos2 += skip_escapes(line, pos2);
                        max2 += fish_wcwidth(line[pos2]);
                        pos2++;
                    }

                    if (max2 <= ourmax) {
                        // We're at the end and everything fits,
                        // no ellipsis.
                        pos = pos2;
                    }
                    break;
                }
            }
        }

        if (opts.quiet && pos != line.size()) {
            return STATUS_CMD_OK;
        }

        if (pos == line.size()) {
            streams.out.append(line + L"\n");
        } else {
            nsub++;
            wcstring newl = line.substr(0, pos);
            newl.append(ell);
            newl.push_back(L'\n');
            streams.out.append(newl);
        }
    }

    // Return true if we have shortened something and false otherwise.
    return nsub > 0 ? STATUS_CMD_OK : STATUS_CMD_ERROR;
}

static int string_trim(parser_t &parser, io_streams_t &streams, int argc, const wchar_t **argv) {
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
        wcstring sep = aiter.want_newline() ? L"\n" : L"";
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
            streams.out.append(wcstring(*arg, begin, end - begin) + sep);
        } else if (ntrim > 0) {
            return STATUS_CMD_OK;
        }
    }

    return ntrim > 0 ? STATUS_CMD_OK : STATUS_CMD_ERROR;
}

// A helper function for lower and upper.
static int string_transform(parser_t &parser, io_streams_t &streams, int argc, const wchar_t **argv,
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
            wcstring sep = aiter.want_newline() ? L"\n" : L"";
            streams.out.append(transformed + sep);
        } else if (n_transformed > 0) {
            return STATUS_CMD_OK;
        }
    }

    return n_transformed > 0 ? STATUS_CMD_OK : STATUS_CMD_ERROR;
}

/// Implementation of `string lower`.
static int string_lower(parser_t &parser, io_streams_t &streams, int argc, const wchar_t **argv) {
    return string_transform(parser, streams, argc, argv, std::towlower);
}

/// Implementation of `string upper`.
static int string_upper(parser_t &parser, io_streams_t &streams, int argc, const wchar_t **argv) {
    return string_transform(parser, streams, argc, argv, std::towupper);
}

// Keep sorted alphabetically
static constexpr const struct string_subcommand {
    const wchar_t *name;
    int (*handler)(parser_t &, io_streams_t &, int argc,  //!OCLINT(unused param)
                   const wchar_t **argv);                 //!OCLINT(unused param)
} string_subcommands[] = {
    {L"collect", &string_collect},   {L"escape", &string_escape},   {L"join", &string_join},
    {L"join0", &string_join0},       {L"length", &string_length},   {L"lower", &string_lower},
    {L"match", &string_match},       {L"pad", &string_pad},         {L"repeat", &string_repeat},
    {L"replace", &string_replace},   {L"shorten", &string_shorten}, {L"split", &string_split},
    {L"split0", &string_split0},     {L"sub", &string_sub},         {L"trim", &string_trim},
    {L"unescape", &string_unescape}, {L"upper", &string_upper},
};
ASSERT_SORTED_BY_NAME(string_subcommands);
}  // namespace

/// The string builtin, for manipulating strings.
maybe_t<int> builtin_string(parser_t &parser, io_streams_t &streams, const wchar_t **argv) {
    const wchar_t *cmd = argv[0];
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
    const auto *subcmd = get_by_sorted_name(subcmd_name, string_subcommands);
    if (!subcmd) {
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
