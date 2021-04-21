// Implementation of the cd builtin.
#include "config.h"  // IWYU pragma: keep

#include "builtin_cd.h"

#include <fcntl.h>
#include <sys/stat.h>

#include <cerrno>

#include "builtin.h"
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
maybe_t<int> builtin_cd(parser_t &parser, io_streams_t &streams, const wchar_t **argv) {
    const wchar_t *cmd = argv[0];
    int argc = builtin_count_args(argv);
    help_only_cmd_opts_t opts;

    int optind;
    int retval = parse_help_only_cmd_opts(opts, &optind, argc, argv, parser, streams);
    if (retval != STATUS_CMD_OK) return retval;

    if (opts.print_help) {
        builtin_print_help(parser, streams, cmd);
        return STATUS_CMD_OK;
    }

    wcstring dir_in;
    if (argv[optind]) {
        dir_in = argv[optind];
    } else {
        auto maybe_dir_in = parser.vars().get(L"HOME");
        if (maybe_dir_in.missing_or_empty()) {
            streams.err.append_format(_(L"%ls: Could not find home directory\n"), cmd);
            return STATUS_CMD_ERROR;
        }
        dir_in = maybe_dir_in->as_string();
    }

    wcstring pwd = parser.vars().get_pwd_slash();
    auto dirs = path_apply_cdpath(dir_in, pwd, parser.vars());
    if (dirs.empty()) {
        streams.err.append_format(_(L"%ls: The directory '%ls' does not exist\n"), cmd,
                                  dir_in.c_str());

        if (!parser.is_interactive()) streams.err.append(parser.current_line());

        return STATUS_CMD_ERROR;
    }

    errno = 0;
    auto best_errno = errno;

    for (const auto &dir : dirs) {
        wcstring norm_dir = normalize_path(dir);

        // We need to keep around the fd for this directory, in the parser.
        errno = 0;
        autoclose_fd_t dir_fd(wopen_cloexec(norm_dir, O_RDONLY));
        bool success = dir_fd.valid() && fchdir(dir_fd.fd()) == 0;

        if (!success) {
            // Some errors we skip and only report if nothing worked.
            // ENOENT in particular is very low priority
            // - if in another directory there was a *file* by the correct name
            // we prefer *that* error because it's more specific
            if (errno == ENOENT) {
                if (!best_errno) best_errno = errno;
                continue;
            } else if (errno == ENOTDIR) {
                best_errno = errno;
                continue;
            }

            best_errno = errno;
            break;
        }

        parser.libdata().cwd_fd = std::make_shared<const autoclose_fd_t>(std::move(dir_fd));
        std::vector<event_t> evts;
        parser.vars().set_one(L"PWD", ENV_EXPORT | ENV_GLOBAL, std::move(norm_dir), &evts);
        for (const auto &evt : evts) {
            event_fire(parser, evt);
        }
        return STATUS_CMD_OK;
    }

    if (best_errno == ENOTDIR) {
        streams.err.append_format(_(L"%ls: '%ls' is not a directory\n"), cmd, dir_in.c_str());
    } else if (best_errno == ENOENT) {
        streams.err.append_format(_(L"%ls: The directory '%ls' does not exist\n"), cmd,
                                  dir_in.c_str());
    } else if (best_errno == EROTTEN) {
        streams.err.append_format(_(L"%ls: '%ls' is a rotten symlink\n"), cmd, dir_in.c_str());
    } else if (best_errno == EACCES) {
        streams.err.append_format(_(L"%ls: Permission denied: '%ls'\n"), cmd, dir_in.c_str());
    } else {
        errno = best_errno;
        wperror(L"cd");
        streams.err.append_format(_(L"%ls: Unknown error trying to locate directory '%ls'\n"), cmd,
                                  dir_in.c_str());
    }

    if (!parser.is_interactive()) {
        streams.err.append(parser.current_line());
    }

    return STATUS_CMD_ERROR;
}
