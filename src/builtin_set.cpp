// Functions used for implementing the set builtin.
#include "config.h"  // IWYU pragma: keep

#include <errno.h>
#include <stdbool.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <wchar.h>
#include <wctype.h>
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

// Error message for invalid path operations.
#define BUILTIN_SET_PATH_ERROR L"%ls: Warning: path component %ls may not be valid in %ls.\n"

// Hint for invalid path operation with a colon.
#define BUILTIN_SET_PATH_HINT L"%ls: Did you mean 'set %ls $%ls %ls'?\n"

// Error for mismatch between index count and elements.
#define BUILTIN_SET_ARG_COUNT \
    L"%ls: The number of variable indexes does not match the number of values\n"

// Test if the specified variable should be subject to path validation.
static int is_path_variable(const wchar_t *env) { return contains(env, L"PATH", L"CDPATH"); }

/// Call env_set. If this is a path variable, e.g. PATH, validate the elements. On error, print a
/// description of the problem to stderr.
static int my_env_set(const wchar_t *key, const wcstring_list_t &val, int scope,
                      io_streams_t &streams) {
    size_t i;
    int retcode = 0;
    const wchar_t *val_str = NULL;

    if (is_path_variable(key)) {
        // Fix for https://github.com/fish-shell/fish-shell/issues/199 . Return success if any path
        // setting succeeds.
        bool any_success = false;

        // Don't bother validating (or complaining about) values that are already present. When
        // determining already-present values, use ENV_DEFAULT instead of the passed-in scope
        // because in:
        //
        //   set -l PATH stuff $PATH
        //
        // where we are temporarily shadowing a variable, we want to compare against the shadowed
        // value, not the (missing) local value. Also don't bother to complain about relative paths,
        // which don't start with /.
        wcstring_list_t existing_values;
        const env_var_t existing_variable = env_get_string(key, ENV_DEFAULT);
        if (!existing_variable.missing_or_empty())
            tokenize_variable_array(existing_variable, existing_values);

        for (i = 0; i < val.size(); i++) {
            const wcstring &dir = val.at(i);
            if (!string_prefixes_string(L"/", dir) || list_contains_string(existing_values, dir)) {
                any_success = true;
                continue;
            }

            bool show_perror = false;
            int show_hint = 0;
            bool error = false;

            struct stat buff;
            if (wstat(dir, &buff)) {
                error = true;
                show_perror = true;
            }

            if (!(S_ISDIR(buff.st_mode))) {
                error = true;
            }

            if (!error) {
                any_success = true;
            } else {
                streams.err.append_format(_(BUILTIN_SET_PATH_ERROR), L"set", dir.c_str(), key);
                const wchar_t *colon = wcschr(dir.c_str(), L':');

                if (colon && *(colon + 1)) {
                    show_hint = 1;
                }
            }

            if (show_perror) {
                builtin_wperror(L"set", streams);
            }

            if (show_hint) {
                streams.err.append_format(_(BUILTIN_SET_PATH_HINT), L"set", key, key,
                                          wcschr(dir.c_str(), L':') + 1);
            }
        }

        // Fail at setting the path if we tried to set it to something non-empty, but it wound up
        // empty.
        if (!val.empty() && !any_success) {
            return 1;
        }
    }

    wcstring sb;
    if (!val.empty()) {
        for (i = 0; i < val.size(); i++) {
            sb.append(val[i]);
            if (i < val.size() - 1) {
                sb.append(ARRAY_SEP_STR);
            }
        }
        val_str = sb.c_str();
    }

    switch (env_set(key, val_str, scope | ENV_USER)) {
        case ENV_PERM: {
            streams.err.append_format(_(L"%ls: Tried to change the read-only variable '%ls'\n"),
                                      L"set", key);
            retcode = 1;
            break;
        }
        case ENV_SCOPE: {
            streams.err.append_format(
                _(L"%ls: Tried to set the special variable '%ls' with the wrong scope\n"), L"set",
                key);
            retcode = 1;
            break;
        }
        case ENV_INVALID: {
            streams.err.append_format(
                _(L"%ls: Tried to set the special variable '%ls' to an invalid value\n"), L"set",
                key);
            retcode = 1;
            break;
        }
    }

    return retcode;
}

