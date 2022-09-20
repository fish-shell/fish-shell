// Implementation of the math builtin.
#include "config.h"  // IWYU pragma: keep

#include "math.h"

#include <cerrno>
#include <cmath>
#include <cwchar>
#include <limits>
#include <string>

#include "../builtin.h"
#include "../common.h"
#include "../fallback.h"  // IWYU pragma: keep
#include "../io.h"
#include "../maybe.h"
#include "../tinyexpr.h"
#include "../wgetopt.h"
#include "../wutil.h"  // IWYU pragma: keep

// The maximum number of points after the decimal that we'll print.
static constexpr int kDefaultScale = 6;

// The end of the range such that every integer is representable as a double.
// i.e. this is the first value such that x + 1 == x (or == x + 2, depending on rounding mode).
static constexpr double kMaximumContiguousInteger =
    double(1LLU << std::numeric_limits<double>::digits);

struct math_cmd_opts_t {
    bool print_help = false;
    bool have_scale = false;
    int scale = kDefaultScale;
    int base = 10;
};

// This command is atypical in using the "+" (REQUIRE_ORDER) option for flag parsing.
// This is needed because of the minus, `-`, operator in math expressions.
static const wchar_t *const short_options = L"+:hs:b:";
static const struct woption long_options[] = {{L"scale", required_argument, nullptr, 's'},
                                              {L"base", required_argument, nullptr, 'b'},
                                              {L"help", no_argument, nullptr, 'h'},
                                              {}};

