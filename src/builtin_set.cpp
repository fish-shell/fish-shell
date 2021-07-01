// Functions used for implementing the set builtin.
#include "config.h"  // IWYU pragma: keep

#include <sys/stat.h>
#include <unistd.h>

#include <algorithm>
#include <cerrno>
#include <cstddef>
#include <cstdlib>
#include <cstring>
#include <cwchar>
#include <iterator>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include "builtin.h"
#include "common.h"
#include "env.h"
#include "expand.h"
#include "fallback.h"  // IWYU pragma: keep
#include "history.h"
#include "io.h"
#include "parser.h"
#include "proc.h"
#include "wcstringutil.h"
#include "wgetopt.h"
#include "wutil.h"  // IWYU pragma: keep

struct set_cmd_opts_t {
    bool print_help = false;
    bool show = false;
    bool local = false;
    bool global = false;
    bool exportv = false;
    bool erase = false;
    bool list = false;
    bool unexport = false;
    bool pathvar = false;
    bool unpathvar = false;
    bool universal = false;
    bool query = false;
    bool shorten_ok = true;
    bool append = false;
    bool prepend = false;
    bool preserve_failure_exit_status = true;
};

/// Values used for long-only options.
enum {
    opt_path = 1,
    opt_unpath = 2,
};

// Variables used for parsing the argument list. This command is atypical in using the "+"
// (REQUIRE_ORDER) option for flag parsing. This is not typical of most fish commands. It means
// we stop scanning for flags when the first non-flag argument is seen.
static const wchar_t *const short_options = L"+:LSUaeghlnpqux";
static const struct woption long_options[] = {
    {L"export", no_argument, nullptr, 'x'},    {L"global", no_argument, nullptr, 'g'},
    {L"local", no_argument, nullptr, 'l'},     {L"erase", no_argument, nullptr, 'e'},
    {L"names", no_argument, nullptr, 'n'},     {L"unexport", no_argument, nullptr, 'u'},
    {L"universal", no_argument, nullptr, 'U'}, {L"long", no_argument, nullptr, 'L'},
    {L"query", no_argument, nullptr, 'q'},     {L"show", no_argument, nullptr, 'S'},
    {L"append", no_argument, nullptr, 'a'},    {L"prepend", no_argument, nullptr, 'p'},
    {L"path", no_argument, nullptr, opt_path}, {L"unpath", no_argument, nullptr, opt_unpath},
    {L"help", no_argument, nullptr, 'h'},      {nullptr, 0, nullptr, 0}};

// Hint for invalid path operation with a colon.
#define BUILTIN_SET_PATH_ERROR _(L"%ls: Warning: $%ls entry \"%ls\" is not valid (%s)\n")
#define BUILTIN_SET_PATH_HINT _(L"%ls: Did you mean 'set %ls $%ls %ls'?\n")
#define BUILTIN_SET_MISMATCHED_ARGS _(L"%ls: You provided %d indexes but %d values\n")
#define BUILTIN_SET_ERASE_NO_VAR _(L"%ls: Erase needs a variable name\n")
#define BUILTIN_SET_ARRAY_BOUNDS_ERR _(L"%ls: Array index out of bounds\n")
#define BUILTIN_SET_UVAR_ERR \
    _(L"%ls: Universal variable '%ls' is shadowed by the global variable of the same name.\n")

