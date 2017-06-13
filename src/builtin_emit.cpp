// Implementation of the emit builtin.
#include "config.h"  // IWYU pragma: keep

#include <stddef.h>

#include "builtin.h"
#include "builtin_emit.h"
#include "common.h"
#include "event.h"
#include "fallback.h"  // IWYU pragma: keep
#include "io.h"
#include "wgetopt.h"
#include "wutil.h"  // IWYU pragma: keep

struct emit_opts {
    bool print_help = false;
};

static int parse_emit_opts(struct emit_opts *opts, int *optind,  //!OCLINT(high ncss method)
                           int argc, wchar_t **argv, parser_t &parser, io_streams_t &streams) {
    wchar_t *cmd = argv[0];
    static const wchar_t *short_options = L"h";
    static const struct woption long_options[] = {{L"help", no_argument, NULL, 'h'},
                                                  {NULL, 0, NULL, 0}};

    int opt;
    wgetopter_t w;
    while ((opt = w.wgetopt_long(argc, argv, short_options, long_options, NULL)) != -1) {
        switch (opt) {  //!OCLINT(too few branches)
            case 'h': {
                opts->print_help = true;
                return STATUS_CMD_OK;
            }
            case '?': {
                builtin_unknown_option(parser, streams, argv[0], argv[w.woptind - 1]);
                return STATUS_INVALID_ARGS;
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

/// Implementation of the builtin emit command, used to create events.
int builtin_emit(parser_t &parser, io_streams_t &streams, wchar_t **argv) {
    const wchar_t *cmd = argv[0];
    int argc = builtin_count_args(argv);
    struct emit_opts opts;

    int optind;
    int retval = parse_emit_opts(&opts, &optind, argc, argv, parser, streams);
    if (retval != STATUS_CMD_OK) return retval;

    if (opts.print_help) {
        builtin_print_help(parser, streams, cmd, streams.out);
        return STATUS_INVALID_ARGS;
    }

    if (!argv[optind]) {
	streams.err.append_format(L"%ls: expected event name\n", argv[0]);
	return STATUS_INVALID_ARGS;
    }

    const wchar_t *eventname = argv[optind];
    wcstring_list_t args(argv + optind + 1, argv + argc);
    event_fire_generic(eventname, &args);
    return STATUS_CMD_OK;
}
