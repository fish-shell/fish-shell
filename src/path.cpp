// Directory utilities. This library contains functions for locating configuration directories, for
// testing if a command with a given name can be found in the PATH, and various other path-related
// issues.
#include "config.h"  // IWYU pragma: keep

#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <wchar.h>

#include <string>
#include <type_traits>
#include <vector>

#include "common.h"
#include "env.h"
#include "expand.h"
#include "fallback.h"  // IWYU pragma: keep
#include "path.h"
#include "wutil.h"  // IWYU pragma: keep

/// Unexpected error in path_get_path().
#define MISSING_COMMAND_ERR_MSG _(L"Error while searching for command '%ls'")

static bool path_get_path_core(const wcstring &cmd, wcstring *out_path,
                               const env_var_t &bin_path_var) {
    int err = ENOENT;
    debug(3, L"path_get_path( '%ls' )", cmd.c_str());

    // If the command has a slash, it must be a full path.
    if (cmd.find(L'/') != wcstring::npos) {
        if (waccess(cmd, X_OK) != 0) {
            return false;
        }

        struct stat buff;
        if (wstat(cmd, &buff)) {
            return false;
        }
        if (S_ISREG(buff.st_mode)) {
            if (out_path) out_path->assign(cmd);
            return true;
        }
        errno = EACCES;
        return false;
    }

    wcstring bin_path;
    if (!bin_path_var.missing()) {
        bin_path = bin_path_var;
    } else {
        // Note that PREFIX is defined in the Makefile and is defined when this module is compiled.
        // This ensures we always default to "/bin", "/usr/bin" and the bin dir defined for the fish
        // programs with no duplicates.
        if (!wcscmp(PREFIX L"/bin", L"/bin") || !wcscmp(PREFIX L"/bin", L"/usr/bin")) {
            bin_path = L"/bin" ARRAY_SEP_STR L"/usr/bin";
        } else {
            bin_path = L"/bin" ARRAY_SEP_STR L"/usr/bin" ARRAY_SEP_STR PREFIX L"/bin";
        }
    }

    std::vector<wcstring> pathsv;
    tokenize_variable_array(bin_path, pathsv);
    for (auto next_path : pathsv) {
        if (next_path.empty()) continue;
        append_path_component(next_path, cmd);
        if (waccess(next_path, X_OK) == 0) {
            struct stat buff;
            if (wstat(next_path, &buff) == -1) {
                if (errno != EACCES) {
                    wperror(L"stat");
                }
                continue;
            }
            if (S_ISREG(buff.st_mode)) {
                if (out_path) *out_path = std::move(next_path);
                return true;
            }
            err = EACCES;
        } else {
            switch (errno) {
                case ENOENT:
                case ENAMETOOLONG:
                case EACCES:
                case ENOTDIR: {
                    break;
                }
                default: {
                    debug(1, MISSING_COMMAND_ERR_MSG, next_path.c_str());
                    wperror(L"access");
                    break;
                }
            }
        }
    }

    errno = err;
    return false;
}

bool path_get_path(const wcstring &cmd, wcstring *out_path, const env_vars_snapshot_t &vars) {
    return path_get_path_core(cmd, out_path, vars.get(L"PATH"));
}

bool path_get_path(const wcstring &cmd, wcstring *out_path) {
    return path_get_path_core(cmd, out_path, env_get_string(L"PATH"));
}

bool path_get_cdpath(const wcstring &dir, wcstring *out, const wchar_t *wd,
                     const env_vars_snapshot_t &env_vars) {
    int err = ENOENT;
    if (dir.empty()) return false;

    if (wd) {
        size_t len = wcslen(wd);
        assert(wd[len - 1] == L'/');
    }

    wcstring_list_t paths;
    if (dir.at(0) == L'/') {
        // Absolute path.
        paths.push_back(dir);
    } else if (string_prefixes_string(L"./", dir) || string_prefixes_string(L"../", dir) ||
               dir == L"." || dir == L"..") {
        // Path is relative to the working directory.
        wcstring path;
        if (wd) path.append(wd);
        path.append(dir);
        paths.push_back(path);
    } else {
        // Respect CDPATH.
        env_var_t cdpaths = env_vars.get(L"CDPATH");
        if (cdpaths.missing_or_empty()) cdpaths = L".";

        std::vector<wcstring> cdpathsv;
        tokenize_variable_array(cdpaths, cdpathsv);
        for (auto next_path : cdpathsv) {
            if (next_path.empty()) next_path = L".";
            if (next_path == L"." && wd != NULL) {
                // next_path is just '.', and we have a working directory, so use the wd instead.
                // TODO: if next_path starts with ./ we need to replace the . with the wd.
                next_path = wd;
            }
            expand_tilde(next_path);
            if (next_path.empty()) continue;

            wcstring whole_path = next_path;
            append_path_component(whole_path, dir);
            paths.push_back(whole_path);
        }
    }

    bool success = false;
    for (wcstring_list_t::const_iterator iter = paths.begin(); iter != paths.end(); ++iter) {
        struct stat buf;
        const wcstring &dir = *iter;
        if (wstat(dir, &buf) == 0) {
            if (S_ISDIR(buf.st_mode)) {
                success = true;
                if (out) out->assign(dir);
                break;
            } else {
                err = ENOTDIR;
            }
        }
    }

    if (!success) errno = err;
    return success;
}

