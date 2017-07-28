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
    bool local = false;
    bool global = false;
    bool exportv = false;
    bool erase = false;
    bool list = false;
    bool unexport = false;
    bool universal = false;
    bool query = false;
    bool shorten_ok = true;
    bool preserve_failure_exit_status = true;
};

// Variables used for parsing the argument list. This command is atypical in using the "+"
// (REQUIRE_ORDER) option for flag parsing. This is not typical of most fish commands. It means
// we stop scanning for flags when the first non-flag argument is seen.
static const wchar_t *short_options = L"+:LUeghlnqux";
static const struct woption long_options[] = {{L"export", no_argument, NULL, 'x'},
                                              {L"global", no_argument, NULL, 'g'},
                                              {L"local", no_argument, NULL, 'l'},
                                              {L"erase", no_argument, NULL, 'e'},
                                              {L"names", no_argument, NULL, 'n'},
                                              {L"unexport", no_argument, NULL, 'u'},
                                              {L"universal", no_argument, NULL, 'U'},
                                              {L"long", no_argument, NULL, 'L'},
                                              {L"query", no_argument, NULL, 'q'},
                                              {L"help", no_argument, NULL, 'h'},
                                              {NULL, 0, NULL, 0}};

// Error message for invalid path operations.
#define BUILTIN_SET_PATH_ERROR _(L"%ls: Warning: $%ls entry \"%ls\" is not valid (%s)\n")
// Hint for invalid path operation with a colon.
#define BUILTIN_SET_PATH_HINT _(L"%ls: Did you mean 'set %ls $%ls %ls'?\n")
#define BUILTIN_SET_MISMATCHED_ARGS _(L"%ls: You provided %d indexes but %d values\n")
#define BUILTIN_SET_ERASE_NO_VAR _(L"%ls: Erase needs a variable name\n")
#define BUILTIN_SET_ARRAY_BOUNDS_ERR _(L"%ls: Array index out of bounds\n")

// Test if the specified variable should be subject to path validation.
static const wcstring_list_t path_variables({L"PATH", L"CDPATH"});
static int is_path_variable(const wchar_t *env) { return contains(path_variables, env); }

