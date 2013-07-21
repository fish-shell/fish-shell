/** \file parse_util.c

    Various mostly unrelated utility functions related to parsing,
    loading and evaluating fish code.

  This library can be seen as a 'toolbox' for functions that are
  used in many places in fish and that are somehow related to
  parsing the code.
*/

#include "config.h"


#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <unistd.h>
#include <wctype.h>

#include <wchar.h>
#include <map>
#include <set>
#include <algorithm>

#include <time.h>
#include <assert.h>

#include "fallback.h"
#include "util.h"

#include "wutil.h"
#include "common.h"
#include "tokenizer.h"
#include "parse_util.h"
#include "expand.h"
#include "intern.h"
#include "exec.h"
#include "env.h"
#include "signal.h"
#include "wildcard.h"

/**
   Maximum number of autoloaded items opf a specific type to keep in
   memory at a time.
*/
#define AUTOLOAD_MAX 10

/**
   Minimum time, in seconds, before an autoloaded item will be
   unloaded
*/
#define AUTOLOAD_MIN_AGE 60

int parse_util_lineno(const wchar_t *str, size_t offset)
{
    if (! str)
        return 0;

    int res = 1;
    for (size_t i=0; str[i] && i<offset; i++)
    {
        if (str[i] == L'\n')
        {
            res++;
        }
    }
    return res;
}


int parse_util_get_line_from_offset(const wcstring &str, size_t pos)
{
    const wchar_t *buff = str.c_str();
    int count = 0;
    for (size_t i=0; i<pos; i++)
    {
        if (!buff[i])
        {
            return -1;
        }

        if (buff[i] == L'\n')
        {
            count++;
        }
    }
    return count;
}


size_t parse_util_get_offset_from_line(const wcstring &str, int line)
{
    const wchar_t *buff = str.c_str();
    size_t i;
    int count = 0;

    if (line < 0)
    {
        return (size_t)(-1);
    }

    if (line == 0)
        return 0;

    for (i=0;; i++)
    {
        if (!buff[i])
        {
            return -1;
        }

        if (buff[i] == L'\n')
        {
            count++;
            if (count == line)
            {
                return (i+1)<str.size()?i+1:i;
            }

        }
    }
}

size_t parse_util_get_offset(const wcstring &str, int line, long line_offset)
{
    const wchar_t *buff = str.c_str();
    size_t off = parse_util_get_offset_from_line(buff, line);
    size_t off2 = parse_util_get_offset_from_line(buff, line+1);
    long line_offset2 = line_offset;

    if (off == (size_t)(-1))
    {
        return -1;
    }

    if (off2 == (size_t)(-1))
    {
        off2 = wcslen(buff)+1;
    }

    if (line_offset2 < 0)
    {
        line_offset2 = 0;
    }

    if (line_offset2 >= off2-off-1)
    {
        line_offset2 = off2-off-1;
    }

    return off + line_offset2;

}


int parse_util_locate_cmdsubst(const wchar_t *in, wchar_t **begin, wchar_t **end, bool allow_incomplete)
{
    wchar_t *pos;
    wchar_t prev=0;
    int syntax_error=0;
    int paran_count=0;

    wchar_t *paran_begin=0, *paran_end=0;

    CHECK(in, 0);

    for (pos = (wchar_t *)in; *pos; pos++)
    {
        if (prev != '\\')
        {
            if (wcschr(L"\'\"", *pos))
            {
                wchar_t *q_end = quote_end(pos);
                if (q_end && *q_end)
                {
                    pos=q_end;
                }
                else
                {
                    break;
                }
            }
            else
            {
                if (*pos == '(')
                {
                    if ((paran_count == 0)&&(paran_begin==0))
                    {
                        paran_begin = pos;
                    }

                    paran_count++;
                }
                else if (*pos == ')')
                {

                    paran_count--;

                    if ((paran_count == 0) && (paran_end == 0))
                    {
                        paran_end = pos;
                        break;
                    }

                    if (paran_count < 0)
                    {
                        syntax_error = 1;
                        break;
                    }
                }
            }

        }
        prev = *pos;
    }

    syntax_error |= (paran_count < 0);
    syntax_error |= ((paran_count>0)&&(!allow_incomplete));

    if (syntax_error)
    {
        return -1;
    }

    if (paran_begin == 0)
    {
        return 0;
    }

    if (begin)
    {
        *begin = paran_begin;
    }

    if (end)
    {
        *end = paran_count?(wchar_t *)in+wcslen(in):paran_end;
    }

    return 1;
}

