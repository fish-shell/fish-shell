/** \file wildcard.c

Fish needs it's own globbing implementation to support
tab-expansion of globbed parameters. Also provides recursive
wildcards using **.

*/

#include "config.h"
#include <algorithm>
#include <stdlib.h>
#include <stdio.h>
#include <limits.h>
#include <wchar.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <errno.h>
#include <string.h>
#include <set>


#include "fallback.h"
#include "util.h"

#include "wutil.h"
#include "complete.h"
#include "common.h"
#include "wildcard.h"
#include "complete.h"
#include "reader.h"
#include "expand.h"
#include "exec.h"
#include <map>

/**
   This flag is set in the flags parameter of wildcard_expand if the
   call is part of a recursiv wildcard search. It is used to make sure
   that the contents of subdirectories are only searched once.
*/
#define WILDCARD_RECURSIVE 64

/**
   The maximum length of a filename token. This is a fallback value,
   an attempt to find the true value using patchconf is always made.
*/
#define MAX_FILE_LENGTH 1024

/**
   The command to run to get a description from a file suffix
*/
#define SUFFIX_CMD_STR L"mimedb 2>/dev/null -fd "

/**
   Description for generic executable
*/
#define COMPLETE_EXEC_DESC _( L"Executable" )
/**
   Description for link to executable
*/
#define COMPLETE_EXEC_LINK_DESC _( L"Executable link" )

/**
   Description for regular file
*/
#define COMPLETE_FILE_DESC _( L"File" )
/**
   Description for character device
*/
#define COMPLETE_CHAR_DESC _( L"Character device" )
/**
   Description for block device
*/
#define COMPLETE_BLOCK_DESC _( L"Block device" )
/**
   Description for fifo buffer
*/
#define COMPLETE_FIFO_DESC _( L"Fifo" )
/**
   Description for symlink
*/
#define COMPLETE_SYMLINK_DESC _( L"Symbolic link" )
/**
   Description for symlink
*/
#define COMPLETE_DIRECTORY_SYMLINK_DESC _( L"Symbolic link to directory" )
/**
   Description for Rotten symlink
*/
#define COMPLETE_ROTTEN_SYMLINK_DESC _( L"Rotten symbolic link" )
/**
   Description for symlink loop
*/
#define COMPLETE_LOOP_SYMLINK_DESC _( L"Symbolic link loop" )
/**
   Description for socket files
*/
#define COMPLETE_SOCKET_DESC _( L"Socket" )
/**
   Description for directories
*/
#define COMPLETE_DIRECTORY_DESC _( L"Directory" )

/** Hashtable containing all descriptions that describe an executable */
static std::map<wcstring, wcstring> suffix_map;

// Implementation of wildcard_has. Needs to take the length to handle embedded nulls (#1631)
static bool wildcard_has_impl(const wchar_t *str, size_t len, bool internal)
{
    assert(str != NULL);
    const wchar_t *end = str + len;
    if (internal)
    {
        for (; str < end; str++)
        {
            if ((*str == ANY_CHAR) || (*str == ANY_STRING) || (*str == ANY_STRING_RECURSIVE))
                return true;
        }
    }
    else
    {
        wchar_t prev=0;
        for (; str < end; str++)
        {
            if (((*str == L'*') || (*str == L'?')) && (prev != L'\\'))
                return true;
            prev = *str;
        }
    }

    return false;
}

bool wildcard_has(const wchar_t *str, bool internal)
{
    assert(str != NULL);
    return wildcard_has_impl(str, wcslen(str), internal);
}

bool wildcard_has(const wcstring &str, bool internal)
{
    return wildcard_has_impl(str.data(), str.size(), internal);
}


