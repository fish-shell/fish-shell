/**\file expand.c

String expansion functions. These functions perform several kinds of
parameter expansion.

*/

#include "config.h"

#include <stdlib.h>
#include <stdio.h>
#include <wchar.h>
#include <string.h>
#include <wctype.h>
#include <errno.h>
#include <pwd.h>
#include <unistd.h>
#include <limits.h>
#include <sys/param.h>
#include <sys/types.h>
#ifdef HAVE_SYS_SYSCTL_H
#include <sys/sysctl.h>
#endif
#include <termios.h>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#include <signal.h>
#include <algorithm>

#include <assert.h>
#include <vector>

#ifdef SunOS
#include <procfs.h>
#endif

#include "fallback.h"
#include "util.h"

#include "common.h"
#include "wutil.h"
#include "env.h"
#include "proc.h"
#include "parser.h"
#include "expand.h"
#include "wildcard.h"
#include "exec.h"
#include "signal.h"
#include "tokenizer.h"
#include "complete.h"

#include "parse_util.h"

/**
   Error issued on invalid variable name
*/
#define COMPLETE_VAR_DESC _( L"The '$' character begins a variable name. The character '%lc', which directly followed a '$', is not allowed as a part of a variable name, and variable names may not be zero characters long. To learn more about variable expansion in fish, type 'help expand-variable'.")

/**
   Error issued on $?
*/
#define COMPLETE_YOU_WANT_STATUS _( L"$? is not a valid variable in fish. If you want the exit status of the last command, try $status.")

/**
   Error issued on invalid variable name
*/
#define COMPLETE_VAR_NULL_DESC _( L"The '$' begins a variable name. It was given at the end of an argument. Variable names may not be zero characters long. To learn more about variable expansion in fish, type 'help expand-variable'.")

/**
   Error issued on invalid variable name
*/
#define COMPLETE_VAR_BRACKET_DESC _( L"Did you mean %ls{$%ls}%ls? The '$' character begins a variable name. A bracket, which directly followed a '$', is not allowed as a part of a variable name, and variable names may not be zero characters long. To learn more about variable expansion in fish, type 'help expand-variable'." )

/**
   Error issued on invalid variable name
*/
#define COMPLETE_VAR_PARAN_DESC _( L"Did you mean (COMMAND)? In fish, the '$' character is only used for accessing variables. To learn more about command substitution in fish, type 'help expand-command-substitution'.")

/**
   Description for child process
*/
#define COMPLETE_CHILD_PROCESS_DESC _( L"Child process")

/**
   Description for non-child process
*/
#define COMPLETE_PROCESS_DESC _( L"Process")

/**
   Description for long job
*/
#define COMPLETE_JOB_DESC _( L"Job")

/**
   Description for short job. The job command is concatenated
*/
#define COMPLETE_JOB_DESC_VAL _( L"Job: %ls")

/**
   Description for the shells own pid
*/
#define COMPLETE_SELF_DESC _( L"Shell process")

/**
   Description for the shells own pid
*/
#define COMPLETE_LAST_DESC _( L"Last background job")

/**
   String in process expansion denoting ourself
*/
#define SELF_STR L"self"

/**
   String in process expansion denoting last background job
*/
#define LAST_STR L"last"

/**
   Characters which make a string unclean if they are the first
   character of the string. See \c expand_is_clean().
*/
#define UNCLEAN_FIRST L"~%"
/**
   Unclean characters. See \c expand_is_clean().
*/
#define UNCLEAN L"$*?\\\"'({})"

static void remove_internal_separator(wcstring &s, bool conv);

int expand_is_clean(const wchar_t *in)
{

    const wchar_t * str = in;

    CHECK(in, 1);

    /*
      Test characters that have a special meaning in the first character position
    */
    if (wcschr(UNCLEAN_FIRST, *str))
        return 0;

    /*
      Test characters that have a special meaning in any character position
    */
    while (*str)
    {
        if (wcschr(UNCLEAN, *str))
            return 0;
        str++;
    }

    return 1;
}

/**
   Return the environment variable value for the string starting at \c in.
*/
static env_var_t expand_var(const wchar_t *in)
{
    if (!in)
        return env_var_t::missing_var();
    return env_get_string(in);
}

/**
   Test if the specified string does not contain character which can
   not be used inside a quoted string.
*/
static int is_quotable(const wchar_t *str)
{
    switch (*str)
    {
        case 0:
            return 1;

        case L'\n':
        case L'\t':
        case L'\r':
        case L'\b':
        case L'\x1b':
            return 0;

        default:
            return is_quotable(str+1);
    }
    return 0;

}

static int is_quotable(const wcstring &str)
{
    return is_quotable(str.c_str());
}

wcstring expand_escape_variable(const wcstring &in)
{

    wcstring_list_t lst;
    wcstring buff;

    tokenize_variable_array(in, lst);

    switch (lst.size())
    {
        case 0:
            buff.append(L"''");
            break;

        case 1:
        {
            const wcstring &el = lst.at(0);

            if (el.find(L' ') != wcstring::npos && is_quotable(el))
            {
                buff.append(L"'");
                buff.append(el);
                buff.append(L"'");
            }
            else
            {
                buff.append(escape_string(el, 1));
            }
            break;
        }
        default:
        {
            for (size_t j=0; j<lst.size(); j++)
            {
                const wcstring &el = lst.at(j);
                if (j)
                    buff.append(L"  ");

                if (is_quotable(el))
                {
                    buff.append(L"'");
                    buff.append(el);
                    buff.append(L"'");
                }
                else
                {
                    buff.append(escape_string(el, 1));
                }
            }
        }
    }
    return buff;
}

/**
   Tests if all characters in the wide string are numeric
*/
static int iswnumeric(const wchar_t *n)
{
    for (; *n; n++)
    {
        if (*n < L'0' || *n > L'9')
        {
            return 0;
        }
    }
    return 1;
}

