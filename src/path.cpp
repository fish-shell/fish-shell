// Directory utilities. This library contains functions for locating configuration directories, for
// testing if a command with a given name can be found in the PATH, and various other path-related
// issues.
#include "config.h"  // IWYU pragma: keep

#include "path.h"

#include <errno.h>
#include <sys/stat.h>
#if defined(__linux__)
#include <sys/statfs.h>
#endif
#include <unistd.h>

#include <cstring>
#include <string>
#include <utility>
#include <vector>

#include "common.h"
#include "env.h"
#include "expand.h"
#include "fallback.h"  // IWYU pragma: keep
#include "flog.h"
#include "wcstringutil.h"
#include "wutil.h"  // IWYU pragma: keep

// PREFIX is defined at build time.
static const std::vector<wcstring> kDefaultPath({L"/bin", L"/usr/bin", PREFIX L"/bin"});

static get_path_result_t path_get_path_core(const wcstring &cmd,
                                            const std::vector<wcstring> &pathsv) {
    const get_path_result_t noent_res{ENOENT, wcstring{}};
    get_path_result_t result{};

    /// Test if the given path can be executed.
    /// \return 0 on success, an errno value on failure.
    auto test_path = [](const wcstring &path) -> int {
        std::string narrow = wcs2zstring(path);
        struct stat buff;
        if (access(narrow.c_str(), X_OK) != 0 || stat(narrow.c_str(), &buff) != 0) {
            return errno;
        }
        return S_ISREG(buff.st_mode) ? 0 : EACCES;
    };

    if (cmd.empty()) {
        return noent_res;
    }

    // Commands cannot contain NUL byte.
    if (cmd.find(L'\0') != wcstring::npos) {
        return noent_res;
    }

    // If the command has a slash, it must be an absolute or relative path and thus we don't bother
    // looking for a matching command.
    if (cmd.find(L'/') != wcstring::npos) {
        int merr = test_path(cmd);
        return get_path_result_t{merr, cmd};
    }

    get_path_result_t best = noent_res;
    wcstring proposed_path;
    for (const auto &next_path : pathsv) {
        if (next_path.empty()) continue;
        proposed_path = next_path;
        append_path_component(proposed_path, cmd);
        int merr = test_path(proposed_path);
        if (merr == 0) {
            // We found one.
            best = get_path_result_t{merr, std::move(proposed_path)};
            break;
        } else if (merr != ENOENT && best.err == ENOENT) {
            // Keep the first *interesting* error and path around.
            // ENOENT isn't interesting because not having a file is the normal case.
            // Ignore if the parent directory is already inaccessible.
            if (waccess(wdirname(proposed_path), X_OK) == 0) {
                best = get_path_result_t{merr, std::move(proposed_path)};
            }
        }
    }
    return best;
}

maybe_t<wcstring> path_get_path(const wcstring &cmd, const environment_t &vars) {
    auto result = path_try_get_path(cmd, vars);
    if (result.err != 0) {
        return none();
    }
    wcstring path = std::move(result.path);
    return path;
}

get_path_result_t path_try_get_path(const wcstring &cmd, const environment_t &vars) {
    auto pathvar = vars.get(L"PATH");
    return path_get_path_core(cmd, pathvar ? pathvar->as_list()->vals : kDefaultPath);
}

/// \return whether the given path is on a remote filesystem.
static dir_remoteness_t path_remoteness(const wcstring &path) {
    std::string narrow = wcs2zstring(path);
#if defined(__linux__)
    struct statfs buf {};
    if (statfs(narrow.c_str(), &buf) < 0) {
        return dir_remoteness_t::unknown;
    }
    // Linux has constants for these like NFS_SUPER_MAGIC, SMB_SUPER_MAGIC, CIFS_MAGIC_NUMBER but
    // these are in varying headers. Simply hard code them.
    // NOTE: The cast is necessary for 32-bit systems because of the 4-byte CIFS_MAGIC_NUMBER
    switch (static_cast<unsigned int>(buf.f_type)) {
        case 0x6969:       // NFS_SUPER_MAGIC
        case 0x517B:       // SMB_SUPER_MAGIC
        case 0xFE534D42U:  // SMB2_MAGIC_NUMBER - not in the manpage
        case 0xFF534D42U:  // CIFS_MAGIC_NUMBER
            return dir_remoteness_t::remote;
        default:
            // Other FSes are assumed local.
            return dir_remoteness_t::local;
    }
#elif defined(ST_LOCAL)
    // ST_LOCAL is a flag to statvfs, which is itself standardized.
    // In practice the only system to use this path is NetBSD.
    struct statvfs buf {};
    if (statvfs(narrow.c_str(), &buf) < 0) return dir_remoteness_t::unknown;
    return (buf.f_flag & ST_LOCAL) ? dir_remoteness_t::local : dir_remoteness_t::remote;
#elif defined(MNT_LOCAL)
    struct statfs buf {};
    if (statfs(narrow.c_str(), &buf) < 0) return dir_remoteness_t::unknown;
    return (buf.f_flags & MNT_LOCAL) ? dir_remoteness_t::local : dir_remoteness_t::remote;
#else
    return dir_remoteness_t::unknown;
#endif
}

