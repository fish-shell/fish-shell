// Implementation of the history builtin.
#include "config.h"  // IWYU pragma: keep

#include <errno.h>
#include <stddef.h>
#include <stdint.h>
#include <wchar.h>

#include <string>
#include <vector>

#include "builtin.h"
#include "builtin_history.h"
#include "common.h"
#include "fallback.h"  // IWYU pragma: keep
#include "history.h"
#include "io.h"
#include "parser.h"
#include "reader.h"
#include "wgetopt.h"
#include "wutil.h"  // IWYU pragma: keep

enum hist_cmd_t { HIST_SEARCH = 1, HIST_DELETE, HIST_CLEAR, HIST_MERGE, HIST_SAVE, HIST_UNDEF };

// Must be sorted by string, not enum or random.
static const enum_map<hist_cmd_t> hist_enum_map[] = {
    {HIST_CLEAR, L"clear"}, {HIST_DELETE, L"delete"}, {HIST_MERGE, L"merge"},
    {HIST_SAVE, L"save"},   {HIST_SEARCH, L"search"}, {HIST_UNDEF, NULL}};

struct history_cmd_opts_t {
    bool print_help = false;
    hist_cmd_t hist_cmd = HIST_UNDEF;
    history_search_type_t search_type = (history_search_type_t)-1;
    size_t max_items = SIZE_MAX;
    bool history_search_type_defined = false;
    const wchar_t *show_time_format = NULL;
    bool case_sensitive = false;
    bool null_terminate = false;
    bool reverse = false;
};

/// Note: Do not add new flags that represent subcommands. We're encouraging people to switch to
/// the non-flag subcommand form. While many of these flags are deprecated they must be
/// supported at least until fish 3.0 and possibly longer to avoid breaking everyones
/// config.fish and other scripts.
static const wchar_t *const short_options = L":CRcehmn:pt::z";
static const struct woption long_options[] = {{L"prefix", no_argument, NULL, 'p'},
                                              {L"contains", no_argument, NULL, 'c'},
                                              {L"help", no_argument, NULL, 'h'},
                                              {L"show-time", optional_argument, NULL, 't'},
                                              {L"exact", no_argument, NULL, 'e'},
                                              {L"max", required_argument, NULL, 'n'},
                                              {L"null", no_argument, NULL, 'z'},
                                              {L"case-sensitive", no_argument, NULL, 'C'},
                                              {L"delete", no_argument, NULL, 1},
                                              {L"search", no_argument, NULL, 2},
                                              {L"save", no_argument, NULL, 3},
                                              {L"clear", no_argument, NULL, 4},
                                              {L"merge", no_argument, NULL, 5},
                                              {L"reverse", no_argument, NULL, 'R'},
                                              {NULL, 0, NULL, 0}};

/// Remember the history subcommand and disallow selecting more than one history subcommand.
static bool set_hist_cmd(wchar_t *const cmd, hist_cmd_t *hist_cmd, hist_cmd_t sub_cmd,
                         io_streams_t &streams) {
    if (*hist_cmd != HIST_UNDEF) {
        wchar_t err_text[1024];
        const wchar_t *subcmd_str1 = enum_to_str(*hist_cmd, hist_enum_map);
        const wchar_t *subcmd_str2 = enum_to_str(sub_cmd, hist_enum_map);
        swprintf(err_text, sizeof(err_text) / sizeof(wchar_t),
                 _(L"you cannot do both '%ls' and '%ls' in the same invocation"), subcmd_str1,
                 subcmd_str2);
        streams.err.append_format(BUILTIN_ERR_COMBO2, cmd, err_text);
        return false;
    }

    *hist_cmd = sub_cmd;
    return true;
}

