// Implementation of the builtin builtin.
#include "config.h"  // IWYU pragma: keep

#include <stddef.h>

#include <algorithm>
#include <string>

#include "builtin.h"
#include "builtin_builtin.h"
#include "common.h"
#include "fallback.h"  // IWYU pragma: keep
#include "io.h"
#include "wgetopt.h"
#include "wutil.h"  // IWYU pragma: keep

struct builtin_cmd_opts_t {
    bool print_help = false;
    bool list_names = false;
};
static const wchar_t *const short_options = L":hn";
static const struct woption long_options[] = {
    {L"help", no_argument, NULL, 'h'}, {L"names", no_argument, NULL, 'n'}, {NULL, 0, NULL, 0}};

static int parse_cmd_opts(builtin_cmd_opts_t &opts, int *optind, int argc, wchar_t **argv,
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
            case 'n': {
                opts.list_names = true;
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

/// The builtin builtin, used for giving builtins precedence over functions. Mostly handled by the
/// parser. All this code does is some additional operational modes, such as printing a list of all
/// builtins, printing help, etc.
int builtin_builtin(parser_t &parser, io_streams_t &streams, wchar_t **argv) {
    const wchar_t *cmd = argv[0];
    int argc = builtin_count_args(argv);
    builtin_cmd_opts_t opts;

    int optind;
    int retval = parse_cmd_opts(opts, &optind, argc, argv, parser, streams);
    if (retval != STATUS_CMD_OK) return retval;

    if (opts.print_help) {
        builtin_print_help(parser, streams, cmd, streams.out);
        return STATUS_CMD_OK;
    }

    if (opts.list_names) {
        wcstring_list_t names = builtin_get_names();
        std::sort(names.begin(), names.end());

        for (size_t i = 0; i < names.size(); i++) {
            const wchar_t *el = names.at(i).c_str();

            streams.out.append(el);
            streams.out.append(L"\n");
        }
    }

    return STATUS_CMD_OK;
}
