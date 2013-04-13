#include "config.h"

#include <stdlib.h>
#include <stdio.h>
#include <wchar.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>
#include <libgen.h>

#include "fallback.h"
#include "util.h"

#include "common.h"
#include "env.h"
#include "wutil.h"
#include "path.h"
#include "expand.h"

/**
   Unexpected error in path_get_path()
*/
#define MISSING_COMMAND_ERR_MSG _( L"Error while searching for command '%ls'" )

static bool path_get_path_core(const wcstring &cmd, wcstring *out_path, const env_var_t &bin_path_var)
{
    int err = ENOENT;

    debug(3, L"path_get_path( '%ls' )", cmd.c_str());

    /* If the command has a slash, it must be a full path */
    if (cmd.find(L'/') != wcstring::npos)
    {
        if (waccess(cmd, X_OK)==0)
        {
            struct stat buff;
            if (wstat(cmd, &buff))
            {
                return false;
            }

            if (S_ISREG(buff.st_mode))
            {
                if (out_path)
                    out_path->assign(cmd);
                return true;
            }
            else
            {
                errno = EACCES;
                return false;
            }
        }
        else
        {
            struct stat buff;
            wstat(cmd, &buff);
            return false;
        }

    }
    else
    {
        wcstring bin_path;
        if (! bin_path_var.missing())
        {
            bin_path = bin_path_var;
        }
        else
        {
            if (contains(PREFIX L"/bin", L"/bin", L"/usr/bin"))
            {
                bin_path = L"/bin" ARRAY_SEP_STR L"/usr/bin";
            }
            else
            {
                bin_path = L"/bin" ARRAY_SEP_STR L"/usr/bin" ARRAY_SEP_STR PREFIX L"/bin";
            }
        }

        wcstring nxt_path;
        wcstokenizer tokenizer(bin_path, ARRAY_SEP_STR);
        while (tokenizer.next(nxt_path))
        {
            if (nxt_path.empty())
                continue;
            append_path_component(nxt_path, cmd);
            if (waccess(nxt_path, X_OK)==0)
            {
                struct stat buff;
                if (wstat(nxt_path, &buff)==-1)
                {
                    if (errno != EACCES)
                    {
                        wperror(L"stat");
                    }
                    continue;
                }
                if (S_ISREG(buff.st_mode))
                {
                    if (out_path)
                        out_path->swap(nxt_path);
                    return true;
                }
                err = EACCES;

            }
            else
            {
                switch (errno)
                {
                    case ENOENT:
                    case ENAMETOOLONG:
                    case EACCES:
                    case ENOTDIR:
                        break;
                    default:
                    {
                        debug(1,
                              MISSING_COMMAND_ERR_MSG,
                              nxt_path.c_str());
                        wperror(L"access");
                    }
                }
            }
        }
    }

    errno = err;
    return false;
}

bool path_get_path(const wcstring &cmd, wcstring *out_path, const env_vars_snapshot_t &vars)
{
    return path_get_path_core(cmd, out_path, vars.get(L"PATH"));
}

bool path_get_path(const wcstring &cmd, wcstring *out_path)
{
    return path_get_path_core(cmd, out_path, env_get_string(L"PATH"));
}

bool path_get_cdpath_string(const wcstring &dir_str, wcstring &result, const env_var_t &cdpath)
{
    wchar_t *res = 0;
    int err = ENOENT;
    bool success = false;

    const wchar_t *const dir = dir_str.c_str();
    if (dir[0] == L'/'|| (wcsncmp(dir, L"./", 2)==0))
    {
        struct stat buf;
        if (wstat(dir, &buf) == 0)
        {
            if (S_ISDIR(buf.st_mode))
            {
                result = dir_str;
                success = true;
            }
            else
            {
                err = ENOTDIR;
            }

        }
    }
    else
    {

        wcstring path = L".";

        // Respect CDPATH
        env_var_t cdpath = env_get_string(L"CDPATH");
        if (! cdpath.missing_or_empty())
        {
            path = cdpath.c_str();
        }

        wcstokenizer tokenizer(path, ARRAY_SEP_STR);
        wcstring next_path;
        while (tokenizer.next(next_path))
        {
            expand_tilde(next_path);
            if (next_path.size() == 0) continue;

            wcstring whole_path = next_path;
            append_path_component(whole_path, dir);

            struct stat buf;
            if (wstat(whole_path, &buf) == 0)
            {
                if (S_ISDIR(buf.st_mode))
                {
                    result = whole_path;
                    success = true;
                    break;
                }
                else
                {
                    err = ENOTDIR;
                }
            }
            else
            {
                if (lwstat(whole_path, &buf) == 0)
                {
                    err = EROTTEN;
                }
            }
        }
    }


    if (!success)
    {
        errno = err;
    }

    return res;
}