std::vector<wcstring> path_apply_cdpath(const wcstring &dir, const wcstring &wd,
                                        const environment_t &env_vars) {
    std::vector<wcstring> paths;
    if (dir.at(0) == L'/') {
        // Absolute path.
        paths.push_back(dir);
    } else if (string_prefixes_string(L"./", dir) || string_prefixes_string(L"../", dir) ||
               dir == L"." || dir == L"..") {
        // Path is relative to the working directory.
        paths.push_back(path_normalize_for_cd(wd, dir));
    } else {
        // Respect CDPATH.
        std::vector<wcstring> cdpathsv;
        if (auto cdpaths = env_vars.get(L"CDPATH")) {
            cdpathsv = cdpaths->as_list()->vals;
        }
        // Always append $PWD
        cdpathsv.push_back(L".");
        for (const wcstring &path : cdpathsv) {
            wcstring abspath;
            // We want to return an absolute path (see issue 6220)
            if (path.empty() || (path.front() != L'/' && path.front() != L'~')) {
                abspath = wd;
                abspath += L'/';
            }
            abspath += path;

            expand_tilde(abspath, env_vars);
            if (abspath.empty()) continue;
            abspath = normalize_path(abspath);

            wcstring whole_path = std::move(abspath);
            append_path_component(whole_path, dir);
            paths.push_back(whole_path);
        }
    }

    return paths;
}

maybe_t<wcstring> path_get_cdpath(const wcstring &dir, const wcstring &wd,
                                  const environment_t &env_vars) {
    int err = ENOENT;
    if (dir.empty()) return none();
    assert(!wd.empty() && wd.back() == L'/');
    auto paths = path_apply_cdpath(dir, wd, env_vars);

    for (const wcstring &a_dir : paths) {
        struct stat buf;
        if (wstat(a_dir, &buf) == 0) {
            if (S_ISDIR(buf.st_mode)) {
                return a_dir;
            }
            err = ENOTDIR;
        }
    }

    errno = err;
    return none();
}

maybe_t<wcstring> path_as_implicit_cd(const wcstring &path, const wcstring &wd,
                                      const environment_t &vars) {
    wcstring exp_path = path;
    expand_tilde(exp_path, vars);
    if (string_prefixes_string(L"/", exp_path) || string_prefixes_string(L"./", exp_path) ||
        string_prefixes_string(L"../", exp_path) || string_suffixes_string(L"/", exp_path) ||
        exp_path == L"..") {
        // These paths can be implicit cd, so see if you cd to the path. Note that a single period
        // cannot (that's used for sourcing files anyways).
        return path_get_cdpath(exp_path, wd, vars);
    }
    return none();
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
                                     int saved_errno, env_stack_t &vars) {
    wcstring warning_var_name = L"_FISH_WARNED_" + which_dir;
    if (vars.getf(warning_var_name, ENV_GLOBAL | ENV_EXPORT)) {
        return;
    }
    vars.set_one(warning_var_name, ENV_GLOBAL | ENV_EXPORT, L"1");

    FLOG(error, custom_error_msg.c_str());
    if (path.empty()) {
        FLOGF(warning_path, _(L"Unable to locate the %ls directory."), which_dir.c_str());
        FLOGF(warning_path,
              _(L"Please set the %ls or HOME environment variable before starting fish."),
              xdg_var.c_str());
    } else {
        const wchar_t *env_var = using_xdg ? xdg_var.c_str() : L"HOME";
        FLOGF(warning_path, _(L"Unable to locate %ls directory derived from $%ls: '%ls'."),
              which_dir.c_str(), env_var, path.c_str());
        FLOGF(warning_path, _(L"The error was '%s'."), std::strerror(saved_errno));
        FLOGF(warning_path, _(L"Please set $%ls to a directory where you have write access."),
              env_var);
    }
    ignore_result(write(STDERR_FILENO, "\n", 1));
}

