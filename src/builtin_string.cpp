// Implementation of the string builtin.
#include "config.h"

#define PCRE2_CODE_UNIT_WIDTH WCHAR_T_BITS
#ifdef _WIN32
#define PCRE2_STATIC
#endif
#include <errno.h>
#include <limits.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <wchar.h>
#include <wctype.h>

#include <algorithm>
#include <cwctype>
#include <iterator>
#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "builtin.h"
#include "common.h"
#include "fallback.h"  // IWYU pragma: keep
#include "io.h"
#include "parse_util.h"
#include "pcre2.h"
#include "wgetopt.h"
#include "wildcard.h"
#include "wutil.h"  // IWYU pragma: keep

class parser_t;

#define STRING_ERR_MISSING _(L"%ls: Expected argument\n")

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
    builtin_print_help(parser, streams, L"string", streams.err);
}

// We read from stdin if we are the second or later process in a pipeline.
static bool string_args_from_stdin(const io_streams_t &streams) {
    return streams.stdin_is_directly_redirected;
}

static const wchar_t *string_get_arg_stdin(wcstring *storage, const io_streams_t &streams) {
    std::string arg;
    for (;;) {
        char ch = '\0';
        long rc = read_blocked(streams.stdin_fd, &ch, 1);

        if (rc < 0) {  // failure
            return NULL;
        }

        if (rc == 0) {  // EOF
            if (arg.empty()) {
                return NULL;
            }
            break;
        }

        if (ch == '\n') {
            break;
        }

        arg += ch;
    }

    *storage = str2wcstring(arg);
    return storage->c_str();
}

static const wchar_t *string_get_arg_argv(int *argidx, wchar_t **argv) {
    return argv && argv[*argidx] ? argv[(*argidx)++] : NULL;
}

static const wchar_t *string_get_arg(int *argidx, wchar_t **argv, wcstring *storage,
                                     const io_streams_t &streams) {
    if (string_args_from_stdin(streams)) {
        return string_get_arg_stdin(storage, streams);
    }
    return string_get_arg_argv(argidx, argv);
}

// This is used by the string subcommands to communicate with the option parser which flags are
// valid and get the result of parsing the command for flags.
typedef struct {  //!OCLINT(too many fields)
    bool all_valid = false;
    bool chars_valid = false;
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
    bool style_valid = false;

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

    long count = 0;
    long length = 0;
    long max = 0;
    long start = 0;

    const wchar_t *chars_to_trim = L" \f\n\r\t";
    const wchar_t *arg1 = NULL;
    const wchar_t *arg2 = NULL;

    escape_string_style_t escape_style = STRING_STYLE_SCRIPT;
} options_t;

/// This handles the `--style=xxx` flag.
static int handle_flag_1(wchar_t **argv, parser_t &parser, io_streams_t &streams, wgetopter_t &w,
                         options_t *opts) {
    const wchar_t *cmd = argv[0];

    if (opts->style_valid) {
        if (wcscmp(w.woptarg, L"script") == 0) {
            opts->escape_style = STRING_STYLE_SCRIPT;
        } else if (wcscmp(w.woptarg, L"url") == 0) {
            opts->escape_style = STRING_STYLE_URL;
        } else if (wcscmp(w.woptarg, L"var") == 0) {
            opts->escape_style = STRING_STYLE_VAR;
        } else {
            string_error(streams, _(L"%ls: Invalid escape style '%ls'\n"), cmd, w.woptarg);
            return STATUS_INVALID_ARGS;
        }
        return STATUS_CMD_OK;
    }

    string_unknown_option(parser, streams, cmd, argv[w.woptind - 1]);
    return STATUS_INVALID_ARGS;
}

static int handle_flag_N(wchar_t **argv, parser_t &parser, io_streams_t &streams, wgetopter_t &w,
                         options_t *opts) {
    if (opts->no_newline_valid) {
        opts->no_newline = true;
        return STATUS_CMD_OK;
    }
    string_unknown_option(parser, streams, argv[0], argv[w.woptind - 1]);
    return STATUS_INVALID_ARGS;
}

static int handle_flag_a(wchar_t **argv, parser_t &parser, io_streams_t &streams, wgetopter_t &w,
                         options_t *opts) {
    if (opts->all_valid) {
        opts->all = true;
        return STATUS_CMD_OK;
    }
    string_unknown_option(parser, streams, argv[0], argv[w.woptind - 1]);
    return STATUS_INVALID_ARGS;
}

static int handle_flag_c(wchar_t **argv, parser_t &parser, io_streams_t &streams, wgetopter_t &w,
                         options_t *opts) {
    if (opts->chars_valid) {
        opts->chars_to_trim = w.woptarg;
        return STATUS_CMD_OK;
    }
    string_unknown_option(parser, streams, argv[0], argv[w.woptind - 1]);
    return STATUS_INVALID_ARGS;
}