/**
   Check whether the string str matches the wildcard string wc.

   \param str String to be matched.
   \param wc The wildcard.
   \param is_first Whether files beginning with dots should not be matched against wildcards.
*/
static bool wildcard_match_internal(const wchar_t *str, const wchar_t *wc, bool leading_dots_fail_to_match, bool is_first)
{
    if (*str == 0 && *wc==0)
        return true;

    /* Hackish fix for https://github.com/fish-shell/fish-shell/issues/270 . Prevent wildcards from matching . or .., but we must still allow literal matches. */
    if (leading_dots_fail_to_match && is_first && contains(str, L".", L".."))
    {
        /* The string is '.' or '..'. Return true if the wildcard exactly matches. */
        return ! wcscmp(str, wc);
    }

    if (*wc == ANY_STRING || *wc == ANY_STRING_RECURSIVE)
    {
        /* Ignore hidden file */
        if (leading_dots_fail_to_match && is_first && *str == L'.')
        {
            return false;
        }

        /* Try all submatches */
        do
        {
            if (wildcard_match_internal(str, wc+1, leading_dots_fail_to_match, false))
                return true;
        }
        while (*(str++) != 0);
        return false;
    }
    else if (*str == 0)
    {
        /*
          End of string, but not end of wildcard, and the next wildcard
          element is not a '*', so this is not a match.
        */
        return false;
    }

    if (*wc == ANY_CHAR)
    {
        if (is_first && *str == L'.')
        {
            return false;
        }

        return wildcard_match_internal(str+1, wc+1, leading_dots_fail_to_match, false);
    }

    if (*wc == *str)
        return wildcard_match_internal(str+1, wc+1, leading_dots_fail_to_match, false);

    return false;
}

/**
   Matches the string against the wildcard, and if the wildcard is a
   possible completion of the string, the remainder of the string is
   inserted into the out vector.
*/
static bool wildcard_complete_internal(const wcstring &orig,
                                       const wchar_t *str,
                                       const wchar_t *wc,
                                       bool is_first,
                                       const wchar_t *desc,
                                       wcstring(*desc_func)(const wcstring &),
                                       std::vector<completion_t> &out,
                                       expand_flags_t expand_flags,
                                       complete_flags_t flags)
{
    if (!wc || ! str || orig.empty())
    {
        debug(2, L"Got null string on line %d of file %s", __LINE__, __FILE__);
        return 0;
    }

    bool has_match = false;
    string_fuzzy_match_t fuzzy_match(fuzzy_match_exact);
    const bool at_end_of_wildcard = (*wc == L'\0');
    const wchar_t *completion_string = NULL;

    // Hack hack hack
    // Implement EXPAND_FUZZY_MATCH by short-circuiting everything if there are no remaining wildcards
    if ((expand_flags & EXPAND_FUZZY_MATCH) && ! at_end_of_wildcard && ! wildcard_has(wc, true))
    {
        string_fuzzy_match_t local_fuzzy_match = string_fuzzy_match_string(wc, str);
        if (local_fuzzy_match.type != fuzzy_match_none)
        {
            has_match = true;
            fuzzy_match = local_fuzzy_match;

            /* If we're not a prefix or exact match, then we need to replace the token. Note that in this case we're not going to call ourselves recursively, so these modified flags won't "leak" except into the completion. */
            if (match_type_requires_full_replacement(local_fuzzy_match.type))
            {
                flags |= COMPLETE_REPLACES_TOKEN;
                completion_string = orig.c_str();
            }
            else
            {
                /* Since we are not replacing the token, be careful to only store the part of the string after the wildcard */
                size_t wc_len = wcslen(wc);
                assert(wcslen(str) >= wc_len);
                completion_string = str + wcslen(wc);
            }
        }
    }

    /* Maybe we satisfied the wildcard normally */
    if (! has_match)
    {
        bool file_has_leading_dot = (is_first && str[0] == L'.');
        if (at_end_of_wildcard && ! file_has_leading_dot)
        {
            has_match = true;
            if (flags & COMPLETE_REPLACES_TOKEN)
            {
                completion_string = orig.c_str();
            }
            else
            {
                completion_string = str;
            }
        }
    }

    if (has_match)
    {
        /* Wildcard complete */
        assert(completion_string != NULL);
        wcstring out_completion = completion_string;
        wcstring out_desc = (desc ? desc : L"");

        size_t complete_sep_loc = out_completion.find(PROG_COMPLETE_SEP);
        if (complete_sep_loc != wcstring::npos)
        {
            /* This completion has an embedded description, do not use the generic description */
            out_desc.assign(out_completion, complete_sep_loc + 1, out_completion.size() - complete_sep_loc - 1);
            out_completion.resize(complete_sep_loc);
        }
        else
        {
            if (desc_func && !(expand_flags & EXPAND_NO_DESCRIPTIONS))
            {
                /*
                  A description generating function is specified, call
                  it. If it returns something, use that as the
                  description.
                */
                wcstring func_desc = desc_func(orig);
                if (! func_desc.empty())
                    out_desc = func_desc;
            }

        }

        /* Note: out_completion may be empty if the completion really is empty, e.g. tab-completing 'foo' when a file 'foo' exists. */
        append_completion(out, out_completion, out_desc, flags, fuzzy_match);
        return true;
    }

    if (*wc == ANY_STRING)
    {
        bool res=false;

        /* Ignore hidden file */
        if (is_first && str[0] == L'.')
            return false;

        /* Try all submatches */
        for (size_t i=0; str[i] != L'\0'; i++)
        {
            const size_t before_count = out.size();
            if (wildcard_complete_internal(orig, str + i, wc+1, false, desc, desc_func, out, expand_flags, flags))
            {
                res = true;

                /* #929: if the recursive call gives us a prefix match, just stop. This is sloppy - what we really want to do is say, once we've seen a match of a particular type, ignore all matches of that type further down the string, such that the wildcard produces the "minimal match." */
                bool has_prefix_match = false;
                const size_t after_count = out.size();
                for (size_t j = before_count; j < after_count; j++)
                {
                    if (out[j].match.type <= fuzzy_match_prefix)
                    {
                        has_prefix_match = true;
                        break;
                    }
                }
                if (has_prefix_match)
                    break;
            }
        }
        return res;

    }
    else if (*wc == ANY_CHAR || *wc == *str)
    {
        return wildcard_complete_internal(orig, str+1, wc+1, false, desc, desc_func, out, expand_flags, flags);
    }
    else if (towlower(*wc) == towlower(*str))
    {
        return wildcard_complete_internal(orig, str+1, wc+1, false, desc, desc_func, out, expand_flags, flags | COMPLETE_REPLACES_TOKEN);
    }
    return false;
}

