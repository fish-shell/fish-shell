// Implementation of the math builtin.
#include "config.h"  // IWYU pragma: keep

#include <errno.h>
#include <stdint.h>
#include <wchar.h>

#include <algorithm>
#include <random>
#include <iostream>

#include "builtin.h"
#include "builtin_math.h"
#include "common.h"
#include "fallback.h"  // IWYU pragma: keep
#include "io.h"
#include "muParser.h"
#include "wutil.h"  // IWYU pragma: keep

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

/// The math builtin evaluates math expressions.
int builtin_math(parser_t &parser, io_streams_t &streams, wchar_t **argv) {
    wchar_t *cmd = argv[0];
    int argc = builtin_count_args(argv);
    help_only_cmd_opts_t opts;

    int optind;
    int retval = parse_help_only_cmd_opts(opts, &optind, argc, argv, parser, streams);
    if (retval != STATUS_CMD_OK) return retval;

    if (opts.print_help) {
        builtin_print_help(parser, streams, cmd, streams.out);
        return STATUS_CMD_OK;
    }

    int arg_count = argc - optind;

    wcstring expression;
    wcstring storage;
    while (const wchar_t *arg = math_get_arg(&optind, argv, &storage, streams)) {
        if (!expression.empty()) expression.push_back(L' ');
        expression.append(arg);
    }

    try
    {
        double fVal = 3;
        mu::Parser p;
        p.DefineVar(L"a", &fVal);
        p.SetExpr(expression);
        fwprintf(stdout, L"WTF muparser output '%f'\n", p.Eval());
    }
    catch (mu::Parser::exception_type &e)
    {
        fwprintf(stderr, L"WTF muparser error '%ls'\n", e.GetMsg().c_str());
    }

#if 0
    if (choice) {
        streams.out.append_format(L"%ls\n", argv[optind + result]);
    } else {
        streams.out.append_format(L"%lld\n", result);
    }
#endif
    return STATUS_CMD_OK;
}