static int handle_flag_e(wchar_t **argv, parser_t &parser, io_streams_t &streams, wgetopter_t &w,
                         options_t *opts) {
    if (opts->entire_valid) {
        opts->entire = true;
        return STATUS_CMD_OK;
    }
    string_unknown_option(parser, streams, argv[0], argv[w.woptind - 1]);
    return STATUS_INVALID_ARGS;
}

static int handle_flag_f(wchar_t **argv, parser_t &parser, io_streams_t &streams, wgetopter_t &w,
                         options_t *opts) {
    if (opts->filter_valid) {
        opts->filter = true;
        return STATUS_CMD_OK;
    }
    string_unknown_option(parser, streams, argv[0], argv[w.woptind - 1]);
    return STATUS_INVALID_ARGS;
}

static int handle_flag_i(wchar_t **argv, parser_t &parser, io_streams_t &streams, wgetopter_t &w,
                         options_t *opts) {
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

static int handle_flag_l(wchar_t **argv, parser_t &parser, io_streams_t &streams, wgetopter_t &w,
                         options_t *opts) {
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

static int handle_flag_m(wchar_t **argv, parser_t &parser, io_streams_t &streams, wgetopter_t &w,
                         options_t *opts) {
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

static int handle_flag_n(wchar_t **argv, parser_t &parser, io_streams_t &streams, wgetopter_t &w,
                         options_t *opts) {
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
    }
    string_unknown_option(parser, streams, argv[0], argv[w.woptind - 1]);
    return STATUS_INVALID_ARGS;
}

static int handle_flag_q(wchar_t **argv, parser_t &parser, io_streams_t &streams, wgetopter_t &w,
                         options_t *opts) {
    if (opts->quiet_valid) {
        opts->quiet = true;
        return STATUS_CMD_OK;
    }
    string_unknown_option(parser, streams, argv[0], argv[w.woptind - 1]);
    return STATUS_INVALID_ARGS;
}

static int handle_flag_r(wchar_t **argv, parser_t &parser, io_streams_t &streams, wgetopter_t &w,
                         options_t *opts) {
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

static int handle_flag_s(wchar_t **argv, parser_t &parser, io_streams_t &streams, wgetopter_t &w,
                         options_t *opts) {
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

static int handle_flag_v(wchar_t **argv, parser_t &parser, io_streams_t &streams, wgetopter_t &w,
                         options_t *opts) {
    if (opts->invert_valid) {
        opts->invert_match = true;
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
    if (opts->chars_valid) short_opts.append(L"c:");
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
    return short_opts;
}

// Note that several long flags share the same short flag. That is okay. The caller is expected
// to indicate that a max of one of the long flags sharing a short flag is valid.
static const struct woption long_options[] = {{L"all", no_argument, NULL, 'a'},
                                              {L"chars", required_argument, NULL, 'c'},
                                              {L"count", required_argument, NULL, 'n'},
                                              {L"entire", no_argument, NULL, 'e'},
                                              {L"filter", no_argument, NULL, 'f'},
                                              {L"ignore-case", no_argument, NULL, 'i'},
                                              {L"index", no_argument, NULL, 'n'},
                                              {L"invert", no_argument, NULL, 'v'},
                                              {L"left", no_argument, NULL, 'l'},
                                              {L"length", required_argument, NULL, 'l'},
                                              {L"max", required_argument, NULL, 'm'},
                                              {L"no-newline", no_argument, NULL, 'N'},
                                              {L"no-quoted", no_argument, NULL, 'n'},
                                              {L"quiet", no_argument, NULL, 'q'},
                                              {L"regex", no_argument, NULL, 'r'},
                                              {L"right", no_argument, NULL, 'r'},
                                              {L"start", required_argument, NULL, 's'},
                                              {L"style", required_argument, NULL, 1},
                                              {NULL, 0, NULL, 0}};

static std::map<char, decltype(*handle_flag_N)> flag_to_function = {
    {'N', handle_flag_N}, {'a', handle_flag_a}, {'c', handle_flag_c}, {'e', handle_flag_e},
    {'f', handle_flag_f}, {'i', handle_flag_i}, {'l', handle_flag_l}, {'m', handle_flag_m},
    {'n', handle_flag_n}, {'q', handle_flag_q}, {'r', handle_flag_r}, {'s', handle_flag_s},
    {'v', handle_flag_v}, {1, handle_flag_1}};

/// Parse the arguments for flags recognized by a specific string subcommand.
static int parse_opts(options_t *opts, int *optind, int n_req_args, int argc, wchar_t **argv,
                      parser_t &parser, io_streams_t &streams) {
    const wchar_t *cmd = argv[0];
    wcstring short_opts = construct_short_opts(opts);
    const wchar_t *short_options = short_opts.c_str();
    int opt;
    wgetopter_t w;
    while ((opt = w.wgetopt_long(argc, argv, short_options, long_options, NULL)) != -1) {
        auto fn = flag_to_function.find(opt);
        if (fn != flag_to_function.end()) {
            int retval = fn->second(argv, parser, streams, w, opts);
            if (retval != STATUS_CMD_OK) return retval;
        } else if (opt == ':') {
            string_error(streams, STRING_ERR_MISSING, cmd);
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
        if (!opts->arg1) {
            string_error(streams, STRING_ERR_MISSING, cmd);
            return STATUS_INVALID_ARGS;
        }
    }
    if (n_req_args > 1) {
        opts->arg2 = string_get_arg_argv(optind, argv);
        if (!opts->arg2) {
            string_error(streams, STRING_ERR_MISSING, cmd);
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

/// Escape a string so that it can be used in a fish script without further word splitting.
static int string_escape_script(options_t &opts, int optind, wchar_t **argv,
                                io_streams_t &streams) {
    wcstring storage;
    int nesc = 0;
    escape_flags_t flags = ESCAPE_ALL;
    if (opts.no_quoted) flags |= ESCAPE_NO_QUOTED;

    while (const wchar_t *arg = string_get_arg(&optind, argv, &storage, streams)) {
        streams.out.append(escape_string(arg, flags, STRING_STYLE_SCRIPT));
        streams.out.append(L'\n');
        nesc++;
    }

    return nesc > 0 ? STATUS_CMD_OK : STATUS_CMD_ERROR;
}

/// Escape a string so that it can be used as a URL.
static int string_escape_url(options_t &opts, int optind, wchar_t **argv, io_streams_t &streams) {
    UNUSED(opts);
    wcstring storage;
    int nesc = 0;
    escape_flags_t flags = 0;

    while (const wchar_t *arg = string_get_arg(&optind, argv, &storage, streams)) {
        streams.out.append(escape_string(arg, flags, STRING_STYLE_URL));
        streams.out.append(L'\n');
        nesc++;
    }

    return nesc > 0 ? STATUS_CMD_OK : STATUS_CMD_ERROR;
}

/// Escape a string so that it can be used as a fish var name.
static int string_escape_var(options_t &opts, int optind, wchar_t **argv, io_streams_t &streams) {
    UNUSED(opts);
    wcstring storage;
    int nesc = 0;
    escape_flags_t flags = 0;

    while (const wchar_t *arg = string_get_arg(&optind, argv, &storage, streams)) {
        streams.out.append(escape_string(arg, flags, STRING_STYLE_VAR));
        streams.out.append(L'\n');
        nesc++;
    }

    return nesc > 0 ? STATUS_CMD_OK : STATUS_CMD_ERROR;
}

/// Unescape a string encoded so it can be used in fish script.
static int string_unescape_script(options_t &opts, int optind, wchar_t **argv,
                                  io_streams_t &streams) {
    UNUSED(opts);
    wcstring storage;
    int nesc = 0;
    unescape_flags_t flags = 0;

    while (const wchar_t *arg = string_get_arg(&optind, argv, &storage, streams)) {
        wcstring result;
        if (unescape_string(arg, &result, flags, STRING_STYLE_SCRIPT)) {
            streams.out.append(result);
            streams.out.append(L'\n');
            nesc++;
        }
    }

    return nesc > 0 ? STATUS_CMD_OK : STATUS_CMD_ERROR;
}

/// Unescape an encoded URL.
static int string_unescape_url(options_t &opts, int optind, wchar_t **argv, io_streams_t &streams) {
    UNUSED(opts);
    wcstring storage;
    int nesc = 0;
    unescape_flags_t flags = 0;

    while (const wchar_t *arg = string_get_arg(&optind, argv, &storage, streams)) {
        wcstring result;
        if (unescape_string(arg, &result, flags, STRING_STYLE_URL)) {
            streams.out.append(result);
            streams.out.append(L'\n');
            nesc++;
        }
    }

    return nesc > 0 ? STATUS_CMD_OK : STATUS_CMD_ERROR;
}

/// Unescape an encoded var name.
static int string_unescape_var(options_t &opts, int optind, wchar_t **argv, io_streams_t &streams) {
    UNUSED(opts);
    wcstring storage;
    int nesc = 0;
    unescape_flags_t flags = 0;

    while (const wchar_t *arg = string_get_arg(&optind, argv, &storage, streams)) {
        wcstring result;
        if (unescape_string(arg, &result, flags, STRING_STYLE_VAR)) {
            streams.out.append(result);
            streams.out.append(L'\n');
            nesc++;
        }
    }

    return nesc > 0 ? STATUS_CMD_OK : STATUS_CMD_ERROR;
}

static int string_escape(parser_t &parser, io_streams_t &streams, int argc, wchar_t **argv) {
    options_t opts;
    opts.no_quoted_valid = true;
    opts.style_valid = true;
    int optind;
    int retval = parse_opts(&opts, &optind, 0, argc, argv, parser, streams);
    if (retval != STATUS_CMD_OK) return retval;

    switch (opts.escape_style) {
        case STRING_STYLE_SCRIPT: {
            return string_escape_script(opts, optind, argv, streams);
        }
        case STRING_STYLE_URL: {
            return string_escape_url(opts, optind, argv, streams);
        }
        case STRING_STYLE_VAR: {
            return string_escape_var(opts, optind, argv, streams);
        }
    }

    DIE("should never reach this statement");
}

static int string_unescape(parser_t &parser, io_streams_t &streams, int argc, wchar_t **argv) {
    options_t opts;
    opts.no_quoted_valid = true;
    opts.style_valid = true;
    int optind;
    int retval = parse_opts(&opts, &optind, 0, argc, argv, parser, streams);
    if (retval != STATUS_CMD_OK) return retval;

    switch (opts.escape_style) {
        case STRING_STYLE_SCRIPT: {
            return string_unescape_script(opts, optind, argv, streams);
        }
        case STRING_STYLE_URL: {
            return string_unescape_url(opts, optind, argv, streams);
        }
        case STRING_STYLE_VAR: {
            return string_unescape_var(opts, optind, argv, streams);
        }
    }

    DIE("should never reach this statement");
}

static int string_join(parser_t &parser, io_streams_t &streams, int argc, wchar_t **argv) {
    options_t opts;
    opts.quiet_valid = true;
    int optind;
    int retval = parse_opts(&opts, &optind, 1, argc, argv, parser, streams);
    if (retval != STATUS_CMD_OK) return retval;

    const wchar_t *sep = opts.arg1;
    int nargs = 0;
    const wchar_t *arg;
    wcstring storage;
    while ((arg = string_get_arg(&optind, argv, &storage, streams)) != 0) {
        if (!opts.quiet) {
            if (nargs > 0) {
                streams.out.append(sep);
            }
            streams.out.append(arg);
        }
        nargs++;
    }
    if (nargs > 0 && !opts.quiet) {
        streams.out.push_back(L'\n');
    }

    return nargs > 1 ? STATUS_CMD_OK : STATUS_CMD_ERROR;
}

static int string_length(parser_t &parser, io_streams_t &streams, int argc, wchar_t **argv) {
    options_t opts;
    opts.quiet_valid = true;
    int optind;
    int retval = parse_opts(&opts, &optind, 0, argc, argv, parser, streams);
    if (retval != STATUS_CMD_OK) return retval;

    const wchar_t *arg;
    int nnonempty = 0;
    wcstring storage;
    while ((arg = string_get_arg(&optind, argv, &storage, streams)) != 0) {
        size_t n = wcslen(arg);
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
    string_matcher_t(const options_t &opts_, io_streams_t &streams_)
        : opts(opts_), streams(streams_), total_matched(0) {}

    virtual ~string_matcher_t() {}
    virtual bool report_matches(const wchar_t *arg) = 0;
    int match_count() { return total_matched; }
};

class wildcard_matcher_t : public string_matcher_t {
   private:
    wcstring wcpattern;

   public:
    wildcard_matcher_t(const wchar_t * /*argv0*/, const wchar_t *pattern, const options_t &opts,
                       io_streams_t &streams)
        : string_matcher_t(opts, streams), wcpattern(parse_util_unescape_wildcards(pattern)) {
        if (opts.ignore_case) {
            for (size_t i = 0; i < wcpattern.length(); i++) {
                wcpattern[i] = towlower(wcpattern[i]);
            }
        }
        if (opts.entire && !wcpattern.empty()) {
            if (wcpattern.front() != ANY_STRING) wcpattern.insert(0, 1, ANY_STRING);
            if (wcpattern.back() != ANY_STRING) wcpattern.push_back(ANY_STRING);
        }
    }

    virtual ~wildcard_matcher_t() {}

    bool report_matches(const wchar_t *arg) {
        // Note: --all is a no-op for glob matching since the pattern is always matched
        // against the entire argument.
        bool match;

        if (opts.ignore_case) {
            wcstring s = arg;
            for (size_t i = 0; i < s.length(); i++) {
                s[i] = towlower(s[i]);
            }
            match = wildcard_match(s, wcpattern, false);
        } else {
            match = wildcard_match(arg, wcpattern, false);
        }
        if (match ^ opts.invert_match) {
            total_matched++;

            if (!opts.quiet) {
                if (opts.index) {
                    streams.out.append_format(L"1 %lu\n", wcslen(arg));
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
    pcre2_get_error_message(err_code, (PCRE2_UCHAR *)buf, sizeof(buf) / sizeof(wchar_t));
    return buf;
}

struct compiled_regex_t {
    pcre2_code *code;
    pcre2_match_data *match;

    compiled_regex_t(const wchar_t *argv0, const wchar_t *pattern, bool ignore_case,
                     io_streams_t &streams)
        : code(0), match(0) {
        // Disable some sequences that can lead to security problems.
        uint32_t options = PCRE2_NEVER_UTF;
#if PCRE2_CODE_UNIT_WIDTH < 32
        options |= PCRE2_NEVER_BACKSLASH_C;
#endif

        int err_code = 0;
        PCRE2_SIZE err_offset = 0;

        code =
            pcre2_compile(PCRE2_SPTR(pattern), PCRE2_ZERO_TERMINATED,
                          options | (ignore_case ? PCRE2_CASELESS : 0), &err_code, &err_offset, 0);
        if (code == 0) {
            string_error(streams, _(L"%ls: Regular expression compile error: %ls\n"), argv0,
                         pcre2_strerror(err_code).c_str());
            string_error(streams, L"%ls: %ls\n", argv0, pattern);
            string_error(streams, L"%ls: %*ls\n", argv0, err_offset, L"^");
            return;
        }

        match = pcre2_match_data_create_from_pattern(code, 0);
        assert(match);
    }

    ~compiled_regex_t() {
        if (match != 0) {
            pcre2_match_data_free(match);
        }
        if (code != 0) {
            pcre2_code_free(code);
        }
    }
};

class pcre2_matcher_t : public string_matcher_t {
    const wchar_t *argv0;
    compiled_regex_t regex;

    int report_match(const wchar_t *arg, int pcre2_rc) {
        // Return values: -1 = error, 0 = no match, 1 = match.
        if (pcre2_rc == PCRE2_ERROR_NOMATCH) {
            if (opts.invert_match && !opts.quiet) {
                if (opts.index) {
                    streams.out.append_format(L"1 %lu\n", wcslen(arg));
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

        if (opts.entire) {
            streams.out.append(arg);
            streams.out.push_back(L'\n');
        }

        PCRE2_SIZE *ovector = pcre2_get_ovector_pointer(regex.match);
        for (int j = (opts.entire ? 1 : 0); j < pcre2_rc; j++) {
            PCRE2_SIZE begin = ovector[2 * j];
            PCRE2_SIZE end = ovector[2 * j + 1];

            if (begin != PCRE2_UNSET && end != PCRE2_UNSET && !opts.quiet) {
                if (opts.index) {
                    streams.out.append_format(L"%lu %lu", (unsigned long)(begin + 1),
                                              (unsigned long)(end - begin));
                } else if (end > begin) {
                    // May have end < begin if \K is used.
                    streams.out.append(wcstring(&arg[begin], end - begin));
                }
                streams.out.push_back(L'\n');
            }
        }

        return opts.invert_match ? 0 : 1;
    }

   public:
    pcre2_matcher_t(const wchar_t *argv0_, const wchar_t *pattern, const options_t &opts,
                    io_streams_t &streams)
        : string_matcher_t(opts, streams),
          argv0(argv0_),
          regex(argv0_, pattern, opts.ignore_case, streams) {}

    virtual ~pcre2_matcher_t() {}

    bool report_matches(const wchar_t *arg) {
        // A return value of true means all is well (even if no matches were found), false indicates
        // an unrecoverable error.
        if (regex.code == 0) {
            // pcre2_compile() failed.
            return false;
        }

        int matched = 0;

        // See pcre2demo.c for an explanation of this logic.
        PCRE2_SIZE arglen = wcslen(arg);
        int rc = report_match(
            arg, pcre2_match(regex.code, PCRE2_SPTR(arg), arglen, 0, 0, regex.match, 0));
        if (rc < 0) {  // pcre2 match error.
            return false;
        } else if (rc == 0) {  // no match
            return true;
        }
        matched++;
        total_matched++;

        if (opts.invert_match) {
            return true;
        }

        // Report any additional matches.
        PCRE2_SIZE *ovector = pcre2_get_ovector_pointer(regex.match);
        while (opts.all || matched == 0) {
            uint32_t options = 0;
            PCRE2_SIZE offset = ovector[1];  // start at end of previous match

            if (ovector[0] == ovector[1]) {
                if (ovector[0] == arglen) {
                    break;
                }
                options = PCRE2_NOTEMPTY_ATSTART | PCRE2_ANCHORED;
            }

            rc = report_match(arg, pcre2_match(regex.code, PCRE2_SPTR(arg), arglen, offset, options,
                                               regex.match, 0));
            if (rc < 0) {
                return false;
            }
            if (rc == 0) {
                if (options == 0) {  // all matches found
                    break;
                }
                ovector[1] = offset + 1;
                continue;
            }
            matched++;
            total_matched++;
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

    const wchar_t *arg;
    wcstring storage;
    while ((arg = string_get_arg(&optind, argv, &storage, streams)) != 0) {
        if (!matcher->report_matches(arg)) {
            return STATUS_INVALID_ARGS;
        }
    }

    return matcher->match_count() > 0 ? STATUS_CMD_OK : STATUS_CMD_ERROR;
}

class string_replacer_t {
   protected:
    const wchar_t *argv0;
    options_t opts;
    int total_replaced;
    io_streams_t &streams;

   public:
    string_replacer_t(const wchar_t *argv0_, const options_t &opts_, io_streams_t &streams_)
        : argv0(argv0_), opts(opts_), total_replaced(0), streams(streams_) {}

    virtual ~string_replacer_t() {}
    int replace_count() { return total_replaced; }
    virtual bool replace_matches(const wchar_t *arg) = 0;
};

class literal_replacer_t : public string_replacer_t {
    const wchar_t *pattern;
    const wchar_t *replacement;
    size_t patlen;

   public:
    literal_replacer_t(const wchar_t *argv0, const wchar_t *pattern_, const wchar_t *replacement_,
                       const options_t &opts, io_streams_t &streams)
        : string_replacer_t(argv0, opts, streams),
          pattern(pattern_),
          replacement(replacement_),
          patlen(wcslen(pattern)) {}

    virtual ~literal_replacer_t() {}
    bool replace_matches(const wchar_t *arg);
};

static wcstring interpret_escapes(const wchar_t *orig) {
    wcstring result;

    while (*orig != L'\0') {
        if (*orig == L'\\') {
            orig += read_unquoted_escape(orig, &result, true, false);
        } else {
            result += *orig;
            orig++;
        }
    }

    return result;
}

class regex_replacer_t : public string_replacer_t {
    compiled_regex_t regex;
    wcstring replacement;

   public:
    regex_replacer_t(const wchar_t *argv0, const wchar_t *pattern, const wchar_t *replacement_,
                     const options_t &opts, io_streams_t &streams)
        : string_replacer_t(argv0, opts, streams),
          regex(argv0, pattern, opts.ignore_case, streams),
          replacement(interpret_escapes(replacement_)) {}

    bool replace_matches(const wchar_t *arg);
};

/// A return value of true means all is well (even if no replacements were performed), false
/// indicates an unrecoverable error.
bool literal_replacer_t::replace_matches(const wchar_t *arg) {
    wcstring result;
    bool replacement_occurred = false;

    if (patlen == 0) {
        replacement_occurred = true;
        result = arg;
    } else {
        auto &cmp_func = opts.ignore_case ? wcsncasecmp : wcsncmp;
        const wchar_t *cur = arg;
        while (*cur != L'\0') {
            if ((opts.all || !replacement_occurred) && cmp_func(cur, pattern, patlen) == 0) {
                result += replacement;
                cur += patlen;
                replacement_occurred = true;
                total_replaced++;
            } else {
                result += *cur;
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
bool regex_replacer_t::replace_matches(const wchar_t *arg) {
    if (!regex.code) return false;  // pcre2_compile() failed

    uint32_t options = PCRE2_SUBSTITUTE_OVERFLOW_LENGTH | PCRE2_SUBSTITUTE_EXTENDED |
                       (opts.all ? PCRE2_SUBSTITUTE_GLOBAL : 0);
    size_t arglen = wcslen(arg);
    PCRE2_SIZE bufsize = (arglen == 0) ? 16 : 2 * arglen;
    wchar_t *output = (wchar_t *)malloc(sizeof(wchar_t) * bufsize);
    int pcre2_rc;

    bool done = false;
    while (!done) {
        assert(output);

        PCRE2_SIZE outlen = bufsize;
        pcre2_rc = pcre2_substitute(regex.code, PCRE2_SPTR(arg), arglen,
                                    0,  // start offset
                                    options, regex.match,
                                    0,  // match context
                                    PCRE2_SPTR(replacement.c_str()), PCRE2_ZERO_TERMINATED,
                                    (PCRE2_UCHAR *)output, &outlen);

        if (pcre2_rc != PCRE2_ERROR_NOMEMORY || bufsize >= outlen) {
            done = true;
        } else {
            bufsize = outlen;
            wchar_t *new_output = (wchar_t *)realloc(output, sizeof(wchar_t) * bufsize);
            if (new_output) output = new_output;
        }
    }

    bool rc = true;
    if (pcre2_rc < 0) {
        string_error(streams, _(L"%ls: Regular expression substitute error: %ls\n"), argv0,
                     pcre2_strerror(pcre2_rc).c_str());
        rc = false;
    } else {
        bool replacement_occurred = pcre2_rc > 0;
        if (!opts.quiet && (!opts.filter || replacement_occurred)) {
            streams.out.append(output);
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

    wcstring storage;
    while (const wchar_t *arg = string_get_arg(&optind, argv, &storage, streams)) {
        if (!replacer->replace_matches(arg)) return STATUS_INVALID_ARGS;
    }

    return replacer->replace_count() > 0 ? STATUS_CMD_OK : STATUS_CMD_ERROR;
}

/// Given iterators into a string (forward or reverse), splits the haystack iterators
/// about the needle sequence, up to max times. Inserts splits into the output array.
/// If the iterators are forward, this does the normal thing.
/// If the iterators are backward, this returns reversed strings, in reversed order!
/// If the needle is empty, split on individual elements (characters).
template <typename ITER>
void split_about(ITER haystack_start, ITER haystack_end, ITER needle_start, ITER needle_end,
                 wcstring_list_t *output, long max) {
    long remaining = max;
    ITER haystack_cursor = haystack_start;
    while (remaining > 0 && haystack_cursor != haystack_end) {
        ITER split_point;
        if (needle_start == needle_end) {  // empty needle, we split on individual elements
            split_point = haystack_cursor + 1;
        } else {
            split_point = std::search(haystack_cursor, haystack_end, needle_start, needle_end);
        }
        if (split_point == haystack_end) {  // not found
            break;
        }
        output->push_back(wcstring(haystack_cursor, split_point));
        remaining--;
        // Need to skip over the needle for the next search note that the needle may be empty.
        haystack_cursor = split_point + std::distance(needle_start, needle_end);
    }
    // Trailing component, possibly empty.
    output->push_back(wcstring(haystack_cursor, haystack_end));
}

static int string_split(parser_t &parser, io_streams_t &streams, int argc, wchar_t **argv) {
    options_t opts;
    opts.quiet_valid = true;
    opts.right_valid = true;
    opts.max_valid = true;
    opts.max = LONG_MAX;
    int optind;
    int retval = parse_opts(&opts, &optind, 1, argc, argv, parser, streams);
    if (retval != STATUS_CMD_OK) return retval;

    const wchar_t *sep = opts.arg1;
    const wchar_t *sep_end = sep + wcslen(sep);

    wcstring_list_t splits;
    size_t arg_count = 0;
    wcstring storage;
    const wchar_t *arg;
    while ((arg = string_get_arg(&optind, argv, &storage, streams)) != 0) {
        const wchar_t *arg_end = arg + wcslen(arg);
        if (opts.right) {
            typedef std::reverse_iterator<const wchar_t *> reverser;
            split_about(reverser(arg_end), reverser(arg), reverser(sep_end), reverser(sep), &splits,
                        opts.max);
        } else {
            split_about(arg, arg_end, sep, sep_end, &splits, opts.max);
        }
        arg_count++;
    }

    // If we are from the right, split_about gave us reversed strings, in reversed order!
    if (opts.right) {
        for (size_t j = 0; j < splits.size(); j++) {
            std::reverse(splits[j].begin(), splits[j].end());
        }
        std::reverse(splits.begin(), splits.end());
    }

    if (!opts.quiet) {
        for (wcstring_list_t::const_iterator si = splits.begin(); si != splits.end(); ++si) {
            streams.out.append(*si);
            streams.out.append(L'\n');
        }
    }

    // We split something if we have more split values than args.
    return splits.size() > arg_count ? STATUS_CMD_OK : STATUS_CMD_ERROR;
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

    const wchar_t *to_repeat;
    wcstring storage;
    bool is_empty = true;

    if ((to_repeat = string_get_arg(&optind, argv, &storage, streams)) != NULL && *to_repeat) {
        const wcstring word(to_repeat);
        const bool limit_repeat =
            (opts.max > 0 && word.length() * opts.count > (size_t)opts.max) || !opts.count;
        const wcstring repeated =
            limit_repeat ? wcsrepeat_until(word, opts.max) : wcsrepeat(word, opts.count);
        is_empty = repeated.empty();

        if (!opts.quiet && !is_empty) {
            streams.out.append(repeated);
            if (!opts.no_newline) streams.out.append(L"\n");
        }
    }

    return !is_empty ? STATUS_CMD_OK : STATUS_CMD_ERROR;
}

static int string_sub(parser_t &parser, io_streams_t &streams, int argc, wchar_t **argv) {
    options_t opts;
    opts.length_valid = true;
    opts.quiet_valid = true;
    opts.start_valid = true;
    opts.length = -1;
    int optind;
    int retval = parse_opts(&opts, &optind, 0, argc, argv, parser, streams);
    if (retval != STATUS_CMD_OK) return retval;

    int nsub = 0;
    const wchar_t *arg;
    wcstring storage;
    while ((arg = string_get_arg(&optind, argv, &storage, streams)) != NULL) {
        typedef wcstring::size_type size_type;
        size_type pos = 0;
        size_type count = wcstring::npos;
        wcstring s(arg);
        if (opts.start > 0) {
            pos = static_cast<size_type>(opts.start - 1);
        } else if (opts.start < 0) {
            assert(opts.start != LONG_MIN);  // checked above
            size_type n = static_cast<size_type>(-opts.start);
            pos = n > s.length() ? 0 : s.length() - n;
        }
        if (pos > s.length()) {
            pos = s.length();
        }

        if (opts.length >= 0) {
            count = static_cast<size_type>(opts.length);
        }

        // Note that std::string permits count to extend past end of string.
        if (!opts.quiet) {
            streams.out.append(s.substr(pos, count));
            streams.out.append(L'\n');
        }
        nsub++;
    }

    return nsub > 0 ? STATUS_CMD_OK : STATUS_CMD_ERROR;
}

static int string_trim(parser_t &parser, io_streams_t &streams, int argc, wchar_t **argv) {
    options_t opts;
    opts.chars_valid = true;
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

    const wchar_t *arg;
    size_t ntrim = 0;

    wcstring argstr;
    wcstring storage;
    while ((arg = string_get_arg(&optind, argv, &storage, streams)) != 0) {
        argstr = arg;
        // Begin and end are respectively the first character to keep on the left, and first
        // character to trim on the right. The length is thus end - start.
        size_t begin = 0, end = argstr.size();
        if (opts.right) {
            size_t last_to_keep = argstr.find_last_not_of(opts.chars_to_trim);
            end = (last_to_keep == wcstring::npos) ? 0 : last_to_keep + 1;
        }
        if (opts.left) {
            size_t first_to_keep = argstr.find_first_not_of(opts.chars_to_trim);
            begin = (first_to_keep == wcstring::npos ? end : first_to_keep);
        }
        assert(begin <= end && end <= argstr.size());
        ntrim += argstr.size() - (end - begin);
        if (!opts.quiet) {
            streams.out.append(wcstring(argstr, begin, end - begin));
            streams.out.append(L'\n');
        }
    }

    return ntrim > 0 ? STATUS_CMD_OK : STATUS_CMD_ERROR;
}

/// Implementation of `string lower`.
static int string_lower(parser_t &parser, io_streams_t &streams, int argc, wchar_t **argv) {
    options_t opts;
    opts.quiet_valid = true;
    int optind;
    int retval = parse_opts(&opts, &optind, 0, argc, argv, parser, streams);
    if (retval != STATUS_CMD_OK) return retval;

    int n_transformed = 0;
    wcstring storage;
    while (const wchar_t *arg = string_get_arg(&optind, argv, &storage, streams)) {
        wcstring transformed(arg);
        std::transform(transformed.begin(), transformed.end(), transformed.begin(), std::towlower);
        if (wcscmp(transformed.c_str(), arg)) n_transformed++;
        if (!opts.quiet) {
            streams.out.append(transformed);
            streams.out.append(L'\n');
        }
    }

    return n_transformed > 0 ? STATUS_CMD_OK : STATUS_CMD_ERROR;
}

/// Implementation of `string upper`.
static int string_upper(parser_t &parser, io_streams_t &streams, int argc, wchar_t **argv) {
    options_t opts;
    opts.quiet_valid = true;
    int optind;
    int retval = parse_opts(&opts, &optind, 0, argc, argv, parser, streams);
    if (retval != STATUS_CMD_OK) return retval;

    int n_transformed = 0;
    wcstring storage;
    while (const wchar_t *arg = string_get_arg(&optind, argv, &storage, streams)) {
        wcstring transformed(arg);
        std::transform(transformed.begin(), transformed.end(), transformed.begin(), std::towupper);
        if (wcscmp(transformed.c_str(), arg)) n_transformed++;
        if (!opts.quiet) {
            streams.out.append(transformed);
            streams.out.append(L'\n');
        }
    }

    return n_transformed > 0 ? STATUS_CMD_OK : STATUS_CMD_ERROR;
}

static const struct string_subcommand {
    const wchar_t *name;
    int (*handler)(parser_t &, io_streams_t &, int argc,  //!OCLINT(unused param)
                   wchar_t **argv);                       //!OCLINT(unused param)
}

string_subcommands[] = {{L"escape", &string_escape},
                        {L"join", &string_join},
                        {L"length", &string_length},
                        {L"match", &string_match},
                        {L"replace", &string_replace},
                        {L"split", &string_split},
                        {L"sub", &string_sub},
                        {L"trim", &string_trim},
                        {L"lower", &string_lower},
                        {L"upper", &string_upper},
                        {L"repeat", &string_repeat},
                        {L"unescape", &string_unescape},
                        {NULL, NULL}};

/// The string builtin, for manipulating strings.
int builtin_string(parser_t &parser, io_streams_t &streams, wchar_t **argv) {
    wchar_t *cmd = argv[0];
    int argc = builtin_count_args(argv);
    if (argc <= 1) {
        streams.err.append_format(BUILTIN_ERR_MISSING_SUBCMD, cmd);
        builtin_print_help(parser, streams, L"string", streams.err);
        return STATUS_INVALID_ARGS;
    }

    if (wcscmp(argv[1], L"-h") == 0 || wcscmp(argv[1], L"--help") == 0) {
        builtin_print_help(parser, streams, L"string", streams.err);
        return STATUS_CMD_OK;
    }

    const string_subcommand *subcmd = &string_subcommands[0];
    while (subcmd->name != 0 && wcscmp(subcmd->name, argv[1]) != 0) {
        subcmd++;
    }
    if (!subcmd->handler) {
        streams.err.append_format(BUILTIN_ERR_INVALID_SUBCMD, cmd, argv[1]);
        builtin_print_help(parser, streams, L"string", streams.err);
        return STATUS_INVALID_ARGS;
    }

    argc--;
    argv++;
    return subcmd->handler(parser, streams, argc, argv);
}