/**
   See if the process described by \c proc matches the commandline \c
   cmd
*/
static bool match_pid(const wcstring &cmd,
                      const wchar_t *proc,
                      int flags,
                      size_t *offset)
{
    /* Test for a direct match. If the proc string is empty (e.g. the user tries to complete against %), then return an offset pointing at the base command. That ensures that you don't see a bunch of dumb paths when completing against all processes. */
    if (proc[0] != L'\0' && wcsncmp(cmd.c_str(), proc, wcslen(proc)) == 0)
    {
        if (offset)
            *offset = 0;
        return true;
    }

    /* Get the command to match against. We're only interested in the last path component. */
    const wcstring base_cmd = wbasename(cmd);

    bool result = string_prefixes_string(proc, base_cmd);
    if (result)
    {
        /* It's a match. Return the offset within the full command. */
        if (offset)
            *offset = cmd.size() - base_cmd.size();
    }
    return result;
}

/** Helper class for iterating over processes. The names returned have been unescaped (e.g. may include spaces) */
#ifdef KERN_PROCARGS2

/* BSD / OS X process completions */

class process_iterator_t
{
    std::vector<pid_t> pids;
    size_t idx;

    wcstring name_for_pid(pid_t pid);

public:
    process_iterator_t();
    bool next_process(wcstring *str, pid_t *pid);
};

wcstring process_iterator_t::name_for_pid(pid_t pid)
{
    wcstring result;
    int mib[4], maxarg = 0, numArgs = 0;
    size_t size = 0;
    char *args = NULL, *stringPtr = NULL;

    mib[0] = CTL_KERN;
    mib[1] = KERN_ARGMAX;

    size = sizeof(maxarg);
    if (sysctl(mib, 2, &maxarg, &size, NULL, 0) == -1)
    {
        return result;
    }

    args = (char *)malloc(maxarg);
    if (args == NULL)
    {
        return result;
    }

    mib[0] = CTL_KERN;
    mib[1] = KERN_PROCARGS2;
    mib[2] = pid;

    size = (size_t)maxarg;
    if (sysctl(mib, 3, args, &size, NULL, 0) == -1)
    {
        free(args);
        return result;;
    }

    memcpy(&numArgs, args, sizeof(numArgs));
    stringPtr = args + sizeof(numArgs);
    result = str2wcstring(stringPtr);
    free(args);
    return result;
}

bool process_iterator_t::next_process(wcstring *out_str, pid_t *out_pid)
{
    wcstring name;
    pid_t pid = 0;
    bool result = false;
    while (idx < pids.size())
    {
        pid = pids.at(idx++);
        name = name_for_pid(pid);
        if (! name.empty())
        {
            result = true;
            break;
        }
    }
    if (result)
    {
        *out_str = name;
        *out_pid = pid;
    }
    return result;
}

process_iterator_t::process_iterator_t() : idx(0)
{
    int                 err;
    struct kinfo_proc * result;
    bool                done;
    static const int    name[] = { CTL_KERN, KERN_PROC, KERN_PROC_ALL, 0 };
    // Declaring name as const requires us to cast it when passing it to
    // sysctl because the prototype doesn't include the const modifier.
    size_t              length;


    // We start by calling sysctl with result == NULL and length == 0.
    // That will succeed, and set length to the appropriate length.
    // We then allocate a buffer of that size and call sysctl again
    // with that buffer.  If that succeeds, we're done.  If that fails
    // with ENOMEM, we have to throw away our buffer and loop.  Note
    // that the loop causes use to call sysctl with NULL again; this
    // is necessary because the ENOMEM failure case sets length to
    // the amount of data returned, not the amount of data that
    // could have been returned.

    result = NULL;
    done = false;
    do
    {
        assert(result == NULL);

        // Call sysctl with a NULL buffer.

        length = 0;
        err = sysctl((int *) name, (sizeof(name) / sizeof(*name)) - 1,
                     NULL, &length,
                     NULL, 0);
        if (err == -1)
        {
            err = errno;
        }

        // Allocate an appropriately sized buffer based on the results
        // from the previous call.

        if (err == 0)
        {
            result = (struct kinfo_proc *)malloc(length);
            if (result == NULL)
            {
                err = ENOMEM;
            }
        }

        // Call sysctl again with the new buffer.  If we get an ENOMEM
        // error, toss away our buffer and start again.

        if (err == 0)
        {
            err = sysctl((int *) name, (sizeof(name) / sizeof(*name)) - 1,
                         result, &length,
                         NULL, 0);
            if (err == -1)
            {
                err = errno;
            }
            if (err == 0)
            {
                done = true;
            }
            else if (err == ENOMEM)
            {
                assert(result != NULL);
                free(result);
                result = NULL;
                err = 0;
            }
        }
    }
    while (err == 0 && ! done);

    // Clean up and establish post conditions.
    if (err == 0 && result != NULL)
    {
        for (size_t idx = 0; idx < length / sizeof(struct kinfo_proc); idx++)
            pids.push_back(result[idx].kp_proc.p_pid);
    }

    if (result)
        free(result);
}

#else

/* /proc style process completions */
class process_iterator_t
{
    DIR *dir;

public:
    process_iterator_t();
    ~process_iterator_t();

    bool next_process(wcstring *out_str, pid_t *out_pid);
};

process_iterator_t::process_iterator_t(void)
{
    dir = opendir("/proc");
}

process_iterator_t::~process_iterator_t(void)
{
    if (dir)
        closedir(dir);
}

bool process_iterator_t::next_process(wcstring *out_str, pid_t *out_pid)
{
    wcstring cmd;
    pid_t pid = 0;
    while (cmd.empty())
    {
        wcstring name;
        if (! dir || ! wreaddir(dir, name))
            break;

        if (!iswnumeric(name.c_str()))
            continue;

        wcstring path = wcstring(L"/proc/") + name;
        struct stat buf;
        if (wstat(path, &buf))
            continue;

        if (buf.st_uid != getuid())
            continue;

        /* remember the pid */
        pid = fish_wcstoi(name.c_str(), NULL, 10);

        /* the 'cmdline' file exists, it should contain the commandline */
        FILE *cmdfile;
        if ((cmdfile=wfopen(path + L"/cmdline", "r")))
        {
            wcstring full_command_line;
            fgetws2(&full_command_line, cmdfile);

            /* The command line needs to be escaped */
            cmd = tok_first(full_command_line.c_str());
        }
#ifdef SunOS
        else if ((cmdfile=wfopen(path + L"/psinfo", "r")))
        {
            psinfo_t info;
            if (fread(&info, sizeof(info), 1, cmdfile))
            {
                /* The filename is unescaped */
                cmd = str2wcstring(info.pr_fname);
            }
        }
#endif
        if (cmdfile)
            fclose(cmdfile);
    }

    bool result = ! cmd.empty();
    if (result)
    {
        *out_str = cmd;
        *out_pid = pid;
    }
    return result;
}

