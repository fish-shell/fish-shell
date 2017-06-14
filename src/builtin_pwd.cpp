// Implementation of the pwd builtin.
#include "config.h"  // IWYU pragma: keep

#include <stddef.h>

#include "builtin.h"
#include "builtin_pwd.h"
#include "common.h"
#include "event.h"
#include "fallback.h"  // IWYU pragma: keep
#include "io.h"
#include "wgetopt.h"
#include "wutil.h"  // IWYU pragma: keep

struct cmd_opts {
    bool print_help = false;
};
static const wchar_t *short_options = L"h";
static const struct woption long_options[] = {{L"help", no_argument, NULL, 'h'},
                                              {NULL, 0, NULL, 0}};

static int parse_cmd_opts(struct cmd_opts *opts, int *optind,
                          int argc, wchar_t **argv, parser_t &parser, io_streams_t &streams) {
    wchar_t *cmd = argv[0];
    int opt;
    wgetopter_t w;
    while ((opt = w.wgetopt_long(argc, argv, short_options, long_options, NULL)) != -1) {
        switch (opt) {  //!OCLINT(too few branches)
            case 'h': {
                opts->print_help = true;
                return STATUS_CMD_OK;
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

/// The pwd builtin. We don't respect -P to resolve symbolic links because we
/// try to always resolve them.
int builtin_pwd(parser_t &parser, io_streams_t &streams, wchar_t **argv) {
    UNUSED(parser);
    const wchar_t *cmd = argv[0];
    int argc = builtin_count_args(argv);
    struct cmd_opts opts;

    int optind;
    int retval = parse_cmd_opts(&opts, &optind, argc, argv, parser, streams);
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
