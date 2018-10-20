// Functions used for implementing the set builtin.
#include "config.h"  // IWYU pragma: keep

#include <errno.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <wchar.h>

#include <algorithm>
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
#include "io.h"
#include "proc.h"
#include "wgetopt.h"
#include "wutil.h"  // IWYU pragma: keep

class parser_t;

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
    {L"export", no_argument, NULL, 'x'},    {L"global", no_argument, NULL, 'g'},
    {L"local", no_argument, NULL, 'l'},     {L"erase", no_argument, NULL, 'e'},
    {L"names", no_argument, NULL, 'n'},     {L"unexport", no_argument, NULL, 'u'},
    {L"universal", no_argument, NULL, 'U'}, {L"long", no_argument, NULL, 'L'},
    {L"query", no_argument, NULL, 'q'},     {L"show", no_argument, NULL, 'S'},
    {L"append", no_argument, NULL, 'a'},    {L"prepend", no_argument, NULL, 'p'},
    {L"path", no_argument, NULL, opt_path}, {L"unpath", no_argument, NULL, opt_unpath},
    {L"help", no_argument, NULL, 'h'},      {NULL, 0, NULL, 0}};

// Hint for invalid path operation with a colon.
#define BUILTIN_SET_PATH_ERROR _(L"%ls: Warning: $%ls entry \"%ls\" is not valid (%s)\n")
#define BUILTIN_SET_PATH_HINT _(L"%ls: Did you mean 'set %ls $%ls %ls'?\n")
#define BUILTIN_SET_MISMATCHED_ARGS _(L"%ls: You provided %d indexes but %d values\n")
#define BUILTIN_SET_ERASE_NO_VAR _(L"%ls: Erase needs a variable name\n")
#define BUILTIN_SET_ARRAY_BOUNDS_ERR _(L"%ls: Array index out of bounds\n")
#define BUILTIN_SET_UVAR_ERR \
    _(L"%ls: Universal variable '%ls' is shadowed by the global variable of the same name.\n")

// Test if the specified variable should be subject to path validation.
static const wchar_t *const path_variables[] = {L"PATH", L"CDPATH"};
static int is_path_variable(const wchar_t *env) { return contains(path_variables, env); }

static int parse_cmd_opts(set_cmd_opts_t &opts, int *optind,  //!OCLINT(high ncss method)
                          int argc, wchar_t **argv, parser_t &parser, io_streams_t &streams) {
    wchar_t *cmd = argv[0];

    int opt;
    wgetopter_t w;
    while ((opt = w.wgetopt_long(argc, argv, short_options, long_options, NULL)) != -1) {
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
                break;
            }
        }
    }

    *optind = w.woptind;
    return STATUS_CMD_OK;
}

static int validate_cmd_opts(const wchar_t *cmd, set_cmd_opts_t &opts,  //!OCLINT(npath complexity)
                             int argc, parser_t &parser, io_streams_t &streams) {
    // Can't query and erase or list.
    if (opts.query && (opts.erase || opts.list)) {
        streams.err.append_format(BUILTIN_ERR_COMBO, cmd);
        builtin_print_help(parser, streams, cmd, streams.err);
        return STATUS_INVALID_ARGS;
    }

    // We can't both list and erase variables.
    if (opts.erase && opts.list) {
        streams.err.append_format(BUILTIN_ERR_COMBO, cmd);
        builtin_print_help(parser, streams, cmd, streams.err);
        return STATUS_INVALID_ARGS;
    }

    // Variables can only have one scope.
    if (opts.local + opts.global + opts.universal > 1) {
        streams.err.append_format(BUILTIN_ERR_GLOCAL, cmd);
        builtin_print_help(parser, streams, cmd, streams.err);
        return STATUS_INVALID_ARGS;
    }

    // Variables can only have one export status.
    if (opts.exportv && opts.unexport) {
        streams.err.append_format(BUILTIN_ERR_EXPUNEXP, cmd);
        builtin_print_help(parser, streams, cmd, streams.err);
        return STATUS_INVALID_ARGS;
    }

    // Variables can only have one path status.
    if (opts.pathvar && opts.unpathvar) {
        streams.err.append_format(BUILTIN_ERR_EXPUNEXP, cmd);
        builtin_print_help(parser, streams, cmd, streams.err);
        return STATUS_INVALID_ARGS;
    }

    // Trying to erase and (un)export at the same time doesn't make sense.
    if (opts.erase && (opts.exportv || opts.unexport)) {
        streams.err.append_format(BUILTIN_ERR_COMBO, cmd);
        builtin_print_help(parser, streams, cmd, streams.err);
        return STATUS_INVALID_ARGS;
    }

    // The --show flag cannot be combined with any other flag.
    if (opts.show &&
        (opts.local || opts.global || opts.erase || opts.list || opts.exportv || opts.universal)) {
        streams.err.append_format(BUILTIN_ERR_COMBO, cmd);
        builtin_print_help(parser, streams, cmd, streams.err);
        return STATUS_INVALID_ARGS;
    }

    if (argc == 0 && opts.erase) {
        streams.err.append_format(BUILTIN_SET_ERASE_NO_VAR, cmd);
        builtin_print_help(parser, streams, cmd, streams.err);
        return STATUS_INVALID_ARGS;
    }

    return STATUS_CMD_OK;
}