#endif

std::vector<wcstring> expand_get_all_process_names(void)
{
    wcstring name;
    pid_t pid;
    process_iterator_t iterator;
    std::vector<wcstring> result;
    while (iterator.next_process(&name, &pid))
    {
        result.push_back(name);
    }
    return result;
}

/**
   Searches for a job with the specified job id, or a job or process
   which has the string \c proc as a prefix of its commandline.

   If the ACCEPT_INCOMPLETE flag is set, the remaining string for any matches
   are inserted.

   Otherwise, any job matching the specified string is matched, and
   the job pgid is returned. If no job matches, all child processes
   are searched. If no child processes match, and <tt>fish</tt> can
   understand the contents of the /proc filesystem, all the users
   processes are searched for matches.
*/

static int find_process(const wchar_t *proc,
                        expand_flags_t flags,
                        std::vector<completion_t> &out)
{
    int found = 0;

    if (!(flags & EXPAND_SKIP_JOBS))
    {
        ASSERT_IS_MAIN_THREAD();
        const job_t *j;

        if (iswnumeric(proc) || (wcslen(proc)==0))
        {
            /*
              This is a numeric job string, like '%2'
            */

            if (flags & ACCEPT_INCOMPLETE)
            {
                job_iterator_t jobs;
                while ((j = jobs.next()))
                {
                    wchar_t jid[16];
                    if (j->command_is_empty())
                        continue;

                    swprintf(jid, 16, L"%d", j->job_id);

                    if (wcsncmp(proc, jid, wcslen(proc))==0)
                    {
                        wcstring desc_buff = format_string(COMPLETE_JOB_DESC_VAL, j->command_wcstr());
                        append_completion(out,
                                          jid+wcslen(proc),
                                          desc_buff,
                                          0);
                    }
                }

            }
            else
            {

                int jid;
                wchar_t *end;

                errno = 0;
                jid = fish_wcstoi(proc, &end, 10);
                if (jid > 0 && !errno && !*end)
                {
                    j = job_get(jid);
                    if ((j != 0) && (j->command_wcstr() != 0))
                    {
                        {
                            append_completion(out, to_string<long>(j->pgid));
                            found = 1;
                        }
                    }
                }
            }
        }
        if (found)
            return 1;

        job_iterator_t jobs;
        while ((j = jobs.next()))
        {

            if (j->command_is_empty())
                continue;

            size_t offset;
            if (match_pid(j->command(), proc, flags, &offset))
            {
                if (flags & ACCEPT_INCOMPLETE)
                {
                    append_completion(out,
                                      j->command_wcstr() + offset + wcslen(proc),
                                      COMPLETE_JOB_DESC,
                                      0);
                }
                else
                {
                    append_completion(out, to_string<long>(j->pgid));
                    found = 1;
                }
            }
        }

        if (found)
        {
            return 1;
        }

        jobs.reset();
        while ((j = jobs.next()))
        {
            process_t *p;
            if (j->command_is_empty())
                continue;
            for (p=j->first_process; p; p=p->next)
            {
                if (p->actual_cmd.empty())
                    continue;

                size_t offset;
                if (match_pid(p->actual_cmd, proc, flags, &offset))
                {
                    if (flags & ACCEPT_INCOMPLETE)
                    {
                        append_completion(out,
                                          wcstring(p->actual_cmd, offset + wcslen(proc)),
                                          COMPLETE_CHILD_PROCESS_DESC,
                                          0);
                    }
                    else
                    {
                        append_completion(out,
                                          to_string<long>(p->pid),
                                          L"",
                                          0);
                        found = 1;
                    }
                }
            }
        }

        if (found)
        {
            return 1;
        }
    }

    /* Iterate over all processes */
    wcstring process_name;
    pid_t process_pid;
    process_iterator_t iterator;
    while (iterator.next_process(&process_name, &process_pid))
    {
        size_t offset;
        if (match_pid(process_name, proc, flags, &offset))
        {
            if (flags & ACCEPT_INCOMPLETE)
            {
                append_completion(out,
                                  process_name.c_str() + offset + wcslen(proc),
                                  COMPLETE_PROCESS_DESC,
                                  0);
            }
            else
            {
                append_completion(out, to_string<long>(process_pid));
            }
        }
    }

    return 1;
}

/**
   Process id expansion
*/
static int expand_pid(const wcstring &instr_with_sep,
                      expand_flags_t flags,
                      std::vector<completion_t> &out)
{

    /* expand_string calls us with internal separators in instr...sigh */
    wcstring instr = instr_with_sep;
    remove_internal_separator(instr, false);

    if (instr.empty() || instr.at(0) != PROCESS_EXPAND)
    {
        append_completion(out, instr);
        return 1;
    }

    const wchar_t * const in = instr.c_str();

    if (flags & ACCEPT_INCOMPLETE)
    {
        if (wcsncmp(in+1, SELF_STR, wcslen(in+1))==0)
        {
            append_completion(out,
                              &SELF_STR[wcslen(in+1)],
                              COMPLETE_SELF_DESC,
                              0);
        }
        else if (wcsncmp(in+1, LAST_STR, wcslen(in+1))==0)
        {
            append_completion(out,
                              &LAST_STR[wcslen(in+1)],
                              COMPLETE_LAST_DESC,
                              0);
        }
    }
    else
    {
        if (wcscmp((in+1), SELF_STR)==0)
        {

            append_completion(out, to_string<long>(getpid()));

            return 1;
        }
        if (wcscmp((in+1), LAST_STR)==0)
        {
            if (proc_last_bg_pid > 0)
            {
                append_completion(out, to_string<long>(proc_last_bg_pid));
            }

            return 1;
        }
    }

    size_t prev = out.size();
    if (!find_process(in+1, flags, out))
        return 0;

    if (prev ==  out.size())
    {
        if (!(flags & ACCEPT_INCOMPLETE))
        {
            return 0;
        }
    }

    return 1;
}


