// Implementation of the pwd builtin.
#include "config.h"  // IWYU pragma: keep

#include "builtin_pwd.h"

#include <cstring>

#include "builtin.h"
#include "common.h"
#include "fallback.h"  // IWYU pragma: keep
#include "io.h"
#include "parser.h"
#include "wgetopt.h"
#include "wutil.h"  // IWYU pragma: keep

/// The pwd builtin. Respect -P to resolve symbolic links. Respect -L to not do that (the default).
static const wchar_t *const short_options = L"LPh";
static const struct woption long_options[] = {{L"help", no_argument, nullptr, 'h'},
                                              {L"logical", no_argument, nullptr, 'L'},
                                              {L"physical", no_argument, nullptr, 'P'},
                                              {nullptr, 0, nullptr, 0}};
maybe_t<int> builtin_pwd(parser_t &parser, io_streams_t &streams, wchar_t **argv) {
    UNUSED(parser);
    const wchar_t *cmd = argv[0];
    int argc = builtin_count_args(argv);
    bool resolve_symlinks = false;
    wgetopter_t w;
    int opt;
    while ((opt = w.wgetopt_long(argc, argv, short_options, long_options, nullptr)) != -1) {
        switch (opt) {
            case 'L':
                resolve_symlinks = false;
                break;
            case 'P':
                resolve_symlinks = true;
                break;
            case 'h':
                builtin_print_help(parser, streams, cmd);
                return STATUS_CMD_OK;
            case '?': {
                builtin_unknown_option(parser, streams, cmd, argv[w.woptind - 1]);
                return STATUS_INVALID_ARGS;
            }
            default: {
                DIE("unexpected retval from wgetopt_long");
            }
        }
    }

    if (w.woptind != argc) {
        streams.err.append_format(BUILTIN_ERR_ARG_COUNT1, cmd, 0, argc - 1);
        return STATUS_INVALID_ARGS;
    }

    wcstring pwd;
    if (auto tmp = parser.vars().get(L"PWD")) {
        pwd = tmp->as_string();
    }
    if (resolve_symlinks) {
        if (auto real_pwd = wrealpath(pwd)) {
            pwd = std::move(*real_pwd);
        } else {
            const char *error = std::strerror(errno);
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