bool wildcard_complete(const wcstring &str,
                       const wchar_t *wc,
                       const wchar_t *desc,
                       wcstring(*desc_func)(const wcstring &),
                       std::vector<completion_t> &out,
                       expand_flags_t expand_flags,
                       complete_flags_t flags)
{
    bool res;
    res =  wildcard_complete_internal(str, str.c_str(), wc, true, desc, desc_func, out, expand_flags, flags);
    return res;
}


bool wildcard_match(const wcstring &str, const wcstring &wc, bool leading_dots_fail_to_match)
{
    return wildcard_match_internal(str.c_str(), wc.c_str(), leading_dots_fail_to_match, true /* first */);
}

/**
   Creates a path from the specified directory and filename.
*/
static wcstring make_path(const wcstring &base_dir, const wcstring &name)
{
    return base_dir + name;
}

/**
   Return a description of a file based on its suffix. This function
   does not perform any caching, it directly calls the mimedb command
   to do a lookup.
 */
static wcstring complete_get_desc_suffix_internal(const wcstring &suff)
{

    wcstring cmd = wcstring(SUFFIX_CMD_STR) + suff;

    wcstring_list_t lst;
    wcstring desc;

    if (exec_subshell(cmd, lst, false /* do not apply exit status */) != -1)
    {
        if (! lst.empty())
        {
            const wcstring & ln = lst.at(0);
            if (ln.size() > 0 && ln != L"unknown")
            {
                desc = ln;
                /*
                  I have decided I prefer to have the description
                  begin in uppercase and the whole universe will just
                  have to accept it. Hah!
                */
                desc[0]=towupper(desc[0]);
            }
        }
    }

    if (desc.empty())
    {
        desc = COMPLETE_FILE_DESC;
    }

    suffix_map[suff] = desc.c_str();
    return desc;
}


/**
   Use the mimedb command to look up a description for a given suffix
*/
static wcstring complete_get_desc_suffix(const wchar_t *suff_orig)
{

    size_t len;
    wchar_t *suff;
    wchar_t *pos;

    len = wcslen(suff_orig);

    if (len == 0)
        return COMPLETE_FILE_DESC;

    suff = wcsdup(suff_orig);

    /*
      Drop characters that are commonly used as backup suffixes from the suffix
    */
    for (pos=suff; *pos; pos++)
    {
        if (wcschr(L"?;#~@&", *pos))
        {
            *pos=0;
            break;
        }
    }

    wcstring tmp = escape(suff, ESCAPE_ALL);
    free(suff);
    suff = wcsdup(tmp.c_str());

    std::map<wcstring, wcstring>::iterator iter = suffix_map.find(suff);
    wcstring desc;
    if (iter != suffix_map.end())
    {
        desc = iter->second;
    }
    else
    {
        desc = complete_get_desc_suffix_internal(suff);
    }

    free(suff);

    return desc;
}