void expand_variable_error(parser_t &parser, const wchar_t *token, size_t token_pos, int error_pos)
{
    size_t stop_pos = token_pos+1;

    switch (token[stop_pos])
    {
        case BRACKET_BEGIN:
        {
            wchar_t *cpy = wcsdup(token);
            *(cpy+token_pos)=0;
            wchar_t *name = &cpy[stop_pos+1];
            wchar_t *end = wcschr(name, BRACKET_END);
            wchar_t *post;
            int is_var=0;
            if (end)
            {
                post = end+1;
                *end = 0;

                if (!wcsvarname(name))
                {
                    is_var = 1;
                }
            }

            if (is_var)
            {
                parser.error(SYNTAX_ERROR,
                             error_pos,
                             COMPLETE_VAR_BRACKET_DESC,
                             cpy,
                             name,
                             post);
            }
            else
            {
                parser.error(SYNTAX_ERROR,
                             error_pos,
                             COMPLETE_VAR_BRACKET_DESC,
                             L"",
                             L"VARIABLE",
                             L"");
            }
            free(cpy);

            break;
        }

        case INTERNAL_SEPARATOR:
        {
            parser.error(SYNTAX_ERROR,
                         error_pos,
                         COMPLETE_VAR_PARAN_DESC);
            break;
        }

        case 0:
        {
            parser.error(SYNTAX_ERROR,
                         error_pos,
                         COMPLETE_VAR_NULL_DESC);
            break;
        }

        default:
        {
            wchar_t token_stop_char = token[stop_pos];
            // Unescape (see http://github.com/fish-shell/fish-shell/issues/50)
            if (token_stop_char == ANY_CHAR)
                token_stop_char = L'?';
            else if (token_stop_char == ANY_STRING || token_stop_char == ANY_STRING_RECURSIVE)
                token_stop_char = L'*';

            parser.error(SYNTAX_ERROR,
                         error_pos,
                         (token_stop_char == L'?' ? COMPLETE_YOU_WANT_STATUS : COMPLETE_VAR_DESC),
                         token_stop_char);
            break;
        }
    }
}

/**
   Parse an array slicing specification
 */
static int parse_slice(const wchar_t *in, wchar_t **end_ptr, std::vector<long> &idx, size_t array_size)
{
    wchar_t *end;

    const long size = (long)array_size;
    size_t pos = 1; //skip past the opening square bracket

    //  debug( 0, L"parse_slice on '%ls'", in );

    while (1)
    {
        long tmp;

        while (iswspace(in[pos]) || (in[pos]==INTERNAL_SEPARATOR))
            pos++;

        if (in[pos] == L']')
        {
            pos++;
            break;
        }

        errno=0;
        tmp = wcstol(&in[pos], &end, 10);
        if ((errno) || (end == &in[pos]))
        {
            return 1;
        }
        //    debug( 0, L"Push idx %d", tmp );

        long i1 = tmp>-1 ? tmp : (long)array_size+tmp+1;
        pos = end-in;
        while (in[pos]==INTERNAL_SEPARATOR)
            pos++;
        if (in[pos]==L'.' && in[pos+1]==L'.')
        {
            pos+=2;
            while (in[pos]==INTERNAL_SEPARATOR)
                pos++;
            long tmp1 = wcstol(&in[pos], &end, 10);
            if ((errno) || (end == &in[pos]))
            {
                return 1;
            }
            pos = end-in;

            // debug( 0, L"Push range %d %d", tmp, tmp1 );
            long i2 = tmp1>-1 ? tmp1 : size+tmp1+1;
            // debug( 0, L"Push range idx %d %d", i1, i2 );
            short direction = i2<i1 ? -1 : 1 ;
            for (long jjj = i1; jjj*direction <= i2*direction; jjj+=direction)
            {
                // debug(0, L"Expand range [subst]: %i\n", jjj);
                idx.push_back(jjj);
            }
            continue;
        }

        // debug( 0, L"Push idx %d", tmp );
        idx.push_back(i1);
    }

    if (end_ptr)
    {
        //    debug( 0, L"Remainder is '%ls', slice def was %d characters long", in+pos, pos );

        *end_ptr = (wchar_t *)(in+pos);
    }
    //  debug( 0, L"ok, done" );

    return 0;
}



/**
   Expand all environment variables in the string *ptr.

   This function is slow, fragile and complicated. There are lots of
   little corner cases, like $$foo should do a double expansion,
   $foo$bar should not double expand bar, etc. Also, it's easy to
   accidentally leak memory on array out of bounds errors an various
   other situations. All in all, this function should be rewritten,
   split out into multiple logical units and carefully tested. After
   that, it can probably be optimized to do fewer memory allocations,
   fewer string scans and overall just less work. But until that
   happens, don't edit it unless you know exactly what you are doing,
   and do proper testing afterwards.
*/
static int expand_variables_internal(parser_t &parser, wchar_t * const in, std::vector<completion_t> &out, long last_idx);

static int expand_variables2(parser_t &parser, const wcstring &instr, std::vector<completion_t> &out, long last_idx)
{
    wchar_t *in = wcsdup(instr.c_str());
    int result = expand_variables_internal(parser, in, out, last_idx);
    free(in);
    return result;
}