static int parse_cmd_opts(math_cmd_opts_t &opts, int *optind,  //!OCLINT(high ncss method)
                          int argc, const wchar_t **argv, parser_t &parser, io_streams_t &streams) {
    const wchar_t *cmd = L"math";
    int opt;
    wgetopter_t w;
    while ((opt = w.wgetopt_long(argc, argv, short_options, long_options, nullptr)) != -1) {
        switch (opt) {
            case 's': {
                opts.have_scale = true;
                // "max" is the special value that tells us to pick the maximum scale.
                if (std::wcscmp(w.woptarg, L"max") == 0) {
                    opts.scale = 15;
                } else {
                    opts.scale = fish_wcstoi(w.woptarg);
                    if (errno || opts.scale < 0 || opts.scale > 15) {
                        streams.err.append_format(_(L"%ls: %ls: invalid scale value\n"), cmd,
                                                  w.woptarg);
                        return STATUS_INVALID_ARGS;
                    }
                }
                break;
            }
            case 'b': {
                if (std::wcscmp(w.woptarg, L"hex") == 0) {
                    opts.base = 16;
                } else if (std::wcscmp(w.woptarg, L"octal") == 0) {
                    opts.base = 8;
                } else {
                    opts.base = fish_wcstoi(w.woptarg);
                    if (errno || (opts.base != 8 && opts.base != 16)) {
                        streams.err.append_format(_(L"%ls: %ls: invalid base value\n"), cmd,
                                                  w.woptarg);
                        return STATUS_INVALID_ARGS;
                    }
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
            }
        }
    }
    if (opts.have_scale && opts.scale != 0 && opts.base != 10) {
        streams.err.append_format(BUILTIN_ERR_COMBO2, cmd,
                                  L"non-zero scale value only valid for base 10");
        return STATUS_INVALID_ARGS;
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

        if (rc < 0) {  // error
            wperror(L"read");
            return nullptr;
        }

        if (rc == 0) {  // EOF
            if (arg.empty()) return nullptr;
            break;
        }

        if (ch == '\n') break;  // we're done

        arg += ch;
    }

    *storage = str2wcstring(arg);
    return storage->c_str();
}

/// Return the next argument from argv.
static const wchar_t *math_get_arg_argv(int *argidx, const wchar_t **argv) {
    return argv && argv[*argidx] ? argv[(*argidx)++] : nullptr;
}

/// Get the arguments from argv or stdin based on the execution context. This mimics how builtin
/// `string` does it.
static const wchar_t *math_get_arg(int *argidx, const wchar_t **argv, wcstring *storage,
                                   const io_streams_t &streams) {
    if (math_args_from_stdin(streams)) {
        assert(streams.stdin_fd >= 0 &&
               "stdin should not be closed since it is directly redirected");
        return math_get_arg_stdin(storage, streams);
    }
    return math_get_arg_argv(argidx, argv);
}

static const wchar_t *math_describe_error(const te_error_t &error) {
    if (error.position == 0) return L"NO ERROR";

    switch (error.type) {
        case TE_ERROR_NONE:
            DIE("Error has no position");
        case TE_ERROR_UNKNOWN_FUNCTION:
            return _(L"Unknown function");
        case TE_ERROR_MISSING_CLOSING_PAREN:
            return _(L"Missing closing parenthesis");
        case TE_ERROR_MISSING_OPENING_PAREN:
            return _(L"Missing opening parenthesis");
        case TE_ERROR_TOO_FEW_ARGS:
            return _(L"Too few arguments");
        case TE_ERROR_TOO_MANY_ARGS:
            return _(L"Too many arguments");
        case TE_ERROR_MISSING_OPERATOR:
            return _(L"Missing operator");
        case TE_ERROR_UNEXPECTED_TOKEN:
            return _(L"Unexpected token");
        case TE_ERROR_LOGICAL_OPERATOR:
            return _(L"Logical operations are not supported, use `test` instead");
        case TE_ERROR_DIV_BY_ZERO:
            return _(L"Division by zero");
        case TE_ERROR_UNKNOWN:
            return _(L"Expression is bogus");
        default:
            return L"Unknown error";
    }
}

/// Return a formatted version of the value \p v respecting the given \p opts.
static wcstring format_double(double v, const math_cmd_opts_t &opts) {
    if (opts.base == 16) {
        v = trunc(v);
        const char *mneg = (v < 0.0 ? "-" : "");
        return format_string(L"%s0x%llx", mneg, (long long)std::fabs(v));
    } else if (opts.base == 8) {
        v = trunc(v);
        if (v == 0.0) return L"0";  // not 00
        const char *mneg = (v < 0.0 ? "-" : "");
        return format_string(L"%s0%llo", mneg, (long long)std::fabs(v));
    }

    // As a special-case, a scale of 0 means to truncate to an integer
    // instead of rounding.
    if (opts.scale == 0) {
        v = trunc(v);
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
        if (!std::wcschr(digits, ret.back())) {
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
static int evaluate_expression(const wchar_t *cmd, const parser_t &parser, io_streams_t &streams,
                               const math_cmd_opts_t &opts, wcstring &expression) {
    UNUSED(parser);

    int retval = STATUS_CMD_OK;
    te_error_t error;
    double v = te_interp(expression.c_str(), &error);

    if (error.position == 0) {
        // Check some runtime errors after the fact.
        // TODO: Really, this should be done in tinyexpr
        // (e.g. infinite is the result of "x / 0"),
        // but that's much more work.
        const wchar_t *error_message = nullptr;
        if (std::isinf(v)) {
            error_message = L"Result is infinite";
        } else if (std::isnan(v)) {
            error_message = L"Result is not a number";
        } else if (std::fabs(v) >= kMaximumContiguousInteger) {
            error_message = L"Result magnitude is too large";
        }
        if (error_message) {
            streams.err.append_format(L"%ls: Error: %ls\n", cmd, error_message);
            streams.err.append_format(L"'%ls'\n", expression.c_str());
            retval = STATUS_CMD_ERROR;
        } else {
            streams.out.append(format_double(v, opts));
            streams.out.push_back(L'\n');
        }
    } else {
        streams.err.append_format(L"%ls: Error: %ls\n", cmd, math_describe_error(error));
        streams.err.append_format(L"'%ls'\n", expression.c_str());
        if (error.len >= 2) {
            wcstring tildes(error.len - 2, L'~');
            streams.err.append_format(L"%*ls%ls%ls%ls\n", error.position - 1, L" ", L"^", tildes.c_str(), L"^");
        } else {
            streams.err.append_format(L"%*ls%ls\n", error.position - 1, L" ", L"^");
        }
        retval = STATUS_CMD_ERROR;
    }
    return retval;
}

/// The math builtin evaluates math expressions.
maybe_t<int> builtin_math(parser_t &parser, io_streams_t &streams, const wchar_t **argv) {
    const wchar_t *cmd = argv[0];
    int argc = builtin_count_args(argv);
    math_cmd_opts_t opts;
    int optind;

    // Is this really the right way to handle no expression present?
    // if (argc == 0) return STATUS_CMD_OK;

    int retval = parse_cmd_opts(opts, &optind, argc, argv, parser, streams);
    if (retval != STATUS_CMD_OK) return retval;

    if (opts.print_help) {
        builtin_print_help(parser, streams, cmd);
        return STATUS_CMD_OK;
    }

    wcstring expression;
    wcstring storage;
    while (const wchar_t *arg = math_get_arg(&optind, argv, &storage, streams)) {
        if (!expression.empty()) expression.push_back(L' ');
        expression.append(arg);
    }

    if (expression.empty()) {
        streams.err.append_format(BUILTIN_ERR_MIN_ARG_COUNT1, cmd, 1, 0);
        return STATUS_CMD_ERROR;
    }
    return evaluate_expression(cmd, parser, streams, opts, expression);
}
