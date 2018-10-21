// Implementation of the math builtin.
#include "config.h"  // IWYU pragma: keep

#include <errno.h>
#include <stddef.h>

#include <algorithm>
#include <cmath>
#include <limits>
#include <string>

#include "tinyexpr.h"

#include "builtin.h"
#include "builtin_math.h"
#include "common.h"
#include "fallback.h"  // IWYU pragma: keep
#include "io.h"
#include "wgetopt.h"
#include "wutil.h"  // IWYU pragma: keep

// The maximum number of points after the decimal that we'll print.
static constexpr int kDefaultScale = 6;

// The end of the range such that every integer is representable as a double.
// i.e. this is the first value such that x + 1 == x (or == x + 2, depending on rounding mode).
static constexpr double kMaximumContiguousInteger =
    double(1LLU << std::numeric_limits<double>::digits);

struct math_cmd_opts_t {
    bool print_help = false;
    int scale = kDefaultScale;
};

// This command is atypical in using the "+" (REQUIRE_ORDER) option for flag parsing.
// This is needed because of the minus, `-`, operator in math expressions.
static const wchar_t *const short_options = L"+:hs:";
static const struct woption long_options[] = {{L"scale", required_argument, NULL, 's'},
                                              {L"help", no_argument, NULL, 'h'},
                                              {NULL, 0, NULL, 0}};