static int expand_variables_internal(parser_t &parser, wchar_t * const in, std::vector<completion_t> &out, long last_idx)
{
    int is_ok= 1;
    int empty=0;

    wcstring var_tmp;
    std::vector<long> var_idx_list;

    //  CHECK( out, 0 );

    for (long i=last_idx; (i>=0) && is_ok && !empty; i--)
    {
        const wchar_t c = in[i];
        if ((c == VARIABLE_EXPAND) || (c == VARIABLE_EXPAND_SINGLE))
        {
            long start_pos = i+1;
            long stop_pos;
            long var_len;
            int is_single = (c==VARIABLE_EXPAND_SINGLE);

            stop_pos = start_pos;

            while (1)
            {
                if (!(in[stop_pos ]))
                    break;
                if (!(iswalnum(in[stop_pos]) ||
                        (wcschr(L"_", in[stop_pos])!= 0)))
                    break;

                stop_pos++;
            }

            /*      printf( "Stop for '%c'\n", in[stop_pos]);*/

            var_len = stop_pos - start_pos;

            if (var_len == 0)
            {
                expand_variable_error(parser, in, stop_pos-1, -1);

                is_ok = 0;
                break;
            }

            var_tmp.append(in + start_pos, var_len);
            env_var_t var_val = expand_var(var_tmp.c_str());

            if (! var_val.missing())
            {
                int all_vars=1;
                wcstring_list_t var_item_list;

                if (is_ok)
                {
                    tokenize_variable_array(var_val.c_str(), var_item_list);

                    if (in[stop_pos] == L'[')
                    {
                        wchar_t *slice_end;
                        all_vars=0;

                        if (parse_slice(in + stop_pos, &slice_end, var_idx_list, var_item_list.size()))
                        {
                            parser.error(SYNTAX_ERROR,
                                         -1,
                                         L"Invalid index value");
                            is_ok = 0;
                            break;
                        }
                        stop_pos = (slice_end-in);
                    }

                    if (!all_vars)
                    {
                        wcstring_list_t string_values(var_idx_list.size());

                        for (size_t j=0; j<var_idx_list.size(); j++)
                        {
                            long tmp = var_idx_list.at(j);
                            /*
                                           Check that we are within array
                                           bounds. If not, truncate the list to
                                           exit.
                                           */
                            if (tmp < 1 || (size_t)tmp > var_item_list.size())
                            {
                                parser.error(SYNTAX_ERROR,
                                             -1,
                                             ARRAY_BOUNDS_ERR);
                                is_ok=0;
                                var_idx_list.resize(j);
                                break;
                            }
                            else
                            {
                                /* Replace each index in var_idx_list inplace with the string value at the specified index */
                                //al_set( var_idx_list, j, wcsdup((const wchar_t *)al_get( &var_item_list, tmp-1 ) ) );
                                string_values.at(j) = var_item_list.at(tmp-1);
                            }
                        }

                        // string_values is the new var_item_list
                        var_item_list.swap(string_values);
                    }
                }

                if (is_ok)
                {

                    if (is_single)
                    {
                        in[i]=0;
                        wcstring res = in;
                        res.push_back(INTERNAL_SEPARATOR);

                        for (size_t j=0; j<var_item_list.size(); j++)
                        {
                            const wcstring &next = var_item_list.at(j);
                            if (is_ok)
                            {
                                if (j != 0)
                                    res.append(L" ");
                                res.append(next);
                            }
                        }
                        res.append(in + stop_pos);
                        is_ok &= expand_variables2(parser, res, out, i);
                    }
                    else
                    {
                        for (size_t j=0; j<var_item_list.size(); j++)
                        {
                            const wcstring &next = var_item_list.at(j);
                            if (is_ok && (i == 0) && (!in[stop_pos]))
                            {
                                append_completion(out, next);
                            }
                            else
                            {

                                if (is_ok)
                                {
                                    wcstring new_in;

                                    if (start_pos > 0)
                                        new_in.append(in, start_pos - 1);

                                    // at this point new_in.size() is start_pos - 1
                                    if (start_pos>1 && new_in[start_pos-2]!=VARIABLE_EXPAND)
                                    {
                                        new_in.push_back(INTERNAL_SEPARATOR);
                                    }
                                    new_in.append(next);
                                    new_in.append(in + stop_pos);
                                    is_ok &= expand_variables2(parser, new_in, out, i);
                                }
                            }

                        }
                    }
                }

                return is_ok;
            }
            else
            {
                /*
                         Expand a non-existing variable
                         */
                if (c == VARIABLE_EXPAND)
                {
                    /*
                               Regular expansion, i.e. expand this argument to nothing
                               */
                    empty = 1;
                }
                else
                {
                    /*
                               Expansion to single argument.
                               */
                    wcstring res;
                    in[i] = 0;
                    res.append(in);
                    res.append(in + stop_pos);

                    is_ok &= expand_variables2(parser, res, out, i);
                    return is_ok;
                }
            }


        }
    }

    if (!empty)
    {
        append_completion(out, in);
    }

    return is_ok;
}

/**
   Perform bracket expansion
*/
static int expand_brackets(parser_t &parser, const wcstring &instr, int flags, std::vector<completion_t> &out)
{
    bool syntax_error = false;
    int bracket_count=0;

    const wchar_t *bracket_begin = NULL, *bracket_end = NULL;
    const wchar_t *last_sep = NULL;

    const wchar_t *item_begin;
    size_t length_preceding_brackets, length_following_brackets, tot_len;

    const wchar_t * const in = instr.c_str();

    /* Locate the first non-nested bracket pair */
    for (const wchar_t *pos = in; (*pos) && !syntax_error; pos++)
    {
        switch (*pos)
        {
            case BRACKET_BEGIN:
            {
                if (bracket_count == 0)
                    bracket_begin = pos;
                bracket_count++;
                break;

            }
            case BRACKET_END:
            {
                bracket_count--;
                if (bracket_count < 0)
                {
                    syntax_error = true;
                }
                else if (bracket_count == 0)
                {
                    bracket_end = pos;
                    break;
                }

            }
            case BRACKET_SEP:
            {
                if (bracket_count == 1)
                    last_sep = pos;
            }
        }
    }

    if (bracket_count > 0)
    {
        if (!(flags & ACCEPT_INCOMPLETE))
        {
            syntax_error = true;
        }
        else
        {
            /* The user hasn't typed an end bracket yet; make one up and append it, then expand that. */
            wcstring mod;
            if (last_sep)
            {
                mod.append(in, bracket_begin-in+1);
                mod.append(last_sep+1);
                mod.push_back(BRACKET_END);
            }
            else
            {
                mod.append(in);
                mod.push_back(BRACKET_END);
            }

            return expand_brackets(parser, mod, 1, out);
        }
    }

    if (syntax_error)
    {
        parser.error(SYNTAX_ERROR,
                     -1,
                     _(L"Mismatched brackets"));
        return 0;
    }

    if (bracket_begin == NULL)
    {
        append_completion(out, instr);
        return 1;
    }

    length_preceding_brackets = (bracket_begin-in);
    length_following_brackets = wcslen(bracket_end)-1;
    tot_len = length_preceding_brackets+length_following_brackets;
    item_begin = bracket_begin+1;
    for (const wchar_t *pos =(bracket_begin+1); true; pos++)
    {
        if (bracket_count == 0)
        {
            if ((*pos == BRACKET_SEP) || (pos==bracket_end))
            {
                assert(pos >= item_begin);
                size_t item_len = pos-item_begin;

                wcstring whole_item;
                whole_item.reserve(tot_len + item_len + 2);
                whole_item.append(in, length_preceding_brackets);
                whole_item.append(item_begin, item_len);
                whole_item.append(bracket_end + 1);
                expand_brackets(parser, whole_item, flags, out);

                item_begin = pos+1;
                if (pos == bracket_end)
                    break;
            }
        }

        if (*pos == BRACKET_BEGIN)
        {
            bracket_count++;
        }

        if (*pos == BRACKET_END)
        {
            bracket_count--;
        }
    }
    return 1;
}