bool path_can_be_implicit_cd(const wcstring &path, wcstring *out_path, const wchar_t *wd,
                             const env_vars_snapshot_t &vars) {
    wcstring exp_path = path;
    expand_tilde(exp_path);

    bool result = false;
    if (string_prefixes_string(L"/", exp_path) || string_prefixes_string(L"./", exp_path) ||
        string_prefixes_string(L"../", exp_path) || string_suffixes_string(L"/", exp_path) ||
        exp_path == L"..") {
        // These paths can be implicit cd, so see if you cd to the path. Note that a single period
        // cannot (that's used for sourcing files anyways).
        result = path_get_cdpath(exp_path, out_path, wd, vars);
    }
    return result;
}

// If the given path looks like it's relative to the working directory, then prepend that working
// directory. This operates on unescaped paths only (so a ~ means a literal ~).
wcstring path_apply_working_directory(const wcstring &path, const wcstring &working_directory) {
    if (path.empty() || working_directory.empty()) return path;

    // We're going to make sure that if we want to prepend the wd, that the string has no leading
    // "/".
    bool prepend_wd = path.at(0) != L'/' && path.at(0) != HOME_DIRECTORY;
    if (!prepend_wd) {
        // No need to prepend the wd, so just return the path we were given.
        return path;
    }

    // Remove up to one "./".
    wcstring path_component = path;
    if (string_prefixes_string(L"./", path_component)) {
        path_component.erase(0, 2);
    }

    // Removing leading /s.
    while (string_prefixes_string(L"/", path_component)) {
        path_component.erase(0, 1);
    }

    // Construct and return a new path.
    wcstring new_path = working_directory;
    append_path_component(new_path, path_component);
    return new_path;
}

/// We separate this from path_create() for two reasons. First it's only caused if there is a
/// problem, and thus is not central to the behavior of that function. Second, we only want to issue
/// the message once. If the current shell starts a new fish shell (e.g., by running `fish -c` from
/// a function) we don't want that subshell to issue the same warnings.
static void maybe_issue_path_warning(const wcstring &which_dir, const wcstring &custom_error_msg,
                                     bool using_xdg, const wcstring &xdg_var, const wcstring &path,
                                     int saved_errno) {
    wcstring warning_var_name = L"_FISH_WARNED_" + which_dir;
    if (env_exist(warning_var_name.c_str(), ENV_GLOBAL | ENV_EXPORT)) {
        return;
    }
    env_set(warning_var_name, L"1", ENV_GLOBAL | ENV_EXPORT);

    debug(0, custom_error_msg.c_str());
    if (path.empty()) {
        debug(0, _(L"Unable to locate the %ls directory."), which_dir.c_str());
        debug(0, _(L"Please set the %ls or HOME environment variable "
                   L"before starting fish."),
              xdg_var.c_str());
    } else {
        const wchar_t *env_var = using_xdg ? xdg_var.c_str() : L"HOME";
        debug(0, _(L"Unable to locate %ls directory derived from $%ls: '%ls'."), which_dir.c_str(),
              env_var, path.c_str());
        debug(0, _(L"The error was '%s'."), strerror(saved_errno));
        debug(0, _(L"Please set $%ls to a directory where you have write access."), env_var);
    }
    fputwc(L'\n', stderr);
}

