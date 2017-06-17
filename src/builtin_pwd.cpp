// Implementation of the pwd builtin.
#include "config.h"  // IWYU pragma: keep

#include "builtin.h"
#include "builtin_pwd.h"
#include "common.h"
#include "fallback.h"  // IWYU pragma: keep
#include "io.h"
#include "wutil.h"  // IWYU pragma: keep

/// The pwd builtin. We don't respect -P to resolve symbolic links because we
/// try to always resolve them.
int builtin_pwd(parser_t &parser, io_streams_t &streams, wchar_t **argv) {
    UNUSED(parser);
    const wchar_t *cmd = argv[0];
    int argc = builtin_count_args(argv);
    help_only_cmd_opts_t opts;

    int optind;
    int retval = parse_help_only_cmd_opts(opts, &optind, argc, argv, parser, streams);
    if (retval != STATUS_CMD_OK) return retval;

    if (opts.print_help) {
        builtin_print_help(parser, streams, cmd, streams.out);
        return STATUS_CMD_OK;
    }

    if (optind != argc) {
        streams.err.append_format(BUILTIN_ERR_ARG_COUNT1, cmd, 0, argc - 1);
        return STATUS_INVALID_ARGS;
    }

    wcstring res = wgetcwd();
    if (res.empty()) {
        return STATUS_CMD_ERROR;
    }
    streams.out.append(res);
    streams.out.push_back(L'\n');
    return STATUS_CMD_OK;
}
