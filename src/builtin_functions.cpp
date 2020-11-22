// Implementation of the functions builtin.
#include "config.h"  // IWYU pragma: keep

#include "builtin_functions.h"

#include <unistd.h>

#include <algorithm>
#include <cstddef>
#include <cwchar>
#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "builtin.h"
#include "common.h"
#include "complete.h"
#include "env.h"
#include "event.h"
#include "fallback.h"  // IWYU pragma: keep
#include "function.h"
#include "highlight.h"
#include "io.h"
#include "parser.h"
#include "parser_keywords.h"
#include "proc.h"
#include "signal.h"
#include "termsize.h"
#include "wcstringutil.h"
#include "wgetopt.h"
#include "wutil.h"  // IWYU pragma: keep

struct functions_cmd_opts_t {
    bool print_help = false;
    bool erase = false;
    bool list = false;
    bool show_hidden = false;
    bool query = false;
    bool copy = false;
    bool report_metadata = false;
    bool verbose = false;
    bool handlers = false;
    wchar_t *handlers_type = nullptr;
    wchar_t *description = nullptr;
};
static const wchar_t *const short_options = L":Ht:Dacd:ehnqv";
static const struct woption long_options[] = {{L"erase", no_argument, nullptr, 'e'},
                                              {L"description", required_argument, nullptr, 'd'},
                                              {L"names", no_argument, nullptr, 'n'},
                                              {L"all", no_argument, nullptr, 'a'},
                                              {L"help", no_argument, nullptr, 'h'},
                                              {L"query", no_argument, nullptr, 'q'},
                                              {L"copy", no_argument, nullptr, 'c'},
                                              {L"details", no_argument, nullptr, 'D'},
                                              {L"verbose", no_argument, nullptr, 'v'},
                                              {L"handlers", no_argument, nullptr, 'H'},
                                              {L"handlers-type", required_argument, nullptr, 't'},
                                              {nullptr, 0, nullptr, 0}};

static int parse_cmd_opts(functions_cmd_opts_t &opts, int *optind,  //!OCLINT(high ncss method)
                          int argc, wchar_t **argv, parser_t &parser, io_streams_t &streams) {
    wchar_t *cmd = argv[0];
    int opt;
    wgetopter_t w;
    while ((opt = w.wgetopt_long(argc, argv, short_options, long_options, nullptr)) != -1) {
        switch (opt) {
            case 'v': {
                opts.verbose = true;
                break;
            }
            case 'e': {
                opts.erase = true;
                break;
            }
            case 'D': {
                opts.report_metadata = true;
                break;
            }
            case 'd': {
                opts.description = w.woptarg;
                break;
            }
            case 'n': {
                opts.list = true;
                break;
            }
            case 'a': {
                opts.show_hidden = true;
                break;
            }
            case 'h': {
                opts.print_help = true;
                break;
            }
            case 'q': {
                opts.query = true;
                break;
            }
            case 'c': {
                opts.copy = true;
                break;
            }
            case 'H': {
                opts.handlers = true;
                break;
            }
            case 't': {
                opts.handlers_type = w.woptarg;
                opts.handlers = true;
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
            }
        }
    }

    *optind = w.woptind;
    return STATUS_CMD_OK;
}

static int report_function_metadata(const wchar_t *funcname, bool verbose, io_streams_t &streams,
                                    parser_t &parser, bool metadata_as_comments) {
    const wchar_t *path = L"n/a";
    const wchar_t *autoloaded = L"n/a";
    const wchar_t *shadows_scope = L"n/a";
    wcstring description = L"n/a";
    int line_number = 0;

    if (function_exists(funcname, parser)) {
        auto props = function_get_properties(funcname);
        path = function_get_definition_file(funcname);
        if (path) {
            autoloaded = function_is_autoloaded(funcname) ? L"autoloaded" : L"not-autoloaded";
            line_number = function_get_definition_lineno(funcname);
        } else {
            path = L"stdin";
        }
        shadows_scope = props->shadow_scope ? L"scope-shadowing" : L"no-scope-shadowing";
        function_get_desc(funcname, description);
        description = escape_string(description, ESCAPE_NO_QUOTED);
    }

    if (metadata_as_comments) {
        if (std::wcscmp(path, L"stdin") != 0) {
            wcstring comment;
            append_format(comment, L"# Defined in %ls @ line %d\n", path, line_number);
            if (!streams.out_is_redirected && isatty(STDOUT_FILENO)) {
                std::vector<highlight_spec_t> colors;
                highlight_shell(comment, colors, parser.context());
                streams.out.append(str2wcstring(colorize(comment, colors, parser.vars())));
            } else {
                streams.out.append(comment);
            }
        }
    } else {
        streams.out.append_format(L"%ls\n", path);
        if (verbose) {
            streams.out.append_format(L"%ls\n", autoloaded);
            streams.out.append_format(L"%d\n", line_number);
            streams.out.append_format(L"%ls\n", shadows_scope);
            streams.out.append_format(L"%ls\n", description.c_str());
        }
    }

    return STATUS_CMD_OK;
}

