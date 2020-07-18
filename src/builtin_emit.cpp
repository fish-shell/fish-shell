// Implementation of the emit builtin.
#include "config.h"  // IWYU pragma: keep

#include "builtin_emit.h"

#include "builtin.h"
#include "common.h"
#include "event.h"
#include "fallback.h"  // IWYU pragma: keep
#include "io.h"
#include "wutil.h"  // IWYU pragma: keep

/// Implementation of the builtin emit command, used to create events.
maybe_t<int> builtin_emit(parser_t &parser, io_streams_t &streams, wchar_t **argv) {
    const wchar_t *cmd = argv[0];
    int argc = builtin_count_args(argv);
    help_only_cmd_opts_t opts;

    int optind;
    int retval = parse_help_only_cmd_opts(opts, &optind, argc, argv, parser, streams);
    if (retval != STATUS_CMD_OK) return retval;

    if (opts.print_help) {
        builtin_print_help(parser, streams, cmd);
        return STATUS_CMD_OK;
    }

    if (!argv[optind]) {
        streams.err.append_format(L"%ls: expected event name\n", cmd);
        return STATUS_INVALID_ARGS;
    }

    const wchar_t *eventname = argv[optind];
    wcstring_list_t args(argv + optind + 1, argv + argc);
    event_fire_generic(parser, eventname, &args);
    return STATUS_CMD_OK;
}