/**
   Obtain a description string for the file specified by the filename.

   The returned value is a string constant and should not be free'd.

   \param filename The file for which to find a description string
   \param lstat_res The result of calling lstat on the file
   \param lbuf The struct buf output of calling lstat on the file
   \param stat_res The result of calling stat on the file
   \param buf The struct buf output of calling stat on the file
   \param err The errno value after a failed stat call on the file.
*/

static wcstring file_get_desc(const wcstring &filename,
                              int lstat_res,
                              struct stat lbuf,
                              int stat_res,
                              struct stat buf,
                              int err)
{
    const wchar_t *suffix;

    if (!lstat_res)
    {
        if (S_ISLNK(lbuf.st_mode))
        {
            if (!stat_res)
            {
                if (S_ISDIR(buf.st_mode))
                {
                    return COMPLETE_DIRECTORY_SYMLINK_DESC;
                }
                else
                {

                    if ((buf.st_mode & S_IXUSR) ||
                            (buf.st_mode & S_IXGRP) ||
                            (buf.st_mode & S_IXOTH))
                    {

                        if (waccess(filename, X_OK) == 0)
                        {
                            /*
                              Weird group permissions and other such
                              issues make it non-trivial to find out
                              if we can actually execute a file using
                              the result from stat. It is much safer
                              to use the access function, since it
                              tells us exactly what we want to know.
                            */
                            return COMPLETE_EXEC_LINK_DESC;
                        }
                    }
                }

                return COMPLETE_SYMLINK_DESC;

            }
            else
            {
                switch (err)
                {
                    case ENOENT:
                    {
                        return COMPLETE_ROTTEN_SYMLINK_DESC;
                    }

                    case ELOOP:
                    {
                        return COMPLETE_LOOP_SYMLINK_DESC;
                    }
                }
                /*
                  On unknown errors we do nothing. The file will be
                  given the default 'File' description or one based on the suffix.
                */
            }

        }
        else if (S_ISCHR(buf.st_mode))
        {
            return COMPLETE_CHAR_DESC;
        }
        else if (S_ISBLK(buf.st_mode))
        {
            return COMPLETE_BLOCK_DESC;
        }
        else if (S_ISFIFO(buf.st_mode))
        {
            return COMPLETE_FIFO_DESC;
        }
        else if (S_ISSOCK(buf.st_mode))
        {
            return COMPLETE_SOCKET_DESC;
        }
        else if (S_ISDIR(buf.st_mode))
        {
            return COMPLETE_DIRECTORY_DESC;
        }
        else
        {
            if ((buf.st_mode & S_IXUSR) ||
                    (buf.st_mode & S_IXGRP) ||
                    (buf.st_mode & S_IXOTH))
            {

                if (waccess(filename, X_OK) == 0)
                {
                    /*
                      Weird group permissions and other such issues
                      make it non-trivial to find out if we can
                      actually execute a file using the result from
                      stat. It is much safer to use the access
                      function, since it tells us exactly what we want
                      to know.
                    */
                    return COMPLETE_EXEC_DESC;
                }
            }
        }
    }

    suffix = wcsrchr(filename.c_str(), L'.');
    if (suffix != 0 && !wcsrchr(suffix, L'/'))
    {
        return complete_get_desc_suffix(suffix);
    }

    return COMPLETE_FILE_DESC ;
}


