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

/**
   Error message for improper use of the exec builtin
*/
#define EXEC_ERR_MSG _(L"The '%ls' command can not be used in a pipeline")

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

int parse_util_locate_cmdsubst_range(const wcstring &str, size_t *inout_cursor_offset, wcstring *out_contents, size_t *out_start, size_t *out_end, bool accept_incomplete)
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
    wchar_t *cmdsub_begin = NULL, *cmdsub_end = NULL;
    int ret = parse_util_locate_cmdsubst(valid_range_start, &cmdsub_begin, &cmdsub_end, accept_incomplete);
    if (ret > 0)
    {
        /* The command substitutions must not be NULL and must be in the valid pointer range, and the end must be bigger than the beginning */
        assert(cmdsub_begin != NULL && cmdsub_begin >= valid_range_start && cmdsub_begin <= valid_range_end);
        assert(cmdsub_end != NULL && cmdsub_end > cmdsub_begin && cmdsub_end >= valid_range_start && cmdsub_end <= valid_range_end);

        /* Assign the substring to the out_contents */
        const wchar_t *interior_begin = cmdsub_begin + 1;
        out_contents->assign(interior_begin, cmdsub_end - interior_begin);

        /* Return the start and end */
        *out_start = cmdsub_begin - buff;
        *out_end = cmdsub_end - buff;

        /* Update the inout_cursor_offset. Note this may cause it to exceed str.size(), though overflow is not likely */
        *inout_cursor_offset = 1 + *out_end;
    }
    return ret;
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

    /* We could implement this by utilizing the fish grammar. But there's an easy trick instead: almost everything that wraps a job list should be indented by 1. So just find all of the job lists. One exception is switch; the other exception is job_list itself: a job_list is a job and a job_list, and we want that child list to be indented the same as the parent. So just find all job_lists whose parent is not a job_list, and increment their indent by 1. */

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
    if (node.has_source())
    {
        assert(node.source_start < indents->size());
        indents->at(node.source_start) = node_indent;
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
    parse_tree_from_string(src, parse_flag_continue_after_error | parse_flag_accept_incomplete_tokens, &tree, NULL /* errors */);

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
        is_help = parser_t::is_help(first_arg_src.c_str(), 3);
    }
    return is_help;
}

parser_test_error_bits_t parse_util_detect_errors(const wcstring &buff_src, parse_error_list_t *out_errors)
{
    parse_node_tree_t node_tree;
    parse_error_list_t parse_errors;

    // Whether we encountered a parse error
    bool errored = false;

    // Whether we encountered an unclosed block
    // We detect this via an 'end_command' block without source
    bool has_unclosed_block = false;

    // Whether there's an unclosed quote, and therefore unfinished
    bool has_unclosed_quote = false;

    // Parse the input string into a parse tree
    // Some errors are detected here
    bool parsed = parse_tree_from_string(buff_src, parse_flag_leave_unterminated, &node_tree, &parse_errors);

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
    // #1238: If the only error was unterminated quote, then consider this to have parsed successfully. A better fix would be to have parse_tree_from_string return this information directly (but it would be a shame to munge up its nice bool return).
    if (parse_errors.empty() && has_unclosed_quote)
        parsed = true;

    if (! parsed)
    {
        errored = true;
    }

    // Expand all commands
    // Verify 'or' and 'and' not used inside pipelines
    // Verify pipes via parser_is_pipe_forbidden
    // Verify return only within a function

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
                // These numbers 0 and 1 correspond to productions for boolean_statement. This should be cleaned up.
                bool is_and = (node.production_idx == 0), is_or = (node.production_idx == 1);
                if ((is_and || is_or) && node_tree.statement_is_in_pipeline(node, false /* don't count first */))
                {
                    errored = append_syntax_error(&parse_errors, node, EXEC_ERR_MSG, is_and ? L"and" : L"or");
                }
            }
            else if (node.type == symbol_plain_statement)
            {
                wcstring command;
                if (node_tree.command_for_plain_statement(node, buff_src, &command))
                {
                    // Check that we can expand the command
                    if (! expand_one(command, EXPAND_SKIP_CMDSUBST | EXPAND_SKIP_VARIABLES | EXPAND_SKIP_JOBS))
                    {
                        errored = append_syntax_error(&parse_errors, node, ILLEGAL_CMD_ERR_MSG, command.c_str());
                    }

                    // Check that pipes are sound
                    if (! errored && parser_is_pipe_forbidden(command))
                    {
                        // forbidden commands cannot be in a pipeline at all
                        if (node_tree.statement_is_in_pipeline(node, true /* count first */))
                        {
                            errored = append_syntax_error(&parse_errors, node, EXEC_ERR_MSG, command.c_str());
                        }
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
                }
            }
        }
    }

    parser_test_error_bits_t res = 0;

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