/// Extract indexes from a destination argument of the form name[index1 index2...]
///
/// \param indexes the list to insert the new indexes into
/// \param src the source string to parse
/// \param name the name of the element. Return null if the name in \c src does not match this name
/// \param var_count the number of elements in the array to parse.
///
/// \return the total number of indexes parsed, or -1 on error
static int parse_index(std::vector<long> &indexes, const wchar_t *src, const wchar_t *name,
                       size_t var_count, io_streams_t &streams) {
    size_t len;

    int count = 0;
    const wchar_t *src_orig = src;

    if (src == 0) {
        return 0;
    }

    while (*src != L'\0' && (iswalnum(*src) || *src == L'_')) {
        src++;
    }

    if (*src != L'[') {
        streams.err.append_format(_(BUILTIN_SET_ARG_COUNT), L"set");
        return 0;
    }

    len = src - src_orig;

    if ((wcsncmp(src_orig, name, len) != 0) || (wcslen(name) != (len))) {
        streams.err.append_format(
            _(L"%ls: Multiple variable names specified in single call (%ls and %.*ls)\n"), L"set",
            name, len, src_orig);
        return 0;
    }

    src++;

    while (iswspace(*src)) {
        src++;
    }

    while (*src != L']') {
        wchar_t *end;

        long l_ind;

        errno = 0;

        l_ind = wcstol(src, &end, 10);

        if (end == src || errno) {
            streams.err.append_format(_(L"%ls: Invalid index starting at '%ls'\n"), L"set", src);
            return 0;
        }

        if (l_ind < 0) {
            l_ind = var_count + l_ind + 1;
        }

        src = end;
        if (*src == L'.' && *(src + 1) == L'.') {
            src += 2;
            long l_ind2 = wcstol(src, &end, 10);
            if (end == src || errno) {
                return 1;
            }
            src = end;

            if (l_ind2 < 0) {
                l_ind2 = var_count + l_ind2 + 1;
            }
            int direction = l_ind2 < l_ind ? -1 : 1;
            for (long jjj = l_ind; jjj * direction <= l_ind2 * direction; jjj += direction) {
                // debug(0, L"Expand range [set]: %i\n", jjj);
                indexes.push_back(jjj);
                count++;
            }
        } else {
            indexes.push_back(l_ind);
            count++;
        }
        while (iswspace(*src)) src++;
    }

    return count;
}

