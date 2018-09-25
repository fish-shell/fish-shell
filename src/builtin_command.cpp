// Implementation of the command builtin.
#include "config.h"  // IWYU pragma: keep

#include <unistd.h>

#include <string>

#include "builtin.h"
#include "builtin_command.h"
#include "common.h"
#include "fallback.h"  // IWYU pragma: keep
#include "io.h"
#include "parser.h"
#include "path.h"
#include "wgetopt.h"
#include "wutil.h"  // IWYU pragma: keep

struct command_cmd_opts_t {
    bool print_help = false;
    bool find_path = false;
    bool quiet = false;
    bool all_paths = false;
};
static const wchar_t *const short_options = L":ahqsv";
static const struct woption long_options[] = {{L"help", no_argument, NULL, 'h'},
                                              {L"all", no_argument, NULL, 'a'},
                                              {L"quiet", no_argument, NULL, 'q'},
                                              {L"search", no_argument, NULL, 's'},
                                              {NULL, 0, NULL, 0}};

static int parse_cmd_opts(command_cmd_opts_t &opts, int *optind, int argc, wchar_t **argv,
                          parser_t &parser, io_streams_t &streams) {
    wchar_t *cmd = argv[0];
    int opt;
    wgetopter_t w;
    while ((opt = w.wgetopt_long(argc, argv, short_options, long_options, NULL)) != -1) {
        switch (opt) {
            case 'a': {
                opts.all_paths = true;
                break;
            }
            case 'h': {
                opts.print_help = true;
                break;
            }
            case 'q': {
                opts.quiet = true;
                break;
            }
            case 's':  // -s and -v are aliases
            case 'v': {
                opts.find_path = true;
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

/// Implementation of the builtin 'command'. Actual command running is handled by the parser, this
/// just processes the flags.
int builtin_command(parser_t &parser, io_streams_t &streams, wchar_t **argv) {
    const wchar_t *cmd = argv[0];
    int argc = builtin_count_args(argv);
    command_cmd_opts_t opts;

    int optind;
    int retval = parse_cmd_opts(opts, &optind, argc, argv, parser, streams);
    if (retval != STATUS_CMD_OK) return retval;

    if (opts.print_help) {
        builtin_print_help(parser, streams, cmd, streams.out);
        return STATUS_CMD_OK;
    }

    if (!opts.find_path && !opts.all_paths) {
        builtin_print_help(parser, streams, cmd, streams.out);
        return STATUS_INVALID_ARGS;
    }

    int found = 0;
    for (int idx = optind; argv[idx]; ++idx) {
        const wchar_t *command_name = argv[idx];
        if (opts.all_paths) {
            wcstring_list_t paths = path_get_paths(command_name, parser.vars());
            for (auto path : paths) {
                if (!opts.quiet) streams.out.append_format(L"%ls\n", path.c_str());
                ++found;
            }
        } else {
            wcstring path;
            if (path_get_path(command_name, &path, parser.vars())) {
                if (!opts.quiet) streams.out.append_format(L"%ls\n", path.c_str());
                ++found;
            }
        }
    }

    return found ? STATUS_CMD_OK : STATUS_CMD_ERROR;
}
