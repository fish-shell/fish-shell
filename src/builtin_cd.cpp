// Implementation of the cd builtin.
#include "config.h"  // IWYU pragma: keep

#include <errno.h>
#include <sys/stat.h>
#include <unistd.h>

#include "builtin.h"
#include "builtin_cd.h"
#include "common.h"
#include "env.h"
#include "fallback.h"  // IWYU pragma: keep
#include "io.h"
#include "parser.h"
#include "path.h"
#include "proc.h"
#include "wgetopt.h"
#include "wutil.h"  // IWYU pragma: keep

struct cmd_opts {
    bool print_help = false;
};
static const wchar_t *short_options = L"h";
static const struct woption long_options[] = {{L"help", no_argument, NULL, 'h'},
                                              {NULL, 0, NULL, 0}};

static int parse_cmd_opts(struct cmd_opts *opts, int *optind, int argc, wchar_t **argv,
                          parser_t &parser, io_streams_t &streams) {
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

/// The cd builtin. Changes the current directory to the one specified or to $HOME if none is
/// specified. The directory can be relative to any directory in the CDPATH variable.
/// The cd builtin. Changes the current directory to the one specified or to $HOME if none is
/// specified. The directory can be relative to any directory in the CDPATH variable.
int builtin_cd(parser_t &parser, io_streams_t &streams, wchar_t **argv) {
    const wchar_t *cmd = argv[0];
    int argc = builtin_count_args(argv);
    struct cmd_opts opts;
    int optind;
    int retval = parse_cmd_opts(&opts, &optind, argc, argv, parser, streams);
    if (retval != STATUS_CMD_OK) return retval;

    if (opts.print_help) {
        builtin_print_help(parser, streams, cmd, streams.out);
        return STATUS_INVALID_ARGS;
    }

    env_var_t dir_in;
    wcstring dir;

    if (argv[optind]) {
        dir_in = env_var_t(argv[optind]);
    } else {
        dir_in = env_get_string(L"HOME");
        if (dir_in.missing_or_empty()) {
            streams.err.append_format(_(L"%ls: Could not find home directory\n"), cmd);
            return STATUS_CMD_ERROR;
        }
    }

    bool got_cd_path = false;
    if (!dir_in.missing()) {
        got_cd_path = path_get_cdpath(dir_in, &dir);
    }

    if (!got_cd_path) {
        if (errno == ENOTDIR) {
            streams.err.append_format(_(L"%ls: '%ls' is not a directory\n"), cmd, dir_in.c_str());
        } else if (errno == ENOENT) {
            streams.err.append_format(_(L"%ls: The directory '%ls' does not exist\n"), cmd,
                                      dir_in.c_str());
        } else if (errno == EROTTEN) {
            streams.err.append_format(_(L"%ls: '%ls' is a rotten symlink\n"), cmd, dir_in.c_str());
        } else {
            streams.err.append_format(_(L"%ls: Unknown error trying to locate directory '%ls'\n"),
                                      cmd, dir_in.c_str());
        }

        if (!shell_is_interactive()) streams.err.append(parser.current_line());

        return STATUS_CMD_ERROR;
    }

    if (wchdir(dir) != 0) {
        struct stat buffer;
        int status;

        status = wstat(dir, &buffer);
        if (!status && S_ISDIR(buffer.st_mode)) {
            streams.err.append_format(_(L"%ls: Permission denied: '%ls'\n"), cmd, dir.c_str());

        } else {
            streams.err.append_format(_(L"%ls: '%ls' is not a directory\n"), cmd, dir.c_str());
        }

        if (!shell_is_interactive()) {
            streams.err.append(parser.current_line());
        }

        return STATUS_CMD_ERROR;
    }

    if (!env_set_pwd()) {
        streams.err.append_format(_(L"%ls: Could not set PWD variable\n"), cmd);
        return STATUS_CMD_ERROR;
    }

    return STATUS_CMD_OK;
}