static bool check_for_unexpected_hist_args(const history_cmd_opts_t &opts, const wchar_t *cmd,
                                           const wcstring_list_t &args, io_streams_t &streams) {
    if (opts.history_search_type_defined || opts.show_time_format || opts.null_terminate) {
        const wchar_t *subcmd_str = enum_to_str(opts.hist_cmd, hist_enum_map);
        streams.err.append_format(_(L"%ls: you cannot use any options with the %ls command\n"), cmd,
                                  subcmd_str);
        return true;
    }
    if (args.size() != 0) {
        const wchar_t *subcmd_str = enum_to_str(opts.hist_cmd, hist_enum_map);
        streams.err.append_format(BUILTIN_ERR_ARG_COUNT2, cmd, subcmd_str, 0, args.size());
        return true;
    }
    return false;
}

static int parse_cmd_opts(history_cmd_opts_t &opts, int *optind,  //!OCLINT(high ncss method)
                          int argc, wchar_t **argv, parser_t &parser, io_streams_t &streams) {
    wchar_t *cmd = argv[0];
    int opt;
    wgetopter_t w;
    while ((opt = w.wgetopt_long(argc, argv, short_options, long_options, NULL)) != -1) {
        switch (opt) {
            case 1: {
                if (!set_hist_cmd(cmd, &opts.hist_cmd, HIST_DELETE, streams)) {
                    return STATUS_CMD_ERROR;
                }
                break;
            }
            case 2: {
                if (!set_hist_cmd(cmd, &opts.hist_cmd, HIST_SEARCH, streams)) {
                    return STATUS_CMD_ERROR;
                }
                break;
            }
            case 3: {
                if (!set_hist_cmd(cmd, &opts.hist_cmd, HIST_SAVE, streams)) {
                    return STATUS_CMD_ERROR;
                }
                break;
            }
            case 4: {
                if (!set_hist_cmd(cmd, &opts.hist_cmd, HIST_CLEAR, streams)) {
                    return STATUS_CMD_ERROR;
                }
                break;
            }
            case 5: {
                if (!set_hist_cmd(cmd, &opts.hist_cmd, HIST_MERGE, streams)) {
                    return STATUS_CMD_ERROR;
                }
                break;
            }
            case 'C': {
                opts.case_sensitive = true;
                break;
            }
            case 'R': {
                opts.reverse = true;
                break;
            }
            case 'p': {
                opts.search_type = HISTORY_SEARCH_TYPE_PREFIX_GLOB;
                opts.history_search_type_defined = true;
                break;
            }
            case 'c': {
                opts.search_type = HISTORY_SEARCH_TYPE_CONTAINS_GLOB;
                opts.history_search_type_defined = true;
                break;
            }
            case 'e': {
                opts.search_type = HISTORY_SEARCH_TYPE_EXACT;
                opts.history_search_type_defined = true;
                break;
            }
            case 't': {
                opts.show_time_format = w.woptarg ? w.woptarg : L"# %c%n";
                break;
            }
            case 'n': {
                long x = fish_wcstol(w.woptarg);
                if (errno) {
                    streams.err.append_format(_(L"%ls: max value '%ls' is not a valid number\n"),
                                              cmd, w.woptarg);
                    return STATUS_INVALID_ARGS;
                }
                opts.max_items = static_cast<size_t>(x);
                break;
            }
            case 'z': {
                opts.null_terminate = true;
                break;
            }
            case 'h': {
                opts.print_help = true;
                break;
            }
            case ':': {
                builtin_missing_argument(parser, streams, cmd, argv[w.woptind - 1]);
                return STATUS_INVALID_ARGS;
            }
            case '?': {
                // Try to parse it as a number; e.g., "-123".
                opts.max_items = fish_wcstol(argv[w.woptind - 1] + 1);
                if (errno) {
                    builtin_unknown_option(parser, streams, cmd, argv[w.woptind - 1]);
                    return STATUS_INVALID_ARGS;
                }
                w.nextchar = NULL;
                break;
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

/// Manipulate history of interactive commands executed by the user.
int builtin_history(parser_t &parser, io_streams_t &streams, wchar_t **argv) {
    wchar_t *cmd = argv[0];
    int argc = builtin_count_args(argv);
    history_cmd_opts_t opts;

    int optind;
    int retval = parse_cmd_opts(opts, &optind, argc, argv, parser, streams);
    if (retval != STATUS_CMD_OK) return retval;

    if (opts.print_help) {
        builtin_print_help(parser, streams, cmd, streams.out);
        return STATUS_CMD_OK;
    }

    // Use the default history if we have none (which happens if invoked non-interactively, e.g.
    // from webconfig.py.
    history_t *history = reader_get_history();
    if (!history) history = &history_t::history_with_name(history_session_id(parser.vars()));

    // If a history command hasn't already been specified via a flag check the first word.
    // Note that this can be simplified after we eliminate allowing subcommands as flags.
    // See the TODO above regarding the `long_options` array.
    if (optind < argc) {
        constexpr size_t hist_enum_map_len = sizeof hist_enum_map / sizeof *hist_enum_map;
        hist_cmd_t subcmd = str_to_enum(argv[optind], hist_enum_map, hist_enum_map_len);
        if (subcmd != HIST_UNDEF) {
            if (!set_hist_cmd(cmd, &opts.hist_cmd, subcmd, streams)) {
                return STATUS_INVALID_ARGS;
            }
            optind++;
        }
    }

    // Every argument that we haven't consumed already is an argument for a subcommand (e.g., a
    // search term).
    const wcstring_list_t args(argv + optind, argv + argc);

    // Establish appropriate defaults.
    if (opts.hist_cmd == HIST_UNDEF) opts.hist_cmd = HIST_SEARCH;
    if (!opts.history_search_type_defined) {
        if (opts.hist_cmd == HIST_SEARCH) opts.search_type = HISTORY_SEARCH_TYPE_CONTAINS_GLOB;
        if (opts.hist_cmd == HIST_DELETE) opts.search_type = HISTORY_SEARCH_TYPE_EXACT;
    }

    int status = STATUS_CMD_OK;
    switch (opts.hist_cmd) {
        case HIST_SEARCH: {
            if (!history->search(opts.search_type, args, opts.show_time_format, opts.max_items,
                                 opts.case_sensitive, opts.null_terminate, opts.reverse, streams)) {
                status = STATUS_CMD_ERROR;
            }
            break;
        }
        case HIST_DELETE: {
            // TODO: Move this code to the history module and support the other search types
            // including case-insensitive matches. At this time we expect the non-exact deletions to
            // be handled only by the history function's interactive delete feature.
            if (opts.search_type != HISTORY_SEARCH_TYPE_EXACT) {
                streams.err.append_format(_(L"builtin history delete only supports --exact\n"));
                status = STATUS_INVALID_ARGS;
                break;
            }
            if (!opts.case_sensitive) {
                streams.err.append_format(
                    _(L"builtin history delete --exact requires --case-sensitive\n"));
                status = STATUS_INVALID_ARGS;
                break;
            }
            for (wcstring delete_string : args) {
                if (delete_string[0] == '"' && delete_string[delete_string.length() - 1] == '"') {
                    delete_string = delete_string.substr(1, delete_string.length() - 2);
                }
                history->remove(delete_string);
            }
            break;
        }
        case HIST_CLEAR: {
            if (check_for_unexpected_hist_args(opts, cmd, args, streams)) {
                status = STATUS_INVALID_ARGS;
                break;
            }
            history->clear();
            history->save();
            break;
        }
        case HIST_MERGE: {
            if (check_for_unexpected_hist_args(opts, cmd, args, streams)) {
                status = STATUS_INVALID_ARGS;
                break;
            }

            history->incorporate_external_changes();
            break;
        }
        case HIST_SAVE: {
            if (check_for_unexpected_hist_args(opts, cmd, args, streams)) {
                status = STATUS_INVALID_ARGS;
                break;
            }
            history->save();
            break;
        }
        case HIST_UNDEF: {
            DIE("Unexpected HIST_UNDEF seen");
            break;
        }
    }

    return status;
}