/**
   Add the specified filename if it matches the specified wildcard.

   If the filename matches, first get the description of the specified
   filename. If this is a regular file, append the filesize to the
   description.

   \param list the list to add he completion to
   \param fullname the full filename of the file
   \param completion the completion part of the file name
   \param wc the wildcard to match against
   \param is_cmd whether we are performing command completion
*/
static void wildcard_completion_allocate(std::vector<completion_t> &list,
        const wcstring &fullname,
        const wcstring &completion,
        const wchar_t *wc,
        expand_flags_t expand_flags)
{
    struct stat buf, lbuf;
    wcstring sb;
    wcstring munged_completion;

    int flags = 0;
    int stat_res, lstat_res;
    int stat_errno=0;

    long long sz;

    /*
      If the file is a symlink, we need to stat both the file itself
      _and_ the destination file. But we try to avoid this with
      non-symlinks by first doing an lstat, and if the file is not a
      link we copy the results over to the regular stat buffer.
    */
    if ((lstat_res = lwstat(fullname, &lbuf)))
    {
        /* lstat failed! */
        sz=-1;
        stat_res = lstat_res;
    }
    else
    {
        if (S_ISLNK(lbuf.st_mode))
        {

            if ((stat_res = wstat(fullname, &buf)))
            {
                sz=-1;
            }
            else
            {
                sz = (long long)buf.st_size;
            }

            /*
              In order to differentiate between e.g. rotten symlinks
              and symlink loops, we also need to know the error status of wstat.
            */
            stat_errno = errno;
        }
        else
        {
            stat_res = lstat_res;
            memcpy(&buf, &lbuf, sizeof(struct stat));
            sz = (long long)buf.st_size;
        }
    }


    bool wants_desc = !(expand_flags & EXPAND_NO_DESCRIPTIONS);
    wcstring desc;
    if (wants_desc)
        desc = file_get_desc(fullname, lstat_res, lbuf, stat_res, buf, stat_errno);

    if (sz >= 0 && S_ISDIR(buf.st_mode))
    {
        flags |= COMPLETE_NO_SPACE;
        munged_completion = completion;
        munged_completion.push_back(L'/');
        if (wants_desc)
            sb.append(desc);
    }
    else
    {
        if (wants_desc)
        {
            if (! desc.empty())
            {
                sb.append(desc);
                sb.append(L", ");
            }
            sb.append(format_size(sz));
        }
    }

    const wcstring &completion_to_use = munged_completion.empty() ? completion : munged_completion;
    wildcard_complete(completion_to_use, wc, sb.c_str(), NULL, list, expand_flags, flags);
}

/**
  Test if the file specified by the given filename matches the
  expansion flags specified. flags can be a combination of
  EXECUTABLES_ONLY and DIRECTORIES_ONLY.
*/
static bool test_flags(const wchar_t *filename, expand_flags_t flags)
{
    if (flags & DIRECTORIES_ONLY)
    {
        struct stat buf;
        if (wstat(filename, &buf) == -1)
        {
            return false;
        }

        if (!S_ISDIR(buf.st_mode))
        {
            return false;
        }
    }


    if (flags & EXECUTABLES_ONLY)
    {
        if (waccess(filename, X_OK) != 0)
            return false;
        struct stat buf;
        if (wstat(filename, &buf) == -1)
        {
            return false;
        }

        if (!S_ISREG(buf.st_mode))
        {
            return false;
        }
    }

    return true;
}

/** Appends a completion to the completion list, if the string is missing from the set. */
static void insert_completion_if_missing(const wcstring &str, std::vector<completion_t> &out, std::set<wcstring> &completion_set)
{
    if (completion_set.insert(str).second)
        append_completion(out, str);
}

/**
   The real implementation of wildcard expansion is in this
   function. Other functions are just wrappers around this one.

   This function traverses the relevant directory tree looking for
   matches, and recurses when needed to handle wildcrards spanning
   multiple components and recursive wildcards.

   Because this function calls itself recursively with substrings,
   it's important that the parameters be raw pointers instead of wcstring,
   which would be too expensive to construct for all substrings.
 */