void parse_util_cmdsubst_extent(const wchar_t *buff, size_t cursor_pos, const wchar_t **a, const wchar_t **b)
{
    const wchar_t * const cursor = buff + cursor_pos;

    CHECK(buff,);
    
    const size_t bufflen = wcslen(buff);
    assert(cursor_pos <= bufflen);
    
    /* ap and bp are the beginning and end of the tightest command substitition found so far */
    const wchar_t *ap = buff, *bp = buff + bufflen;
    const wchar_t *pos = buff;
    for (;;)
    {
        wchar_t *begin = NULL, *end = NULL;
        if (parse_util_locate_cmdsubst(pos, &begin, &end, true) <= 0)
        {
            /* No subshell found, all done */
            break;
        }
        
        /* Intrepret NULL to mean the end */
        if (end == NULL)
        {
            end = const_cast<wchar_t *>(buff) + bufflen;
        }
        
        if (begin < cursor && end >= cursor)
        {
            /* This command substitution surrounds the cursor, so it's a tighter fit */
            begin++;
            ap = begin;
            bp = end;
            pos = begin + 1;
        }
        else if (begin >= cursor)
        {
            /* This command substitution starts at or after the cursor. Since it was the first command substitution in the string, we're done. */
            break;
        }
        else
        {
            /* This command substitution ends before the cursor. Skip it. */
            assert(end < cursor);
            pos = end + 1;
            assert(pos <= buff + bufflen);
        }
    }
    
    if (a != NULL) *a = ap;
    if (b != NULL) *b = bp;
}

/**
   Get the beginning and end of the job or process definition under the cursor
*/
static void job_or_process_extent(const wchar_t *buff,
                                  size_t cursor_pos,
                                  const wchar_t **a,
                                  const wchar_t **b,
                                  int process)
{
    const wchar_t *begin, *end;
    long pos;
    wchar_t *buffcpy;
    int finished=0;

    CHECK(buff,);

    if (a)
    {
        *a=0;
    }

    if (b)
    {
        *b = 0;
    }

    parse_util_cmdsubst_extent(buff, cursor_pos, &begin, &end);
    if (!end || !begin)
    {
        return;
    }

    pos = cursor_pos - (begin - buff);

    if (a)
    {
        *a = begin;
    }

    if (b)
    {
        *b = end;
    }

    buffcpy = wcsndup(begin, end-begin);

    if (!buffcpy)
    {
        DIE_MEM();
    }

    tokenizer_t tok(buffcpy, TOK_ACCEPT_UNFINISHED);
    for (; tok_has_next(&tok) && !finished; tok_next(&tok))
    {
        int tok_begin = tok_get_pos(&tok);

        switch (tok_last_type(&tok))
        {
            case TOK_PIPE:
            {
                if (!process)
                {
                    break;
                }
            }

            case TOK_END:
            case TOK_BACKGROUND:
            {

                if (tok_begin >= pos)
                {
                    finished=1;
                    if (b)
                    {
                        *b = (wchar_t *)buff + tok_begin;
                    }
                }
                else
                {
                    if (a)
                    {
                        *a = (wchar_t *)buff + tok_begin+1;
                    }
                }

                break;
            }
        }
    }

    free(buffcpy);
}

void parse_util_process_extent(const wchar_t *buff,
                               size_t pos,
                               const wchar_t **a,
                               const wchar_t **b)
{
    job_or_process_extent(buff, pos, a, b, 1);
}

void parse_util_job_extent(const wchar_t *buff,
                           size_t pos,
                           const wchar_t **a,
                           const wchar_t **b)
{
    job_or_process_extent(buff,pos,a, b, 0);
}


