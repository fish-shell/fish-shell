// Implementation of the functions builtin.
#include "config.h"  // IWYU pragma: keep

#include "functions.h"

#include <unistd.h>

#include <algorithm>
#include <memory>
#include <string>
#include <vector>

#include "../builtin.h"
#include "../common.h"
#include "../env.h"
#include "../event.h"
#include "../fallback.h"  // IWYU pragma: keep
#include "../function.h"
#include "../highlight.h"
#include "../io.h"
#include "../maybe.h"
#include "../parser.h"
#include "../parser_keywords.h"
#include "../termsize.h"
#include "../wgetopt.h"
#include "../wutil.h"  // IWYU pragma: keep

struct functions_cmd_opts_t {
    bool print_help = false;
    bool erase = false;
    bool list = false;
    bool show_hidden = false;
    bool query = false;
    bool copy = false;
    bool report_metadata = false;
    bool no_metadata = false;
    bool verbose = false;
    bool handlers = false;
    const wchar_t *handlers_type = nullptr;
    const wchar_t *description = nullptr;
};
static const wchar_t *const short_options = L":Ht:Dacd:ehnqv";
static const struct woption long_options[] = {{L"erase", no_argument, 'e'},
                                              {L"description", required_argument, 'd'},
                                              {L"names", no_argument, 'n'},
                                              {L"all", no_argument, 'a'},
                                              {L"help", no_argument, 'h'},
                                              {L"query", no_argument, 'q'},
                                              {L"copy", no_argument, 'c'},
                                              {L"details", no_argument, 'D'},
                                              {L"no-details", no_argument, 1},
                                              {L"verbose", no_argument, 'v'},
                                              {L"handlers", no_argument, 'H'},
                                              {L"handlers-type", required_argument, 't'},
                                              {}};