static int parse_cmd_opts(set_cmd_opts_t &opts, int *optind,  //!OCLINT(high ncss method)
                          int argc, wchar_t **argv, parser_t &parser, io_streams_t &streams) {
    wchar_t *cmd = argv[0];

    int opt;
    wgetopter_t w;
    while ((opt = w.wgetopt_long(argc, argv, short_options, long_options, NULL)) != -1) {
        switch (opt) {
            case 'e': {
                opts.erase = true;
                opts.preserve_failure_exit_status = false;
                break;
            }
            case 'n': {
                opts.list = true;
                opts.preserve_failure_exit_status = false;
                break;
            }
            case 'x': {
                opts.exportv = true;
                break;
            }
            case 'l': {
                opts.local = true;
                break;
            }
            case 'g': {
                opts.global = true;
                break;
            }
            case 'u': {
                opts.unexport = true;
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
            case 'q': {
                opts.query = true;
                opts.preserve_failure_exit_status = false;
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

static int validate_cmd_opts(const wchar_t *cmd, set_cmd_opts_t &opts, int argc,
                             parser_t &parser, io_streams_t &streams) {
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
        env_var_t global_dest = env_get_string(dest, ENV_GLOBAL);
        if (!global_dest.missing()) {
            streams.err.append_format(
                _(L"%ls: Warning: universal scope selected, but a global variable '%ls' exists.\n"),
                cmd, dest);
        }
    }

    return STATUS_CMD_OK;
}

// Fix for https://github.com/fish-shell/fish-shell/issues/199 . Return success if any path setting
// succeeds.
static int my_env_path_setup(const wchar_t *cmd, const wchar_t *key,  //!OCLINT(npath complexity)
                             const wcstring_list_t &list, io_streams_t &streams) {
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
    const env_var_t existing_variable = env_get_string(key, ENV_DEFAULT);
    if (!existing_variable.missing_or_empty())
        tokenize_variable_array(existing_variable, existing_values);

    for (size_t i = 0; i < list.size(); i++) {
        const wcstring &dir = list.at(i);
        if (!string_prefixes_string(L"/", dir) || contains(existing_values, dir)) {
            any_success = true;
            continue;
        }

        bool show_hint = false;
        bool error = false;
        struct stat buff;
        if (wstat(dir, &buff) == -1) {
            error = true;
        } else if (!S_ISDIR(buff.st_mode)) {
            error = true;
            errno = ENOTDIR;
        } else if (waccess(dir, X_OK) == -1) {
            error = true;
        }

        if (!error) {
            any_success = true;
        } else {
            streams.err.append_format(BUILTIN_SET_PATH_ERROR, cmd, key, dir.c_str(),
                                      strerror(errno));
            const wchar_t *colon = wcschr(dir.c_str(), L':');
            if (colon && *(colon + 1)) show_hint = true;
        }

        if (show_hint) {
            streams.err.append_format(BUILTIN_SET_PATH_HINT, cmd, key, key,
                                      wcschr(dir.c_str(), L':') + 1);
        }
    }

    // Fail at setting the path if we tried to set it to something non-empty, but it wound up
    // empty.
    if (!list.empty() && !any_success) return STATUS_CMD_ERROR;
    return STATUS_CMD_OK;
}

/// Call env_set. If this is a path variable, e.g. PATH, validate the elements. On error, print a
/// description of the problem to stderr.
static int my_env_set(const wchar_t *cmd, const wchar_t *key, const wcstring_list_t &list,
                      int scope, io_streams_t &streams) {
    int retval;

    if (is_path_variable(key)) {
        retval = my_env_path_setup(cmd, key, list, streams);
        if (retval != STATUS_CMD_OK) return retval;
    }

    // We don't check `val->empty()` because an array var with a single empty string will be
    // "empty". A truly empty array var is set to the special value `ENV_NULL`.
    auto val = list_to_array_val(list);
    retval = env_set(key, *val == ENV_NULL ? NULL : val->c_str(), scope | ENV_USER);
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
                _(L"%ls: Tried to set the special variable '%ls' with the wrong scope\n"), cmd,
                key);
            retval = STATUS_CMD_ERROR;
            break;
        }
        case ENV_INVALID: {
            streams.err.append_format(
                _(L"%ls: Tried to set the special variable '%ls' to an invalid value\n"), cmd, key);
            retval = STATUS_CMD_ERROR;
            break;
        }
        default: {
            DIE("unexpected env_set() ret val");
            break;
        }
    }

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

    env_var_t var_str = env_get_string(src, scope);
    wcstring_list_t var;
    if (!var_str.missing()) tokenize_variable_array(var_str, var);

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
    std::set<long>::const_reverse_iterator iter;
    for (iter = indexes_set.rbegin(); iter != indexes_set.rend(); ++iter) {
        long val = *iter;
        if (val > 0 && (size_t)val <= list.size()) {
            // One-based indexing!
            list.erase(list.begin() + val - 1);
        }
    }
}

static int compute_scope(set_cmd_opts_t &opts) {
    int scope = ENV_USER;
    if (opts.local) scope |= ENV_LOCAL;
    if (opts.global) scope |= ENV_GLOBAL;
    if (opts.exportv) scope |= ENV_EXPORT;
    if (opts.unexport) scope |= ENV_UNEXPORT;
    if (opts.universal) scope |= ENV_UNIVERSAL;
    return scope;
}

/// Print the names of all environment variables in the scope. It will include the values unless the
/// `set --list` flag was used.
static int builtin_set_list(const wchar_t *cmd, set_cmd_opts_t &opts, int argc,
                            wchar_t **argv, parser_t &parser, io_streams_t &streams) {
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
            env_var_t value = env_get_string(key, compute_scope(opts));
            if (!value.missing()) {
                bool shorten = false;
                if (opts.shorten_ok && value.length() > 64) {
                    shorten = true;
                    value.resize(60);
                }

                wcstring e_value = expand_escape_variable(value);
                streams.out.append(L" ");
                streams.out.append(e_value);

                if (shorten) streams.out.push_back(ellipsis_char);
            }
        }

        streams.out.append(L"\n");
    }

    return STATUS_CMD_OK;
}

// Query mode. Return the number of variables that do not exist out of the specified variables.
static int builtin_set_query(const wchar_t *cmd, set_cmd_opts_t &opts, int argc,
                             wchar_t **argv, parser_t &parser, io_streams_t &streams) {
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
            env_var_t dest_str = env_get_string(dest, scope);
            if (!dest_str.missing()) tokenize_variable_array(dest_str, result);

            for (auto idx : indexes) {
                if (idx < 1 || (size_t)idx > result.size()) retval++;
            }
        } else if (!env_exist(arg, scope)) {
            retval++;
        }

        free(dest);
    }

    return retval;
}