void parse_util_token_extent(const wchar_t *buff,
                             size_t cursor_pos,
                             const wchar_t **tok_begin,
                             const wchar_t **tok_end,
                             const wchar_t **prev_begin,
                             const wchar_t **prev_end)
{
    const wchar_t *begin, *end;
    long pos;

    const wchar_t *a = NULL, *b = NULL, *pa = NULL, *pb = NULL;

    CHECK(buff,);

    assert(cursor_pos >= 0);

    parse_util_cmdsubst_extent(buff, cursor_pos, &begin, &end);

    if (!end || !begin)
    {
        return;
    }

    pos = cursor_pos - (begin - buff);

    a = buff + pos;
    b = a;
    pa = buff + pos;
    pb = pa;

    assert(begin >= buff);
    assert(begin <= (buff+wcslen(buff)));
    assert(end >= begin);
    assert(end <= (buff+wcslen(buff)));

    const wcstring buffcpy = wcstring(begin, end-begin);

    tokenizer_t tok(buffcpy.c_str(), TOK_ACCEPT_UNFINISHED | TOK_SQUASH_ERRORS);
    for (; tok_has_next(&tok); tok_next(&tok))
    {
        size_t tok_begin = tok_get_pos(&tok);
        size_t tok_end = tok_begin;

        /*
          Calculate end of token
        */
        if (tok_last_type(&tok) == TOK_STRING)
        {
            tok_end += wcslen(tok_last(&tok));
        }

        /*
          Cursor was before beginning of this token, means that the
          cursor is between two tokens, so we set it to a zero element
          string and break
        */
        if (tok_begin > pos)
        {
            a = b = (wchar_t *)buff + pos;
            break;
        }

        /*
          If cursor is inside the token, this is the token we are
          looking for. If so, set a and b and break
        */
        if ((tok_last_type(&tok) == TOK_STRING) && (tok_end >= pos))
        {
            a = begin + tok_get_pos(&tok);
            b = a + wcslen(tok_last(&tok));
            break;
        }

        /*
          Remember previous string token
        */
        if (tok_last_type(&tok) == TOK_STRING)
        {
            pa = begin + tok_get_pos(&tok);
            pb = pa + wcslen(tok_last(&tok));
        }
    }

    if (tok_begin)
    {
        *tok_begin = a;
    }

    if (tok_end)
    {
        *tok_end = b;
    }

    if (prev_begin)
    {
        *prev_begin = pa;
    }

    if (prev_end)
    {
        *prev_end = pb;
    }

    assert(pa >= buff);
    assert(pa <= (buff+wcslen(buff)));
    assert(pb >= pa);
    assert(pb <= (buff+wcslen(buff)));

}

void parse_util_set_argv(const wchar_t * const *argv, const wcstring_list_t &named_arguments)
{
    if (*argv)
    {
        const wchar_t * const *arg;
        wcstring sb;

        for (arg=argv; *arg; arg++)
        {
            if (arg != argv)
            {
                sb.append(ARRAY_SEP_STR);
            }
            sb.append(*arg);
        }

        env_set(L"argv", sb.c_str(), ENV_LOCAL);
    }
    else
    {
        env_set(L"argv", 0, ENV_LOCAL);
    }

    if (! named_arguments.empty())
    {
        const wchar_t * const *arg;
        size_t i;
        for (i=0, arg=argv; i < named_arguments.size(); i++)
        {
            env_set(named_arguments.at(i).c_str(), *arg, ENV_LOCAL);

            if (*arg)
                arg++;
        }
    }
}

