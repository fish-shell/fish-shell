// Implementation of the pwd builtin.
#include "config.h"  // IWYU pragma: keep

#include "builtin.h"
#include "builtin_pwd.h"
#include "common.h"
#include "fallback.h"  // IWYU pragma: keep
#include "io.h"
#include "wgetopt.h"
#include "wutil.h"  // IWYU pragma: keep

/// The pwd builtin. Respect -P to resolve symbolic links. Respect -L to not do that (the default).
static const wchar_t *short_options = L"LPh";
static const struct woption long_options[] = {{L"help", no_argument, NULL, 'h'},
                                              {NULL, 0, NULL, 0}};
int builtin_pwd(parser_t &parser, io_streams_t &streams, wchar_t **argv) {
    UNUSED(parser);
    const wchar_t *cmd = argv[0];
    int argc = builtin_count_args(argv);
    bool resolve_symlinks = false;
    wgetopter_t w;
    int opt;
    while ((opt = w.wgetopt_long(argc, argv, short_options, long_options, NULL)) != -1) {
        switch (opt) {
            case 'L':
                resolve_symlinks = false;
                break;
            case 'P':
                resolve_symlinks = true;
                break;
            case 'h':
                builtin_print_help(parser, streams, cmd, streams.out);
                return STATUS_CMD_OK;
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

    if (w.woptind != argc) {
        streams.err.append_format(BUILTIN_ERR_ARG_COUNT1, cmd, 0, argc - 1);
        return STATUS_INVALID_ARGS;
    }

    wcstring pwd;
    if (auto tmp = env_get(L"PWD")) {
        pwd = tmp->as_string();
    }
    if (resolve_symlinks) {
        if (auto real_pwd = wrealpath(pwd)) {
            pwd = std::move(*real_pwd);
        } else {
            const char *error = strerror(errno);
            streams.err.append_format(L"%ls: realpath failed:", cmd, error);
            return STATUS_CMD_ERROR;
        }
    }
    if (pwd.empty()) {
        return STATUS_CMD_ERROR;
    }
    streams.out.append(pwd);
    streams.out.push_back(L'\n');
    return STATUS_CMD_OK;
}
