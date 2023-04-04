// Implementation of the history builtin.
#include "config.h"  // IWYU pragma: keep

#include "history.h"

#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "../builtin.h"
#include "../common.h"
#include "../enum_map.h"
#include "../env.h"
#include "../fallback.h"  // IWYU pragma: keep
#include "../history.h"
#include "../io.h"
#include "../maybe.h"
#include "../parser.h"
#include "../reader.h"
#include "../wgetopt.h"
#include "../wutil.h"  // IWYU pragma: keep

enum hist_cmd_t {
    HIST_SEARCH = 1,
    HIST_DELETE,
    HIST_CLEAR,
    HIST_MERGE,
    HIST_SAVE,
    HIST_UNDEF,
    HIST_CLEAR_SESSION
};

// Must be sorted by string, not enum or random.
static const enum_map<hist_cmd_t> hist_enum_map[] = {
    {HIST_CLEAR, L"clear"},   {HIST_CLEAR_SESSION, L"clear-session"},
    {HIST_DELETE, L"delete"}, {HIST_MERGE, L"merge"},
    {HIST_SAVE, L"save"},     {HIST_SEARCH, L"search"},
    {HIST_UNDEF, nullptr},
};

struct history_cmd_opts_t {
    hist_cmd_t hist_cmd = HIST_UNDEF;
    history_search_type_t search_type = static_cast<history_search_type_t>(-1);
    const wchar_t *show_time_format = nullptr;
    size_t max_items = SIZE_MAX;
    bool print_help = false;
    bool history_search_type_defined = false;
    bool case_sensitive = false;
    bool null_terminate = false;
    bool reverse = false;
};

/// Note: Do not add new flags that represent subcommands. We're encouraging people to switch to
/// the non-flag subcommand form. While many of these flags are deprecated they must be
/// supported at least until fish 3.0 and possibly longer to avoid breaking everyones
/// config.fish and other scripts.
static const wchar_t *const short_options = L":CRcehmn:pt::z";
static const struct woption long_options[] = {{L"prefix", no_argument, 'p'},
                                              {L"contains", no_argument, 'c'},
                                              {L"help", no_argument, 'h'},
                                              {L"show-time", optional_argument, 't'},
                                              {L"exact", no_argument, 'e'},
                                              {L"max", required_argument, 'n'},
                                              {L"null", no_argument, 'z'},
                                              {L"case-sensitive", no_argument, 'C'},
                                              {L"delete", no_argument, 1},
                                              {L"search", no_argument, 2},
                                              {L"save", no_argument, 3},
                                              {L"clear", no_argument, 4},
                                              {L"merge", no_argument, 5},
                                              {L"reverse", no_argument, 'R'},
                                              {}};

/// Remember the history subcommand and disallow selecting more than one history subcommand.
static bool set_hist_cmd(const wchar_t *cmd, hist_cmd_t *hist_cmd, hist_cmd_t sub_cmd,
                         io_streams_t &streams) {
    if (*hist_cmd != HIST_UNDEF) {
        streams.err.append_format(BUILTIN_ERR_COMBO2_EXCLUSIVE, cmd,
                                  enum_to_str(*hist_cmd, hist_enum_map),
                                  enum_to_str(sub_cmd, hist_enum_map));
        return false;
    }

    *hist_cmd = sub_cmd;
    return true;
}

static bool check_for_unexpected_hist_args(const history_cmd_opts_t &opts, const wchar_t *cmd,
                                           const std::vector<wcstring> &args,
                                           io_streams_t &streams) {
    if (opts.history_search_type_defined || opts.show_time_format || opts.null_terminate) {
        const wchar_t *subcmd_str = enum_to_str(opts.hist_cmd, hist_enum_map);
        streams.err.append_format(_(L"%ls: %ls: subcommand takes no options\n"), cmd, subcmd_str);
        return true;
    }
    if (!args.empty()) {
        const wchar_t *subcmd_str = enum_to_str(opts.hist_cmd, hist_enum_map);
        streams.err.append_format(BUILTIN_ERR_ARG_COUNT2, cmd, subcmd_str, 0, args.size());
        return true;
    }
    return false;
}

