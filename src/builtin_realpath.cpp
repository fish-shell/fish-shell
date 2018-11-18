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

/// An implementation of the external realpath command. Desn't support any options.
/// In general scripts shouldn't invoke this directly. They should just use `realpath` which
/// will fallback to this builtin if an external command cannot be found.
int builtin_realpath(parser_t &parser, io_streams_t &streams, wchar_t **argv) {
    const wchar_t *cmd = argv[0];
    help_only_cmd_opts_t opts;
    int argc = builtin_count_args(argv);
    int optind;
    int retval = parse_help_only_cmd_opts(opts, &optind, argc, argv, parser, streams);

    if (retval != STATUS_CMD_OK) return retval;

    if (opts.print_help) {
        builtin_print_help(parser, streams, cmd, streams.out);
        return STATUS_CMD_OK;
    }

    if (optind + 1 != argc) { // TODO: allow arbitrary args. `realpath *` should print many paths
        streams.err.append_format(BUILTIN_ERR_ARG_COUNT1, cmd, 1, argc - optind);
        builtin_print_help(parser, streams, cmd, streams.out);
        return STATUS_INVALID_ARGS;
    }

    if (auto real_path = wrealpath(argv[optind])) {
        streams.out.append(*real_path);
    } else {
        if (errno) {
            // realpath() just couldn't do it. Report the error and make it clear
            // this is an error from our builtin, not the system's realpath.
            streams.err.append_format(L"builtin %ls: %ls: %s\n", cmd, argv[optind],
                                      strerror(errno));
        } else {
            // Who knows. Probably a bug in our wrealpath() implementation.
            streams.err.append_format(_(L"builtin %ls: Invalid path: %ls\n"), cmd, argv[optind]);
        }

        return STATUS_CMD_ERROR;
    }

    streams.out.append(L"\n");

    return STATUS_CMD_OK;
}
