// Directory utilities. This library contains functions for locating configuration directories, for
// testing if a command with a given name can be found in the PATH, and various other path-related
// issues.
#include "config.h"  // IWYU pragma: keep

#include <errno.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <wchar.h>

#include <memory>
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

// Note that PREFIX is defined in the `Makefile` and is thus defined when this module is compiled.
// This ensures we always default to "/bin", "/usr/bin" and the bin dir defined for the fish
// programs. Possibly with a duplicate dir if PREFIX is empty, "/", "/usr" or "/usr/". If the PREFIX
// duplicates /bin or /usr/bin that is harmless other than a trivial amount of time testing a path
// we've already tested.
const wcstring_list_t dflt_pathsv({L"/bin", L"/usr/bin", PREFIX L"/bin"});

static bool path_get_path_core(const wcstring &cmd, wcstring *out_path,
                               const maybe_t<env_var_t> &bin_path_var) {
    debug(3, L"path_get_path( '%ls' )", cmd.c_str());

    // If the command has a slash, it must be an absolute or relative path and thus we don't bother
    // looking for a matching command.
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

    const wcstring_list_t *pathsv;
    if (bin_path_var) {
        pathsv = &bin_path_var->as_list();
    } else {
        pathsv = &dflt_pathsv;
    }

    int err = ENOENT;
    for (auto next_path : *pathsv) {
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
                case EACCES:
                case ENAMETOOLONG:
                case ENOENT:
                case ENOTDIR: {
                    break;
                }
#ifdef __sun
                //Solaris 5.11 can return any of the following three if the path
                //does not exist. Yes, even 0. No, none of this is documented.
                case 0:
                case EAGAIN:
                case EEXIST: {
                    break;
                }
#endif
                //WSL has a bug where access(2) can return EINVAL
                //See https://github.com/Microsoft/BashOnWindows/issues/2522
                //The only other way EINVAL can happen is if the wrong
                //mode was specified, but we have X_OK hard-coded above.
                case EINVAL: {
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
    return path_get_path_core(cmd, out_path, env_get(L"PATH"));
}

wcstring_list_t path_get_paths(const wcstring &cmd) {
    debug(3, L"path_get_paths('%ls')", cmd.c_str());
    wcstring_list_t paths;

    // If the command has a slash, it must be an absolute or relative path and thus we don't bother
    // looking for matching commands in the PATH var.
    if (cmd.find(L'/') != wcstring::npos) {
        struct stat buff;
        if (wstat(cmd, &buff)) return paths;
        if (!S_ISREG(buff.st_mode)) return paths;
        if (waccess(cmd, X_OK)) return paths;
        paths.push_back(cmd);
        return paths;
    }

    auto path_var = env_get(L"PATH");
    std::vector<wcstring> pathsv;
    if (path_var) path_var->to_list(pathsv);
    for (auto path : pathsv) {
        if (path.empty()) continue;
        append_path_component(path, cmd);
        if (waccess(path, X_OK) == 0) {
            struct stat buff;
            if (wstat(path, &buff) == -1) {
                if (errno != EACCES) wperror(L"stat");
                continue;
            }
            if (S_ISREG(buff.st_mode)) paths.push_back(path);
        }
    }

    return paths;
}

bool path_get_cdpath(const wcstring &dir, wcstring *out, const wcstring &wd,
                     const env_vars_snapshot_t &env_vars) {
    int err = ENOENT;
    if (dir.empty()) return false;

    assert(wd.empty() || wd.back() == L'/');

    wcstring_list_t paths;
    if (dir.at(0) == L'/') {
        // Absolute path.
        paths.push_back(dir);
    } else if (string_prefixes_string(L"./", dir) || string_prefixes_string(L"../", dir) ||
               dir == L"." || dir == L"..") {
        // Path is relative to the working directory.
        wcstring path;
        if (!wd.empty()) path.append(wd);
        path.append(dir);
        paths.push_back(path);
    } else {
        // Respect CDPATH.
        wcstring_list_t cdpathsv;
        if (auto cdpaths = env_vars.get(L"CDPATH")) {
            cdpathsv = cdpaths->as_list();
        }
        if (cdpathsv.empty()) {
            cdpathsv.push_back(L".");
        }
        for (wcstring next_path : cdpathsv) {
            if (next_path.empty()) next_path = L".";
            if (next_path == L"." && !wd.empty()) {
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
    for (const wcstring &dir : paths) {
        struct stat buf;
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

bool path_can_be_implicit_cd(const wcstring &path, wcstring *out_path, const wcstring &wd,
                             const env_vars_snapshot_t &vars) {
    wcstring exp_path = path;
    expand_tilde(exp_path);

    bool result = false;
    if (string_prefixes_string(L"/", exp_path) || string_prefixes_string(L"./", exp_path) ||
        string_prefixes_string(L"../", exp_path) || string_suffixes_string(L"/", exp_path) ||
        exp_path == L"..") {
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
    auto var = env_get(warning_var_name, ENV_GLOBAL | ENV_EXPORT);
    if (!var) return;
    env_set_one(warning_var_name, ENV_GLOBAL | ENV_EXPORT, L"1");

    debug(0, custom_error_msg.c_str());
    if (path.empty()) {
        debug(0, _(L"Unable to locate the %ls directory."), which_dir.c_str());
        debug(0, _(L"Please set the %ls or HOME environment variable before starting fish."),
              xdg_var.c_str());
    } else {
        const wchar_t *env_var = using_xdg ? xdg_var.c_str() : L"HOME";
        debug(0, _(L"Unable to locate %ls directory derived from $%ls: '%ls'."), which_dir.c_str(),
              env_var, path.c_str());
        debug(0, _(L"The error was '%s'."), strerror(saved_errno));
        debug(0, _(L"Please set $%ls to a directory where you have write access."), env_var);
    }
    ignore_result(write(STDERR_FILENO, "\n", 1));
}

static void path_create(wcstring &path, const wcstring &xdg_var, const wcstring &which_dir,
                        const wcstring &custom_error_msg) {
    bool path_done = false;
    bool using_xdg = false;
    int saved_errno = 0;

    // The vars we fetch must be exported. Allowing them to be universal doesn't make sense and
    // allowing that creates a lock inversion that deadlocks the shell since we're called before
    // uvars are available.
    const auto xdg_dir = env_get(xdg_var, ENV_GLOBAL | ENV_EXPORT);
    if (!xdg_dir.missing_or_empty()) {
        using_xdg = true;
        path = xdg_dir->as_string() + L"/fish";
        if (create_directory(path) != -1) {
            path_done = true;
        } else {
            saved_errno = errno;
        }
    } else {
        const auto home = env_get(L"HOME", ENV_GLOBAL | ENV_EXPORT);
        if (!home.missing_or_empty()) {
            path = home->as_string() +
                   (which_dir == L"config" ? L"/.config/fish" : L"/.local/share/fish");
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