static void path_create(wcstring &path, const wcstring &xdg_var, const wcstring &which_dir,
                        const wcstring &custom_error_msg) {
    bool path_done = false;
    bool using_xdg = false;
    int saved_errno = 0;

    // The vars we fetch must be exported. Allowing them to be universal doesn't make sense and
    // allowing that creates a lock inversion that deadlocks the shell since we're called before
    // uvars are available.
    const env_var_t xdg_dir = env_get_string(xdg_var, ENV_GLOBAL | ENV_EXPORT);
    if (!xdg_dir.missing_or_empty()) {
        using_xdg = true;
        path = xdg_dir + L"/fish";
        if (create_directory(path) != -1) {
            path_done = true;
        } else {
            saved_errno = errno;
        }
    } else {
        const env_var_t home = env_get_string(L"HOME", ENV_GLOBAL | ENV_EXPORT);
        if (!home.missing_or_empty()) {
            path = home + (which_dir == L"config" ? L"/.config/fish" : L"/.local/share/fish");
            if (create_directory(path) != -1) {
                path_done = true;
            } else {
                saved_errno = errno;
            }
        }
    }

    if (!path_done) {
        maybe_issue_path_warning(which_dir, custom_error_msg, using_xdg, xdg_var, path,
                                 saved_errno);
        path.clear();
    }

    return;
}

/// Cache the config path.
bool path_get_config(wcstring &path) {
    static bool config_path_done = false;
    static wcstring config_path(L"");

    if (!config_path_done) {
        path_create(config_path, L"XDG_CONFIG_HOME", L"config",
                    _(L"Your personal settings will not be saved."));
        config_path_done = true;
    }

    path = config_path;
    return !config_path.empty();
}

/// Cache the data path.
bool path_get_data(wcstring &path) {
    static bool data_path_done = false;
    static wcstring data_path(L"");

    if (!data_path_done) {
        data_path_done = true;
        path_create(data_path, L"XDG_DATA_HOME", L"data", _(L"Your history will not be saved."));
    }

    path = data_path;
    return !data_path.empty();
}

void path_make_canonical(wcstring &path) {
    // Ignore trailing slashes, unless it's the first character.
    size_t len = path.size();
    while (len > 1 && path.at(len - 1) == L'/') len--;

    // Turn runs of slashes into a single slash.
    size_t trailing = 0;
    bool prev_was_slash = false;
    for (size_t leading = 0; leading < len; leading++) {
        wchar_t c = path.at(leading);
        bool is_slash = (c == '/');
        if (!prev_was_slash || !is_slash) {
            // This is either the first slash in a run, or not a slash at all.
            path.at(trailing++) = c;
        }
        prev_was_slash = is_slash;
    }
    assert(trailing <= len);
    if (trailing < len) path.resize(trailing);
}

bool paths_are_equivalent(const wcstring &p1, const wcstring &p2) {
    if (p1 == p2) return true;

    size_t len1 = p1.size(), len2 = p2.size();

    // Ignore trailing slashes after the first character.
    while (len1 > 1 && p1.at(len1 - 1) == L'/') len1--;
    while (len2 > 1 && p2.at(len2 - 1) == L'/') len2--;

    // Start walking
    size_t idx1 = 0, idx2 = 0;
    while (idx1 < len1 && idx2 < len2) {
        wchar_t c1 = p1.at(idx1), c2 = p2.at(idx2);

        // If the characters are different, the strings are not equivalent.
        if (c1 != c2) break;

        idx1++;
        idx2++;

        // If the character was a slash, walk forwards until we hit the end of the string, or a
        // non-slash. Note the first condition is invariant within the loop.
        while (c1 == L'/' && idx1 < len1 && p1.at(idx1) == L'/') idx1++;
        while (c2 == L'/' && idx2 < len2 && p2.at(idx2) == L'/') idx2++;
    }

    // We matched if we consumed all of the characters in both strings.
    return idx1 == len1 && idx2 == len2;
}

bool path_is_valid(const wcstring &path, const wcstring &working_directory) {
    bool path_is_valid;
    // Some special paths are always valid.
    if (path.empty()) {
        path_is_valid = false;
    } else if (path == L"." || path == L"./") {
        path_is_valid = true;
    } else if (path == L".." || path == L"../") {
        path_is_valid = (!working_directory.empty() && working_directory != L"/");
    } else if (path.at(0) != '/') {
        // Prepend the working directory. Note that we know path is not empty here.
        wcstring tmp = working_directory;
        tmp.append(path);
        path_is_valid = (0 == waccess(tmp, F_OK));
    } else {
        // Simple check.
        path_is_valid = (0 == waccess(path, F_OK));
    }
    return path_is_valid;
}

bool paths_are_same_file(const wcstring &path1, const wcstring &path2) {
    if (paths_are_equivalent(path1, path2)) return true;

    struct stat s1, s2;
    if (wstat(path1, &s1) == 0 && wstat(path2, &s2) == 0) {
        return s1.st_ino == s2.st_ino && s1.st_dev == s2.st_dev;
    }

    return false;
}
