// Implementation of the contains builtin.
#include "config.h"  // IWYU pragma: keep

#include <unistd.h>
#include <wchar.h>

#include "builtin.h"
#include "builtin_contains.h"
#include "common.h"
#include "fallback.h"  // IWYU pragma: keep
#include "io.h"
#include "wgetopt.h"
#include "wutil.h"  // IWYU pragma: keep

struct contains_cmd_opts_t {
    bool print_help = false;
    bool print_index = false;
};
static const wchar_t *const short_options = L"+:hi";
static const struct woption long_options[] = {
    {L"help", no_argument, NULL, 'h'}, {L"index", no_argument, NULL, 'i'}, {NULL, 0, NULL, 0}};

static int parse_cmd_opts(contains_cmd_opts_t &opts, int *optind, int argc, wchar_t **argv,
                          parser_t &parser, io_streams_t &streams) {
    wchar_t *cmd = argv[0];
    int opt;
    wgetopter_t w;
    while ((opt = w.wgetopt_long(argc, argv, short_options, long_options, NULL)) != -1) {
        switch (opt) {
            case 'h': {
                opts.print_help = true;
                break;
            }
            case 'i': {
                opts.print_index = true;
                break;
            }
            case ':': {
                builtin_missing_argument(parser, streams, cmd, argv[w.woptind - 1]);
                return STATUS_INVALID_ARGS;
            }
            case '?': {
                builtin_unknown_option(parser, streams, cmd, argv[w.woptind - 1]);
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

/// Implementation of the builtin contains command, used to check if a specified string is part of
/// a list.
int builtin_contains(parser_t &parser, io_streams_t &streams, wchar_t **argv) {
    const wchar_t *cmd = argv[0];
    int argc = builtin_count_args(argv);
    contains_cmd_opts_t opts;

    int optind;
    int retval = parse_cmd_opts(opts, &optind, argc, argv, parser, streams);
    if (retval != STATUS_CMD_OK) return retval;

    if (opts.print_help) {
        builtin_print_help(parser, streams, cmd, streams.out);
        return STATUS_CMD_OK;
    }

    wchar_t *needle = argv[optind];
    if (!needle) {
        streams.err.append_format(_(L"%ls: Key not specified\n"), cmd);
    } else {
        for (int i = optind + 1; i < argc; i++) {
            if (!wcscmp(needle, argv[i])) {
                if (opts.print_index) streams.out.append_format(L"%d\n", i - optind);
                return STATUS_CMD_OK;
            }
        }
    }

    return STATUS_CMD_ERROR;
}