static int parse_cmd_opts(math_cmd_opts_t &opts, int *optind,  //!OCLINT(high ncss method)
                          int argc, wchar_t **argv, parser_t &parser, io_streams_t &streams) {
    const wchar_t *cmd = L"math";
    int opt;
    wgetopter_t w;
    while ((opt = w.wgetopt_long(argc, argv, short_options, long_options, NULL)) != -1) {
        switch (opt) {
            case 's': {
                opts.scale = fish_wcstoi(w.woptarg);
                if (errno || opts.scale < 0 || opts.scale > 15) {
                    streams.err.append_format(_(L"%ls: '%ls' is not a valid scale value\n"), cmd,
                                              w.woptarg);
                    return STATUS_INVALID_ARGS;
                }
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
            case '?': {
                // For most commands this is an error. We ignore it because a math expression
                // can begin with a minus sign.
                *optind = w.woptind - 1;
                return STATUS_CMD_OK;
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

// We read from stdin if we are the second or later process in a pipeline.
static bool math_args_from_stdin(const io_streams_t &streams) {
    return streams.stdin_is_directly_redirected;
}

/// Get the arguments from stdin.
static const wchar_t *math_get_arg_stdin(wcstring *storage, const io_streams_t &streams) {
    std::string arg;
    for (;;) {
        char ch = '\0';
        long rc = read_blocked(streams.stdin_fd, &ch, 1);

        if (rc < 0) return NULL;  // failure

        if (rc == 0) {  // EOF
            if (arg.empty()) return NULL;
            break;
        }

        if (ch == '\n') break;  // we're done

        arg += ch;
    }

    *storage = str2wcstring(arg);
    return storage->c_str();
}

/// Return the next argument from argv.
static const wchar_t *math_get_arg_argv(int *argidx, wchar_t **argv) {
    return argv && argv[*argidx] ? argv[(*argidx)++] : NULL;
}

/// Get the arguments from argv or stdin based on the execution context. This mimics how builtin
/// `string` does it.
static const wchar_t *math_get_arg(int *argidx, wchar_t **argv, wcstring *storage,
                                   const io_streams_t &streams) {
    if (math_args_from_stdin(streams)) {
        return math_get_arg_stdin(storage, streams);
    }
    return math_get_arg_argv(argidx, argv);
}

static wcstring math_describe_error(te_error_t& error) {
    if (error.position == 0) return L"NO ERROR?!?";
    assert(error.type != TE_ERROR_NONE && L"Error has no position");

    switch(error.type) {
        case TE_ERROR_UNKNOWN_VARIABLE: return _(L"Unknown variable");
        case TE_ERROR_MISSING_CLOSING_PAREN: return _(L"Missing closing parenthesis");
        case TE_ERROR_MISSING_OPENING_PAREN: return _(L"Missing opening parenthesis");
        case TE_ERROR_TOO_FEW_ARGS: return _(L"Too few arguments");
        case TE_ERROR_TOO_MANY_ARGS: return _(L"Too many arguments");
        case TE_ERROR_MISSING_OPERATOR: return _(L"Missing operator");
        case TE_ERROR_UNKNOWN: return _(L"Expression is bogus");
        default: return L"Unknown error";
    }
}

/// Return a formatted version of the value \p v respecting the given \p opts.
static wcstring format_double(double v, const math_cmd_opts_t &opts) {
    // As a special-case, a scale of 0 means to truncate to an integer
    // instead of rounding.
    if (opts.scale == 0) {
        v = std::trunc(v);
        return format_string(L"%.*f", opts.scale, v);
    }

    wcstring ret = format_string(L"%.*f", opts.scale, v);
    // If we contain a decimal separator, trim trailing zeros after it, and then the separator
    // itself if there's nothing after it. Detect a decimal separator as a non-digit.
    const wchar_t *const digits = L"0123456789";
    if (ret.find_first_not_of(digits) != wcstring::npos) {
        while (ret.back() == L'0') {
            ret.pop_back();
        }
        if (!wcschr(digits, ret.back())) {
            ret.pop_back();
        }
    }
    // If we trimmed everything it must have just been zero.
    if (ret.empty()) {
        ret.push_back(L'0');
    }
    return ret;
}

/// Evaluate math expressions.
static int evaluate_expression(const wchar_t *cmd, parser_t &parser, io_streams_t &streams,
                               math_cmd_opts_t &opts, wcstring &expression) {
    UNUSED(parser);

    int retval = STATUS_CMD_OK;
    te_error_t error;
    std::string narrow_str = wcs2string(expression);
    // Switch locale while computing stuff.
    // This means that the "." is always the radix character,
    // so numbers work the same across locales.
    char *saved_locale = strdup(setlocale(LC_NUMERIC, NULL));
    setlocale(LC_NUMERIC, "C");
    double v = te_interp(narrow_str.c_str(), &error);

    if (error.position == 0) {
        // Check some runtime errors after the fact.
        // TODO: Really, this should be done in tinyexpr
        // (e.g. infinite is the result of "x / 0"),
        // but that's much more work.
        const char *error_message = NULL;
        if (std::isinf(v)) {
            error_message = "Result is infinite";
        } else if (std::isnan(v)) {
            error_message = "Result is not a number";
        } else if (std::abs(v) >= kMaximumContiguousInteger) {
            error_message = "Result magnitude is too large";
        }
        if (error_message) {
            streams.err.append_format(L"%ls: Error: %s\n", cmd, error_message);
            streams.err.append_format(L"'%ls'\n", expression.c_str());
            retval = STATUS_CMD_ERROR;
        } else {
            streams.out.append(format_double(v, opts));
            streams.out.push_back(L'\n');
        }
    } else {
        streams.err.append_format(L"%ls: Error: %ls\n", cmd, math_describe_error(error).c_str());
        streams.err.append_format(L"'%ls'\n", expression.c_str());
        streams.err.append_format(L"%*lc^\n", error.position - 1, L' ');
        retval = STATUS_CMD_ERROR;
    }
    setlocale(LC_NUMERIC, saved_locale);
    free(saved_locale);
    return retval;
}

/// The math builtin evaluates math expressions.
int builtin_math(parser_t &parser, io_streams_t &streams, wchar_t **argv) {
    wchar_t *cmd = argv[0];
    int argc = builtin_count_args(argv);
    math_cmd_opts_t opts;
    int optind;

    // Is this really the right way to handle no expression present?
    // if (argc == 0) return STATUS_CMD_OK;

    int retval = parse_cmd_opts(opts, &optind, argc, argv, parser, streams);
    if (retval != STATUS_CMD_OK) return retval;

    if (opts.print_help) {
        builtin_print_help(parser, streams, cmd, streams.out);
        return STATUS_CMD_OK;
    }

    wcstring expression;
    wcstring storage;
    while (const wchar_t *arg = math_get_arg(&optind, argv, &storage, streams)) {
        if (!expression.empty()) expression.push_back(L' ');
        expression.append(arg);
    }

    if (expression.empty()) {
        streams.err.append_format(BUILTIN_ERR_MIN_ARG_COUNT1, L"math", 1, 0);
        return STATUS_CMD_ERROR;
    }
    return evaluate_expression(cmd, parser, streams, opts, expression);
}
