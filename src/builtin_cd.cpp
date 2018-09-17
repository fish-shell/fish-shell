// Implementation of the cd builtin.
#include "config.h"  // IWYU pragma: keep

#include <errno.h>
#include <sys/stat.h>

#include "builtin.h"
#include "builtin_cd.h"
#include "common.h"
#include "env.h"
#include "fallback.h"  // IWYU pragma: keep
#include "io.h"
#include "parser.h"
#include "path.h"
#include "proc.h"
#include "wutil.h"  // IWYU pragma: keep

/// The cd builtin. Changes the current directory to the one specified or to $HOME if none is
/// specified. The directory can be relative to any directory in the CDPATH variable.
/// The cd builtin. Changes the current directory to the one specified or to $HOME if none is
/// specified. The directory can be relative to any directory in the CDPATH variable.
int builtin_cd(parser_t &parser, io_streams_t &streams, wchar_t **argv) {
    const wchar_t *cmd = argv[0];
    int argc = builtin_count_args(argv);
    help_only_cmd_opts_t opts;

    int optind;
    int retval = parse_help_only_cmd_opts(opts, &optind, argc, argv, parser, streams);
    if (retval != STATUS_CMD_OK) return retval;

    if (opts.print_help) {
        builtin_print_help(parser, streams, cmd, streams.out);
        return STATUS_CMD_OK;
    }

    wcstring dir_in;
    wcstring dir;

    if (argv[optind]) {
        dir_in = argv[optind];
    } else {
        auto maybe_dir_in = env_get(L"HOME");
        if (maybe_dir_in.missing_or_empty()) {
            streams.err.append_format(_(L"%ls: Could not find home directory\n"), cmd);
            return STATUS_CMD_ERROR;
        }
        dir_in = maybe_dir_in->as_string();
    }

    if (!path_get_cdpath(dir_in, &dir)) {
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

    // Prepend the PWD if we don't start with a slash, and then normalize the directory.
    wcstring norm_dir =
        normalize_path(string_prefixes_string(L"/", dir) ? dir : env_get_pwd_slash() + dir);

    if (wchdir(norm_dir) != 0) {
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

    env_set_one(L"PWD", ENV_EXPORT | ENV_GLOBAL, std::move(norm_dir));
    return STATUS_CMD_OK;
}