/// Erase a variable.
static int builtin_set_erase(const wchar_t *cmd, set_cmd_opts_t &opts, int argc,
                             wchar_t **argv, parser_t &parser, io_streams_t &streams) {
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
    } else {  // remove just the specified indexes of the var
        const env_var_t dest_var = env_get_string(dest, scope);
        if (dest_var.missing()) return STATUS_CMD_ERROR;
        wcstring_list_t result;
        tokenize_variable_array(dest_var, result);
        erase_values(result, indexes);
        retval = my_env_set(cmd, dest, result, scope, streams);
    }

    if (retval != STATUS_CMD_OK) return retval;
    return check_global_scope_exists(cmd, opts, dest, streams);
}

/// This handles the more difficult case of setting individual slices of a var.
static int set_slices(const wchar_t *cmd, set_cmd_opts_t &opts, const wchar_t *dest,
                      std::vector<long> &indexes, int argc, wchar_t **argv, parser_t &parser,
                      io_streams_t &streams) {
    UNUSED(parser);

    if (indexes.size() != static_cast<size_t>(argc)) {
        streams.err.append_format(BUILTIN_SET_MISMATCHED_ARGS, cmd, indexes.size(), argc);
    }

    int scope = compute_scope(opts);  // calculate the variable scope based on the provided options
    wcstring_list_t result;
    const env_var_t dest_str = env_get_string(dest, scope);
    if (!dest_str.missing()) tokenize_variable_array(dest_str, result);

    // Slice indexes have been calculated, do the actual work.
    wcstring_list_t new_values;
    for (int i = 0; i < argc; i++) new_values.push_back(argv[i]);

    int retval = update_values(result, indexes, new_values);
    if (retval != STATUS_CMD_OK) {
        streams.err.append_format(BUILTIN_SET_ARRAY_BOUNDS_ERR, cmd);
        return retval;
    }

    retval = my_env_set(cmd, dest, result, scope, streams);
    if (retval != STATUS_CMD_OK) return retval;
    return check_global_scope_exists(cmd, opts, dest, streams);
}

/// Set a variable.
static int builtin_set_set(const wchar_t *cmd, set_cmd_opts_t &opts, int argc,
                           wchar_t **argv, parser_t &parser, io_streams_t &streams) {
    if (argc == 0) {
        streams.err.append_format(BUILTIN_ERR_MIN_ARG_COUNT1, cmd, 1);
        builtin_print_help(parser, streams, cmd, streams.err);
        return STATUS_INVALID_ARGS;
    }

    int retval;
    int scope = compute_scope(opts);  // calculate the variable scope based on the provided options
    wchar_t *dest = argv[0];
    argv++;
    argc--;

    std::vector<long> indexes;
    int idx_count = parse_index(indexes, dest, scope, streams);
    if (idx_count == -1) {
        builtin_print_help(parser, streams, cmd, streams.err);
        return STATUS_INVALID_ARGS;
    }

    if (!valid_var_name(dest)) {
        streams.err.append_format(BUILTIN_ERR_VARNAME, cmd, dest);
        builtin_print_help(parser, streams, cmd, streams.err);
        return STATUS_INVALID_ARGS;
    }

    if (idx_count != 0) {
        // Handle the uncommon case of setting specific slices of a var.
        return set_slices(cmd, opts, dest, indexes, argc, argv, parser, streams);
    }

    // This is the simple, common, case. Set the var to the specified values.
    wcstring_list_t values;
    for (int i = 0; i < argc; i++) values.push_back(argv[i]);
    retval = my_env_set(cmd, dest, values, scope, streams);
    if (retval != STATUS_CMD_OK) return retval;
    return check_global_scope_exists(cmd, opts, dest, streams);
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
    } else if (argc == 0) {  // implicit list the vars we know about
        retval = builtin_set_list(cmd, opts, argc, argv, parser, streams);
    } else {
        retval = builtin_set_set(cmd, opts, argc, argv, parser, streams);
    }

    if (retval == STATUS_CMD_OK && opts.preserve_failure_exit_status) retval = incoming_exit_status;
    return retval;
}