// Check if we are setting a uvar and a global of the same name exists. See
// https://github.com/fish-shell/fish-shell/issues/806
static int check_global_scope_exists(const wchar_t *cmd, set_cmd_opts_t &opts, const wchar_t *dest,
                                     io_streams_t &streams) {
    if (opts.universal) {
        auto global_dest = env_get(dest, ENV_GLOBAL);
        if (global_dest && shell_is_interactive()) {
            streams.err.append_format(BUILTIN_SET_UVAR_ERR, cmd, dest);
        }
    }

    return STATUS_CMD_OK;
}

// Validate the given path `list`. If there are any entries referring to invalid directories which
// contain a colon, then complain. Return true if any path element was valid, false if not.
static bool validate_path_warning_on_colons(const wchar_t *cmd,
                                            const wchar_t *key,  //!OCLINT(npath complexity)
                                            const wcstring_list_t &list, io_streams_t &streams) {
    // Always allow setting an empty value.
    if (list.empty()) return true;

    bool any_success = false;

    // Don't bother validating (or complaining about) values that are already present. When
    // determining already-present values, use ENV_DEFAULT instead of the passed-in scope because
    // in:
    //
    //   set -l PATH stuff $PATH
    //
    // where we are temporarily shadowing a variable, we want to compare against the shadowed value,
    // not the (missing) local value. Also don't bother to complain about relative paths, which
    // don't start with /.
    wcstring_list_t existing_values;
    const auto existing_variable = env_get(key, ENV_DEFAULT);
    if (!existing_variable.missing_or_empty()) existing_variable->to_list(existing_values);

    for (const wcstring &dir : list) {
        if (!string_prefixes_string(L"/", dir) || contains(existing_values, dir)) {
            any_success = true;
            continue;
        }

        const wchar_t *colon = wcschr(dir.c_str(), L':');
        bool looks_like_colon_sep = colon && colon[1];
        if (!looks_like_colon_sep && any_success) {
            // Once we have one valid entry, skip the remaining ones unless we might warn.
            continue;
        }
        struct stat buff;
        bool valid = true;
        if (wstat(dir, &buff) == -1) {
            valid = false;
        } else if (!S_ISDIR(buff.st_mode)) {
            errno = ENOTDIR;
            valid = false;
        } else if (waccess(dir, X_OK) == -1) {
            valid = false;
        }
        if (valid) {
            any_success = true;
        } else if (looks_like_colon_sep) {
            streams.err.append_format(BUILTIN_SET_PATH_ERROR, cmd, key, dir.c_str(),
                                      strerror(errno));
            streams.err.append_format(BUILTIN_SET_PATH_HINT, cmd, key, key,
                                      wcschr(dir.c_str(), L':') + 1);
        }
    }
    return any_success;
}