static int parse_cmd_opts(history_cmd_opts_t &opts, int *optind,  //!OCLINT(high ncss method)
                          int argc, const wchar_t **argv, parser_t &parser, io_streams_t &streams) {
    const wchar_t *cmd = argv[0];
    int opt;
    wgetopter_t w;
    while ((opt = w.wgetopt_long(argc, argv, short_options, long_options, nullptr)) != -1) {
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
                opts.search_type = history_search_type_t::PrefixGlob;
                opts.history_search_type_defined = true;
                break;
            }
            case 'c': {
                opts.search_type = history_search_type_t::ContainsGlob;
                opts.history_search_type_defined = true;
                break;
            }
            case 'e': {
                opts.search_type = history_search_type_t::Exact;
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
                    streams.err.append_format(BUILTIN_ERR_NOT_NUMBER, cmd, w.woptarg);
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
                w.nextchar = nullptr;
                break;
            }
            default: {
                DIE("unexpected retval from wgetopt_long");
            }
        }
    }

    *optind = w.woptind;
    return STATUS_CMD_OK;
}

/// Manipulate history of interactive commands executed by the user.
maybe_t<int> builtin_history(parser_t &parser, io_streams_t &streams, const wchar_t **argv) {
    const wchar_t *cmd = argv[0];
    int argc = builtin_count_args(argv);
    history_cmd_opts_t opts;

    int optind;
    int retval = parse_cmd_opts(opts, &optind, argc, argv, parser, streams);
    if (retval != STATUS_CMD_OK) return retval;

    if (opts.print_help) {
        builtin_print_help(parser, streams, cmd);
        return STATUS_CMD_OK;
    }

    // Use the default history if we have none (which happens if invoked non-interactively, e.g.
    // from webconfig.py.
    auto history = commandline_get_state().history;
    if (!history) history = history_with_name(history_session_id(parser.vars()).c_str());

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
    const std::vector<wcstring> args(argv + optind, argv + argc);

    // Establish appropriate defaults.
    if (opts.hist_cmd == HIST_UNDEF) opts.hist_cmd = HIST_SEARCH;
    if (!opts.history_search_type_defined) {
        if (opts.hist_cmd == HIST_SEARCH) opts.search_type = history_search_type_t::ContainsGlob;
        if (opts.hist_cmd == HIST_DELETE) opts.search_type = history_search_type_t::Exact;
    }

    int status = STATUS_CMD_OK;
    switch (opts.hist_cmd) {
        case HIST_SEARCH: {
            if (!(*history)->search(opts.search_type,
                                    args,
                                    opts.show_time_format, opts.max_items, opts.case_sensitive,
                                    opts.null_terminate, opts.reverse, true, streams)) {
                status = STATUS_CMD_ERROR;
            }
            break;
        }
        case HIST_DELETE: {
            // TODO: Move this code to the history module and support the other search types
            // including case-insensitive matches. At this time we expect the non-exact deletions to
            // be handled only by the history function's interactive delete feature.
            if (opts.search_type != history_search_type_t::Exact) {
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
            for (const wcstring &delete_string : args) {
                (*history)->remove(delete_string.c_str());
            }
            break;
        }
        case HIST_CLEAR: {
            if (check_for_unexpected_hist_args(opts, cmd, args, streams)) {
                status = STATUS_INVALID_ARGS;
                break;
            }
            (*history)->clear();
            (*history)->save();
            break;
        }
        case HIST_CLEAR_SESSION: {
            if (check_for_unexpected_hist_args(opts, cmd, args, streams)) {
                status = STATUS_INVALID_ARGS;
                break;
            }
            (*history)->clear_session();
            (*history)->save();
            break;
        }
        case HIST_MERGE: {
            if (check_for_unexpected_hist_args(opts, cmd, args, streams)) {
                status = STATUS_INVALID_ARGS;
                break;
            }

            if (in_private_mode(parser.vars())) {
                streams.err.append_format(_(L"%ls: can't merge history in private mode\n"), cmd);
                status = STATUS_INVALID_ARGS;
                break;
            }
            (*history)->incorporate_external_changes();
            break;
        }
        case HIST_SAVE: {
            if (check_for_unexpected_hist_args(opts, cmd, args, streams)) {
                status = STATUS_INVALID_ARGS;
                break;
            }
            (*history)->save();
            break;
        }
        case HIST_UNDEF: {
            DIE("Unexpected HIST_UNDEF seen");
        }
    }

    return status;
}