/// Make sure the specified directory exists. If needed, try to create it and any currently not
/// existing parent directories, like mkdir -p,.
///
/// \return 0 if, at the time of function return the directory exists, -1 otherwise.
static int create_directory(const wcstring &d) {
    bool ok = false;
    struct stat buf;
    int stat_res = 0;

    while ((stat_res = wstat(d, &buf)) != 0) {
        if (errno != EAGAIN) break;
    }

    if (stat_res == 0) {
        if (S_ISDIR(buf.st_mode)) ok = true;
    } else if (errno == ENOENT) {
        wcstring dir = wdirname(d);
        if (!create_directory(dir) && !wmkdir(d, 0700)) ok = true;
    }

    return ok ? 0 : -1;
}

namespace {
/// The following type wraps up a user's "base" directories, corresponding (conceptually if not
/// actually) to XDG spec.
struct base_directory_t {
    wcstring path{};  /// the path where we attempted to create the directory.
    dir_remoteness_t remoteness{dir_remoteness_t::unknown};  // whether the dir is remote
    int err{0};  /// the error code if creating the directory failed, or 0 on success.
    bool success() const { return err == 0; }
    bool used_xdg{false};  /// whether an XDG variable was used in resolving the directory.
};
}  // namespace

/// Attempt to get a base directory, creating it if necessary. If a variable named \p xdg_var is
/// set, use that directory; otherwise use the path \p non_xdg_homepath rooted in $HOME. \return the
/// result; see the base_directory_t fields.
static base_directory_t make_base_directory(const wcstring &xdg_var,
                                            const wchar_t *non_xdg_homepath) {
    // The vars we fetch must be exported. Allowing them to be universal doesn't make sense and
    // allowing that creates a lock inversion that deadlocks the shell since we're called before
    // uvars are available.
    const auto &vars = env_stack_globals();
    base_directory_t result{};
    const auto xdg_dir = vars.getf_unless_empty(xdg_var, ENV_GLOBAL | ENV_EXPORT);
    if (xdg_dir) {
        result.path = *xdg_dir->as_string() + L"/fish";
        result.used_xdg = true;
    } else {
        const auto home = vars.getf_unless_empty(L"HOME", ENV_GLOBAL | ENV_EXPORT);
        if (home) {
            result.path = *home->as_string() + non_xdg_homepath;
        }
    }

    errno = 0;
    if (result.path.empty()) {
        result.err = ENOENT;
    } else if (create_directory(result.path) < 0) {
        result.err = errno;
    } else {
        result.err = 0;
        // Need to append a trailing slash to check the contents of the directory, not its parent.
        result.remoteness = path_remoteness(result.path + L'/');
    }
    return result;
}

static const base_directory_t &get_data_directory() {
    static base_directory_t s_dir = make_base_directory(L"XDG_DATA_HOME", L"/.local/share/fish");
    return s_dir;
}

static const base_directory_t &get_config_directory() {
    static base_directory_t s_dir = make_base_directory(L"XDG_CONFIG_HOME", L"/.config/fish");
    return s_dir;
}

void path_emit_config_directory_messages(env_stack_t &vars) {
    const auto &data = get_data_directory();
    if (!data.success()) {
        maybe_issue_path_warning(L"data", _(L"can not save history"), data.used_xdg,
                                 L"XDG_DATA_HOME", data.path, data.err, vars);
    }
    if (data.remoteness == dir_remoteness_t::remote) {
        FLOG(path, "data path appears to be on a network volume");
    }

    const auto &config = get_config_directory();
    if (!config.success()) {
        maybe_issue_path_warning(L"config", _(L"can not save universal variables or functions"),
                                 config.used_xdg, L"XDG_CONFIG_HOME", config.path, config.err,
                                 vars);
    }
    if (config.remoteness == dir_remoteness_t::remote) {
        FLOG(path, "config path appears to be on a network volume");
    }
}

bool path_get_config(wcstring &path) {
    const auto &dir = get_config_directory();
    path = dir.success() ? dir.path : L"";
    return dir.success();
}

bool path_get_data(wcstring &path) {
    const auto &dir = get_data_directory();
    path = dir.success() ? dir.path : L"";
    return dir.success();
}

dir_remoteness_t path_get_data_remoteness() { return get_data_directory().remoteness; }

dir_remoteness_t path_get_config_remoteness() { return get_config_directory().remoteness; }

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

void append_path_component(wcstring &path, const wcstring &component) {
    if (path.empty() || component.empty()) {
        path.append(component);
    } else {
        size_t path_len = path.size();
        bool path_slash = path.at(path_len - 1) == L'/';
        bool comp_slash = component.at(0) == L'/';
        if (!path_slash && !comp_slash) {
            // Need a slash
            path.push_back(L'/');
        } else if (path_slash && comp_slash) {
            // Too many slashes.
            path.erase(path_len - 1, 1);
        }
        path.append(component);
    }
}
