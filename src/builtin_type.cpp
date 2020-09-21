// Implementation of the type builtin.
#include "config.h"  // IWYU pragma: keep

#include "builtin_type.h"

#include <unistd.h>
#include <string>

#include "builtin.h"
#include "common.h"
#include "complete.h"
#include "fallback.h"  // IWYU pragma: keep
#include "function.h"
#include "highlight.h"
#include "io.h"
#include "parser.h"
#include "path.h"
#include "signal.h"
#include "wcstringutil.h"
#include "wgetopt.h"
#include "wutil.h"  // IWYU pragma: keep

struct type_cmd_opts_t {
    bool all = false;
    bool short_output = false;
    bool no_functions = false;
    bool type = false;
    bool path = false;
    bool force_path = false;
    bool print_help = false;
    bool query = false;
};
static const wchar_t *const short_options = L":hasftpPq";
static const struct woption long_options[] = {{L"help", no_argument, nullptr, 'h'},
                                              {L"all", no_argument, nullptr, 'a'},
                                              {L"short", no_argument, nullptr, 's'},
                                              {L"no-functions", no_argument, nullptr, 'f'},
                                              {L"type", no_argument, nullptr, 't'},
                                              {L"path", no_argument, nullptr, 'p'},
                                              {L"force-path", no_argument, nullptr, 'P'},
                                              {L"query", no_argument, nullptr, 'q'},
                                              {nullptr, 0, nullptr, 0}};

static int parse_cmd_opts(type_cmd_opts_t &opts, int *optind, int argc, wchar_t **argv,
                          parser_t &parser, io_streams_t &streams) {
    UNUSED(parser);
    wchar_t *cmd = argv[0];
    int opt;
    wgetopter_t w;
    while ((opt = w.wgetopt_long(argc, argv, short_options, long_options, nullptr)) != -1) {
        switch (opt) {
            case 'h': {
                opts.print_help = true;
                break;
            }
            case 'a': {
                opts.all = true;
                break;
            }
            case 's': {
                opts.short_output = true;
                break;
            }
            case 'f': {
                opts.no_functions = true;
                break;
            }
            case 't': {
                opts.type = true;
                break;
            }
            case 'p': {
                opts.path = true;
                break;
            }
            case 'P': {
                opts.force_path = true;
                break;
            }
            case 'q': {
                opts.query = true;
                break;
            }
            case ':': {
                streams.err.append_format(BUILTIN_ERR_MISSING, cmd, argv[w.woptind - 1]);
                return STATUS_INVALID_ARGS;
            }
            case '?': {
                streams.err.append_format(BUILTIN_ERR_UNKNOWN, cmd, argv[w.woptind - 1]);
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

/// Implementation of the builtin 'type'.
maybe_t<int> builtin_type(parser_t &parser, io_streams_t &streams, wchar_t **argv) {
    UNUSED(parser);
    const wchar_t *cmd = argv[0];
    int argc = builtin_count_args(argv);
    type_cmd_opts_t opts;

    int optind;
    int retval = parse_cmd_opts(opts, &optind, argc, argv, parser, streams);
    if (retval != STATUS_CMD_OK) return retval;

    if (opts.print_help) {
        builtin_print_help(parser, streams, cmd);
        return STATUS_CMD_OK;
    }

    // Mutually exclusive options
    if (opts.query + opts.path + opts.type + opts.force_path > 1) {
        streams.err.append_format(BUILTIN_ERR_COMBO, cmd);
        return STATUS_INVALID_ARGS;
    }

    wcstring_list_t builtins = builtin_get_names();
    bool res = false;
    for (int idx = optind; argv[idx]; ++idx) {
        int found = 0;
        const wchar_t *name = argv[idx];
        // Functions
        if (!opts.force_path && !opts.no_functions && function_exists(name, parser)) {
            ++found;
            res = true;
            if (!opts.query && !opts.type) {
                streams.out.append_format(_(L"%ls is a function"), name);
                auto path = function_get_definition_file(name);
                if (opts.path) {
                    if (path) {
                        streams.out.append(path);
                        streams.out.append(L"\n");
                    }
                } else if (!opts.short_output) {
                    streams.out.append(_(L" with definition"));
                    streams.out.append(L"\n");
                    // Function path
                    wcstring def = functions_def(name);
                    if (path) {
                        int line_number = function_get_definition_lineno(name);
                        wcstring comment;
                        append_format(comment, L"# Defined in %ls @ line %d\n", path, line_number);
                        def = comment + def;
                    }
                    if (!streams.out_is_redirected && isatty(STDOUT_FILENO)) {
                        std::vector<highlight_spec_t> colors;
                        highlight_shell(def, colors, parser.context());
                        streams.out.append(str2wcstring(colorize(def, colors, parser.vars())));
                    } else {
                        streams.out.append(def);
                    }
                } else {
                    auto path = function_get_definition_file(name);
                    if (path) {
                        streams.out.append_format(_(L" (defined in %ls)"), path);
                    }
                    streams.out.append(L"\n");
                }
            } else if (opts.type) {
                streams.out.append(L"function\n");
            }
            if (!opts.all) continue;
        }

        // Builtins
        if (!opts.force_path && contains(builtins, name)) {
            ++found;
            res = true;
            if (!opts.query && !opts.type) {
                streams.out.append_format(_(L"%ls is a builtin\n"), name);
            } else if (opts.type) {
                streams.out.append(_(L"builtin\n"));
            }
            if (!opts.all) continue;
        }

        // Commands
        wcstring_list_t paths = path_get_paths(name, parser.vars());
        for (const auto &path : paths) {
            ++found;
            res = true;
            if (!opts.query && !opts.type) {
                streams.out.append_format(L"%ls is %ls\n", name, path.c_str());
            } else if (opts.type) {
                streams.out.append(_(L"file\n"));
                break;
            }
            if (!opts.all) break;
        }

        if (!found && !opts.query && !opts.path) {
            streams.err.append_format(_(L"%ls: Could not find '%ls'\n"), L"type", name);
        }
    }

    return res ? STATUS_CMD_OK : STATUS_CMD_ERROR;
}
