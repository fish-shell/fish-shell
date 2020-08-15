// Implementation of the exit builtin.
#include "config.h"  // IWYU pragma: keep

#include "builtin_exit.h"

#include <cerrno>
#include <cstddef>

#include "builtin.h"
#include "common.h"
#include "fallback.h"  // IWYU pragma: keep
#include "io.h"
#include "parser.h"
#include "proc.h"
#include "reader.h"
#include "wgetopt.h"
#include "wutil.h"  // IWYU pragma: keep

struct exit_cmd_opts_t {
    bool print_help = false;
};
static const wchar_t *const short_options = L":h";
static const struct woption long_options[] = {{L"help", no_argument, nullptr, 'h'},
                                              {nullptr, 0, nullptr, 0}};

static int parse_cmd_opts(exit_cmd_opts_t &opts, int *optind,  //!OCLINT(high ncss method)
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

/// The exit builtin. Calls reader_exit to exit and returns the value specified.
maybe_t<int> builtin_exit(parser_t &parser, io_streams_t &streams, wchar_t **argv) {
    const wchar_t *cmd = argv[0];
    int argc = builtin_count_args(argv);
    exit_cmd_opts_t opts;

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
        retval = fish_wcstoi(argv[optind]);
        if (errno) {
            streams.err.append_format(BUILTIN_ERR_NOT_NUMBER, cmd, argv[optind]);
            builtin_print_error_trailer(parser, streams.err, cmd);
            return STATUS_INVALID_ARGS;
        }
    }
    // Mark that we are exiting in the parser.
    // TODO: in concurrent mode this won't successfully exit a pipeline, as there are other parsers
    // involved. That is, `exit | sleep 1000` may not exit as hoped. Need to rationalize what
    // behavior we want here.
    parser.libdata().exit_current_script = true;
    return retval;
}