static int update_values(wcstring_list_t &list, std::vector<long> &indexes,
                         wcstring_list_t &values) {
    size_t i;

    // Replace values where needed.
    for (i = 0; i < indexes.size(); i++) {
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

        // free((void *) al_get(list, ind));
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

/// Print the names of all environment variables in the scope, with or without shortening, with or
/// without values, with or without escaping
static void print_variables(int include_values, int esc, bool shorten_ok, int scope,
                            io_streams_t &streams) {
    wcstring_list_t names = env_get_names(scope);
    sort(names.begin(), names.end());

    for (size_t i = 0; i < names.size(); i++) {
        const wcstring key = names.at(i);
        const wcstring e_key = escape_string(key, 0);

        streams.out.append(e_key);

        if (include_values) {
            env_var_t value = env_get_string(key, scope);
            if (!value.missing()) {
                int shorten = 0;

                if (shorten_ok && value.length() > 64) {
                    shorten = 1;
                    value.resize(60);
                }

                wcstring e_value = esc ? expand_escape_variable(value) : value;

                streams.out.append(L" ");
                streams.out.append(e_value);

                if (shorten) {
                    streams.out.push_back(ellipsis_char);
                }
            }
        }

        streams.out.append(L"\n");
    }
}

/// The set builtin. Creates, updates and erases environment variables and environemnt variable
/// arrays.
int builtin_set(parser_t &parser, io_streams_t &streams, wchar_t **argv) {
    wgetopter_t w;
    // Variables used for parsing the argument list.
    const struct woption long_options[] = {{L"export", no_argument, 0, 'x'},
                                           {L"global", no_argument, 0, 'g'},
                                           {L"local", no_argument, 0, 'l'},
                                           {L"erase", no_argument, 0, 'e'},
                                           {L"names", no_argument, 0, 'n'},
                                           {L"unexport", no_argument, 0, 'u'},
                                           {L"universal", no_argument, 0, 'U'},
                                           {L"long", no_argument, 0, 'L'},
                                           {L"query", no_argument, 0, 'q'},
                                           {L"help", no_argument, 0, 'h'},
                                           {0, 0, 0, 0}};

    const wchar_t *short_options = L"+xglenuULqh";

    int argc = builtin_count_args(argv);

    // Flags to set the work mode.
    int local = 0, global = 0, exportv = 0;
    int erase = 0, list = 0, unexport = 0;
    int universal = 0, query = 0;
    bool shorten_ok = true;
    bool preserve_incoming_failure_exit_status = true;
    const int incoming_exit_status = proc_get_last_status();

    // Variables used for performing the actual work.
    wchar_t *dest = 0;
    int retcode = 0;
    int scope;
    int slice = 0;

    const wchar_t *bad_char = NULL;

    // Parse options to obtain the requested operation and the modifiers.
    w.woptind = 0;
    while (1) {
        int c = w.wgetopt_long(argc, argv, short_options, long_options, 0);

        if (c == -1) {
            break;
        }

        switch (c) {
            case 0: {
                break;
            }
            case 'e': {
                erase = 1;
                preserve_incoming_failure_exit_status = false;
                break;
            }
            case 'n': {
                list = 1;
                preserve_incoming_failure_exit_status = false;
                break;
            }
            case 'x': {
                exportv = 1;
                break;
            }
            case 'l': {
                local = 1;
                break;
            }
            case 'g': {
                global = 1;
                break;
            }
            case 'u': {
                unexport = 1;
                break;
            }
            case 'U': {
                universal = 1;
                break;
            }
            case 'L': {
                shorten_ok = false;
                break;
            }
            case 'q': {
                query = 1;
                preserve_incoming_failure_exit_status = false;
                break;
            }
            case 'h': {
                builtin_print_help(parser, streams, argv[0], streams.out);
                return 0;
            }
            case '?': {
                builtin_unknown_option(parser, streams, argv[0], argv[w.woptind - 1]);
                return 1;
            }
            default: { break; }
        }
    }

    // Ok, all arguments have been parsed, let's validate them. If we are checking the existance of
    // a variable (-q) we can not also specify scope.
    if (query && (erase || list)) {
        streams.err.append_format(BUILTIN_ERR_COMBO, argv[0]);

        builtin_print_help(parser, streams, argv[0], streams.err);
        return 1;
    }

    // We can't both list and erase variables.
    if (erase && list) {
        streams.err.append_format(BUILTIN_ERR_COMBO, argv[0]);

        builtin_print_help(parser, streams, argv[0], streams.err);
        return 1;
    }

    // Variables can only have one scope.
    if (local + global + universal > 1) {
        streams.err.append_format(BUILTIN_ERR_GLOCAL, argv[0]);
        builtin_print_help(parser, streams, argv[0], streams.err);
        return 1;
    }

    // Variables can only have one export status.
    if (exportv && unexport) {
        streams.err.append_format(BUILTIN_ERR_EXPUNEXP, argv[0]);
        builtin_print_help(parser, streams, argv[0], streams.err);
        return 1;
    }

    // Calculate the scope value for variable assignement.
    scope = (local ? ENV_LOCAL : 0) | (global ? ENV_GLOBAL : 0) | (exportv ? ENV_EXPORT : 0) |
            (unexport ? ENV_UNEXPORT : 0) | (universal ? ENV_UNIVERSAL : 0) | ENV_USER;

    if (query) {
        // Query mode. Return the number of variables that do not exist out of the specified
        // variables.
        int i;
        for (i = w.woptind; i < argc; i++) {
            wchar_t *arg = argv[i];
            int slice = 0;

            if (!(dest = wcsdup(arg))) {
                DIE_MEM();
            }

            if (wcschr(dest, L'[')) {
                slice = 1;
                *wcschr(dest, L'[') = 0;
            }

            if (slice) {
                std::vector<long> indexes;
                wcstring_list_t result;
                size_t j;

                env_var_t dest_str = env_get_string(dest, scope);
                if (!dest_str.missing()) tokenize_variable_array(dest_str, result);

                if (!parse_index(indexes, arg, dest, result.size(), streams)) {
                    builtin_print_help(parser, streams, argv[0], streams.err);
                    retcode = 1;
                    break;
                }
                for (j = 0; j < indexes.size(); j++) {
                    long idx = indexes[j];
                    if (idx < 1 || (size_t)idx > result.size()) {
                        retcode++;
                    }
                }
            } else {
                if (!env_exist(arg, scope)) {
                    retcode++;
                }
            }

            free(dest);
        }
        return retcode;
    }

    if (list) {
        // Maybe we should issue an error if there are any other arguments?
        print_variables(0, 0, shorten_ok, scope, streams);
        return 0;
    }

    if (w.woptind == argc) {
        // Print values of variables.
        if (erase) {
            streams.err.append_format(_(L"%ls: Erase needs a variable name\n"), argv[0]);

            builtin_print_help(parser, streams, argv[0], streams.err);
            retcode = 1;
        } else {
            print_variables(1, 1, shorten_ok, scope, streams);
        }

        return retcode;
    }

    if (!(dest = wcsdup(argv[w.woptind]))) {
        DIE_MEM();
    }

    if (wcschr(dest, L'[')) {
        slice = 1;
        *wcschr(dest, L'[') = 0;
    }

    if (!wcslen(dest)) {
        free(dest);
        streams.err.append_format(BUILTIN_ERR_VARNAME_ZERO, argv[0]);
        builtin_print_help(parser, streams, argv[0], streams.err);
        return 1;
    }

    if ((bad_char = wcsvarname(dest))) {
        streams.err.append_format(BUILTIN_ERR_VARCHAR, argv[0], *bad_char);
        builtin_print_help(parser, streams, argv[0], streams.err);
        free(dest);
        return 1;
    }

    // Set assignment can work in two modes, either using slices or using the whole array. We detect
    // which mode is used here.
    if (slice) {
        // Slice mode.
        std::vector<long> indexes;
        wcstring_list_t result;

        const env_var_t dest_str = env_get_string(dest, scope);
        if (!dest_str.missing()) {
            tokenize_variable_array(dest_str, result);
        } else if (erase) {
            retcode = 1;
        }

        if (!retcode) {
            for (; w.woptind < argc; w.woptind++) {
                if (!parse_index(indexes, argv[w.woptind], dest, result.size(), streams)) {
                    builtin_print_help(parser, streams, argv[0], streams.err);
                    retcode = 1;
                    break;
                }

                size_t idx_count = indexes.size();
                size_t val_count = argc - w.woptind - 1;

                if (!erase) {
                    if (val_count < idx_count) {
                        streams.err.append_format(_(BUILTIN_SET_ARG_COUNT), argv[0]);
                        builtin_print_help(parser, streams, argv[0], streams.err);
                        retcode = 1;
                        break;
                    }
                    if (val_count == idx_count) {
                        w.woptind++;
                        break;
                    }
                }
            }
        }

        if (!retcode) {
            // Slice indexes have been calculated, do the actual work.
            if (erase) {
                erase_values(result, indexes);
                my_env_set(dest, result, scope, streams);
            } else {
                wcstring_list_t value;

                while (w.woptind < argc) {
                    value.push_back(argv[w.woptind++]);
                }

                if (update_values(result, indexes, value)) {
                    streams.err.append_format(L"%ls: ", argv[0]);
                    streams.err.append_format(ARRAY_BOUNDS_ERR);
                    streams.err.push_back(L'\n');
                }

                my_env_set(dest, result, scope, streams);
            }
        }
    } else {
        w.woptind++;
        // No slicing.
        if (erase) {
            if (w.woptind != argc) {
                streams.err.append_format(_(L"%ls: Values cannot be specfied with erase\n"),
                                          argv[0]);
                builtin_print_help(parser, streams, argv[0], streams.err);
                retcode = 1;
            } else {
                retcode = env_remove(dest, scope);
            }
        } else {
            wcstring_list_t val;
            for (int i = w.woptind; i < argc; i++) val.push_back(argv[i]);
            retcode = my_env_set(dest, val, scope, streams);
        }
    }

    // Check if we are setting variables above the effective scope. See
    // https://github.com/fish-shell/fish-shell/issues/806
    env_var_t global_dest = env_get_string(dest, ENV_GLOBAL);
    if (universal && !global_dest.missing()) {
        streams.err.append_format(
            _(L"%ls: Warning: universal scope selected, but a global variable '%ls' exists.\n"),
            L"set", dest);
    }

    free(dest);

    if (retcode == STATUS_BUILTIN_OK && preserve_incoming_failure_exit_status)
        retcode = incoming_exit_status;
    return retcode;
}
