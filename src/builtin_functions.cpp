// Implementation of the functions builtin.
#include "config.h"  // IWYU pragma: keep

#include <stddef.h>
#include <unistd.h>
#include <wchar.h>

#include <algorithm>
#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "builtin.h"
#include "builtin_functions.h"
#include "common.h"
#include "env.h"
#include "event.h"
#include "fallback.h"  // IWYU pragma: keep
#include "function.h"
#include "io.h"
#include "parser_keywords.h"
#include "proc.h"
#include "signal.h"
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
    wchar_t *description = NULL;
};
static const wchar_t *short_options = L":Dacehnqv";
static const struct woption long_options[] = {
    {L"erase", no_argument, NULL, 'e'},   {L"description", required_argument, NULL, 'd'},
    {L"names", no_argument, NULL, 'n'},   {L"all", no_argument, NULL, 'a'},
    {L"help", no_argument, NULL, 'h'},    {L"query", no_argument, NULL, 'q'},
    {L"copy", no_argument, NULL, 'c'},    {L"details", no_argument, NULL, 'D'},
    {L"verbose", no_argument, NULL, 'v'}, {NULL, 0, NULL, 0}};

static int parse_cmd_opts(functions_cmd_opts_t &opts, int *optind,  //!OCLINT(high ncss method)
                          int argc, wchar_t **argv, parser_t &parser, io_streams_t &streams) {
    wchar_t *cmd = argv[0];
    int opt;
    wgetopter_t w;
    while ((opt = w.wgetopt_long(argc, argv, short_options, long_options, NULL)) != -1) {
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

/// Return a definition of the specified function. Used by the functions builtin.
static wcstring functions_def(const wcstring &name) {
    CHECK(!name.empty(), L"");  //!OCLINT(multiple unary operator)
    wcstring out;
    wcstring desc, def;
    function_get_desc(name, &desc);
    function_get_definition(name, &def);
    event_t search(EVENT_ANY);
    search.function_name = name;
    std::vector<std::shared_ptr<event_t>> ev;
    event_get(search, &ev);

    out.append(L"function ");

    // Typically we prefer to specify the function name first, e.g. "function foo --description bar"
    // But If the function name starts with a -, we'll need to output it after all the options.
    bool defer_function_name = (name.at(0) == L'-');
    if (!defer_function_name) {
        out.append(escape_string(name, true));
    }

    if (!desc.empty()) {
        wcstring esc_desc = escape_string(desc, true);
        out.append(L" --description ");
        out.append(esc_desc);
    }

    if (!function_get_shadow_scope(name)) {
        out.append(L" --no-scope-shadowing");
    }

    for (size_t i = 0; i < ev.size(); i++) {
        const event_t *next = ev.at(i).get();
        switch (next->type) {
            case EVENT_SIGNAL: {
                append_format(out, L" --on-signal %ls", sig2wcs(next->param1.signal));
                break;
            }
            case EVENT_VARIABLE: {
                append_format(out, L" --on-variable %ls", next->str_param1.c_str());
                break;
            }
            case EVENT_EXIT: {
                if (next->param1.pid > 0)
                    append_format(out, L" --on-process-exit %d", next->param1.pid);
                else
                    append_format(out, L" --on-job-exit %d", -next->param1.pid);
                break;
            }
            case EVENT_JOB_ID: {
                const job_t *j = job_get(next->param1.job_id);
                if (j) append_format(out, L" --on-job-exit %d", j->pgid);
                break;
            }
            case EVENT_GENERIC: {
                append_format(out, L" --on-event %ls", next->str_param1.c_str());
                break;
            }
            default: {
                DIE("unexpected next->type");
                break;
            }
        }
    }

    wcstring_list_t named = function_get_named_arguments(name);
    if (!named.empty()) {
        append_format(out, L" --argument");
        for (size_t i = 0; i < named.size(); i++) {
            append_format(out, L" %ls", named.at(i).c_str());
        }
    }

    // Output the function name if we deferred it.
    if (defer_function_name) {
        out.append(L" -- ");
        out.append(escape_string(name, true));
    }

    // Output any inherited variables as `set -l` lines.
    std::map<wcstring, env_var_t> inherit_vars = function_get_inherit_vars(name);
    for (std::map<wcstring, env_var_t>::const_iterator it = inherit_vars.begin(),
                                                       end = inherit_vars.end();
         it != end; ++it) {
        wcstring_list_t lst;
        if (!it->second.missing()) {
            tokenize_variable_array(it->second, lst);
        }

        // This forced tab is crummy, but we don't know what indentation style the function uses.
        append_format(out, L"\n\tset -l %ls", it->first.c_str());
        for (wcstring_list_t::const_iterator arg_it = lst.begin(), arg_end = lst.end();
             arg_it != arg_end; ++arg_it) {
            wcstring earg = escape_string(*arg_it, ESCAPE_ALL);
            out.push_back(L' ');
            out.append(earg);
        }
    }

    // This forced tab is sort of crummy - not all functions start with a tab.
    append_format(out, L"\n\t%ls", def.c_str());

    // Append a newline before the 'end', unless there already is one there.
    if (!string_suffixes_string(L"\n", def)) {
        out.push_back(L'\n');
    }
    out.append(L"end\n");
    return out;
}

static int report_function_metadata(const wchar_t *funcname, bool verbose, io_streams_t &streams,
                                    bool metadata_as_comments) {
    const wchar_t *path = L"n/a";
    const wchar_t *autoloaded = L"n/a";
    const wchar_t *shadows_scope = L"n/a";
    wcstring description = L"n/a";
    int line_number = 0;

    if (function_exists(funcname)) {
        path = function_get_definition_file(funcname);
        if (path) {
            autoloaded = function_is_autoloaded(funcname) ? L"autoloaded" : L"not-autoloaded";
            line_number = function_get_definition_offset(funcname);
        } else {
            path = L"stdin";
        }
        shadows_scope =
            function_get_shadow_scope(funcname) ? L"scope-shadowing" : L"no-scope-shadowing";
        function_get_desc(funcname, &description);
        description = escape_string(description, ESCAPE_NO_QUOTED);
    }

    if (metadata_as_comments) {
        if (wcscmp(path, L"stdin")) {
            streams.out.append_format(L"# Defined in %ls @ line %d\n", path, line_number);
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
int builtin_functions(parser_t &parser, io_streams_t &streams, wchar_t **argv) {
    const wchar_t *cmd = argv[0];
    int argc = builtin_count_args(argv);
    functions_cmd_opts_t opts;

    int optind;
    int retval = parse_cmd_opts(opts, &optind, argc, argv, parser, streams);
    if (retval != STATUS_CMD_OK) return retval;

    if (opts.print_help) {
        builtin_print_help(parser, streams, cmd, streams.out);
        return STATUS_CMD_OK;
    }

    // Erase, desc, query, copy and list are mutually exclusive.
    bool describe = opts.description ? true : false;
    if (describe + opts.erase + opts.list + opts.query + opts.copy > 1) {
        streams.err.append_format(_(L"%ls: Invalid combination of options\n"), cmd);
        builtin_print_help(parser, streams, cmd, streams.err);
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
            builtin_print_help(parser, streams, cmd, streams.err);
            return STATUS_INVALID_ARGS;
        }

        func = argv[optind];
        if (!function_exists(func)) {
            streams.err.append_format(_(L"%ls: Function '%ls' does not exist\n"), cmd, func);
            builtin_print_help(parser, streams, cmd, streams.err);
            return STATUS_CMD_ERROR;
        }

        function_set_desc(func, opts.description);
        return STATUS_CMD_OK;
    }

    if (opts.report_metadata) {
        if (argc - optind != 1) {
            streams.err.append_format(_(L"%ls: Expected exactly one function name for --details\n"),
                                      cmd);
            return STATUS_INVALID_ARGS;
        }

        const wchar_t *funcname = argv[optind];
        return report_function_metadata(funcname, opts.verbose, streams, false);
    }

    if (opts.list || argc == optind) {
        wcstring_list_t names = function_get_names(opts.show_hidden);
        std::sort(names.begin(), names.end());
        bool is_screen = !streams.out_is_redirected && isatty(STDOUT_FILENO);
        if (is_screen) {
            wcstring buff;
            for (size_t i = 0; i < names.size(); i++) {
                buff.append(names.at(i));
                buff.append(L", ");
            }
            streams.out.append(reformat_for_screen(buff));
        } else {
            for (size_t i = 0; i < names.size(); i++) {
                streams.out.append(names.at(i).c_str());
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
            builtin_print_help(parser, streams, cmd, streams.err);
            return STATUS_INVALID_ARGS;
        }
        current_func = argv[optind];
        new_func = argv[optind + 1];

        if (!function_exists(current_func)) {
            streams.err.append_format(_(L"%ls: Function '%ls' does not exist\n"), cmd,
                                      current_func.c_str());
            builtin_print_help(parser, streams, cmd, streams.err);
            return STATUS_CMD_ERROR;
        }

        if (!valid_func_name(new_func) || parser_keywords_is_reserved(new_func)) {
            streams.err.append_format(_(L"%ls: Illegal function name '%ls'\n"), cmd,
                                      new_func.c_str());
            builtin_print_help(parser, streams, cmd, streams.err);
            return STATUS_INVALID_ARGS;
        }

        // Keep things simple: don't allow existing names to be copy targets.
        if (function_exists(new_func)) {
            streams.err.append_format(
                _(L"%ls: Function '%ls' already exists. Cannot create copy '%ls'\n"), cmd,
                new_func.c_str(), current_func.c_str());
            builtin_print_help(parser, streams, cmd, streams.err);
            return STATUS_CMD_ERROR;
        }

        if (function_copy(current_func, new_func)) return STATUS_CMD_OK;
        return STATUS_CMD_ERROR;
    }

    int res = STATUS_CMD_OK;
    for (int i = optind; i < argc; i++) {
        if (!function_exists(argv[i])) {
            res++;
        } else {
            if (!opts.query) {
                if (i != optind) streams.out.append(L"\n");
                const wchar_t *funcname = argv[optind];
                report_function_metadata(funcname, opts.verbose, streams, true);
                streams.out.append(functions_def(funcname));
            }
        }
    }

    return res;
}