static void handle_env_return(int retval, const wchar_t *cmd, const wchar_t *key, io_streams_t &streams)
{
    switch (retval) {
        case ENV_OK: {
            retval = STATUS_CMD_OK;
            break;
        }
        case ENV_PERM: {
            streams.err.append_format(_(L"%ls: Tried to change the read-only variable '%ls'\n"),
                                      cmd, key);
            retval = STATUS_CMD_ERROR;
            break;
        }
        case ENV_SCOPE: {
            streams.err.append_format(
                _(L"%ls: Tried to modify the special variable '%ls' with the wrong scope\n"), cmd,
                key);
            retval = STATUS_CMD_ERROR;
            break;
        }
        case ENV_INVALID: {
            streams.err.append_format(
                _(L"%ls: Tried to modify the special variable '%ls' to an invalid value\n"), cmd, key);
            retval = STATUS_CMD_ERROR;
            break;
        }
        case ENV_NOT_FOUND: {
            streams.err.append_format(
                _(L"%ls: The variable '%ls' does not exist\n"), cmd, key);
            retval = STATUS_CMD_ERROR;
            break;
        }
        default: {
            DIE("unexpected env_set() ret val");
            break;
        }
    }
}

/// Call env_set. If this is a path variable, e.g. PATH, validate the elements. On error, print a
/// description of the problem to stderr.
static int env_set_reporting_errors(const wchar_t *cmd, const wchar_t *key, int scope,
                                    wcstring_list_t &list, io_streams_t &streams) {
    if (is_path_variable(key) && !validate_path_warning_on_colons(cmd, key, list, streams)) {
        return STATUS_CMD_ERROR;
    }

    int retval = env_set(key, scope | ENV_USER, list);
    handle_env_return(retval, cmd, key, streams);

    return retval;
}

/// Extract indexes from an argument of the form `var_name[index1 index2...]`.
///
/// Inputs:
///   indexes: the list to insert the new indexes into
///   src: The source string to parse. This must be a var spec of the form "var[...]"
///
/// Returns:
///   The total number of indexes parsed, or -1 on error. If any indexes were found the `src` string
///   is modified to omit the index expression leaving just the var name.
static int parse_index(std::vector<long> &indexes, wchar_t *src, int scope, io_streams_t &streams) {
    wchar_t *p = wcschr(src, L'[');
    if (!p) return 0;  // no slices so nothing for us to do
    *p = L'\0';        // split the var name from the indexes/slices
    p++;

    auto var_str = env_get(src, scope);
    wcstring_list_t var;
    if (var_str) var_str->to_list(var);

    int count = 0;

    while (*p != L']') {
        const wchar_t *end;
        long l_ind = fish_wcstol(p, &end);
        if (errno > 0) {  // ignore errno == -1 meaning the int did not end with a '\0'
            streams.err.append_format(_(L"%ls: Invalid index starting at '%ls'\n"), L"set", src);
            return -1;
        }
        p = (wchar_t *)end;

        // Convert negative index to a positive index.
        if (l_ind < 0) l_ind = var.size() + l_ind + 1;

        if (*p == L'.' && *(p + 1) == L'.') {
            p += 2;
            long l_ind2 = fish_wcstol(p, &end);
            if (errno > 0) {  // ignore errno == -1 meaning the int did not end with a '\0'
                return -1;
            }
            p = (wchar_t *)end;

            // Convert negative index to a positive index.
            if (l_ind2 < 0) l_ind2 = var.size() + l_ind2 + 1;

            int direction = l_ind2 < l_ind ? -1 : 1;
            for (long jjj = l_ind; jjj * direction <= l_ind2 * direction; jjj += direction) {
                indexes.push_back(jjj);
                count++;
            }
        } else {
            indexes.push_back(l_ind);
            count++;
        }
    }

    return count;
}

static int update_values(wcstring_list_t &list, std::vector<long> &indexes,
                         wcstring_list_t &values) {
    // Replace values where needed.
    for (size_t i = 0; i < indexes.size(); i++) {
        // The '- 1' below is because the indices in fish are one-based, but the vector uses
        // zero-based indices.
        long ind = indexes[i] - 1;
        const wcstring newv = values[i];
        if (ind < 0) {
            return 1;
        }
        if ((size_t)ind >= list.size()) {
            list.resize(ind + 1);
        }

        list[ind] = newv;
    }

    return 0;
}

