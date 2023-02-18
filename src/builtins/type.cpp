// Implementation of the type builtin.
#include "config.h"  // IWYU pragma: keep

#include "type.h"

#include <unistd.h>

#include <memory>
#include <string>
#include <vector>

#include "../builtin.h"
#include "../common.h"
#include "../env.h"
#include "../fallback.h"  // IWYU pragma: keep
#include "../function.h"
#include "../highlight.h"
#include "../io.h"
#include "../maybe.h"
#include "../parser.h"
#include "../path.h"
#include "../wgetopt.h"
#include "../wutil.h"  // IWYU pragma: keep

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
static const struct woption long_options[] = {
    {L"help", no_argument, 'h'},       {L"all", no_argument, 'a'},
    {L"short", no_argument, 's'},      {L"no-functions", no_argument, 'f'},
    {L"type", no_argument, 't'},       {L"path", no_argument, 'p'},
    {L"force-path", no_argument, 'P'}, {L"query", no_argument, 'q'},
    {L"quiet", no_argument, 'q'},      {}};

static int parse_cmd_opts(type_cmd_opts_t &opts, int *optind, int argc, const wchar_t **argv,
                          parser_t &parser, io_streams_t &streams) {
    UNUSED(parser);
    const wchar_t *cmd = argv[0];
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
maybe_t<int> builtin_type(parser_t &parser, io_streams_t &streams, const wchar_t **argv) {
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
        function_properties_ref_t func{};
        if (!opts.force_path && !opts.no_functions &&
            (func = function_get_props_autoload(name, parser))) {
            ++found;
            res = true;
            if (!opts.query && !opts.type) {
                auto path = func->definition_file;
                auto copy_path = func->copy_definition_file;
                auto final_path = func->is_copy ? copy_path : path;
                wcstring comment;

                if (!path) {
                    append_format(comment, _(L"Defined interactively"));
                } else if (*path == L"-") {
                    append_format(comment, _(L"Defined via `source`"));
                } else {
                    append_format(comment, _(L"Defined in %ls @ line %d"), path->c_str(),
                                  func->definition_lineno());
                }

                if (func->is_copy) {
                    if (!copy_path) {
                        append_format(comment, _(L", copied interactively"));
                    } else if (*copy_path == L"-") {
                        append_format(comment, _(L", copied via `source`"));
                    } else {
                        append_format(comment, _(L", copied in %ls @ line %d"), copy_path->c_str(),
                                      func->copy_definition_lineno);
                    }
                }

                if (opts.path) {
                    if (final_path) {
                        streams.out.append(*final_path);
                        streams.out.append(L"\n");
                    }
                } else if (!opts.short_output) {
                    streams.out.append_format(_(L"%ls is a function"), name);
                    streams.out.append(_(L" with definition"));
                    streams.out.append(L"\n");

                    wcstring def;
                    append_format(def, L"# %ls\n%ls", comment.c_str(),
                                  func->annotated_definition(name).c_str());

                    if (!streams.out_is_redirected && isatty(STDOUT_FILENO)) {
                        std::vector<highlight_spec_t> colors;
                        highlight_shell(def, colors, parser.context());
                        streams.out.append(str2wcstring(colorize(def, colors, parser.vars())));
                    } else {
                        streams.out.append(def);
                    }
                } else {
                    streams.out.append_format(_(L"%ls is a function"), name);
                    streams.out.append_format(_(L" (%ls)\n"), comment.c_str());
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
                if (opts.path || opts.force_path) {
                    streams.out.append_format(L"%ls\n", path.c_str());
                } else {
                    streams.out.append_format(_(L"%ls is %ls\n"), name, path.c_str());
                }
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
