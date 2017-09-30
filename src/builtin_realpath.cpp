// Implementation of the realpath builtin.
#include "config.h"  // IWYU pragma: keep

#include <stdio.h>
#include <stdlib.h>
#include <wchar.h>

#include "builtin.h"
#include "builtin_realpath.h"
#include "common.h"
#include "fallback.h"  // IWYU pragma: keep
#include "io.h"
#include "wutil.h"  // IWYU pragma: keep

/// An implementation of the external realpath command that doesn't support any options. It's meant
/// to be used only by scripts which need to be portable. In general scripts shouldn't invoke this
/// directly. They should just use `realpath` which will fallback to this builtin if an external
/// command cannot be found. This behaves like the external `realpath --canonicalize-existing`;
/// that is, it requires all path components, including the final, to exist.
int builtin_realpath(parser_t &parser, io_streams_t &streams, wchar_t **argv) {
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

    if (optind + 1 != argc) {
        streams.err.append_format(BUILTIN_ERR_ARG_COUNT1, cmd, 1, argc - optind);
        builtin_print_help(parser, streams, cmd, streams.out);
        return STATUS_INVALID_ARGS;
    }

    wchar_t *real_path = wrealpath(argv[optind], NULL);
    if (real_path) {
        streams.out.append(real_path);
        free((void *)real_path);
    } else {
        // We don't actually know why it failed. We should check errno.
        streams.err.append_format(_(L"%ls: Invalid path: %ls\n"), cmd, argv[optind]);
        return STATUS_CMD_ERROR;
    }
    streams.out.append(L"\n");
    return STATUS_CMD_OK;
}