/// Erase from a list of wcstring values at specified indexes.
static void erase_values(wcstring_list_t &list, const std::vector<long> &indexes) {
    // Make a set of indexes.
    // This both sorts them into ascending order and removes duplicates.
    const std::set<long> indexes_set(indexes.begin(), indexes.end());

    // Now walk the set backwards, so we encounter larger indexes first, and remove elements at the
    // given (1-based) indexes.
    decltype(indexes_set)::const_reverse_iterator iter;
    for (iter = indexes_set.rbegin(); iter != indexes_set.rend(); ++iter) {
        long val = *iter;
        if (val > 0 && (size_t)val <= list.size()) {
            // One-based indexing!
            list.erase(list.begin() + val - 1);
        }
    }
}

static env_mode_flags_t compute_scope(set_cmd_opts_t &opts) {
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
/// `set --list` flag was used.
static int builtin_set_list(const wchar_t *cmd, set_cmd_opts_t &opts, int argc, wchar_t **argv,
                            parser_t &parser, io_streams_t &streams) {
    UNUSED(cmd);
    UNUSED(argc);
    UNUSED(argv);
    UNUSED(parser);

    bool names_only = opts.list;
    wcstring_list_t names = env_get_names(compute_scope(opts));
    sort(names.begin(), names.end());

    for (size_t i = 0; i < names.size(); i++) {
        const wcstring key = names.at(i);
        const wcstring e_key = escape_string(key, 0);
        streams.out.append(e_key);

        if (!names_only) {
            auto var = env_get(key, compute_scope(opts));
            if (!var.missing_or_empty()) {
                bool shorten = false;
                wcstring val = expand_escape_variable(*var);
                if (opts.shorten_ok && val.length() > 64) {
                    shorten = true;
                    val.resize(60);
                }
                streams.out.append(L" ");
                streams.out.append(val);

                if (shorten) streams.out.push_back(ellipsis_char);
            }
        }

        streams.out.append(L"\n");
    }

    return STATUS_CMD_OK;
}

// Query mode. Return the number of variables that do not exist out of the specified variables.
static int builtin_set_query(const wchar_t *cmd, set_cmd_opts_t &opts, int argc, wchar_t **argv,
                             parser_t &parser, io_streams_t &streams) {
    int retval = 0;
    int scope = compute_scope(opts);

    for (int i = 0; i < argc; i++) {
        wchar_t *arg = argv[i];
        wchar_t *dest = wcsdup(arg);
        assert(dest);

        std::vector<long> indexes;
        int idx_count = parse_index(indexes, dest, scope, streams);
        if (idx_count == -1) {
            free(dest);
            builtin_print_help(parser, streams, cmd, streams.err);
            return STATUS_CMD_ERROR;
        }

        if (idx_count) {
            wcstring_list_t result;
            auto dest_str = env_get(dest, scope);
            if (dest_str) dest_str->to_list(result);

            for (auto idx : indexes) {
                if (idx < 1 || (size_t)idx > result.size()) retval++;
            }
        } else {
            if (! env_get(arg, scope)) retval++;
        }

        free(dest);
    }

    return retval;
}

static void show_scope(const wchar_t *var_name, int scope, io_streams_t &streams) {
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
            break;
        }
    }

    const auto var = env_get(var_name, scope);
    if (!var) {
        streams.out.append_format(_(L"$%ls: not set in %ls scope\n"), var_name, scope_name);
        return;
    }

    const wchar_t *exportv = var->exports() ? _(L"exported") : _(L"unexported");
    wcstring_list_t vals = var->as_list();
    streams.out.append_format(_(L"$%ls: set in %ls scope, %ls, with %d elements\n"), var_name,
                              scope_name, exportv, vals.size());
    for (size_t i = 0; i < vals.size(); i++) {
        if (vals.size() > 100) {
            if (i == 50) streams.out.append(L"...\n");
            if (i >= 50 && i < vals.size() - 50) continue;
        }
        const wcstring value = vals[i];
        const wcstring escaped_val =
            escape_string(value, ESCAPE_NO_QUOTED, STRING_STYLE_SCRIPT);
        streams.out.append_format(_(L"$%ls[%d]: length=%d value=|%ls|\n"), var_name, i + 1,
                                  value.size(), escaped_val.c_str());
    }
}