static int wildcard_expand_internal(const wchar_t *wc,
                                    const wchar_t * const base_dir,
                                    expand_flags_t flags,
                                    std::vector<completion_t> &out,
                                    std::set<wcstring> &completion_set,
                                    std::set<file_id_t> &visited_files)
{

    /* Variables for traversing a directory */
    DIR *dir;

    /* The result returned */
    int res = 0;

    /* Variables for testing for presense of recursive wildcards */
    const wchar_t *wc_recursive;
    bool is_recursive;

    /* Slightly mangled version of base_dir */
    const wchar_t *dir_string;

    //  debug( 3, L"WILDCARD_EXPAND %ls in %ls", wc, base_dir );

    if (is_main_thread() ? reader_interrupted() : reader_thread_job_is_stale())
    {
        return -1;
    }

    if (!wc || !base_dir)
    {
        debug(2, L"Got null string on line %d of file %s", __LINE__, __FILE__);
        return 0;
    }

    const size_t base_dir_len = wcslen(base_dir);

    if (flags & ACCEPT_INCOMPLETE)
    {
        /*
           Avoid excessive number of returned matches for wc ending with a *
        */
        size_t len = wcslen(wc);
        if (len > 0 && (wc[len-1]==ANY_STRING))
        {
            wchar_t * foo = wcsdup(wc);
            foo[len-1]=0;
            int res = wildcard_expand_internal(foo, base_dir, flags, out, completion_set, visited_files);
            free(foo);
            return res;
        }
    }

    /* Initialize various variables */

    dir_string = (base_dir[0] == L'\0') ? L"." : base_dir;

    if (!(dir = wopendir(dir_string)))
    {
        return 0;
    }

    /* Points to the end of the current wildcard segment */
    const wchar_t * const wc_end = wcschr(wc,L'/');

    /*
      Test for recursive match string in current segment
    */
    wc_recursive = wcschr(wc, ANY_STRING_RECURSIVE);
    is_recursive = (wc_recursive && (!wc_end || wc_recursive < wc_end));

    /*
      Is this segment of the wildcard the last?
    */
    if (!wc_end)
    {
        /*
          Wildcard segment is the last segment,

          Insert all matching files/directories
        */
        if (wc[0]=='\0')
        {
            /*
              The last wildcard segment is empty. Insert everything if
              completing, the directory itself otherwise.
            */
            if (flags & ACCEPT_INCOMPLETE)
            {
                wcstring next;
                while (wreaddir(dir, next))
                {
                    if (next[0] != L'.')
                    {
                        wcstring long_name = make_path(base_dir, next);

                        if (test_flags(long_name.c_str(), flags))
                        {
                            wildcard_completion_allocate(out, long_name, next, L"", flags);
                        }
                    }
                }
            }
            else
            {
                res = 1;
                insert_completion_if_missing(base_dir, out, completion_set);
            }
        }
        else
        {
            /* This is the last wildcard segment, and it is not empty. Match files/directories. */
            wcstring name_str;
            while (wreaddir(dir, name_str))
            {
                if (flags & ACCEPT_INCOMPLETE)
                {

                    const wcstring long_name = make_path(base_dir, name_str);

                    /* Test for matches before stating file, so as to minimize the number of calls to the much slower stat function. The only expand flag we care about is EXPAND_FUZZY_MATCH; we have no complete flags. */
                    std::vector<completion_t> test;
                    if (wildcard_complete(name_str, wc, L"", NULL, test, flags & EXPAND_FUZZY_MATCH, 0))
                    {
                        if (test_flags(long_name.c_str(), flags))
                        {
                            wildcard_completion_allocate(out, long_name, name_str, wc, flags);

                        }
                    }
                }
                else
                {
                    if (wildcard_match(name_str, wc, true /* skip files with leading dots */))
                    {
                        const wcstring long_name = make_path(base_dir, name_str);
                        int skip = 0;

                        if (is_recursive)
                        {
                            /*
                              In recursive mode, we are only
                              interested in adding files -directories
                              will be added in the next pass.
                            */
                            struct stat buf;
                            if (!wstat(long_name, &buf))
                            {
                                skip = S_ISDIR(buf.st_mode);
                            }
                        }
                        if (! skip)
                        {
                            insert_completion_if_missing(long_name, out, completion_set);
                        }
                        res = 1;
                    }
                }
            }
        }
    }

    if (wc_end || is_recursive)
    {
        /*
          Wilcard segment is not the last segment.  Recursively call
          wildcard_expand for all matching subdirectories.
        */

        /*
          In recursive mode, we look through the directory twice. If
          so, this rewind is needed.
        */
        rewinddir(dir);

        /*
          wc_str is the part of the wildcarded string from the
          beginning to the first slash
        */
        const wcstring wc_str = wcstring(wc, wc_end ? wc_end - wc : wcslen(wc));

        /* new_dir is a scratch area containing the full path to a file/directory we are iterating over */
        wcstring new_dir = base_dir;

        wcstring name_str;
        while (wreaddir(dir, name_str))
        {
            /*
              Test if the file/directory name matches the whole
              wildcard element, i.e. regular matching.
            */
            int whole_match = wildcard_match(name_str, wc_str, true /* ignore leading dots */);
            int partial_match = 0;

            /*
               If we are doing recursive matching, also check if this
               directory matches the part up to the recusrive
               wildcard, if so, then we can search all subdirectories
               for matches.
            */
            if (is_recursive)
            {
                const wchar_t *end = wcschr(wc, ANY_STRING_RECURSIVE);
                wchar_t *wc_sub = wcsndup(wc, end-wc+1);
                partial_match = wildcard_match(name_str, wc_sub, true /* ignore leading dots */);
                free(wc_sub);
            }

            if (whole_match || partial_match)
            {
                struct stat buf;
                int new_res;

                // new_dir is base_dir + some other path components
                // Replace everything after base_dir with the new path component
                new_dir.replace(base_dir_len, wcstring::npos, name_str);

                int stat_res = wstat(new_dir, &buf);

                if (!stat_res)
                {
                    // Insert a "file ID" into visited_files
                    // If the insertion fails, we've already visited this file (i.e. a symlink loop)
                    // If we're not recursive, insert anyways (in case we loop back around in a future recursive segment), but continue on; the idea being that literal path components should still work
                    const file_id_t file_id = file_id_t::file_id_from_stat(&buf);
                    if (S_ISDIR(buf.st_mode) && (visited_files.insert(file_id).second || ! is_recursive))
                    {
                        new_dir.push_back(L'/');

                        /*
                          Regular matching
                        */
                        if (whole_match)
                        {
                            const wchar_t *new_wc = L"";
                            if (wc_end)
                            {
                                new_wc=wc_end+1;
                                /*
                                  Accept multiple '/' as a single direcotry separator
                                */
                                while (*new_wc==L'/')
                                {
                                    new_wc++;
                                }
                            }

                            new_res = wildcard_expand_internal(new_wc,
                                                               new_dir.c_str(),
                                                               flags,
                                                               out,
                                                               completion_set,
                                                               visited_files);

                            if (new_res == -1)
                            {
                                res = -1;
                                break;
                            }
                            res |= new_res;

                        }

                        /*
                          Recursive matching
                        */
                        if (partial_match)
                        {

                            new_res = wildcard_expand_internal(wcschr(wc, ANY_STRING_RECURSIVE),
                                                               new_dir.c_str(),
                                                               flags | WILDCARD_RECURSIVE,
                                                               out,
                                                               completion_set,
                                                               visited_files);

                            if (new_res == -1)
                            {
                                res = -1;
                                break;
                            }
                            res |= new_res;

                        }
                    }
                }
            }
        }
    }
    closedir(dir);

    return res;
}