wchar_t *parse_util_unescape_wildcards(const wchar_t *str)
{
    wchar_t *in, *out;
    wchar_t *unescaped;

    CHECK(str, 0);

    unescaped = wcsdup(str);

    if (!unescaped)
    {
        DIE_MEM();
    }

    for (in=out=unescaped; *in; in++)
    {
        switch (*in)
        {
            case L'\\':
            {
                switch (*(in + 1))
                {
                    case L'*':
                    case L'?':
                    {
                        in++;
                        *(out++)=*in;
                        break;
                    }
                    case L'\\':
                    {
                        in++;
                        *(out++)=L'\\';
                        *(out++)=L'\\';
                        break;
                    }
                    default:
                    {
                        *(out++)=*in;
                        break;
                    }
                }
                break;
            }

            case L'*':
            {
                *(out++)=ANY_STRING;
                break;
            }

            case L'?':
            {
                *(out++)=ANY_CHAR;
                break;
            }

            default:
            {
                *(out++)=*in;
                break;
            }
        }
    }
    *out = *in;
    return unescaped;
}


/**
   Find the outermost quoting style of current token. Returns 0 if
   token is not quoted.

*/
static wchar_t get_quote(const wchar_t *cmd, size_t len)
{
    size_t i=0;
    wchar_t res=0;

    while (1)
    {
        if (!cmd[i])
            break;

        if (cmd[i] == L'\\')
        {
            i++;
            if (!cmd[i])
                break;
            i++;
        }
        else
        {
            if (cmd[i] == L'\'' || cmd[i] == L'\"')
            {
                const wchar_t *end = quote_end(&cmd[i]);
                //fwprintf( stderr, L"Jump %d\n",  end-cmd );
                if ((end == 0) || (!*end) || (end > cmd + len))
                {
                    res = cmd[i];
                    break;
                }
                i = end-cmd+1;
            }
            else
                i++;
        }
    }

    return res;
}

void parse_util_get_parameter_info(const wcstring &cmd, const size_t pos, wchar_t *quote, size_t *offset, int *type)
{
    size_t prev_pos=0;
    wchar_t last_quote = '\0';
    int unfinished;

    tokenizer_t tok(cmd.c_str(), TOK_ACCEPT_UNFINISHED | TOK_SQUASH_ERRORS);
    for (; tok_has_next(&tok); tok_next(&tok))
    {
        if (tok_get_pos(&tok) > pos)
            break;

        if (tok_last_type(&tok) == TOK_STRING)
            last_quote = get_quote(tok_last(&tok),
                                   pos - tok_get_pos(&tok));

        if (type != NULL)
            *type = tok_last_type(&tok);

        prev_pos = tok_get_pos(&tok);
    }

    wchar_t *cmd_tmp = wcsdup(cmd.c_str());
    cmd_tmp[pos]=0;
    size_t cmdlen = wcslen(cmd_tmp);
    unfinished = (cmdlen==0);
    if (!unfinished)
    {
        unfinished = (quote != 0);

        if (!unfinished)
        {
            if (wcschr(L" \t\n\r", cmd_tmp[cmdlen-1]) != 0)
            {
                if ((cmdlen == 1) || (cmd_tmp[cmdlen-2] != L'\\'))
                {
                    unfinished=1;
                }
            }
        }
    }

    if (quote)
        *quote = last_quote;

    if (offset != 0)
    {
        if (!unfinished)
        {
            while ((cmd_tmp[prev_pos] != 0) && (wcschr(L";|",cmd_tmp[prev_pos])!= 0))
                prev_pos++;

            *offset = prev_pos;
        }
        else
        {
            *offset = pos;
        }
    }
    free(cmd_tmp);
}

wcstring parse_util_escape_string_with_quote(const wcstring &cmd, wchar_t quote)
{
    wcstring result;
    if (quote == L'\0')
    {
        result = escape_string(cmd, ESCAPE_ALL | ESCAPE_NO_QUOTED | ESCAPE_NO_TILDE);
    }
    else
    {
        bool unescapable = false;
        for (size_t i = 0; i < cmd.size(); i++)
        {
            wchar_t c = cmd.at(i);
            switch (c)
            {
                case L'\n':
                case L'\t':
                case L'\b':
                case L'\r':
                    unescapable = true;
                    break;
                default:
                    if (c == quote)
                        result.push_back(L'\\');
                    result.push_back(c);
                    break;
            }
        }

        if (unescapable)
        {
            result = escape_string(cmd, ESCAPE_ALL | ESCAPE_NO_QUOTED);
            result.insert(0, &quote, 1);
        }
    }
    return result;
}