/// Show mode. Show information about the named variable(s).
static int builtin_set_show(const wchar_t *cmd, set_cmd_opts_t &opts, int argc, wchar_t **argv,
                            parser_t &parser, io_streams_t &streams) {
    UNUSED(opts);

    if (argc == 0) {  // show all vars
        wcstring_list_t names = env_get_names(ENV_USER);
        sort(names.begin(), names.end());
        for (auto it : names) {
            show_scope(it.c_str(), ENV_LOCAL, streams);
            show_scope(it.c_str(), ENV_GLOBAL, streams);
            show_scope(it.c_str(), ENV_UNIVERSAL, streams);
            streams.out.push_back(L'\n');
        }
    } else {
        for (int i = 0; i < argc; i++) {
            wchar_t *arg = argv[i];

            if (!valid_var_name(arg)) {
                streams.err.append_format(_(L"$%ls: invalid var name\n"), arg);
                continue;
            }

            if (wcschr(arg, L'[')) {
                streams.err.append_format(
                    _(L"%ls: `set --show` does not allow slices with the var names\n"), cmd);
                builtin_print_help(parser, streams, cmd, streams.err);
                return STATUS_CMD_ERROR;
            }

            show_scope(arg, ENV_LOCAL, streams);
            show_scope(arg, ENV_GLOBAL, streams);
            show_scope(arg, ENV_UNIVERSAL, streams);
            streams.out.push_back(L'\n');
        }
    }

    return STATUS_CMD_OK;
}

/// Erase a variable.
static int builtin_set_erase(const wchar_t *cmd, set_cmd_opts_t &opts, int argc, wchar_t **argv,
                             parser_t &parser, io_streams_t &streams) {
    if (argc != 1) {
        streams.err.append_format(BUILTIN_ERR_ARG_COUNT2, cmd, L"--erase", 1, argc);
        builtin_print_help(parser, streams, cmd, streams.err);
        return STATUS_CMD_ERROR;
    }

    int scope = compute_scope(opts);  // calculate the variable scope based on the provided options
    wchar_t *dest = argv[0];

    std::vector<long> indexes;
    int idx_count = parse_index(indexes, dest, scope, streams);
    if (idx_count == -1) {
        builtin_print_help(parser, streams, cmd, streams.err);
        return STATUS_CMD_ERROR;
    }

    int retval;
    if (!valid_var_name(dest)) {
        streams.err.append_format(BUILTIN_ERR_VARNAME, cmd, dest);
        builtin_print_help(parser, streams, cmd, streams.err);
        return STATUS_INVALID_ARGS;
    }

    if (idx_count == 0) {  // unset the var
        retval = env_remove(dest, scope);
        // Temporarily swallowing ENV_NOT_FOUND errors to prevent
        // breaking all tests that unset variables that aren't set.
        if (retval != ENV_NOT_FOUND) {
            handle_env_return(retval, cmd, dest, streams);
        }
    } else {  // remove just the specified indexes of the var
        const auto dest_var = env_get(dest, scope);
        if (!dest_var) return STATUS_CMD_ERROR;
        wcstring_list_t result;
        dest_var->to_list(result);
        erase_values(result, indexes);
        retval = env_set_reporting_errors(cmd, dest, scope, result, streams);
    }

    if (retval != STATUS_CMD_OK) return retval;
    return check_global_scope_exists(cmd, opts, dest, streams);
}

/// This handles the common case of setting the entire var to a set of values.
static int set_var_array(const wchar_t *cmd, set_cmd_opts_t &opts, const wchar_t *varname,
                         wcstring_list_t &new_values, int argc, wchar_t **argv, parser_t &parser,
                         io_streams_t &streams) {
    UNUSED(cmd);
    UNUSED(parser);
    UNUSED(streams);

    if (opts.prepend || opts.append) {
        if (opts.prepend) {
            for (int i = 0; i < argc; i++) new_values.push_back(argv[i]);
        }

        auto var_str = env_get(varname, ENV_DEFAULT);
        wcstring_list_t var_array;
        if (var_str) var_str->to_list(var_array);
        new_values.insert(new_values.end(), var_array.begin(), var_array.end());

        if (opts.append) {
            for (int i = 0; i < argc; i++) new_values.push_back(argv[i]);
        }
    } else {
        for (int i = 0; i < argc; i++) new_values.push_back(argv[i]);
    }

    return STATUS_CMD_OK;
}

