// Implementation of the return builtin.
#include "config.h"  // IWYU pragma: keep

#include "builtin_return.h"

#include <cerrno>
#include <cstddef>

#include "builtin.h"
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
static const struct woption long_options[] = {{L"help", no_argument, nullptr, 'h'},
                                              {nullptr, 0, nullptr, 0}};

static int parse_cmd_opts(return_cmd_opts_t &opts, int *optind,  //!OCLINT(high ncss method)
                          int argc, wchar_t **argv, parser_t &parser, io_streams_t &streams) {
    UNUSED(parser);
    UNUSED(streams);
    wchar_t *cmd = argv[0];
    int opt;
    wgetopter_t w;
    while ((opt = w.wgetopt_long(argc, argv, short_options, long_options, nullptr)) != -1) {
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
            }
        }
    }

    *optind = w.woptind;
    return STATUS_CMD_OK;
}

/// Function for handling the return builtin.
maybe_t<int> builtin_return(parser_t &parser, io_streams_t &streams, wchar_t **argv) {
    const wchar_t *cmd = argv[0];
    int argc = builtin_count_args(argv);
    return_cmd_opts_t opts;

    int optind;
    int retval = parse_cmd_opts(opts, &optind, argc, argv, parser, streams);
    if (retval != STATUS_CMD_OK) return retval;

    if (opts.print_help) {
        builtin_print_help(parser, streams, cmd);
        return STATUS_CMD_OK;
    }

    if (optind + 1 < argc) {
        streams.err.append_format(BUILTIN_ERR_TOO_MANY_ARGUMENTS, cmd);
        builtin_print_error_trailer(parser, streams.err, cmd);
        return STATUS_INVALID_ARGS;
    }

    if (optind == argc) {
        retval = parser.get_last_status();
    } else {
        retval = fish_wcstoi(argv[1]);
        if (errno) {
            streams.err.append_format(BUILTIN_ERR_NOT_NUMBER, cmd, argv[1]);
            builtin_print_error_trailer(parser, streams.err, cmd);
            return STATUS_INVALID_ARGS;
        }
        retval &= 0xFF;
    }

    // Find the function block.
    bool has_function_block = false;
    for (const auto &b : parser.blocks()) {
        if (b.is_function_call()) {
            has_function_block = true;
            break;
        }
    }

    if (!has_function_block) {
        streams.err.append_format(_(L"%ls: Not inside of function\n"), cmd);
        builtin_print_error_trailer(parser, streams.err, cmd);
        return STATUS_CMD_ERROR;
    }

    // Mark a return in the libdata.
    parser.libdata().returning = true;

    return retval;
}
