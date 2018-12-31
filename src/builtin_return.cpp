// Implementation of the return builtin.
#include "config.h"  // IWYU pragma: keep

#include <errno.h>
#include <stddef.h>

#include "builtin.h"
#include "builtin_return.h"
#include "common.h"
#include "fallback.h"  // IWYU pragma: keep
#include "io.h"
#include "parser.h"
#include "proc.h"
#include "wgetopt.h"
#include "wutil.h"  // IWYU pragma: keep

struct return_cmd_opts_t {
    bool print_help = false;
};
static const wchar_t *const short_options = L":h";
static const struct woption long_options[] = {{L"help", no_argument, NULL, 'h'},
                                              {NULL, 0, NULL, 0}};

static int parse_cmd_opts(return_cmd_opts_t &opts, int *optind,  //!OCLINT(high ncss method)
                          int argc, wchar_t **argv, parser_t &parser, io_streams_t &streams) {
    UNUSED(parser);
    UNUSED(streams);
    wchar_t *cmd = argv[0];
    int opt;
    wgetopter_t w;
    while ((opt = w.wgetopt_long(argc, argv, short_options, long_options, NULL)) != -1) {
        switch (opt) {  //!OCLINT(too few branches)
            case 'h': {
                opts.print_help = true;
                break;
            }
            case ':': {
                builtin_missing_argument(parser, streams, cmd, argv[w.woptind - 1]);
                return STATUS_INVALID_ARGS;
            }
            case '?': {
                // We would normally invoke builtin_unknown_option() and return an error.
                // But for this command we want to let it try and parse the value as a negative
                // return value.
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

/// Function for handling the return builtin.
int builtin_return(parser_t &parser, io_streams_t &streams, wchar_t **argv) {
    const wchar_t *cmd = argv[0];
    int argc = builtin_count_args(argv);
    return_cmd_opts_t opts;

    int optind;
    int retval = parse_cmd_opts(opts, &optind, argc, argv, parser, streams);
    if (retval != STATUS_CMD_OK) return retval;

    if (opts.print_help) {
        builtin_print_help(parser, streams, cmd, streams.out);
        return STATUS_CMD_OK;
    }

    if (optind + 1 < argc) {
        streams.err.append_format(BUILTIN_ERR_TOO_MANY_ARGUMENTS, cmd);
        builtin_print_help(parser, streams, cmd, streams.err);
        return STATUS_INVALID_ARGS;
    }

    if (optind == argc) {
        retval = proc_get_last_status();
    } else {
        retval = fish_wcstoi(argv[1]);
        if (errno) {
            streams.err.append_format(_(L"%ls: Argument '%ls' must be an integer\n"), cmd, argv[1]);
            builtin_print_help(parser, streams, cmd, streams.err);
            return STATUS_INVALID_ARGS;
        }
        retval &= 0xFF;
    }

    // Find the function block.
    size_t function_block_idx;
    for (function_block_idx = 0; function_block_idx < parser.block_count(); function_block_idx++) {
        const block_t *b = parser.block_at_index(function_block_idx);
        if (b->type() == FUNCTION_CALL || b->type() == FUNCTION_CALL_NO_SHADOW) break;
    }

    if (function_block_idx >= parser.block_count()) {
        streams.err.append_format(_(L"%ls: Not inside of function\n"), cmd);
        builtin_print_help(parser, streams, cmd, streams.err);
        return STATUS_CMD_ERROR;
    }

    // Skip everything up to and including the function block.
    for (size_t i = 0; i <= function_block_idx; i++) {
        block_t *b = parser.block_at_index(i);
        b->skip = true;
    }

    return retval;
}