/// The functions builtin, used for listing and erasing functions.
maybe_t<int> builtin_functions(parser_t &parser, io_streams_t &streams, wchar_t **argv) {
    const wchar_t *cmd = argv[0];
    int argc = builtin_count_args(argv);
    functions_cmd_opts_t opts;

    int optind;
    int retval = parse_cmd_opts(opts, &optind, argc, argv, parser, streams);
    if (retval != STATUS_CMD_OK) return retval;

    if (opts.print_help) {
        builtin_print_help(parser, streams, cmd);
        return STATUS_CMD_OK;
    }

    // Erase, desc, query, copy and list are mutually exclusive.
    bool describe = opts.description != nullptr;
    if (describe + opts.erase + opts.list + opts.query + opts.copy > 1) {
        streams.err.append_format(BUILTIN_ERR_COMBO, cmd);
        builtin_print_error_trailer(parser, streams.err, cmd);
        return STATUS_INVALID_ARGS;
    }

    if (opts.erase) {
        for (int i = optind; i < argc; i++) function_remove(argv[i]);
        return STATUS_CMD_OK;
    }

    if (opts.description) {
        wchar_t *func;

        if (argc - optind != 1) {
            streams.err.append_format(_(L"%ls: Expected exactly one function name\n"), cmd);
            builtin_print_error_trailer(parser, streams.err, cmd);
            return STATUS_INVALID_ARGS;
        }

        func = argv[optind];
        if (!function_exists(func, parser)) {
            streams.err.append_format(_(L"%ls: Function '%ls' does not exist\n"), cmd, func);
            builtin_print_error_trailer(parser, streams.err, cmd);
            return STATUS_CMD_ERROR;
        }

        function_set_desc(func, opts.description, parser);
        return STATUS_CMD_OK;
    }

    if (opts.report_metadata) {
        if (argc - optind != 1) {
            streams.err.append_format(BUILTIN_ERR_ARG_COUNT2, cmd, argv[optind - 1], 1,
                                      argc - optind);
            return STATUS_INVALID_ARGS;
        }

        const wchar_t *funcname = argv[optind];
        return report_function_metadata(funcname, opts.verbose, streams, parser, false);
    }

    if (opts.handlers) {
        maybe_t<event_type_t> type_filter{};
        if (opts.handlers_type) {
            type_filter = event_type_for_name(opts.handlers_type);
            if (!type_filter) {
                streams.err.append_format(_(L"%ls: Expected generic | variable | signal | exit | "
                                            L"job-id for --handlers-type\n"),
                                          cmd);
                return STATUS_INVALID_ARGS;
            }
        }
        event_print(streams, type_filter);
        return STATUS_CMD_OK;
    }

    // If we query with no argument, just return false.
    if (opts.query && argc == optind) {
        return STATUS_CMD_ERROR;
    }

    if (opts.list || argc == optind) {
        wcstring_list_t names = function_get_names(opts.show_hidden);
        std::sort(names.begin(), names.end());
        bool is_screen = !streams.out_is_redirected && isatty(STDOUT_FILENO);
        if (is_screen) {
            wcstring buff;
            for (const auto &name : names) {
                buff.append(name);
                buff.append(L", ");
            }
            if (names.size() > 0) {
                // Trim trailing ", "
                buff.resize(buff.size() - 2, '\0');
            }

            streams.out.append(reformat_for_screen(buff, termsize_last()));
        } else {
            for (const auto &name : names) {
                streams.out.append(name.c_str());
                streams.out.append(L"\n");
            }
        }

        return STATUS_CMD_OK;
    }

    if (opts.copy) {
        wcstring current_func;
        wcstring new_func;

        if (argc - optind != 2) {
            streams.err.append_format(_(L"%ls: Expected exactly two names (current function name, "
                                        L"and new function name)\n"),
                                      cmd);
            builtin_print_error_trailer(parser, streams.err, cmd);
            return STATUS_INVALID_ARGS;
        }
        current_func = argv[optind];
        new_func = argv[optind + 1];

        if (!function_exists(current_func, parser)) {
            streams.err.append_format(_(L"%ls: Function '%ls' does not exist\n"), cmd,
                                      current_func.c_str());
            builtin_print_error_trailer(parser, streams.err, cmd);
            return STATUS_CMD_ERROR;
        }

        if (!valid_func_name(new_func) || parser_keywords_is_reserved(new_func)) {
            streams.err.append_format(_(L"%ls: Illegal function name '%ls'\n"), cmd,
                                      new_func.c_str());
            builtin_print_error_trailer(parser, streams.err, cmd);
            return STATUS_INVALID_ARGS;
        }

        // Keep things simple: don't allow existing names to be copy targets.
        if (function_exists(new_func, parser)) {
            streams.err.append_format(
                _(L"%ls: Function '%ls' already exists. Cannot create copy '%ls'\n"), cmd,
                new_func.c_str(), current_func.c_str());
            builtin_print_error_trailer(parser, streams.err, cmd);
            return STATUS_CMD_ERROR;
        }

        if (function_copy(current_func, new_func)) return STATUS_CMD_OK;
        return STATUS_CMD_ERROR;
    }

    int res = STATUS_CMD_OK;
    for (int i = optind; i < argc; i++) {
        if (!function_exists(argv[i], parser)) {
            res++;
        } else {
            if (!opts.query) {
                if (i != optind) streams.out.append(L"\n");
                const wchar_t *funcname = argv[optind];
                report_function_metadata(funcname, opts.verbose, streams, parser, true);
                wcstring def = functions_def(funcname);

                if (!streams.out_is_redirected && isatty(STDOUT_FILENO)) {
                    std::vector<highlight_spec_t> colors;
                    highlight_shell(def, colors, parser.context());
                    streams.out.append(str2wcstring(colorize(def, colors, parser.vars())));
                } else {
                    streams.out.append(def);
                }
            }
        }
    }

    return res;
}
