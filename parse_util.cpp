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
#include "parse_tree.h"
#include "parser.h"
#include "builtin.h"

/** Error message for improper use of the exec builtin */
#define EXEC_ERR_MSG _(L"The '%ls' command can not be used in a pipeline")

/** Error message for use of backgrounded commands before and/or */
#define BOOL_AFTER_BACKGROUND_ERROR_MSG _(L"The '%ls' command can not be used immediately after a backgrounded job")

/** Error message for backgrounded commands as conditionals */
#define BACKGROUND_IN_CONDITIONAL_ERROR_MSG _(L"Backgrounded commands can not be used as conditionals")


int parse_util_lineno(const wchar_t *str, size_t offset)
{
    if (! str)
        return 0;

    int res = 1;
    for (size_t i=0; i < offset && str[i] != L'\0'; i++)
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

static int parse_util_locate_brackets_of_type(const wchar_t *in, wchar_t **begin, wchar_t **end, bool allow_incomplete, wchar_t open_type, wchar_t close_type)
{
    /* open_type is typically ( or [, and close type is the corresponding value */
    wchar_t *pos;
    wchar_t prev=0;
    int syntax_error=0;
    int paran_count=0;

    wchar_t *paran_begin=0, *paran_end=0;

    CHECK(in, 0);

    for (pos = const_cast<wchar_t *>(in); *pos; pos++)
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
                if (*pos == open_type)
                {
                    if ((paran_count == 0)&&(paran_begin==0))
                    {
                        paran_begin = pos;
                    }

                    paran_count++;
                }
                else if (*pos == close_type)
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


int parse_util_locate_cmdsubst(const wchar_t *in, wchar_t **begin, wchar_t **end, bool accept_incomplete)
{
    return parse_util_locate_brackets_of_type(in, begin, end, accept_incomplete, L'(', L')');
}

int parse_util_locate_slice(const wchar_t *in, wchar_t **begin, wchar_t **end, bool accept_incomplete)
{
    return parse_util_locate_brackets_of_type(in, begin, end, accept_incomplete, L'[', L']');
}


static int parse_util_locate_brackets_range(const wcstring &str, size_t *inout_cursor_offset, wcstring *out_contents, size_t *out_start, size_t *out_end, bool accept_incomplete, wchar_t open_type, wchar_t close_type)
{
    /* Clear the return values */
    out_contents->clear();
    *out_start = 0;
    *out_end = str.size();

    /* Nothing to do if the offset is at or past the end of the string. */
    if (*inout_cursor_offset >= str.size())
        return 0;

    /* Defer to the wonky version */
    const wchar_t * const buff = str.c_str();
    const wchar_t * const valid_range_start = buff + *inout_cursor_offset, *valid_range_end = buff + str.size();
    wchar_t *bracket_range_begin = NULL, *bracket_range_end = NULL;
    int ret = parse_util_locate_brackets_of_type(valid_range_start, &bracket_range_begin, &bracket_range_end, accept_incomplete, open_type, close_type);
    if (ret > 0)
    {
        /* The command substitutions must not be NULL and must be in the valid pointer range, and the end must be bigger than the beginning */
        assert(bracket_range_begin != NULL && bracket_range_begin >= valid_range_start && bracket_range_begin <= valid_range_end);
        assert(bracket_range_end != NULL && bracket_range_end > bracket_range_begin && bracket_range_end >= valid_range_start && bracket_range_end <= valid_range_end);

        /* Assign the substring to the out_contents */
        const wchar_t *interior_begin = bracket_range_begin + 1;
        out_contents->assign(interior_begin, bracket_range_end - interior_begin);

        /* Return the start and end */
        *out_start = bracket_range_begin - buff;
        *out_end = bracket_range_end - buff;

        /* Update the inout_cursor_offset. Note this may cause it to exceed str.size(), though overflow is not likely */
        *inout_cursor_offset = 1 + *out_end;
    }
    return ret;
}

int parse_util_locate_cmdsubst_range(const wcstring &str, size_t *inout_cursor_offset, wcstring *out_contents, size_t *out_start, size_t *out_end, bool accept_incomplete)
{
    return parse_util_locate_brackets_range(str, inout_cursor_offset, out_contents, out_start, out_end, accept_incomplete, L'(', L')');
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
        /* Interpret NULL to mean the end */
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
            /* pos is where to begin looking for the next one. But if we reached the end there's no next one. */
            if (begin >= end)
                break;
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

            default:
            {
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
    const wchar_t *a = NULL, *b = NULL, *pa = NULL, *pb = NULL;

    CHECK(buff,);

    assert(cursor_pos >= 0);

    const wchar_t *cmdsubst_begin, *cmdsubst_end;
    parse_util_cmdsubst_extent(buff, cursor_pos, &cmdsubst_begin, &cmdsubst_end);

    if (!cmdsubst_end || !cmdsubst_begin)
    {
        return;
    }

    /* pos is equivalent to cursor_pos within the range of the command substitution {begin, end} */
    long offset_within_cmdsubst = cursor_pos - (cmdsubst_begin - buff);

    a = cmdsubst_begin + offset_within_cmdsubst;
    b = a;
    pa = cmdsubst_begin + offset_within_cmdsubst;
    pb = pa;

    assert(cmdsubst_begin >= buff);
    assert(cmdsubst_begin <= (buff+wcslen(buff)));
    assert(cmdsubst_end >= cmdsubst_begin);
    assert(cmdsubst_end <= (buff+wcslen(buff)));

    const wcstring buffcpy = wcstring(cmdsubst_begin, cmdsubst_end-cmdsubst_begin);

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
        if (tok_begin > offset_within_cmdsubst)
        {
            a = b = cmdsubst_begin + offset_within_cmdsubst;
            break;
        }

        /*
          If cursor is inside the token, this is the token we are
          looking for. If so, set a and b and break
        */
        if ((tok_last_type(&tok) == TOK_STRING) && (tok_end >= offset_within_cmdsubst))
        {
            a = cmdsubst_begin + tok_get_pos(&tok);
            b = a + wcslen(tok_last(&tok));
            break;
        }

        /*
          Remember previous string token
        */
        if (tok_last_type(&tok) == TOK_STRING)
        {
            pa = cmdsubst_begin + tok_get_pos(&tok);
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
            env_set(named_arguments.at(i).c_str(), *arg, ENV_LOCAL | ENV_USER);

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

/* We are given a parse tree, the index of a node within the tree, its indent, and a vector of indents the same size as the original source string. Set the indent correspdonding to the node's source range, if appropriate.

   trailing_indent is the indent for nodes with unrealized source, i.e. if I type 'if false <ret>' then we have an if node with an empty job list (without source) but we want the last line to be indented anyways.

   switch statements also indent.

   max_visited_node_idx is the largest index we visited.
*/
static void compute_indents_recursive(const parse_node_tree_t &tree, node_offset_t node_idx, int node_indent, parse_token_type_t parent_type, std::vector<int> *indents, int *trailing_indent, node_offset_t *max_visited_node_idx)
{
    /* Guard against incomplete trees */
    if (node_idx > tree.size())
        return;

    /* Update max_visited_node_idx */
    if (node_idx > *max_visited_node_idx)
        *max_visited_node_idx = node_idx;

    /* We could implement this by utilizing the fish grammar. But there's an easy trick instead: almost everything that wraps a job list should be indented by 1. So just find all of the job lists. One exception is switch, which wraps a case_item_list instead of a job_list. The other exception is job_list itself: a job_list is a job and a job_list, and we want that child list to be indented the same as the parent. So just find all job_lists whose parent is not a job_list, and increment their indent by 1. */

    const parse_node_t &node = tree.at(node_idx);
    const parse_token_type_t node_type = node.type;

    /* Increment the indent if we are either a root job_list, or root case_item_list */
    const bool is_root_job_list = (node_type == symbol_job_list && parent_type != symbol_job_list);
    const bool is_root_case_item_list = (node_type == symbol_case_item_list && parent_type != symbol_case_item_list);
    if (is_root_job_list || is_root_case_item_list)
    {
        node_indent += 1;
    }

    /* If we have source, store the trailing indent unconditionally. If we do not have source, store the trailing indent only if ours is bigger; this prevents the trailing "run" of terminal job lists from affecting the trailing indent. For example, code like this:

            if foo

      will be parsed as this:

      job_list
        job
           if_statement
               job [if]
               job_list [empty]
         job_list [empty]

      There's two "terminal" job lists, and we want the innermost one.

      Note we are relying on the fact that nodes are in the same order as the source, i.e. an in-order traversal of the node tree also traverses the source from beginning to end.
    */
    if (node.has_source() || node_indent > *trailing_indent)
    {
        *trailing_indent = node_indent;
    }


    /* Store the indent into the indent array */
    if (node.source_start != SOURCE_OFFSET_INVALID && node.source_start < indents->size())
    {
        if (node.has_source())
        {
            /* A normal non-empty node. Store the indent unconditionally. */
            indents->at(node.source_start) = node_indent;
        }
        else
        {
            /* An empty node. We have a source offset but no source length. This can come about when a node legitimately empty:

                  while true; end

               The job_list inside the while loop is empty. It still has a source offset (at the end of the while statement) but no source extent.
               We still need to capture that indent, because there may be comments inside:
                    while true
                       # loop forever
                    end

               The 'loop forever' comment must be indented, by virtue of storing the indent.

               Now consider what happens if we remove the end:

                   while true
                     # loop forever

                Now both the job_list and end_command are unmaterialized. However, we want the indent to be of the job_list and not the end_command.  Therefore, we only store the indent if it's bigger.
            */
            if (node_indent > indents->at(node.source_start))
            {
                indents->at(node.source_start) = node_indent;
            }
        }
    }


    /* Recursive to all our children */
    for (node_offset_t idx = 0; idx < node.child_count; idx++)
    {
        /* Note we pass our type to our child, which becomes its parent node type */
        compute_indents_recursive(tree, node.child_start + idx, node_indent, node_type, indents, trailing_indent, max_visited_node_idx);
    }
}

std::vector<int> parse_util_compute_indents(const wcstring &src)
{
    /* Make a vector the same size as the input string, which contains the indents. Initialize them to -1. */
    const size_t src_size = src.size();
    std::vector<int> indents(src_size, -1);

    /* Parse the string. We pass continue_after_error to produce a forest; the trailing indent of the last node we visited becomes the input indent of the next. I.e. in the case of 'switch foo ; cas', we get an invalid parse tree (since 'cas' is not valid) but we indent it as if it were a case item list */
    parse_node_tree_t tree;
    parse_tree_from_string(src, parse_flag_continue_after_error | parse_flag_include_comments | parse_flag_accept_incomplete_tokens, &tree, NULL /* errors */);

    /* Start indenting at the first node. If we have a parse error, we'll have to start indenting from the top again */
    node_offset_t start_node_idx = 0;
    int last_trailing_indent = 0;

    while (start_node_idx < tree.size())
    {
        /* The indent that we'll get for the last line */
        int trailing_indent = 0;

        /* Biggest offset we visited */
        node_offset_t max_visited_node_idx = 0;

        /* Invoke the recursive version. As a hack, pass job_list for the 'parent' token type, which will prevent the really-root job list from indenting */
        compute_indents_recursive(tree, start_node_idx, last_trailing_indent, symbol_job_list, &indents, &trailing_indent, &max_visited_node_idx);

        /* We may have more to indent. The trailing indent becomes our current indent. Start at the node after the last we visited. */
        last_trailing_indent = trailing_indent;
        start_node_idx = max_visited_node_idx + 1;
    }

    /* Handle comments. Each comment node has a parent (which is whatever the top of the symbol stack was when the comment was encountered). So the source range of the comment has the same indent as its parent. */
    const size_t tree_size = tree.size();
    for (node_offset_t i=0; i < tree_size; i++)
    {
        const parse_node_t &node = tree.at(i);
        if (node.type == parse_special_type_comment && node.has_source() && node.parent < tree_size)
        {
            const parse_node_t &parent = tree.at(node.parent);
            if (parent.source_start != SOURCE_OFFSET_INVALID)
            {
                indents.at(node.source_start) = indents.at(parent.source_start);
            }
        }
    }

    /* Now apply the indents. The indents array has -1 for places where the indent does not change, so start at each value and extend it along the run of -1s */
    int last_indent = 0;
    for (size_t i=0; i<src_size; i++)
    {
        int this_indent = indents.at(i);
        if (this_indent < 0)
        {
            indents.at(i) = last_indent;
        }
        else
        {
            /* New indent level */
            last_indent = this_indent;
            /* Make all whitespace before a token have the new level. This avoid using the wrong indentation level if a new line starts with whitespace. */
            size_t prev_char_idx = i;
            while (prev_char_idx--)
            {
                if (!wcschr(L" \n\t\r", src.at(prev_char_idx)))
                    break;
                indents.at(prev_char_idx) = last_indent;
            }
        }
    }

    /* Ensure trailing whitespace has the trailing indent. This makes sure a new line is correctly indented even if it is empty. */
    size_t suffix_idx = src_size;
    while (suffix_idx--)
    {
        if (!wcschr(L" \n\t\r", src.at(suffix_idx)))
            break;
        indents.at(suffix_idx) = last_trailing_indent;
    }

    return indents;
}

/* Append a syntax error to the given error list */
static bool append_syntax_error(parse_error_list_t *errors, const parse_node_t &node, const wchar_t *fmt, ...)
{
    parse_error_t error;
    error.source_start = node.source_start;
    error.source_length = node.source_length;
    error.code = parse_error_syntax;

    va_list va;
    va_start(va, fmt);
    error.text = vformat_string(fmt, va);
    va_end(va);

    errors->push_back(error);
    return true;
}

/**
   Returns 1 if the specified command is a builtin that may not be used in a pipeline
*/
static int parser_is_pipe_forbidden(const wcstring &word)
{
    return contains(word,
                    L"exec",
                    L"case",
                    L"break",
                    L"return",
                    L"continue");
}

bool parse_util_argument_is_help(const wchar_t *s, int min_match)
{
    CHECK(s, 0);

    size_t len = wcslen(s);

    min_match = maxi(min_match, 3);

    return (wcscmp(L"-h", s) == 0) ||
           (len >= (size_t)min_match && (wcsncmp(L"--help", s, len) == 0));
}

// Check if the first argument under the given node is --help
static bool first_argument_is_help(const parse_node_tree_t &node_tree, const parse_node_t &node, const wcstring &src)
{
    bool is_help = false;
    const parse_node_tree_t::parse_node_list_t arg_nodes = node_tree.find_nodes(node, symbol_argument, 1);
    if (! arg_nodes.empty())
    {
        // Check the first argument only
        const parse_node_t &arg = *arg_nodes.at(0);
        const wcstring first_arg_src = arg.get_source(src);
        is_help = parse_util_argument_is_help(first_arg_src.c_str(), 3);
    }
    return is_help;
}

void parse_util_expand_variable_error(const parse_node_t &node, const wcstring &token, size_t token_pos, size_t error_pos, parse_error_list_t *out_errors)
{
    size_t stop_pos = token_pos+1;

    switch (token[stop_pos])
    {
        case BRACKET_BEGIN:
        {
            wchar_t *cpy = wcsdup(token.c_str());
            *(cpy+token_pos)=0;
            wchar_t *name = &cpy[stop_pos+1];
            wchar_t *end = wcschr(name, BRACKET_END);
            wchar_t *post = NULL;
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
                append_syntax_error(out_errors,
                                    node,
                                    COMPLETE_VAR_BRACKET_DESC,
                                    cpy,
                                    name,
                                    post);
            }
            else
            {
                append_syntax_error(out_errors,
                                    node,
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
            append_syntax_error(out_errors,
                                node,
                                COMPLETE_VAR_PARAN_DESC);
            break;
        }

        case 0:
        {
            append_syntax_error(out_errors,
                                node,
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

            append_syntax_error(out_errors,
                                node,
                                (token_stop_char == L'?' ? COMPLETE_YOU_WANT_STATUS : COMPLETE_VAR_DESC),
                                token_stop_char);
            break;
        }
    }
}


/**
   Test if this argument contains any errors. Detected errors include
   syntax errors in command substitutions, improperly escaped
   characters and improper use of the variable expansion operator.
*/
parser_test_error_bits_t parse_util_detect_errors_in_argument(const parse_node_t &node, const wcstring &arg_src, parse_error_list_t *out_errors)
{
    assert(node.type == symbol_argument);

    int err=0;

    wchar_t *paran_begin, *paran_end;
    int do_loop = 1;
    
    wcstring working_copy = arg_src;

    while (do_loop)
    {
        const wchar_t *working_copy_cstr = working_copy.c_str();
        switch (parse_util_locate_cmdsubst(working_copy_cstr,
                                           &paran_begin,
                                           &paran_end,
                                           false))
        {
            case -1:
            {
                err=1;
                if (out_errors)
                {
                    append_syntax_error(out_errors, node, L"Mismatched parenthesis");
                }
                return err;
            }

            case 0:
            {
                do_loop = 0;
                break;
            }

            case 1:
            {

                const wcstring subst(paran_begin + 1, paran_end);

                // Replace the command substitution with just INTERNAL_SEPARATOR
                size_t cmd_sub_start = paran_begin - working_copy_cstr;
                size_t cmd_sub_len = paran_end + 1 - paran_begin;
                working_copy.replace(cmd_sub_start, cmd_sub_len, wcstring(1, INTERNAL_SEPARATOR));

                parse_error_list_t subst_errors;
                err |= parse_util_detect_errors(subst, &subst_errors, false /* do not accept incomplete */);

                /* Our command substitution produced error offsets relative to its source. Tweak the offsets of the errors in the command substitution to account for both its offset within the string, and the offset of the node */
                size_t error_offset = cmd_sub_start + 1 + node.source_start;
                parse_error_offset_source_start(&subst_errors, error_offset);

                if (out_errors != NULL)
                {
                    out_errors->insert(out_errors->end(), subst_errors.begin(), subst_errors.end());
                }
                break;
            }
        }
    }

    wcstring unesc;
    if (! unescape_string(working_copy, &unesc, UNESCAPE_SPECIAL))
    {
        if (out_errors)
        {
            append_syntax_error(out_errors, node, L"Invalid token '%ls'", working_copy.c_str());
        }
        return 1;
    }
    else
    {
        /* Check for invalid variable expansions */
        const size_t unesc_size = unesc.size();
        for (size_t idx = 0; idx < unesc_size; idx++)
        {
            switch (unesc.at(idx))
            {
                case VARIABLE_EXPAND:
                case VARIABLE_EXPAND_SINGLE:
                {
                    wchar_t next_char = (idx + 1 < unesc_size ? unesc.at(idx + 1) : L'\0');

                    if (next_char != VARIABLE_EXPAND &&
                            next_char != VARIABLE_EXPAND_SINGLE &&
                            ! wcsvarchr(next_char))
                    {
                        err=1;
                        if (out_errors)
                        {
                            parse_util_expand_variable_error(node, unesc, idx, node.source_start, out_errors);
                        }
                    }

                    break;
                }
            }
        }
    }

    return err;
}

parser_test_error_bits_t parse_util_detect_errors(const wcstring &buff_src, parse_error_list_t *out_errors, bool allow_incomplete)
{
    parse_node_tree_t node_tree;
    parse_error_list_t parse_errors;

    parser_test_error_bits_t res = 0;

    // Whether we encountered a parse error
    bool errored = false;

    // Whether we encountered an unclosed block
    // We detect this via an 'end_command' block without source
    bool has_unclosed_block = false;

    // Whether there's an unclosed quote, and therefore unfinished
    // This is only set if allow_incomplete is set
    bool has_unclosed_quote = false;

    // Parse the input string into a parse tree
    // Some errors are detected here
    bool parsed = parse_tree_from_string(buff_src, allow_incomplete ? parse_flag_leave_unterminated : parse_flag_none, &node_tree, &parse_errors);

    if (allow_incomplete)
    {
        for (size_t i=0; i < parse_errors.size(); i++)
        {
            if (parse_errors.at(i).code == parse_error_tokenizer_unterminated_quote)
            {
                // Remove this error, since we don't consider it a real error
                has_unclosed_quote = true;
                parse_errors.erase(parse_errors.begin() + i);
                i--;
            }
        }
    }
    
    // #1238: If the only error was unterminated quote, then consider this to have parsed successfully. A better fix would be to have parse_tree_from_string return this information directly (but it would be a shame to munge up its nice bool return).
    if (parse_errors.empty() && has_unclosed_quote)
    {
        parsed = true;
    }

    if (! parsed)
    {
        errored = true;
    }

    // has_unclosed_quote may only be set if allow_incomplete is true
    assert(! has_unclosed_quote || allow_incomplete);
    
    // Expand all commands
    // Verify 'or' and 'and' not used inside pipelines
    // Verify pipes via parser_is_pipe_forbidden
    // Verify return only within a function
    // Verify no variable expansions

    if (! errored)
    {
        const size_t node_tree_size = node_tree.size();
        for (size_t i=0; i < node_tree_size; i++)
        {
            const parse_node_t &node = node_tree.at(i);
            if (node.type == symbol_end_command && ! node.has_source())
            {
                // an 'end' without source is an unclosed block
                has_unclosed_block = true;
            }
            else if (node.type == symbol_boolean_statement)
            {
                // 'or' and 'and' can be in a pipeline, as long as they're first
                parse_bool_statement_type_t type = parse_node_tree_t::statement_boolean_type(node);
                if ((type ==  parse_bool_and || type == parse_bool_or) && node_tree.statement_is_in_pipeline(node, false /* don't count first */))
                {
                    errored = append_syntax_error(&parse_errors, node, EXEC_ERR_MSG, (type ==  parse_bool_and) ? L"and" : L"or");
                }
            }
            else if (node.type == symbol_argument)
            {
                const wcstring arg_src = node.get_source(buff_src);
                res |= parse_util_detect_errors_in_argument(node, arg_src, &parse_errors);
            }
            else if (node.type == symbol_job)
            {
                if (node_tree.job_should_be_backgrounded(node))
                {
                    /* Disallow background in the following cases:
                    
                       foo & ; and bar
                       foo & ; or bar
                       if foo & ; end
                       while foo & ; end
                    */
                    const parse_node_t *job_parent = node_tree.get_parent(node);
                    assert(job_parent != NULL);
                    switch (job_parent->type)
                    {
                        case symbol_if_clause:
                        case symbol_while_header:
                        {
                            assert(node_tree.get_child(*job_parent, 1) == &node);
                            errored = append_syntax_error(&parse_errors, node, BACKGROUND_IN_CONDITIONAL_ERROR_MSG);
                            break;
                        }
                        
                        case symbol_job_list:
                        {
                            // This isn't very complete, e.g. we don't catch 'foo & ; not and bar'
                            assert(node_tree.get_child(*job_parent, 0) == &node);
                            const parse_node_t *next_job_list = node_tree.get_child(*job_parent, 1, symbol_job_list);
                            assert(next_job_list != NULL);
                            const parse_node_t *next_job = node_tree.next_node_in_node_list(*next_job_list, symbol_job, NULL);
                            if (next_job != NULL)
                            {
                                const parse_node_t *next_statement = node_tree.get_child(*next_job, 0, symbol_statement);
                                if (next_statement != NULL)
                                {
                                    const parse_node_t *spec_statement = node_tree.get_child(*next_statement, 0);
                                    if (spec_statement && spec_statement->type == symbol_boolean_statement)
                                    {
                                        switch (parse_node_tree_t::statement_boolean_type(*spec_statement))
                                        {
                                            // These are not allowed
                                            case parse_bool_and:
                                                errored = append_syntax_error(&parse_errors, *spec_statement, BOOL_AFTER_BACKGROUND_ERROR_MSG, L"and");
                                                break;
                                            case parse_bool_or:
                                                errored = append_syntax_error(&parse_errors, *spec_statement, BOOL_AFTER_BACKGROUND_ERROR_MSG, L"or");
                                                break;
                                            case parse_bool_not:
                                                // This one is OK
                                                break;
                                        }
                                    }
                                }
                            }
                            break;
                        }
                        
                        default:
                        break;
                    }
                }
            }
            else if (node.type == symbol_plain_statement)
            {
                // In a few places below, we want to know if we are in a pipeline
                const bool is_in_pipeline = node_tree.statement_is_in_pipeline(node, true /* count first */);

                // We need to know the decoration
                const enum parse_statement_decoration_t decoration = node_tree.decoration_for_plain_statement(node);

                // Check that we don't try to pipe through exec
                if (is_in_pipeline && decoration == parse_statement_decoration_exec)
                {
                    errored = append_syntax_error(&parse_errors, node, EXEC_ERR_MSG, L"exec");
                }

                wcstring command;
                if (node_tree.command_for_plain_statement(node, buff_src, &command))
                {
                    // Check that we can expand the command
                    if (! expand_one(command, EXPAND_SKIP_CMDSUBST | EXPAND_SKIP_VARIABLES | EXPAND_SKIP_JOBS, NULL))
                    {
                        // TODO: leverage the resulting errors
                        errored = append_syntax_error(&parse_errors, node, ILLEGAL_CMD_ERR_MSG, command.c_str());
                    }

                    // Check that pipes are sound
                    if (! errored && parser_is_pipe_forbidden(command) && is_in_pipeline)
                    {
                        errored = append_syntax_error(&parse_errors, node, EXEC_ERR_MSG, command.c_str());
                    }

                    // Check that we don't return from outside a function
                    // But we allow it if it's 'return --help'
                    if (! errored && command == L"return")
                    {
                        const parse_node_t *ancestor = &node;
                        bool found_function = false;
                        while (ancestor != NULL)
                        {
                            const parse_node_t *possible_function_header = node_tree.header_node_for_block_statement(*ancestor);
                            if (possible_function_header != NULL && possible_function_header->type == symbol_function_header)
                            {
                                found_function = true;
                                break;
                            }
                            ancestor = node_tree.get_parent(*ancestor);

                        }
                        if (! found_function && ! first_argument_is_help(node_tree, node, buff_src))
                        {
                            errored = append_syntax_error(&parse_errors, node, INVALID_RETURN_ERR_MSG);
                        }
                    }

                    // Check that we don't break or continue from outside a loop
                    if (! errored && (command == L"break" || command == L"continue"))
                    {
                        // Walk up until we hit a 'for' or 'while' loop. If we hit a function first, stop the search; we can't break an outer loop from inside a function.
                        // This is a little funny because we can't tell if it's a 'for' or 'while' loop from the ancestor alone; we need the header. That is, we hit a block_statement, and have to check its header.
                        bool found_loop = false, end_search = false;
                        const parse_node_t *ancestor = &node;
                        while (ancestor != NULL && ! end_search)
                        {
                            const parse_node_t *loop_or_function_header = node_tree.header_node_for_block_statement(*ancestor);
                            if (loop_or_function_header != NULL)
                            {
                                switch (loop_or_function_header->type)
                                {
                                    case symbol_while_header:
                                    case symbol_for_header:
                                        // this is a loop header, so we can break or continue
                                        found_loop = true;
                                        end_search = true;
                                        break;

                                    case symbol_function_header:
                                        // this is a function header, so we cannot break or continue. We stop our search here.
                                        found_loop = false;
                                        end_search = true;
                                        break;

                                    default:
                                        // most likely begin / end style block, which makes no difference
                                        break;
                                }
                            }
                            ancestor = node_tree.get_parent(*ancestor);
                        }

                        if (! found_loop && ! first_argument_is_help(node_tree, node, buff_src))
                        {
                            errored = append_syntax_error(&parse_errors, node, (command == L"break" ? INVALID_BREAK_ERR_MSG : INVALID_CONTINUE_ERR_MSG));
                        }
                    }

                    // Check that we don't do an invalid builtin (#1252)
                    if (! errored && decoration == parse_statement_decoration_builtin && ! builtin_exists(command))
                    {
                        errored = append_syntax_error(&parse_errors, node, UNKNOWN_BUILTIN_ERR_MSG, command.c_str());
                    }

                }
            }
        }
    }

    if (errored)
        res |= PARSER_TEST_ERROR;

    if (has_unclosed_block || has_unclosed_quote)
        res |= PARSER_TEST_INCOMPLETE;

    if (out_errors)
    {
        out_errors->swap(parse_errors);
    }

    return res;

}
