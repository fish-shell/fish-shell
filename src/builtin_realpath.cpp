// Implementation of the realpath builtin.
#include "config.h"  // IWYU pragma: keep

#include "builtin_realpath.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cwchar>

#include "builtin.h"
#include "common.h"
#include "fallback.h"  // IWYU pragma: keep
#include "io.h"
#include "wgetopt.h"
#include "wutil.h"  // IWYU pragma: keep

struct realpath_cmd_opts_t {
    bool print_help = false;
    bool no_symlinks = false;
};

static const wchar_t *const short_options = L"+:hs";
static const struct woption long_options[] = {{L"no-symlinks", no_argument, nullptr, 's'},
                                              {L"help", no_argument, nullptr, 'h'},
                                              {nullptr, 0, nullptr, 0}};

static int parse_cmd_opts(realpath_cmd_opts_t &opts, int *optind,  //!OCLINT(high ncss method)
                          int argc, wchar_t **argv, parser_t &parser, io_streams_t &streams) {
    const wchar_t *cmd = argv[0];
    int opt;
    wgetopter_t w;
    while ((opt = w.wgetopt_long(argc, argv, short_options, long_options, nullptr)) != -1) {
        switch (opt) {
            case 's': {
                opts.no_symlinks = true;
                break;
            }
            case 'h': {
                opts.print_help = true;
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
            }
        }
    }

    *optind = w.woptind;
    return STATUS_CMD_OK;
}
/// An implementation of the external realpath command. Doesn't support any options.
/// In general scripts shouldn't invoke this directly. They should just use `realpath` which
/// will fallback to this builtin if an external command cannot be found.
maybe_t<int> builtin_realpath(parser_t &parser, io_streams_t &streams, wchar_t **argv) {
    const wchar_t *cmd = argv[0];
    realpath_cmd_opts_t opts;
    int argc = builtin_count_args(argv);
    int optind;

    int retval = parse_cmd_opts(opts, &optind, argc, argv, parser, streams);
    if (retval != STATUS_CMD_OK) return retval;

    if (opts.print_help) {
        builtin_print_help(parser, streams, cmd);
        return STATUS_CMD_OK;
    }

    if (optind + 1 != argc) {  // TODO: allow arbitrary args. `realpath *` should print many paths
        streams.err.append_format(BUILTIN_ERR_ARG_COUNT1, cmd, 1, argc - optind);
        builtin_print_help(parser, streams, cmd);
        return STATUS_INVALID_ARGS;
    }

    if (!opts.no_symlinks) {
        if (auto real_path = wrealpath(argv[optind])) {
            streams.out.append(*real_path);
        } else {
            if (errno) {
                // realpath() just couldn't do it. Report the error and make it clear
                // this is an error from our builtin, not the system's realpath.
                streams.err.append_format(L"builtin %ls: %ls: %s\n", cmd, argv[optind],
                                          std::strerror(errno));
            } else {
                // Who knows. Probably a bug in our wrealpath() implementation.
                streams.err.append_format(_(L"builtin %ls: Invalid path: %ls\n"), cmd, argv[optind]);
            }

            return STATUS_CMD_ERROR;
        }
    } else {
        streams.out.append(normalize_path(argv[optind], /* allow leading double slashes */ false));
    }

    streams.out.append(L"\n");

    return STATUS_CMD_OK;
}