/**
 Perform cmdsubst expansion
 */
static int expand_cmdsubst(parser_t &parser, const wcstring &input, std::vector<completion_t> &outList)
{
    wchar_t *paran_begin=0, *paran_end=0;
    std::vector<wcstring> sub_res;
    size_t i, j;
    wchar_t *tail_begin = 0;

    const wchar_t * const in = input.c_str();

    int parse_ret;
    switch (parse_ret = parse_util_locate_cmdsubst(in, &paran_begin, &paran_end, false))
    {
        case -1:
            parser.error(SYNTAX_ERROR,
                         -1,
                         L"Mismatched parenthesis");
            return 0;
        case 0:
            outList.push_back(completion_t(input));
            return 1;
        case 1:

            break;
    }

    const wcstring subcmd(paran_begin + 1, paran_end-paran_begin - 1);

    if (exec_subshell(subcmd, sub_res, true /* do apply exit status */) == -1)
    {
        parser.error(CMDSUBST_ERROR, -1, L"Unknown error while evaulating command substitution");
        return 0;
    }

    tail_begin = paran_end + 1;
    if (*tail_begin == L'[')
    {
        std::vector<long> slice_idx;
        wchar_t *slice_end;

        if (parse_slice(tail_begin, &slice_end, slice_idx, sub_res.size()))
        {
            parser.error(SYNTAX_ERROR, -1, L"Invalid index value");
            return 0;
        }
        else
        {
            std::vector<wcstring> sub_res2;
            tail_begin = slice_end;
            for (i=0; i < slice_idx.size(); i++)
            {
                long idx = slice_idx.at(i);
                if (idx < 1 || (size_t)idx > sub_res.size())
                {
                    parser.error(SYNTAX_ERROR,
                                 -1,
                                 ARRAY_BOUNDS_ERR);
                    return 0;
                }
                idx = idx-1;

                sub_res2.push_back(sub_res.at(idx));
                //        debug( 0, L"Pushing item '%ls' with index %d onto sliced result", al_get( sub_res, idx ), idx );
                //sub_res[idx] = 0; // ??
            }
            sub_res = sub_res2;
        }
    }


    /*
       Recursively call ourselves to expand any remaining command
       substitutions. The result of this recursive call using the tail
       of the string is inserted into the tail_expand array list
       */
    std::vector<completion_t> tail_expand;
    expand_cmdsubst(parser, tail_begin, tail_expand);

    /*
       Combine the result of the current command substitution with the
       result of the recursive tail expansion
       */
    for (i=0; i<sub_res.size(); i++)
    {
        wcstring sub_item = sub_res.at(i);
        wcstring sub_item2 = escape_string(sub_item, 1);

        for (j=0; j < tail_expand.size(); j++)
        {

            wcstring whole_item;

            wcstring tail_item = tail_expand.at(j).completion;

            //sb_append_substring( &whole_item, in, len1 );
            whole_item.append(in, paran_begin-in);

            //sb_append_char( &whole_item, INTERNAL_SEPARATOR );
            whole_item.push_back(INTERNAL_SEPARATOR);

            //sb_append_substring( &whole_item, sub_item2, item_len );
            whole_item.append(sub_item2);

            //sb_append_char( &whole_item, INTERNAL_SEPARATOR );
            whole_item.push_back(INTERNAL_SEPARATOR);

            //sb_append( &whole_item, tail_item );
            whole_item.append(tail_item);

            //al_push( out, whole_item.buff );
            outList.push_back(completion_t(whole_item));
        }
    }

    return 1;
}

/**
   Wrapper around unescape funtion. Issues an error() on failiure.
*/
__attribute__((unused))
static wchar_t *expand_unescape(parser_t &parser, const wchar_t * in, int escape_special)
{
    wchar_t *res = unescape(in, escape_special);
    if (!res)
        parser.error(SYNTAX_ERROR, -1, L"Unexpected end of string");
    return res;
}

static wcstring expand_unescape_string(const wcstring &in, int escape_special)
{
    wcstring tmp = in;
    unescape_string(tmp, escape_special);
    /* Need to detect error here */
    return tmp;
}

/* Given that input[0] is HOME_DIRECTORY or tilde (ugh), return the user's name. Return the empty string if it is just a tilde. Also return by reference the index of the first character of the remaining part of the string (e.g. the subsequent slash) */
static wcstring get_home_directory_name(const wcstring &input, size_t *out_tail_idx)
{
    const wchar_t * const in = input.c_str();
    assert(in[0] == HOME_DIRECTORY || in[0] == L'~');
    size_t tail_idx;

    const wchar_t *name_end = wcschr(in, L'/');
    if (name_end)
    {
        tail_idx = name_end - in;
    }
    else
    {
        tail_idx = wcslen(in);
    }
    *out_tail_idx = tail_idx;
    return input.substr(1, tail_idx - 1);
}

/** Attempts tilde expansion of the string specified, modifying it in place. */
static void expand_home_directory(wcstring &input)
{
    if (! input.empty() && input.at(0) == HOME_DIRECTORY)
    {
        size_t tail_idx;
        wcstring username = get_home_directory_name(input, &tail_idx);

        bool tilde_error = false;
        wcstring home;
        if (username.empty())
        {
            /* Current users home directory */
            home = env_get_string(L"HOME");
            tail_idx = 1;
        }
        else
        {
            /* Some other users home directory */
            std::string name_cstr = wcs2string(username);
            struct passwd *userinfo = getpwnam(name_cstr.c_str());
            if (userinfo == NULL)
            {
                tilde_error = true;
                input[0] = L'~';
            }
            else
            {
                home = str2wcstring(userinfo->pw_dir);
            }
        }

        if (! tilde_error)
        {
            input.replace(input.begin(), input.begin() + tail_idx, home);
        }
    }
}

void expand_tilde(wcstring &input)
{
    // Avoid needless COW behavior by ensuring we use const at
    const wcstring &tmp = input;
    if (! tmp.empty() && tmp.at(0) == L'~')
    {
        input.at(0) = HOME_DIRECTORY;
        expand_home_directory(input);
    }
}

