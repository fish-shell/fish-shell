// Implementation of the math builtin.
#include "config.h"  // IWYU pragma: keep

#include <errno.h>
#include <stddef.h>

#include <algorithm>
#include <string>

#include "builtin.h"
#include "builtin_math.h"
#include "common.h"
#include "fallback.h"  // IWYU pragma: keep
#include "io.h"
#include "wgetopt.h"
#include "wutil.h"  // IWYU pragma: keep

#include "muParser.h"
#include "muParserBase.h"
#include "muParserDef.h"

struct math_cmd_opts_t {
    bool print_help = false;
    int scale = 0;
};

// This command is atypical in using the "+" (REQUIRE_ORDER) option for flag parsing.
// This is needed because of the minus, `-`, operator in math expressions.
static const wchar_t *short_options = L"+:hs:";
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

// The MuParser mechanism for dynamic lookup of variables requires that we return a unique address
// for each variable. The following limit is arbitrary but anyone writing a math expression in fish
// that references more than one hundred unique variables is abusing fish.
#define MAX_RESULTS 100
static double double_results[MAX_RESULTS];
static int next_result = 0;

/// Return a fish var converted to a double. This allows the user to use a bar var name in the
/// expression. That is `math a + 1` rather than `math $a + 1`.
static double *retrieve_var(const wchar_t *var_name, void *user_data) {
    UNUSED(user_data);
    static double zero_result = 0.0;

    auto var = env_get(var_name, ENV_DEFAULT);
    if (!var) {
        // We could report an error but we normally don't treat missing vars as a fatal error.
        // throw mu::ParserError(L"Var '%ls' does not exist.");
        return &zero_result;
    }
    if (var->empty()) {
        return &zero_result;
    }

    const wchar_t *first_val = var->as_list()[0].c_str();
    wchar_t *endptr;
    errno = 0;
    double result = wcstod(first_val, &endptr);
    if (*endptr != L'\0' || errno) {
        wchar_t errmsg[500];
        swprintf(errmsg, sizeof(errmsg) / sizeof(wchar_t),
                 _(L"Var '%ls' not a valid floating point number: '%ls'."), var_name, first_val);
        throw mu::ParserError(errmsg);
    }

    // We need to return a unique address for the var. If we used a `static double` var and returned
    // it's address then multiple vars in the expression would all refer to the same value.
    if (next_result == MAX_RESULTS - 1) {
        wchar_t errmsg[500];
        swprintf(errmsg, sizeof(errmsg) / sizeof(wchar_t),
                 _(L"More than %d var names in math expression."), MAX_RESULTS);
        throw mu::ParserError(errmsg);
    }
    double_results[next_result++] = result;
    return double_results + next_result - 1;
}

/// Implement integer modulo math operator.
static double moduloOperator(double v, double w) { return (int)v % std::max(1, (int)w); };

/// Evaluate math expressions.
static int evaluate_expression(wchar_t *cmd, parser_t &parser, io_streams_t &streams,
                               math_cmd_opts_t &opts, wcstring &expression) {
    UNUSED(parser);
    next_result = 0;

    try {
        mu::Parser p;
        // Setup callback so variables can be retrieved dynamically.
        p.SetVarFactory(retrieve_var, nullptr);
        // MuParser doesn't implement the modulo operator so we add it ourselves since there are
        // likely users of our old math wrapper around bc that expect it to be available.
        p.DefineOprtChars(L"%");
        p.DefineOprt(L"%", moduloOperator, mu::prINFIX);

        p.SetExpr(expression);
        int nNum;
        mu::value_type *v = p.Eval(nNum);
        for (int i = 0; i < nNum; ++i) {
            if (opts.scale == 0) {
                streams.out.append_format(L"%ld\n", static_cast<long>(v[i]));
            } else {
                streams.out.append_format(L"%.*lf\n", opts.scale, v[i]);
            }
        }
        return STATUS_CMD_OK;
    } catch (mu::Parser::exception_type &e) {
        streams.err.append_format(_(L"%ls: Invalid expression: %ls\n"), cmd, e.GetMsg().c_str());
        return STATUS_CMD_ERROR;
    }
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

    return evaluate_expression(cmd, parser, streams, opts, expression);
}