static int parse_cmd_opts(set_cmd_opts_t &opts, int *optind,  //!OCLINT(high ncss method)
                          int argc, const wchar_t **argv, parser_t &parser, io_streams_t &streams) {
    const wchar_t *cmd = argv[0];

    int opt;
    wgetopter_t w;
    while ((opt = w.wgetopt_long(argc, argv, short_options, long_options, nullptr)) != -1) {
        switch (opt) {
            case 'a': {
                opts.append = true;
                break;
            }
            case 'e': {
                opts.erase = true;
                opts.preserve_failure_exit_status = false;
                break;
            }
            case 'g': {
                opts.global = true;
                break;
            }
            case 'h': {
                opts.print_help = true;
                break;
            }
            case 'l': {
                opts.local = true;
                break;
            }
            case 'n': {
                opts.list = true;
                opts.preserve_failure_exit_status = false;
                break;
            }
            case 'p': {
                opts.prepend = true;
                break;
            }
            case 'q': {
                opts.query = true;
                opts.preserve_failure_exit_status = false;
                break;
            }
            case 'x': {
                opts.exportv = true;
                break;
            }
            case 'u': {
                opts.unexport = true;
                break;
            }
            case opt_path: {
                opts.pathvar = true;
                break;
            }
            case opt_unpath: {
                opts.unpathvar = true;
                break;
            }
            case 'U': {
                opts.universal = true;
                break;
            }
            case 'L': {
                opts.shorten_ok = false;
                break;
            }
            case 'S': {
                opts.show = true;
                opts.preserve_failure_exit_status = false;
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

static int validate_cmd_opts(const wchar_t *cmd,
                             const set_cmd_opts_t &opts,  //!OCLINT(npath complexity)
                             int argc, parser_t &parser, io_streams_t &streams) {
    // Can't query and erase or list.
    if (opts.query && (opts.erase || opts.list)) {
        streams.err.append_format(BUILTIN_ERR_COMBO, cmd);
        builtin_print_error_trailer(parser, streams.err, cmd);
        return STATUS_INVALID_ARGS;
    }

    // We can't both list and erase variables.
    if (opts.erase && opts.list) {
        streams.err.append_format(BUILTIN_ERR_COMBO, cmd);
        builtin_print_error_trailer(parser, streams.err, cmd);
        return STATUS_INVALID_ARGS;
    }

    // Variables can only have one scope.
    if (opts.local + opts.global + opts.universal > 1) {
        streams.err.append_format(BUILTIN_ERR_GLOCAL, cmd);
        builtin_print_error_trailer(parser, streams.err, cmd);
        return STATUS_INVALID_ARGS;
    }

    // Variables can only have one export status.
    if (opts.exportv && opts.unexport) {
        streams.err.append_format(BUILTIN_ERR_EXPUNEXP, cmd);
        builtin_print_error_trailer(parser, streams.err, cmd);
        return STATUS_INVALID_ARGS;
    }

    // Variables can only have one path status.
    if (opts.pathvar && opts.unpathvar) {
        streams.err.append_format(BUILTIN_ERR_EXPUNEXP, cmd);
        builtin_print_error_trailer(parser, streams.err, cmd);
        return STATUS_INVALID_ARGS;
    }

    // Trying to erase and (un)export at the same time doesn't make sense.
    if (opts.erase && (opts.exportv || opts.unexport)) {
        streams.err.append_format(BUILTIN_ERR_COMBO, cmd);
        builtin_print_error_trailer(parser, streams.err, cmd);
        return STATUS_INVALID_ARGS;
    }

    // The --show flag cannot be combined with any other flag.
    if (opts.show &&
        (opts.local || opts.global || opts.erase || opts.list || opts.exportv || opts.universal)) {
        streams.err.append_format(BUILTIN_ERR_COMBO, cmd);
        builtin_print_error_trailer(parser, streams.err, cmd);
        return STATUS_INVALID_ARGS;
    }

    if (argc == 0 && opts.erase) {
        streams.err.append_format(BUILTIN_SET_ERASE_NO_VAR, cmd);
        builtin_print_error_trailer(parser, streams.err, cmd);
        return STATUS_INVALID_ARGS;
    }

    return STATUS_CMD_OK;
}

// Check if we are setting a uvar and a global of the same name exists. See
// https://github.com/fish-shell/fish-shell/issues/806
static int check_global_scope_exists(const wchar_t *cmd, const set_cmd_opts_t &opts,
                                     const wcstring &dest, io_streams_t &streams,
                                     const parser_t &parser) {
    if (opts.universal) {
        auto global_dest = parser.vars().get(dest, ENV_GLOBAL);
        if (global_dest && parser.is_interactive()) {
            streams.err.append_format(BUILTIN_SET_UVAR_ERR, cmd, dest.c_str());
        }
    }

    return STATUS_CMD_OK;
}

static void handle_env_return(int retval, const wchar_t *cmd, const wcstring &key,
                              io_streams_t &streams) {
    switch (retval) {
        case ENV_OK: {
            break;
        }
        case ENV_PERM: {
            streams.err.append_format(_(L"%ls: Tried to change the read-only variable '%ls'\n"),
                                      cmd, key.c_str());
            break;
        }
        case ENV_SCOPE: {
            streams.err.append_format(
                _(L"%ls: Tried to modify the special variable '%ls' with the wrong scope\n"), cmd,
                key.c_str());
            break;
        }
        case ENV_INVALID: {
            streams.err.append_format(
                _(L"%ls: Tried to modify the special variable '%ls' to an invalid value\n"), cmd,
                key.c_str());
            break;
        }
        case ENV_NOT_FOUND: {
            streams.err.append_format(_(L"%ls: The variable '%ls' does not exist\n"), cmd,
                                      key.c_str());
            break;
        }
        default: {
            DIE("unexpected vars.set() ret val");
        }
    }
}

/// Call vars.set. If this is a path variable, e.g. PATH, validate the elements. On error, print a
/// description of the problem to stderr.
static int env_set_reporting_errors(const wchar_t *cmd, const wcstring &key, int scope,
                                    wcstring_list_t list, io_streams_t &streams, env_stack_t &vars,
                                    std::vector<event_t> *evts) {
    int retval = vars.set(key, scope | ENV_USER, std::move(list), evts);
    handle_env_return(retval, cmd, key, streams);

    return retval;
}

/// A helper type returned by split_var_and_indexes.
struct split_var_t {
    wcstring varname;             // name of the variable
    maybe_t<env_var_t> var{};     // value of the variable, or none if missing
    std::vector<long> indexes{};  // list of requested indexes

    /// \return the number of elements in our variable, or 0 if missing.
    long varsize() const { return var ? static_cast<long>(var->as_list().size()) : 0L; }
};

/// Extract indexes from an argument of the form `var_name[index1 index2...]`.
/// The argument \p arg is split into a variable name and list of indexes, which is returned by
/// reference. Indexes are "expanded" in the sense that range expressions .. and negative values are
/// handled.
///
/// Returns:
///   a split var on success, none() on error, in which case an error will have been printed.
///   If no index is found, this leaves indexes empty.
maybe_t<split_var_t> split_var_and_indexes(const wchar_t *arg, env_mode_flags_t mode,
                                           const environment_t &vars, io_streams_t &streams) {
    split_var_t res{};
    const wchar_t *open_bracket = std::wcschr(arg, L'[');
    size_t varname_len = open_bracket ? open_bracket - arg : wcslen(arg);
    res.varname.assign(arg, varname_len);
    res.var = vars.get(res.varname, mode);
    if (!open_bracket) {
        // Common case of no bracket.
        return res;
    }

    long varsize = res.varsize();
    const wchar_t *p = open_bracket + 1;
    while (*p != L']') {
        const wchar_t *end;
        long l_ind;
        if (res.indexes.empty() && p[0] == L'.' && p[1] == L'.') {
            // If we are at the first index expression, a missing start-index means the range starts
            // at the first item.
            l_ind = 1;  // first index
        } else {
            const wchar_t *end = nullptr;
            l_ind = fish_wcstol(p, &end);
            if (errno > 0) {  // ignore errno == -1 meaning the int did not end with a '\0'
                streams.err.append_format(_(L"%ls: Invalid index starting at '%ls'\n"), L"set",
                                          res.varname.c_str());
                return none();
            }
            p = end;
        }

        // Convert negative index to a positive index.
        if (l_ind < 0) l_ind = varsize + l_ind + 1;

        if (p[0] == L'.' && p[1] == L'.') {
            p += 2;
            long l_ind2;
            // If we are at the last index range expression, a missing end-index means the range
            // spans until the last item.
            if (res.indexes.empty() && *p == L']') {
                l_ind2 = -1;
            } else {
                l_ind2 = fish_wcstol(p, &end);
                if (errno > 0) {  // ignore errno == -1 meaning there was text after the int
                    return none();
                }
                p = end;
            }

            // Convert negative index to a positive index.
            if (l_ind2 < 0) l_ind2 = varsize + l_ind2 + 1;

            int direction = l_ind2 < l_ind ? -1 : 1;
            for (long jjj = l_ind; jjj * direction <= l_ind2 * direction; jjj += direction) {
                res.indexes.push_back(jjj);
            }
        } else {
            res.indexes.push_back(l_ind);
        }
    }
    return res;
}

/// Given a list of values and 1-based indexes, return a new list, with those elements removed.
/// Note this deliberately accepts both args by value, as it modifies them both.
static wcstring_list_t erased_at_indexes(wcstring_list_t input, std::vector<long> indexes) {
    // Sort our indexes into *descending* order.
    std::sort(indexes.begin(), indexes.end(), std::greater<long>());

    // Remove duplicates.
    indexes.erase(std::unique(indexes.begin(), indexes.end()), indexes.end());

    // Now when we walk indexes, we encounter larger indexes first.
    for (long idx : indexes) {
        if (idx > 0 && static_cast<size_t>(idx) <= input.size()) {
            // One-based indexing!
            input.erase(input.begin() + idx - 1);
        }
    }
    return input;
}

static env_mode_flags_t compute_scope(const set_cmd_opts_t &opts) {
    int scope = ENV_USER;
    if (opts.local) scope |= ENV_LOCAL;
    if (opts.global) scope |= ENV_GLOBAL;
    if (opts.exportv) scope |= ENV_EXPORT;
    if (opts.unexport) scope |= ENV_UNEXPORT;
    if (opts.universal) scope |= ENV_UNIVERSAL;
    if (opts.pathvar) scope |= ENV_PATHVAR;
    if (opts.unpathvar) scope |= ENV_UNPATHVAR;
    return scope;
}

/// Print the names of all environment variables in the scope. It will include the values unless the
/// `set --names` flag was used.
static int builtin_set_list(const wchar_t *cmd, set_cmd_opts_t &opts, int argc,
                            const wchar_t **argv, parser_t &parser, io_streams_t &streams) {
    UNUSED(cmd);
    UNUSED(argc);
    UNUSED(argv);
    UNUSED(parser);

    bool names_only = opts.list;
    wcstring_list_t names = parser.vars().get_names(compute_scope(opts));
    sort(names.begin(), names.end());

    for (const auto &key : names) {
        const wcstring e_key = escape_string(key, 0);
        streams.out.append(e_key);

        if (!names_only) {
            wcstring val;
            if (opts.shorten_ok && key == L"history") {
                std::shared_ptr<history_t> history =
                    history_t::with_name(history_session_id(parser.vars()));
                for (size_t i = 1; i < history->size() && val.size() < 64; i++) {
                    if (i > 1) val += L' ';
                    val += expand_escape_string(history->item_at_index(i).str());
                }
            } else {
                auto var = parser.vars().get(key, compute_scope(opts));
                if (!var.missing_or_empty()) {
                    val = expand_escape_variable(*var);
                }
            }
            if (!val.empty()) {
                bool shorten = false;
                if (opts.shorten_ok && val.length() > 64) {
                    shorten = true;
                    val.resize(60);
                }
                streams.out.append(L" ");
                streams.out.append(val);

                if (shorten) streams.out.push_back(get_ellipsis_char());
            }
        }

        streams.out.append(L"\n");
    }

    return STATUS_CMD_OK;
}

// Query mode. Return the number of variables that do NOT exist out of the specified variables.
static int builtin_set_query(const wchar_t *cmd, set_cmd_opts_t &opts, int argc,
                             const wchar_t **argv, parser_t &parser, io_streams_t &streams) {
    int retval = 0;
    env_mode_flags_t scope = compute_scope(opts);

    for (int i = 0; i < argc; i++) {
        auto split = split_var_and_indexes(argv[i], scope, parser.vars(), streams);
        if (!split) {
            builtin_print_error_trailer(parser, streams.err, cmd);
            return STATUS_CMD_ERROR;
        }

        if (split->indexes.empty()) {
            // No indexes, just increment if our variable is missing.
            if (!split->var) retval++;
        } else {
            // Increment for every index out of range.
            long varsize = split->varsize();
            for (long idx : split->indexes) {
                if (idx < 1 || idx > varsize) retval++;
            }
        }
    }

    return retval;
}

static void show_scope(const wchar_t *var_name, int scope, io_streams_t &streams,
                       const environment_t &vars) {
    const wchar_t *scope_name;
    switch (scope) {
        case ENV_LOCAL: {
            scope_name = L"local";
            break;
        }
        case ENV_GLOBAL: {
            scope_name = L"global";
            break;
        }
        case ENV_UNIVERSAL: {
            scope_name = L"universal";
            break;
        }
        default: {
            DIE("invalid scope");
        }
    }

    const auto var = vars.get(var_name, scope);
    if (!var) {
        return;
    }

    const wchar_t *exportv = var->exports() ? _(L"exported") : _(L"unexported");
    const wchar_t *pathvarv = var->is_pathvar() ? _(L" a path variable") : L"";
    wcstring_list_t vals = var->as_list();
    streams.out.append_format(_(L"$%ls: set in %ls scope, %ls,%ls with %d elements\n"), var_name,
                              scope_name, exportv, pathvarv, vals.size());

    for (size_t i = 0; i < vals.size(); i++) {
        if (vals.size() > 100) {
            if (i == 50) {
                // try to print a mid-line ellipsis because we are eliding lines not words
                streams.out.append(get_ellipsis_char() > 256 ? L"\u22EF" : get_ellipsis_str());
                streams.out.push_back(L'\n');
            }
            if (i >= 50 && i < vals.size() - 50) continue;
        }
        const wcstring value = vals[i];
        const wcstring escaped_val = escape_string(value, ESCAPE_NO_QUOTED, STRING_STYLE_SCRIPT);
        streams.out.append_format(_(L"$%ls[%d]: |%ls|\n"), var_name, i + 1, escaped_val.c_str());
    }
}

/// Show mode. Show information about the named variable(s).
static int builtin_set_show(const wchar_t *cmd, const set_cmd_opts_t &opts, int argc,
                            const wchar_t **argv, parser_t &parser, io_streams_t &streams) {
    UNUSED(opts);
    const auto &vars = parser.vars();
    if (argc == 0) {  // show all vars
        wcstring_list_t names = parser.vars().get_names(ENV_USER);
        sort(names.begin(), names.end());
        for (const auto &name : names) {
            if (name == L"history") continue;
            show_scope(name.c_str(), ENV_LOCAL, streams, vars);
            show_scope(name.c_str(), ENV_GLOBAL, streams, vars);
            show_scope(name.c_str(), ENV_UNIVERSAL, streams, vars);
        }
    } else {
        for (int i = 0; i < argc; i++) {
            const wchar_t *arg = argv[i];

            if (!valid_var_name(arg)) {
                streams.err.append_format(BUILTIN_ERR_VARNAME, cmd, arg);
                builtin_print_error_trailer(parser, streams.err, cmd);
                return STATUS_INVALID_ARGS;
            }

            if (std::wcschr(arg, L'[')) {
                streams.err.append_format(
                    _(L"%ls: `set --show` does not allow slices with the var names\n"), cmd);
                builtin_print_error_trailer(parser, streams.err, cmd);
                return STATUS_CMD_ERROR;
            }

            show_scope(arg, ENV_LOCAL, streams, vars);
            show_scope(arg, ENV_GLOBAL, streams, vars);
            show_scope(arg, ENV_UNIVERSAL, streams, vars);
        }
    }

    return STATUS_CMD_OK;
}

/// Erase a variable.
static int builtin_set_erase(const wchar_t *cmd, set_cmd_opts_t &opts, int argc,
                             const wchar_t **argv, parser_t &parser, io_streams_t &streams) {
    int ret = STATUS_CMD_OK;
    env_mode_flags_t scope = compute_scope(opts);
    for (int i = 0; i < argc; i++) {
        auto split = split_var_and_indexes(argv[i], scope, parser.vars(), streams);
        if (!split) {
            builtin_print_error_trailer(parser, streams.err, cmd);
            return STATUS_CMD_ERROR;
        }

        if (!valid_var_name(split->varname)) {
            streams.err.append_format(BUILTIN_ERR_VARNAME, cmd, split->varname.c_str());
            builtin_print_error_trailer(parser, streams.err, cmd);
            return STATUS_INVALID_ARGS;
        }

        int retval = STATUS_CMD_OK;
        std::vector<event_t> evts;
        if (split->indexes.empty()) {  // unset the var
            retval = parser.vars().remove(split->varname, scope, &evts);
            // When a non-existent-variable is unset, return ENV_NOT_FOUND as $status
            // but do not emit any errors at the console as a compromise between user
            // friendliness and correctness.
            if (retval != ENV_NOT_FOUND) {
                handle_env_return(retval, cmd, split->varname, streams);
            }
        } else {  // remove just the specified indexes of the var
            if (!split->var) return STATUS_CMD_ERROR;
            wcstring_list_t result = erased_at_indexes(split->var->as_list(), split->indexes);
            retval = env_set_reporting_errors(cmd, split->varname, scope, std::move(result),
                                              streams, parser.vars(), &evts);
        }

        // Fire any events.
        for (const auto &evt : evts) {
            event_fire(parser, evt);
        }

        // Set $status to the last error value.
        // This is cheesy, but I don't expect this to be checked often.
        if (retval != STATUS_CMD_OK) {
            ret = retval;
        } else {
            retval = check_global_scope_exists(cmd, opts, split->varname, streams, parser);
            if (retval != STATUS_CMD_OK) ret = retval;
        }
    }
    return ret;
}

/// Return a list of new values for the variable \p varname, respecting the \p opts.
/// The arguments are given as the argc, argv pair.
/// This handles the simple case where there are no indexes.
static wcstring_list_t new_var_values(const wcstring &varname, const set_cmd_opts_t &opts, int argc,
                                      const wchar_t *const *argv, const environment_t &vars) {
    wcstring_list_t result;
    if (!opts.prepend && !opts.append) {
        // Not prepending or appending.
        result.assign(argv, argv + argc);
    } else {
        // Note: when prepending or appending, we always use default scoping when fetching existing
        // values. For example:
        //   set --global var 1 2
        //   set --local --append var 3 4
        // This starts with the existing global variable, appends to it, and sets it locally.
        // So do not use the given variable: we must re-fetch it.
        // TODO: this races under concurrent execution.
        if (auto existing = vars.get(varname, ENV_DEFAULT)) {
            result = existing->as_list();
        }

        if (opts.prepend) {
            result.insert(result.begin(), argv, argv + argc);
        }

        if (opts.append) {
            result.insert(result.end(), argv, argv + argc);
        }
    }
    return result;
}

/// This handles the more difficult case of setting individual slices of a var.
static wcstring_list_t new_var_values_by_index(const split_var_t &split, int argc,
                                               const wchar_t *const *argv) {
    assert(static_cast<size_t>(argc) == split.indexes.size() &&
           "Must have the same number of indexes as arguments");

    // Inherit any existing values.
    // Note unlike the append/prepend case, we start with a variable in the same scope as we are
    // setting.
    wcstring_list_t result;
    if (split.var) result = split.var->as_list();

    // For each (index, argument) pair, set the element in our \p result to the replacement string.
    // Extend the list with empty strings as needed. The indexes are 1-based.
    for (int i = 0; i < argc; i++) {
        long lidx = split.indexes.at(i);
        assert(lidx >= 1 && "idx should have been verified in range already");
        // Convert from 1-based to 0-based.
        size_t idx = static_cast<size_t>(lidx - 1);
        // Extend as needed with empty strings.
        if (idx >= result.size()) result.resize(idx + 1);
        result.at(idx) = argv[i];
    }
    return result;
}

/// Set a variable.
static int builtin_set_set(const wchar_t *cmd, set_cmd_opts_t &opts, int argc, const wchar_t **argv,
                           parser_t &parser, io_streams_t &streams) {
    if (argc == 0) {
        streams.err.append_format(BUILTIN_ERR_MIN_ARG_COUNT1, cmd, 1);
        builtin_print_error_trailer(parser, streams.err, cmd);
        return STATUS_INVALID_ARGS;
    }

    env_mode_flags_t scope = compute_scope(opts);
    const wchar_t *var_expr = argv[0];
    argv++;
    argc--;

    auto split = split_var_and_indexes(var_expr, scope, parser.vars(), streams);
    if (!split) {
        builtin_print_error_trailer(parser, streams.err, cmd);
        return STATUS_INVALID_ARGS;
    }

    // Is the variable valid?
    if (!valid_var_name(split->varname)) {
        streams.err.append_format(BUILTIN_ERR_VARNAME, cmd, split->varname.c_str());
        builtin_print_error_trailer(parser, streams.err, cmd);
        return STATUS_INVALID_ARGS;
    }

    // Setting with explicit indexes like `set foo[3] ...` has additional error handling.
    if (!split->indexes.empty()) {
        // Indexes must be > 0. (Note split_var_and_indexes negates negative values).
        for (long v : split->indexes) {
            if (v <= 0) {
                streams.err.append_format(BUILTIN_SET_ARRAY_BOUNDS_ERR, cmd);
                return STATUS_INVALID_ARGS;
            }
        }

        // Append and prepend are disallowed.
        if (opts.append || opts.prepend) {
            streams.err.append_format(
                L"%ls: Cannot use --append or --prepend when assigning to a slice", cmd);
            builtin_print_error_trailer(parser, streams.err, cmd);
            return STATUS_INVALID_ARGS;
        }

        // Argument count and index count must agree.
        if (split->indexes.size() != static_cast<size_t>(argc)) {
            streams.err.append_format(BUILTIN_SET_MISMATCHED_ARGS, cmd, split->indexes.size(),
                                      argc);
            return STATUS_INVALID_ARGS;
        }
    }

    wcstring_list_t new_values;
    if (split->indexes.empty()) {
        // Handle the simple, common, case. Set the var to the specified values.
        new_values = new_var_values(split->varname, opts, argc, argv, parser.vars());
    } else {
        // Handle the uncommon case of setting specific slices of a var.
        new_values = new_var_values_by_index(*split, argc, argv);
    }

    bool have_shadowing_global = check_global_scope_exists(cmd, opts, split->varname, streams, parser);
    // Set the value back in the variable stack and fire any events.
    std::vector<event_t> evts;
    int retval = env_set_reporting_errors(cmd, split->varname, scope, std::move(new_values),
                                          streams, parser.vars(), &evts);
    for (const auto &evt : evts) {
        event_fire(parser, evt);
    }

    if (retval != STATUS_CMD_OK) return retval;
    return have_shadowing_global;
}

/// The set builtin creates, updates, and erases (removes, deletes) variables.
maybe_t<int> builtin_set(parser_t &parser, io_streams_t &streams, const wchar_t **argv) {
    const wchar_t *cmd = argv[0];
    int argc = builtin_count_args(argv);
    set_cmd_opts_t opts;

    int optind;
    int retval = parse_cmd_opts(opts, &optind, argc, argv, parser, streams);
    if (retval != STATUS_CMD_OK) return retval;
    argv += optind;
    argc -= optind;

    if (opts.print_help) {
        builtin_print_help(parser, streams, cmd);
        return STATUS_CMD_OK;
    }

    retval = validate_cmd_opts(cmd, opts, argc, parser, streams);
    if (retval != STATUS_CMD_OK) return retval;

    if (opts.query) {
        retval = builtin_set_query(cmd, opts, argc, argv, parser, streams);
    } else if (opts.erase) {
        retval = builtin_set_erase(cmd, opts, argc, argv, parser, streams);
    } else if (opts.list) {  // explicit list the vars we know about
        retval = builtin_set_list(cmd, opts, argc, argv, parser, streams);
    } else if (opts.show) {
        retval = builtin_set_show(cmd, opts, argc, argv, parser, streams);
    } else if (argc == 0) {  // implicit list the vars we know about
        retval = builtin_set_list(cmd, opts, argc, argv, parser, streams);
    } else {
        retval = builtin_set_set(cmd, opts, argc, argv, parser, streams);
    }

    if (retval == STATUS_CMD_OK && opts.preserve_failure_exit_status) return none();
    return retval;
}