static void unexpand_tildes(const wcstring &input, std::vector<completion_t> *completions)
{
    // If input begins with tilde, then try to replace the corresponding string in each completion with the tilde
    // If it does not, there's nothing to do
    if (input.empty() || input.at(0) != L'~')
        return;

    // We only operate on completions that replace their contents
    // If we don't have any, we're done.
    // In particular, empty vectors are common.
    bool has_candidate_completion = false;
    for (size_t i=0; i < completions->size(); i++)
    {
        if (completions->at(i).flags & COMPLETE_REPLACES_TOKEN)
        {
            has_candidate_completion = true;
            break;
        }
    }
    if (! has_candidate_completion)
        return;

    size_t tail_idx;
    wcstring username_with_tilde = L"~";
    username_with_tilde.append(get_home_directory_name(input, &tail_idx));

    // Expand username_with_tilde
    wcstring home = username_with_tilde;
    expand_tilde(home);

    // Now for each completion that starts with home, replace it with the username_with_tilde
    for (size_t i=0; i < completions->size(); i++)
    {
        completion_t &comp = completions->at(i);
        if ((comp.flags & COMPLETE_REPLACES_TOKEN) && string_prefixes_string(home, comp.completion))
        {
            comp.completion.replace(0, home.size(), username_with_tilde);

            // And mark that our tilde is literal, so it doesn't try to escape it
            comp.flags |= COMPLETE_DONT_ESCAPE_TILDES;
        }
    }
}

/**
   Remove any internal separators. Also optionally convert wildcard characters to
   regular equivalents. This is done to support EXPAND_SKIP_WILDCARDS.
*/
static void remove_internal_separator(wcstring &str, bool conv)
{
    /* Remove all instances of INTERNAL_SEPARATOR */
    str.erase(std::remove(str.begin(), str.end(), (wchar_t)INTERNAL_SEPARATOR), str.end());

    /* If conv is true, replace all instances of ANY_CHAR with '?', ANY_STRING with '*', ANY_STRING_RECURSIVE with '*' */
    if (conv)
    {
        for (size_t idx = 0; idx < str.size(); idx++)
        {
            switch (str.at(idx))
            {
                case ANY_CHAR:
                    str.at(idx) = L'?';
                    break;
                case ANY_STRING:
                case ANY_STRING_RECURSIVE:
                    str.at(idx) = L'*';
                    break;
            }
        }
    }
}


int expand_string(const wcstring &input, std::vector<completion_t> &output, expand_flags_t flags)
{
    parser_t parser(PARSER_TYPE_ERRORS_ONLY, true /* show errors */);

    size_t i;
    int res = EXPAND_OK;

    if ((!(flags & ACCEPT_INCOMPLETE)) && expand_is_clean(input.c_str()))
    {
        output.push_back(completion_t(input));
        return EXPAND_OK;
    }

    std::vector<completion_t> clist1, clist2;
    std::vector<completion_t> *in = &clist1, *out = &clist2;

    if (EXPAND_SKIP_CMDSUBST & flags)
    {
        wchar_t *begin, *end;

        if (parse_util_locate_cmdsubst(input.c_str(), &begin, &end, true) != 0)
        {
            parser.error(CMDSUBST_ERROR, -1, L"Command substitutions not allowed");
            return EXPAND_ERROR;
        }
        in->push_back(completion_t(input));
    }
    else
    {
        int cmdsubst_ok = expand_cmdsubst(parser, input, *in);
        if (! cmdsubst_ok)
            return EXPAND_ERROR;
    }

    for (i=0; i < in->size(); i++)
    {
        /*
         We accept incomplete strings here, since complete uses
         expand_string to expand incomplete strings from the
         commandline.
         */
        int unescape_flags = UNESCAPE_SPECIAL | UNESCAPE_INCOMPLETE;
        wcstring next = expand_unescape_string(in->at(i).completion, unescape_flags);

        if (EXPAND_SKIP_VARIABLES & flags)
        {
            for (size_t i=0; i < next.size(); i++)
            {
                if (next.at(i) == VARIABLE_EXPAND)
                {
                    next[i] = L'$';
                }
            }
            out->push_back(completion_t(next));
        }
        else
        {
            if (!expand_variables2(parser, next, *out, next.size() - 1))
            {
                return EXPAND_ERROR;
            }
        }
    }

    in->clear();
    std::swap(in, out); // note: this swaps the pointers only (last output is next input)

    for (i=0; i < in->size(); i++)
    {
        wcstring next = in->at(i).completion;

        if (!expand_brackets(parser, next, flags, *out))
        {
            return EXPAND_ERROR;
        }
    }
    in->clear();
    std::swap(in, out); // note: this swaps the pointers only (last output is next input)

    for (i=0; i < in->size(); i++)
    {
        wcstring next = in->at(i).completion;

        if (!(EXPAND_SKIP_HOME_DIRECTORIES & flags))
            expand_home_directory(next);


        if (flags & ACCEPT_INCOMPLETE)
        {
            if (next[0] == PROCESS_EXPAND)
            {
                /*
                 If process expansion matches, we are not
                 interested in other completions, so we
                 short-circuit and return
                 */
                if (!(flags & EXPAND_SKIP_PROCESS))
                    expand_pid(next, flags, output);
                return EXPAND_OK;
            }
            else
            {
                out->push_back(completion_t(next));
            }
        }
        else
        {
            if (!(flags & EXPAND_SKIP_PROCESS) && ! expand_pid(next, flags, *out))
            {
                return EXPAND_ERROR;
            }
        }
    }

    in->clear();
    std::swap(in, out); // note: this swaps the pointers only (last output is next input)

    for (i=0; i < in->size(); i++)
    {
        wcstring next_str = in->at(i).completion;
        int wc_res;

        remove_internal_separator(next_str, (EXPAND_SKIP_WILDCARDS & flags) ? true : false);
        const wchar_t *next = next_str.c_str();

        if (((flags & ACCEPT_INCOMPLETE) && (!(flags & EXPAND_SKIP_WILDCARDS))) ||
                wildcard_has(next, 1))
        {
            const wchar_t *start, *rest;

            if (next[0] == '/')
            {
                start = L"/";
                rest = &next[1];
            }
            else
            {
                start = L"";
                rest = next;
            }

            std::vector<completion_t> expanded;
            wc_res = wildcard_expand_string(rest, start, flags, expanded);
            if (flags & ACCEPT_INCOMPLETE)
            {
                out->insert(out->end(), expanded.begin(), expanded.end());
            }
            else
            {
                switch (wc_res)
                {
                    case 0:
                    {
                        if (res == EXPAND_OK)
                            res = EXPAND_WILDCARD_NO_MATCH;
                        break;
                    }

                    case 1:
                    {
                        res = EXPAND_WILDCARD_MATCH;
                        std::sort(expanded.begin(), expanded.end(), completion_t::is_alphabetically_less_than);
                        out->insert(out->end(), expanded.begin(), expanded.end());
                        break;
                    }

                    case -1:
                    {
                        return EXPAND_ERROR;
                    }

                }
            }
        }
        else
        {
            if (!(flags & ACCEPT_INCOMPLETE))
            {
                out->push_back(completion_t(next_str));
            }
        }
    }

    // Hack to un-expand tildes (see #647)
    if (!(flags & EXPAND_SKIP_HOME_DIRECTORIES))
    {
        unexpand_tildes(input, out);
    }

    // Return our output
    output.insert(output.end(), out->begin(), out->end());

    return res;
}

