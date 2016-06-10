// Directory utilities. This library contains functions for locating configuration directories, for
// testing if a command with a given name can be found in the PATH, and various other path-related
// issues.
#include "config.h"  // IWYU pragma: keep

#include <assert.h>
#include <errno.h>
#include <sys/stat.h>
#include <unistd.h>
#include <wchar.h>
#include <string>
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
        if (contains(PREFIX L"/bin", L"/bin", L"/usr/bin")) {
            bin_path = L"/bin" ARRAY_SEP_STR L"/usr/bin";
        } else {
            bin_path = L"/bin" ARRAY_SEP_STR L"/usr/bin" ARRAY_SEP_STR PREFIX L"/bin";
        }
    }

    wcstring nxt_path;
    wcstokenizer tokenizer(bin_path, ARRAY_SEP_STR);
    while (tokenizer.next(nxt_path)) {
        if (nxt_path.empty()) continue;
        append_path_component(nxt_path, cmd);
        if (waccess(nxt_path, X_OK) == 0) {
            struct stat buff;
            if (wstat(nxt_path, &buff) == -1) {
                if (errno != EACCES) {
                    wperror(L"stat");
                }
                continue;
            }
            if (S_ISREG(buff.st_mode)) {
                if (out_path) out_path->swap(nxt_path);
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
                    debug(1, MISSING_COMMAND_ERR_MSG, nxt_path.c_str());
                    wperror(L"access");
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
        env_var_t path = env_vars.get(L"CDPATH");
        if (path.missing_or_empty()) path = L".";  // we'll change this to the wd if we have one

        wcstring nxt_path;
        wcstokenizer tokenizer(path, ARRAY_SEP_STR);
        while (tokenizer.next(nxt_path)) {
            if (nxt_path == L"." && wd != NULL) {
                // nxt_path is just '.', and we have a working directory, so use the wd instead.
                // TODO: if nxt_path starts with ./ we need to replace the . with the wd.
                nxt_path = wd;
            }
            expand_tilde(nxt_path);

            // debug( 2, L"woot %ls\n", expanded_path.c_str() );

            if (nxt_path.empty()) continue;

            wcstring whole_path = nxt_path;
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
    bool prepend_wd;
    switch (path.at(0)) {
        case L'/':
        case HOME_DIRECTORY: {
            prepend_wd = false;
            break;
        }
        default: {
            prepend_wd = true;
            break;
        }
    }

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

static wcstring path_create_config() {
    bool done = false;
    wcstring res;

    const env_var_t xdg_dir = env_get_string(L"XDG_CONFIG_HOME");
    if (!xdg_dir.missing()) {
        res = xdg_dir + L"/fish";
        if (!create_directory(res)) {
            done = true;
        }
    } else {
        const env_var_t home = env_get_string(L"HOME");
        if (!home.missing()) {
            res = home + L"/.config/fish";
            if (!create_directory(res)) {
                done = true;
            }
        }
    }

    if (!done) {
        res.clear();

        debug(0, _(L"Unable to create a configuration directory for fish. Your personal settings "
                   L"will not be saved. Please set the $XDG_CONFIG_HOME variable to a directory "
                   L"where the current user has write access."));
    }
    return res;
}

static wcstring path_create_data() {
    bool done = false;
    wcstring res;

    const env_var_t xdg_dir = env_get_string(L"XDG_DATA_HOME");
    if (!xdg_dir.missing()) {
        res = xdg_dir + L"/fish";
        if (!create_directory(res)) {
            done = true;
        }
    } else {
        const env_var_t home = env_get_string(L"HOME");
        if (!home.missing()) {
            res = home + L"/.local/share/fish";
            if (!create_directory(res)) {
                done = true;
            }
        }
    }

    if (!done) {
        res.clear();

        debug(0, _(L"Unable to create a data directory for fish. Your history will not be saved. "
                   L"Please set the $XDG_DATA_HOME variable to a directory where the current user "
                   L"has write access."));
    }
    return res;
}

/// Cache the config path.
bool path_get_config(wcstring &path) {
    static const wcstring result = path_create_config();
    path = result;
    return !result.empty();
}

/// Cache the data path.
bool path_get_data(wcstring &path) {
    static const wcstring result = path_create_data();
    path = result;
    return !result.empty();
}

__attribute__((unused)) static void replace_all(wcstring &str, const wchar_t *needle,
                                                const wchar_t *replacement) {
    size_t needle_len = wcslen(needle);
    size_t offset = 0;
    while ((offset = str.find(needle, offset)) != wcstring::npos) {
        str.replace(offset, needle_len, replacement);
        offset += needle_len;
    }
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
    } else {
        return false;
    }
}