static int parse_cmd_opts(functions_cmd_opts_t &opts, int *optind,  //!OCLINT(high ncss method)
                          int argc, const wchar_t **argv, parser_t &parser, io_streams_t &streams) {
    const wchar_t *cmd = argv[0];
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
            case 1: {
                opts.no_metadata = true;
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

static int report_function_metadata(const wcstring &funcname, bool verbose, io_streams_t &streams,
                                    parser_t &parser, bool metadata_as_comments) {
    wcstring path = L"n/a";
    const wchar_t *autoloaded = L"n/a";
    const wchar_t *shadows_scope = L"n/a";
    wcstring description = L"n/a";
    int line_number = 0;
    bool is_copy = false;
    wcstring copy_path = L"n/a";
    int copy_line_number = 0;

    if (auto props = function_get_props_autoload(funcname, parser)) {
        if (props->definition_file) {
            path = *props->definition_file;
            autoloaded = props->is_autoload ? L"autoloaded" : L"not-autoloaded";
            line_number = props->definition_lineno();
        } else {
            path = L"stdin";
        }

        is_copy = props->is_copy;

        if (props->copy_definition_file) {
            copy_path = *props->copy_definition_file;
            copy_line_number = props->copy_definition_lineno;
        } else {
            copy_path = L"stdin";
        }

        shadows_scope = props->shadow_scope ? L"scope-shadowing" : L"no-scope-shadowing";
        description = escape_string(props->description, ESCAPE_NO_PRINTABLES | ESCAPE_NO_QUOTED);
    }

    if (metadata_as_comments) {
        // "stdin" means it was defined interactively, "-" means it was defined via `source`.
        // Neither is useful information.
        wcstring comment;

        if (path == L"stdin") {
            append_format(comment, L"# Defined interactively");
        } else if (path == L"-") {
            append_format(comment, L"# Defined via `source`");
        } else {
            append_format(comment, L"# Defined in %ls @ line %d", path.c_str(), line_number);
        }

        if (is_copy) {
            if (copy_path == L"stdin") {
                append_format(comment, L", copied interactively\n");
            } else if (copy_path == L"-") {
                append_format(comment, L", copied via `source`\n");
            } else {
                append_format(comment, L", copied in %ls @ line %d\n", copy_path.c_str(),
                              copy_line_number);
            }
        } else {
            append_format(comment, L"\n");
        }

        if (!streams.out_is_redirected && isatty(STDOUT_FILENO)) {
            std::vector<highlight_spec_t> colors;
            highlight_shell(comment, colors, parser.context());
            streams.out.append(str2wcstring(colorize(comment, colors, parser.vars())));
        } else {
            streams.out.append(comment);
        }
    } else {
        streams.out.append_format(L"%ls\n", is_copy ? copy_path.c_str() : path.c_str());

        if (verbose) {
            streams.out.append_format(L"%ls\n", is_copy ? path.c_str() : autoloaded);
            streams.out.append_format(L"%d\n", line_number);
            streams.out.append_format(L"%ls\n", shadows_scope);
            streams.out.append_format(L"%ls\n", description.c_str());
        }
    }

    return STATUS_CMD_OK;
}

/// \return whether a type filter is valid.
static bool type_filter_valid(const wcstring &filter) {
    if (filter.empty()) return true;
    for (size_t i = 0; event_filter_names[i]; i++) {
        if (filter == event_filter_names[i]) return true;
    }
    return false;
}

/// The functions builtin, used for listing and erasing functions.
maybe_t<int> builtin_functions(parser_t &parser, io_streams_t &streams, const wchar_t **argv) {
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

    if (opts.report_metadata && opts.no_metadata) {
        streams.err.append_format(BUILTIN_ERR_COMBO, cmd);
        builtin_print_error_trailer(parser, streams.err, cmd);
        return STATUS_INVALID_ARGS;
    }

    if (opts.erase) {
        for (int i = optind; i < argc; i++) function_remove(argv[i]);
        return STATUS_CMD_OK;
    }

    if (opts.description) {
        if (argc - optind != 1) {
            streams.err.append_format(_(L"%ls: Expected exactly one function name\n"), cmd);
            builtin_print_error_trailer(parser, streams.err, cmd);
            return STATUS_INVALID_ARGS;
        }

        const wchar_t *func = argv[optind];
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
        wcstring type_filter = opts.handlers_type ? opts.handlers_type : L"";
        if (!type_filter_valid(type_filter)) {
            streams.err.append_format(_(L"%ls: Expected generic | variable | signal | exit | "
                                        L"job-id for --handlers-type\n"),
                                      cmd);
            return STATUS_INVALID_ARGS;
        }
        event_print(streams, type_filter);
        return STATUS_CMD_OK;
    }

    // If we query with no argument, just return false.
    if (opts.query && argc == optind) {
        return STATUS_CMD_ERROR;
    }

    if (opts.list || argc == optind) {
        std::vector<wcstring> names = function_get_names(opts.show_hidden);
        std::sort(names.begin(), names.end());
        bool is_screen = !streams.out_is_redirected && isatty(STDOUT_FILENO);
        if (is_screen) {
            wcstring buff;
            for (const auto &name : names) {
                buff.append(name);
                buff.append(L", ");
            }
            if (!names.empty()) {
                // Trim trailing ", "
                buff.resize(buff.size() - 2, '\0');
            }

            streams.out.append(reformat_for_screen(buff, termsize_last()));
        } else {
            for (auto &name : names) {
                streams.out.append(name + L"\n");
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

        if (function_copy(current_func, new_func, parser)) return STATUS_CMD_OK;
        return STATUS_CMD_ERROR;
    }

    int res = STATUS_CMD_OK;
    for (int i = optind; i < argc; i++) {
        wcstring funcname = argv[i];
        auto func = function_get_props_autoload(argv[i], parser);
        if (!func) {
            res++;
        } else {
            if (!opts.query) {
                if (i != optind) streams.out.append(L"\n");
                if (!opts.no_metadata) {
                    report_function_metadata(funcname, opts.verbose, streams, parser, true);
                }
                wcstring def = func->annotated_definition(funcname);

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