bool expand_one(wcstring &string, expand_flags_t flags)
{
    std::vector<completion_t> completions;
    bool result = false;

    if ((!(flags & ACCEPT_INCOMPLETE)) &&  expand_is_clean(string.c_str()))
    {
        return true;
    }

    if (expand_string(string, completions, flags))
    {
        if (completions.size() == 1)
        {
            string = completions.at(0).completion;
            result = true;
        }
    }
    return result;
}


/*

https://github.com/fish-shell/fish-shell/issues/367

With them the Seed of Wisdom did I sow,
And with my own hand labour'd it to grow:
And this was all the Harvest that I reap'd---
"I came like Water, and like Wind I go."

*/

static std::string escape_single_quoted_hack_hack_hack_hack(const char *str)
{
    std::string result;
    size_t len = strlen(str);
    result.reserve(len + 2);
    result.push_back('\'');
    for (size_t i=0; i < len; i++)
    {
        char c = str[i];
        // Escape backslashes and single quotes only
        if (c == '\\' || c == '\'')
            result.push_back('\\');
        result.push_back(c);
    }
    result.push_back('\'');
    return result;
}

bool fish_xdm_login_hack_hack_hack_hack(std::vector<std::string> *cmds, int argc, const char * const *argv)
{
    bool result = false;
    if (cmds && cmds->size() == 1)
    {
        const std::string &cmd = cmds->at(0);
        if (cmd == "exec \"${@}\"" || cmd == "exec \"$@\"")
        {
            /* We're going to construct a new command that starts with exec, and then has the remaining arguments escaped */
            std::string new_cmd = "exec";
            for (int i=1; i < argc; i++)
            {
                const char *arg = argv[i];
                if (arg)
                {
                    new_cmd.push_back(' ');
                    new_cmd.append(escape_single_quoted_hack_hack_hack_hack(arg));
                }
            }

            cmds->at(0) = new_cmd;
            result = true;
        }
    }
    return result;
}

bool fish_openSUSE_dbus_hack_hack_hack_hack(std::vector<completion_t> *args)
{
    static signed char isSUSE = -1;
    if (isSUSE == 0)
        return false;

    bool result = false;
    if (args && ! args->empty())
    {
        const wcstring &cmd = args->at(0).completion;
        if (cmd.find(L"DBUS_SESSION_BUS_") != wcstring::npos)
        {
            /* See if we are SUSE */
            if (isSUSE < 0)
            {
                struct stat buf = {};
                isSUSE = (0 == stat("/etc/SuSE-release", &buf));
            }

            if (isSUSE)
            {
                /* Look for an equal sign */
                size_t where = cmd.find(L'=');
                if (where != wcstring::npos)
                {
                    /* Oh my. It's presumably of the form foo=bar; find the = and split */
                    const wcstring key = wcstring(cmd, 0, where);

                    /* Trim whitespace and semicolon */
                    wcstring val = wcstring(cmd, where+1);
                    size_t last_good = val.find_last_not_of(L"\n ;");
                    if (last_good != wcstring::npos)
                        val.resize(last_good + 1);

                    args->clear();
                    args->push_back(completion_t(L"set"));
                    if (key == L"DBUS_SESSION_BUS_ADDRESS")
                        args->push_back(completion_t(L"-x"));
                    args->push_back(completion_t(key));
                    args->push_back(completion_t(val));
                    result = true;
                }
                else if (string_prefixes_string(L"export DBUS_SESSION_BUS_ADDRESS;", cmd))
                {
                    /* Nothing, we already exported it */
                    args->clear();
                    args->push_back(completion_t(L"echo"));
                    args->push_back(completion_t(L"-n"));
                    result = true;
                }
            }
        }
    }
    return result;
}

bool expand_abbreviation(const wcstring &src, wcstring *output)
{
    if (src.empty())
        return false;

    /* Get the abbreviations. Return false if we have none */
    env_var_t var = env_get_string(USER_ABBREVIATIONS_VARIABLE_NAME);
    if (var.missing_or_empty())
        return false;

    bool result = false;
    wcstring line;
    wcstokenizer tokenizer(var, ARRAY_SEP_STR);
    while (tokenizer.next(line))
    {
        /* Line is expected to be of the form 'foo=bar'. Parse out the first =. Be forgiving about spaces, but silently skip on failure (no equals, or equals at the end or beginning). Try to avoid copying any strings until we are sure this is a match. */
        size_t equals = line.find(L'=');
        if (equals == wcstring::npos || equals == 0 || equals + 1 == line.size())
            continue;

        /* Find the character just past the end of the command. Walk backwards, skipping spaces. */
        size_t cmd_end = equals;
        while (cmd_end > 0 && iswspace(line.at(cmd_end - 1)))
            cmd_end--;

        /* See if this command matches */
        if (line.compare(0, cmd_end, src) == 0)
        {
            /* Success. Set output to everythign past the end of the string. */
            if (output != NULL)
                output->assign(line, equals + 1, wcstring::npos);

            result = true;
            break;
        }
    }
    return result;
}