bool path_get_cdpath(const wcstring &dir, wcstring *out, const wchar_t *wd, const env_vars_snapshot_t &env_vars)
{
    int err = ENOENT;
    if (dir.empty())
        return false;

    if (wd)
    {
        size_t len = wcslen(wd);
        assert(wd[len - 1] == L'/');
    }

    wcstring_list_t paths;
    if (dir.at(0) == L'/')
    {
        /* Absolute path */
        paths.push_back(dir);
    }
    else if (string_prefixes_string(L"./", dir) ||
             string_prefixes_string(L"../", dir) ||
             dir == L"." || dir == L"..")
    {
        /* Path is relative to the working directory */
        wcstring path;
        if (wd)
            path.append(wd);
        path.append(dir);
        paths.push_back(path);
    }
    else
    {
        // Respect CDPATH
        env_var_t path = env_vars.get(L"CDPATH");
        if (path.missing_or_empty())
            path = L"."; //We'll change this to the wd if we have one

        wcstring nxt_path;
        wcstokenizer tokenizer(path, ARRAY_SEP_STR);
        while (tokenizer.next(nxt_path))
        {

            if (nxt_path == L"." && wd != NULL)
            {
                // nxt_path is just '.', and we have a working directory, so use the wd instead
                // TODO: if nxt_path starts with ./ we need to replace the . with the wd
                nxt_path = wd;
            }
            expand_tilde(nxt_path);

//      debug( 2, L"woot %ls\n", expanded_path.c_str() );

            if (nxt_path.empty())
                continue;

            wcstring whole_path = nxt_path;
            append_path_component(whole_path, dir);
            paths.push_back(whole_path);
        }
    }

    bool success = false;
    for (wcstring_list_t::const_iterator iter = paths.begin(); iter != paths.end(); ++iter)
    {
        struct stat buf;
        const wcstring &dir = *iter;
        if (wstat(dir, &buf) == 0)
        {
            if (S_ISDIR(buf.st_mode))
            {
                success = true;
                if (out)
                    out->assign(dir);
                break;
            }
            else
            {
                err = ENOTDIR;
            }
        }
    }

    if (! success)
        errno = err;
    return success;
}

bool path_can_be_implicit_cd(const wcstring &path, wcstring *out_path, const wchar_t *wd, const env_vars_snapshot_t &vars)
{
    wcstring exp_path = path;
    expand_tilde(exp_path);

    bool result = false;
    if (string_prefixes_string(L"/", exp_path) ||
            string_prefixes_string(L"./", exp_path) ||
            string_prefixes_string(L"../", exp_path) ||
            exp_path == L"..")
    {
        /* These paths can be implicit cd, so see if you cd to the path. Note that a single period cannot (that's used for sourcing files anyways) */
        result = path_get_cdpath(exp_path, out_path, wd, vars);
    }
    return result;
}

static wcstring path_create_config()
{
    bool done = false;
    wcstring res;

    const env_var_t xdg_dir  = env_get_string(L"XDG_CONFIG_HOME");
    if (! xdg_dir.missing())
    {
        res = xdg_dir + L"/fish";
        if (!create_directory(res))
        {
            done = true;
        }
    }
    else
    {
        const env_var_t home = env_get_string(L"HOME");
        if (! home.missing())
        {
            res = home + L"/.config/fish";
            if (!create_directory(res))
            {
                done = true;
            }
        }
    }

    if (! done)
    {
        res.clear();

        debug(0, _(L"Unable to create a configuration directory for fish. Your personal settings will not be saved. Please set the $XDG_CONFIG_HOME variable to a directory where the current user has write access."));
    }
    return res;
}

/* Cache the config path */
bool path_get_config(wcstring &path)
{
    static const wcstring result = path_create_config();
    path = result;
    return ! result.empty();
}

static void replace_all(wcstring &str, const wchar_t *needle, const wchar_t *replacement)
{
    size_t needle_len = wcslen(needle);
    size_t offset = 0;
    while ((offset = str.find(needle, offset)) != wcstring::npos)
    {
        str.replace(offset, needle_len, replacement);
        offset += needle_len;
    }
}

void path_make_canonical(wcstring &path)
{

    /* Remove double slashes */
    size_t size;
    do
    {
        size = path.size();
        replace_all(path, L"//", L"/");
    }
    while (path.size() != size);

    /* Remove trailing slashes, except don't remove the first one */
    while (size-- > 1)
    {
        if (path.at(size) != L'/')
            break;
    }
    /* Now size is either -1 (if the entire string was slashes) or is the index of the last non-slash character. Either way this will set it to the correct size. */
    path.resize(size+1);
}

bool path_is_valid(const wcstring &path, const wcstring &working_directory)
{
    bool path_is_valid;
    /* Some special paths are always valid */
    if (path.empty())
    {
        path_is_valid = false;
    }
    else if (path == L"." || path == L"./")
    {
        path_is_valid = true;
    }
    else if (path == L".." || path == L"../")
    {
        path_is_valid = (! working_directory.empty() && working_directory != L"/");
    }
    else if (path.at(0) != '/')
    {
        /* Prepend the working directory. Note that we know path is not empty here. */
        wcstring tmp = working_directory;
        tmp.append(path);
        path_is_valid = (0 == waccess(tmp, F_OK));
    }
    else
    {
        /* Simple check */
        path_is_valid = (0 == waccess(path, F_OK));
    }
    return path_is_valid;
}

bool paths_are_same_file(const wcstring &path1, const wcstring &path2)
{
    if (path1 == path2)
        return true;

    struct stat s1, s2;
    if (wstat(path1, &s1) == 0 && wstat(path2, &s2) == 0)
    {
        return s1.st_ino == s2.st_ino && s1.st_dev == s2.st_dev;
    }
    else
    {
        return false;
    }
}

wcstring get_working_directory(void)
{
    wcstring wd = L"./";
    wchar_t dir_path[4096];
    const wchar_t *cwd = wgetcwd(dir_path, 4096);
    if (cwd)
    {
        wd = cwd;
        /* Make sure the working directory ends with a slash */
        if (! wd.empty() && wd.at(wd.size() - 1) != L'/')
            wd.push_back(L'/');
    }
    return wd;
}