/// This handles the more difficult case of setting individual slices of a var.
static int set_var_slices(const wchar_t *cmd, set_cmd_opts_t &opts, const wchar_t *varname,
                          wcstring_list_t &new_values, std::vector<long> &indexes, int argc,
                          wchar_t **argv, parser_t &parser, io_streams_t &streams) {
    UNUSED(parser);

    if (opts.append || opts.prepend) {
        streams.err.append_format(
            L"%ls: Cannot use --append or --prepend when assigning to a slice", cmd);
        builtin_print_help(parser, streams, cmd, streams.err);
        return STATUS_INVALID_ARGS;
    }

    if (indexes.size() != static_cast<size_t>(argc)) {
        streams.err.append_format(BUILTIN_SET_MISMATCHED_ARGS, cmd, indexes.size(), argc);
        return STATUS_INVALID_ARGS;
    }

    int scope = compute_scope(opts);  // calculate the variable scope based on the provided options
    const auto var_str = env_get(varname, scope);
    if (var_str) var_str->to_list(new_values);

    // Slice indexes have been calculated, do the actual work.
    wcstring_list_t result;
    for (int i = 0; i < argc; i++) result.push_back(argv[i]);

    int retval = update_values(new_values, indexes, result);
    if (retval != STATUS_CMD_OK) {
        streams.err.append_format(BUILTIN_SET_ARRAY_BOUNDS_ERR, cmd);
        return retval;
    }

    return STATUS_CMD_OK;
}

/// Set a variable.
static int builtin_set_set(const wchar_t *cmd, set_cmd_opts_t &opts, int argc, wchar_t **argv,
                           parser_t &parser, io_streams_t &streams) {
    if (argc == 0) {
        streams.err.append_format(BUILTIN_ERR_MIN_ARG_COUNT1, cmd, 1);
        builtin_print_help(parser, streams, cmd, streams.err);
        return STATUS_INVALID_ARGS;
    }

    int scope = compute_scope(opts);  // calculate the variable scope based on the provided options
    wchar_t *varname = argv[0];
    argv++;
    argc--;

    std::vector<long> indexes;
    int idx_count = parse_index(indexes, varname, scope, streams);
    if (idx_count == -1) {
        builtin_print_help(parser, streams, cmd, streams.err);
        return STATUS_INVALID_ARGS;
    }

    if (!valid_var_name(varname)) {
        streams.err.append_format(BUILTIN_ERR_VARNAME, cmd, varname);
        builtin_print_help(parser, streams, cmd, streams.err);
        return STATUS_INVALID_ARGS;
    }

    int retval;
    wcstring_list_t new_values;
    if (idx_count == 0) {
        // Handle the simple, common, case. Set the var to the specified values.
        retval = set_var_array(cmd, opts, varname, new_values, argc, argv, parser, streams);
    } else {
        // Handle the uncommon case of setting specific slices of a var.
        retval =
            set_var_slices(cmd, opts, varname, new_values, indexes, argc, argv, parser, streams);
    }
    if (retval != STATUS_CMD_OK) return retval;

    retval = env_set_reporting_errors(cmd, varname, scope, new_values, streams);
    if (retval != STATUS_CMD_OK) return retval;
    return check_global_scope_exists(cmd, opts, varname, streams);
}

/// The set builtin creates, updates, and erases (removes, deletes) variables.
int builtin_set(parser_t &parser, io_streams_t &streams, wchar_t **argv) {
    const int incoming_exit_status = proc_get_last_status();
    wchar_t *cmd = argv[0];
    int argc = builtin_count_args(argv);
    set_cmd_opts_t opts;

    int optind;
    int retval = parse_cmd_opts(opts, &optind, argc, argv, parser, streams);
    if (retval != STATUS_CMD_OK) return retval;
    argv += optind;
    argc -= optind;

    if (opts.print_help) {
        builtin_print_help(parser, streams, cmd, streams.out);
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

    if (retval == STATUS_CMD_OK && opts.preserve_failure_exit_status) retval = incoming_exit_status;
    return retval;
}