int wildcard_expand(const wchar_t *wc,
                    const wchar_t *base_dir,
                    expand_flags_t flags,
                    std::vector<completion_t> &out)
{
    size_t c = out.size();

    /* Make a set of used completion strings so we can do fast membership tests inside wildcard_expand_internal. Otherwise wildcards like '**' are very slow, because we end up with an N^2 membership test.
    */
    std::set<wcstring> completion_set;
    for (std::vector<completion_t>::const_iterator iter = out.begin(); iter != out.end(); ++iter)
    {
        completion_set.insert(iter->completion);
    }

    std::set<file_id_t> visited_files;
    int res = wildcard_expand_internal(wc, base_dir, flags, out, completion_set, visited_files);

    if (flags & ACCEPT_INCOMPLETE)
    {
        wcstring wc_base;
        const wchar_t *wc_base_ptr = wcsrchr(wc, L'/');
        if (wc_base_ptr)
        {
            wc_base = wcstring(wc, (wc_base_ptr-wc)+1);
        }

        for (size_t i=c; i<out.size(); i++)
        {
            completion_t &c = out.at(i);

            if (c.flags & COMPLETE_REPLACES_TOKEN)
            {
                c.completion = format_string(L"%ls%ls%ls", base_dir, wc_base.c_str(), c.completion.c_str());
            }
        }
    }
    return res;
}

int wildcard_expand_string(const wcstring &wc, const wcstring &base_dir, expand_flags_t flags, std::vector<completion_t> &outputs)
{
    /* Hackish fix for 1631. We are about to call c_str(), which will produce a string truncated at any embedded nulls. We could fix this by passing around the size, etc. However embedded nulls are never allowed in a filename, so we just check for them and return 0 (no matches) if there is an embedded null. This isn't quite right, e.g. it will fail for \0?, but that is an edge case. */
    if (wc.find(L'\0') != wcstring::npos)
    {
        return 0;
    }
    // PCA: not convinced this temporary variable is really necessary
    std::vector<completion_t> lst;
    int res = wildcard_expand(wc.c_str(), base_dir.c_str(), flags, lst);
    outputs.insert(outputs.end(), lst.begin(), lst.end());
    return res;
}
