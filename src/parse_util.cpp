// Various mostly unrelated utility functions related to parsing, loading and evaluating fish code.
//
// This library can be seen as a 'toolbox' for functions that are used in many places in fish and
// that are somehow related to parsing the code.
#include "config.h"  // IWYU pragma: keep

#include <stdarg.h>
#include <stdlib.h>
#include <wchar.h>

#include <memory>
#include <string>
#include <type_traits>

#include "builtin.h"
#include "common.h"
#include "expand.h"
#include "fallback.h"  // IWYU pragma: keep
#include "future_feature_flags.h"
#include "parse_constants.h"
#include "parse_util.h"
#include "parser.h"
#include "tnode.h"
#include "tokenizer.h"
#include "util.h"
#include "wildcard.h"
#include "wutil.h"  // IWYU pragma: keep

/// Error message for improper use of the exec builtin.
#define EXEC_ERR_MSG _(L"The '%ls' command can not be used in a pipeline")

/// Error message for use of backgrounded commands before and/or.
#define BOOL_AFTER_BACKGROUND_ERROR_MSG \
    _(L"The '%ls' command can not be used immediately after a backgrounded job")

/// Error message for backgrounded commands as conditionals.
#define BACKGROUND_IN_CONDITIONAL_ERROR_MSG \
    _(L"Backgrounded commands can not be used as conditionals")

/// Error message for arguments to 'end'
#define END_ARG_ERR_MSG _(L"'end' does not take arguments. Did you forget a ';'?")

int parse_util_lineno(const wchar_t *str, size_t offset) {
    if (!str) return 0;

    int res = 1;
    for (size_t i = 0; i < offset && str[i] != L'\0'; i++) {
        if (str[i] == L'\n') {
            res++;
        }
    }
    return res;
}

int parse_util_get_line_from_offset(const wcstring &str, size_t pos) {
    const wchar_t *buff = str.c_str();
    int count = 0;
    for (size_t i = 0; i < pos; i++) {
        if (!buff[i]) {
            return -1;
        }

        if (buff[i] == L'\n') {
            count++;
        }
    }
    return count;
}

size_t parse_util_get_offset_from_line(const wcstring &str, int line) {
    const wchar_t *buff = str.c_str();
    size_t i;
    int count = 0;

    if (line < 0) return (size_t)-1;
    if (line == 0) return 0;

    for (i = 0;; i++) {
        if (!buff[i]) return (size_t)-1;

        if (buff[i] == L'\n') {
            count++;
            if (count == line) {
                return (i + 1) < str.size() ? i + 1 : i;
            }
        }
    }
}

size_t parse_util_get_offset(const wcstring &str, int line, long line_offset) {
    size_t off = parse_util_get_offset_from_line(str, line);
    size_t off2 = parse_util_get_offset_from_line(str, line + 1);

    if (off == (size_t)-1) return (size_t)-1;
    if (off2 == (size_t)-1) off2 = str.length() + 1;
    if (line_offset < 0) line_offset = 0;  //!OCLINT(parameter reassignment)

    if ((size_t)line_offset >= off2 - off - 1) {
        line_offset = off2 - off - 1;  //!OCLINT(parameter reassignment)
    }

    return off + line_offset;
}

static int parse_util_locate_brackets_of_type(const wchar_t *in, wchar_t **begin, wchar_t **end,
                                              bool allow_incomplete, wchar_t open_type,
                                              wchar_t close_type) {
    // open_type is typically ( or [, and close type is the corresponding value.
    wchar_t *pos;
    wchar_t prev = 0;
    int syntax_error = 0;
    int paran_count = 0;

    wchar_t *paran_begin = 0, *paran_end = 0;

    CHECK(in, 0);

    for (pos = const_cast<wchar_t *>(in); *pos; pos++) {
        if (prev != '\\') {
            if (wcschr(L"\'\"", *pos)) {
                wchar_t *q_end = quote_end(pos);
                if (q_end && *q_end) {
                    pos = q_end;
                } else {
                    break;
                }
            } else {
                if (*pos == open_type) {
                    if ((paran_count == 0) && (paran_begin == 0)) {
                        paran_begin = pos;
                    }

                    paran_count++;
                } else if (*pos == close_type) {
                    paran_count--;

                    if ((paran_count == 0) && (paran_end == 0)) {
                        paran_end = pos;
                        break;
                    }

                    if (paran_count < 0) {
                        syntax_error = 1;
                        break;
                    }
                }
            }
        }
        prev = *pos;
    }

    syntax_error |= (paran_count < 0);
    syntax_error |= ((paran_count > 0) && (!allow_incomplete));

    if (syntax_error) {
        return -1;
    }

    if (paran_begin == 0) {
        return 0;
    }

    if (begin) {
        *begin = paran_begin;
    }

    if (end) {
        *end = paran_count ? (wchar_t *)in + wcslen(in) : paran_end;
    }

    return 1;
}

int parse_util_locate_cmdsubst(const wchar_t *in, wchar_t **begin, wchar_t **end,
                               bool accept_incomplete) {
    return parse_util_locate_brackets_of_type(in, begin, end, accept_incomplete, L'(', L')');
}

int parse_util_locate_slice(const wchar_t *in, wchar_t **begin, wchar_t **end,
                            bool accept_incomplete) {
    return parse_util_locate_brackets_of_type(in, begin, end, accept_incomplete, L'[', L']');
}

static int parse_util_locate_brackets_range(const wcstring &str, size_t *inout_cursor_offset,
                                            wcstring *out_contents, size_t *out_start,
                                            size_t *out_end, bool accept_incomplete,
                                            wchar_t open_type, wchar_t close_type) {
    // Clear the return values.
    out_contents->clear();
    *out_start = 0;
    *out_end = str.size();

    // Nothing to do if the offset is at or past the end of the string.
    if (*inout_cursor_offset >= str.size()) return 0;

    // Defer to the wonky version.
    const wchar_t *const buff = str.c_str();
    const wchar_t *const valid_range_start = buff + *inout_cursor_offset,
                         *valid_range_end = buff + str.size();
    wchar_t *bracket_range_begin = NULL, *bracket_range_end = NULL;
    int ret = parse_util_locate_brackets_of_type(valid_range_start, &bracket_range_begin,
                                                 &bracket_range_end, accept_incomplete, open_type,
                                                 close_type);
    if (ret <= 0) {
        return ret;
    }

    // The command substitutions must not be NULL and must be in the valid pointer range, and
    // the end must be bigger than the beginning.
    assert(bracket_range_begin != NULL && bracket_range_begin >= valid_range_start &&
           bracket_range_begin <= valid_range_end);
    assert(bracket_range_end != NULL && bracket_range_end > bracket_range_begin &&
           bracket_range_end >= valid_range_start && bracket_range_end <= valid_range_end);

    // Assign the substring to the out_contents.
    const wchar_t *interior_begin = bracket_range_begin + 1;
    out_contents->assign(interior_begin, bracket_range_end - interior_begin);

    // Return the start and end.
    *out_start = bracket_range_begin - buff;
    *out_end = bracket_range_end - buff;

    // Update the inout_cursor_offset. Note this may cause it to exceed str.size(), though
    // overflow is not likely.
    *inout_cursor_offset = 1 + *out_end;
    return ret;
}

int parse_util_locate_cmdsubst_range(const wcstring &str, size_t *inout_cursor_offset,
                                     wcstring *out_contents, size_t *out_start, size_t *out_end,
                                     bool accept_incomplete) {
    return parse_util_locate_brackets_range(str, inout_cursor_offset, out_contents, out_start,
                                            out_end, accept_incomplete, L'(', L')');
}

void parse_util_cmdsubst_extent(const wchar_t *buff, size_t cursor_pos, const wchar_t **a,
                                const wchar_t **b) {
    const wchar_t *const cursor = buff + cursor_pos;

    CHECK(buff, );

    const size_t bufflen = wcslen(buff);
    assert(cursor_pos <= bufflen);

    // ap and bp are the beginning and end of the tightest command substitition found so far.
    const wchar_t *ap = buff, *bp = buff + bufflen;
    const wchar_t *pos = buff;
    for (;;) {
        wchar_t *begin = NULL, *end = NULL;
        if (parse_util_locate_cmdsubst(pos, &begin, &end, true) <= 0) {
            // No subshell found, all done.
            break;
        }
        // Interpret NULL to mean the end.
        if (end == NULL) {
            end = const_cast<wchar_t *>(buff) + bufflen;
        }

        if (begin < cursor && end >= cursor) {
            // This command substitution surrounds the cursor, so it's a tighter fit.
            begin++;
            ap = begin;
            bp = end;
            // pos is where to begin looking for the next one. But if we reached the end there's no
            // next one.
            if (begin >= end) break;
            pos = begin + 1;
        } else if (begin >= cursor) {
            // This command substitution starts at or after the cursor. Since it was the first
            // command substitution in the string, we're done.
            break;
        } else {
            // This command substitution ends before the cursor. Skip it.
            assert(end < cursor);
            pos = end + 1;
            assert(pos <= buff + bufflen);
        }
    }

    if (a != NULL) *a = ap;
    if (b != NULL) *b = bp;
}

/// Get the beginning and end of the job or process definition under the cursor.
static void job_or_process_extent(const wchar_t *buff, size_t cursor_pos, const wchar_t **a,
                                  const wchar_t **b, int process) {
    const wchar_t *begin, *end;
    wchar_t *buffcpy;
    int finished = 0;

    CHECK(buff, );

    if (a) *a = 0;
    if (b) *b = 0;
    parse_util_cmdsubst_extent(buff, cursor_pos, &begin, &end);
    if (!end || !begin) {
        return;
    }

    assert(cursor_pos >= (size_t)(begin - buff));
    const size_t pos = cursor_pos - (begin - buff);

    if (a) *a = begin;
    if (b) *b = end;
    buffcpy = wcsndup(begin, end - begin);
    assert(buffcpy != NULL);

    tokenizer_t tok(buffcpy, TOK_ACCEPT_UNFINISHED);
    tok_t token;
    while (tok.next(&token) && !finished) {
        size_t tok_begin = token.offset;

        switch (token.type) {
            case TOK_PIPE: {
                if (!process) {
                    break;
                }
            }
            /* FALLTHROUGH */
            case TOK_END:
            case TOK_BACKGROUND: {
                if (tok_begin >= pos) {
                    finished = 1;
                    if (b) *b = (wchar_t *)begin + tok_begin;
                } else {
                    if (a) *a = (wchar_t *)begin + tok_begin + 1;
                }
                break;
            }
            default: { break; }
        }
    }

    free(buffcpy);
}

void parse_util_process_extent(const wchar_t *buff, size_t pos, const wchar_t **a,
                               const wchar_t **b) {
    job_or_process_extent(buff, pos, a, b, 1);
}

void parse_util_job_extent(const wchar_t *buff, size_t pos, const wchar_t **a, const wchar_t **b) {
    job_or_process_extent(buff, pos, a, b, 0);
}

void parse_util_token_extent(const wchar_t *buff, size_t cursor_pos, const wchar_t **tok_begin,
                             const wchar_t **tok_end, const wchar_t **prev_begin,
                             const wchar_t **prev_end) {
    const wchar_t *a = NULL, *b = NULL, *pa = NULL, *pb = NULL;

    CHECK(buff, );

    const wchar_t *cmdsubst_begin, *cmdsubst_end;
    parse_util_cmdsubst_extent(buff, cursor_pos, &cmdsubst_begin, &cmdsubst_end);

    if (!cmdsubst_end || !cmdsubst_begin) {
        return;
    }

    // pos is equivalent to cursor_pos within the range of the command substitution {begin, end}.
    size_t offset_within_cmdsubst = cursor_pos - (cmdsubst_begin - buff);

    size_t bufflen = wcslen(buff);

    a = cmdsubst_begin + offset_within_cmdsubst;
    b = a;
    pa = cmdsubst_begin + offset_within_cmdsubst;
    pb = pa;

    assert(cmdsubst_begin >= buff);
    assert(cmdsubst_begin <= (buff + bufflen));
    assert(cmdsubst_end >= cmdsubst_begin);
    assert(cmdsubst_end <= (buff + bufflen));

    const wcstring buffcpy = wcstring(cmdsubst_begin, cmdsubst_end - cmdsubst_begin);

    tokenizer_t tok(buffcpy.c_str(), TOK_ACCEPT_UNFINISHED);
    tok_t token;
    while (tok.next(&token)) {
        size_t tok_begin = token.offset;
        size_t tok_end = tok_begin;

        // Calculate end of token.
        if (token.type == TOK_STRING) {
            tok_end += token.length;
        }

        // Cursor was before beginning of this token, means that the cursor is between two tokens,
        // so we set it to a zero element string and break.
        if (tok_begin > offset_within_cmdsubst) {
            a = b = cmdsubst_begin + offset_within_cmdsubst;
            break;
        }

        // If cursor is inside the token, this is the token we are looking for. If so, set a and b
        // and break.
        if (token.type == TOK_STRING && tok_end >= offset_within_cmdsubst) {
            a = cmdsubst_begin + token.offset;
            b = a + token.length;
            break;
        }

        // Remember previous string token.
        if (token.type == TOK_STRING) {
            pa = cmdsubst_begin + token.offset;
            pb = pa + token.length;
        }
    }

    if (tok_begin) *tok_begin = a;
    if (tok_end) *tok_end = b;
    if (prev_begin) *prev_begin = pa;
    if (prev_end) *prev_end = pb;

    assert(pa >= buff);
    assert(pa <= (buff + bufflen));
    assert(pb >= pa);
    assert(pb <= (buff + bufflen));
}

wcstring parse_util_unescape_wildcards(const wcstring &str) {
    wcstring result;
    result.reserve(str.size());
    bool unesc_qmark = !feature_test(features_t::qmark_noglob);

    const wchar_t *const cs = str.c_str();
    for (size_t i = 0; cs[i] != L'\0'; i++) {
        if (cs[i] == L'*') {
            result.push_back(ANY_STRING);
        } else if (cs[i] == L'?' && unesc_qmark) {
            result.push_back(ANY_CHAR);
        } else if (cs[i] == L'\\' && cs[i + 1] == L'*') {
            result.push_back(cs[i + 1]);
            i += 1;
        } else if (cs[i] == L'\\' && cs[i + 1] == L'?' && unesc_qmark) {
            result.push_back(cs[i + 1]);
            i += 1;
        } else if (cs[i] == L'\\' && cs[i + 1] == L'\\') {
            // Not a wildcard, but ensure the next iteration doesn't see this escaped backslash.
            result.append(L"\\\\");
            i += 1;
        } else {
            result.push_back(cs[i]);
        }
    }
    return result;
}

/// Find the outermost quoting style of current token. Returns 0 if token is not quoted.
static wchar_t get_quote(const wcstring &cmd_str, size_t len) {
    size_t i = 0;
    wchar_t res = 0;
    const wchar_t *const cmd = cmd_str.c_str();

    while (1) {
        if (!cmd[i]) break;

        if (cmd[i] == L'\\') {
            i++;
            if (!cmd[i]) break;
            i++;
        } else {
            if (cmd[i] == L'\'' || cmd[i] == L'\"') {
                const wchar_t *end = quote_end(&cmd[i]);
                // fwprintf( stderr, L"Jump %d\n",  end-cmd );
                if ((end == 0) || (!*end) || (end > cmd + len)) {
                    res = cmd[i];
                    break;
                }
                i = end - cmd + 1;
            } else
                i++;
        }
    }

    return res;
}

void parse_util_get_parameter_info(const wcstring &cmd, const size_t pos, wchar_t *quote,
                                   size_t *offset, enum token_type *out_type) {
    size_t prev_pos = 0;
    wchar_t last_quote = L'\0';

    tokenizer_t tok(cmd.c_str(), TOK_ACCEPT_UNFINISHED);
    tok_t token;
    while (tok.next(&token)) {
        if (token.offset > pos) break;

        if (token.type == TOK_STRING)
            last_quote = get_quote(tok.text_of(token), pos - token.offset);

        if (out_type != NULL) *out_type = token.type;

        prev_pos = token.offset;
    }

    wchar_t *cmd_tmp = wcsdup(cmd.c_str());
    cmd_tmp[pos] = 0;
    size_t cmdlen = pos;
    bool finished = cmdlen != 0;
    if (finished) {
        finished = (quote == NULL);
        if (finished && wcschr(L" \t\n\r", cmd_tmp[cmdlen - 1])) {
            finished = cmdlen > 1 && cmd_tmp[cmdlen - 2] == L'\\';
        }
    }

    if (quote) *quote = last_quote;

    if (offset != 0) {
        if (finished) {
            while ((cmd_tmp[prev_pos] != 0) && (wcschr(L";|", cmd_tmp[prev_pos]) != 0)) prev_pos++;
            *offset = prev_pos;
        } else {
            *offset = pos;
        }
    }

    free(cmd_tmp);
}

wcstring parse_util_escape_string_with_quote(const wcstring &cmd, wchar_t quote, bool no_tilde) {
    wcstring result;
    if (quote == L'\0') {
        escape_flags_t flags = ESCAPE_ALL | ESCAPE_NO_QUOTED | (no_tilde ? ESCAPE_NO_TILDE : 0);
        result = escape_string(cmd, flags);
    } else {
        // Here we are going to escape a string with quotes.
        // A few characters cannot be represented inside quotes, e.g. newlines. In that case,
        // terminate the quote and then re-enter it.
        result.reserve(cmd.size());
        for (wchar_t c : cmd) {
            switch (c) {
                case L'\n':
                    result.append({quote, L'\\', L'n', quote});
                    break;
                case L'\t':
                    result.append({quote, L'\\', L't', quote});
                    break;
                case L'\b':
                    result.append({quote, L'\\', L'b', quote});
                    break;
                case L'\r':
                    result.append({quote, L'\\', L'r', quote});
                    break;
                case L'\\':
                    result.append({L'\\', L'\\'});
                    break;
                default:
                    if (c == quote) result.push_back(L'\\');
                    result.push_back(c);
                    break;
            }
        }
    }
    return result;
}

/// We are given a parse tree, the index of a node within the tree, its indent, and a vector of
/// indents the same size as the original source string. Set the indent correspdonding to the node's
/// source range, if appropriate.
///
/// trailing_indent is the indent for nodes with unrealized source, i.e. if I type 'if false <ret>'
/// then we have an if node with an empty job list (without source) but we want the last line to be
/// indented anyways.
///
/// switch statements also indent.
///
/// max_visited_node_idx is the largest index we visited.
static void compute_indents_recursive(const parse_node_tree_t &tree, node_offset_t node_idx,
                                      int node_indent, parse_token_type_t parent_type,
                                      std::vector<int> *indents, int *trailing_indent,
                                      node_offset_t *max_visited_node_idx) {
    // Guard against incomplete trees.
    if (node_idx > tree.size()) return;

    // Update max_visited_node_idx.
    if (node_idx > *max_visited_node_idx) *max_visited_node_idx = node_idx;

    // We could implement this by utilizing the fish grammar. But there's an easy trick instead:
    // almost everything that wraps a job list should be indented by 1. So just find all of the job
    // lists. One exception is switch, which wraps a case_item_list instead of a job_list. The other
    // exception is job_list itself: a job_list is a job and a job_list, and we want that child list
    // to be indented the same as the parent. So just find all job_lists whose parent is not a
    // job_list, and increment their indent by 1. We also want to treat andor_job_list like
    // job_lists.
    const parse_node_t &node = tree.at(node_idx);
    const parse_token_type_t node_type = node.type;

    // Increment the indent if we are either a root job_list, or root case_item_list.
    const bool is_root_job_list = node_type != parent_type && (node_type == symbol_job_list ||
                                                               node_type == symbol_andor_job_list);
    const bool is_root_case_item_list =
        node_type == symbol_case_item_list && parent_type != symbol_case_item_list;
    if (is_root_job_list || is_root_case_item_list) {
        node_indent += 1;
    }

    // If we have source, store the trailing indent unconditionally. If we do not have source, store
    // the trailing indent only if ours is bigger; this prevents the trailing "run" of terminal job
    // lists from affecting the trailing indent. For example, code like this:
    //
    //   if foo
    //
    // will be parsed as this:
    //
    //   job_list
    //     job
    //        if_statement
    //          job [if]
    //          job_list [empty]
    //     job_list [empty]
    //
    // There's two "terminal" job lists, and we want the innermost one.
    //
    // Note we are relying on the fact that nodes are in the same order as the source, i.e. an
    // in-order traversal of the node tree also traverses the source from beginning to end.
    if (node.has_source() || node_indent > *trailing_indent) {
        *trailing_indent = node_indent;
    }

    // Store the indent into the indent array.
    if (node.source_start != SOURCE_OFFSET_INVALID && node.source_start < indents->size()) {
        if (node.has_source()) {
            // A normal non-empty node. Store the indent unconditionally.
            indents->at(node.source_start) = node_indent;
        } else {
            // An empty node. We have a source offset but no source length. This can come about when
            // a node legitimately empty:
            //
            //   while true; end
            //
            // The job_list inside the while loop is empty. It still has a source offset (at the end
            // of the while statement) but no source extent. We still need to capture that indent,
            // because there may be comments inside:
            //
            //      while true
            //         # loop forever
            //      end
            //
            // The 'loop forever' comment must be indented, by virtue of storing the indent.
            //
            // Now consider what happens if we remove the end:
            //
            //     while true
            //       # loop forever
            //
            // Now both the job_list and end_command are unmaterialized. However, we want the indent
            // to be of the job_list and not the end_command.  Therefore, we only store the indent
            // if it's bigger.
            if (node_indent > indents->at(node.source_start)) {
                indents->at(node.source_start) = node_indent;
            }
        }
    }

    // Recursive to all our children.
    for (node_offset_t idx = 0; idx < node.child_count; idx++) {
        // Note we pass our type to our child, which becomes its parent node type.
        compute_indents_recursive(tree, node.child_start + idx, node_indent, node_type, indents,
                                  trailing_indent, max_visited_node_idx);
    }
}

std::vector<int> parse_util_compute_indents(const wcstring &src) {
    // Make a vector the same size as the input string, which contains the indents. Initialize them
    // to -1.
    const size_t src_size = src.size();
    std::vector<int> indents(src_size, -1);

    // Parse the string. We pass continue_after_error to produce a forest; the trailing indent of
    // the last node we visited becomes the input indent of the next. I.e. in the case of 'switch
    // foo ; cas', we get an invalid parse tree (since 'cas' is not valid) but we indent it as if it
    // were a case item list.
    parse_node_tree_t tree;
    parse_tree_from_string(src, parse_flag_continue_after_error | parse_flag_include_comments |
                                    parse_flag_accept_incomplete_tokens,
                           &tree, NULL /* errors */);

    // Start indenting at the first node. If we have a parse error, we'll have to start indenting
    // from the top again.
    node_offset_t start_node_idx = 0;
    int last_trailing_indent = 0;

    while (start_node_idx < tree.size()) {
        // The indent that we'll get for the last line.
        int trailing_indent = 0;

        // Biggest offset we visited.
        node_offset_t max_visited_node_idx = 0;

        // Invoke the recursive version. As a hack, pass job_list for the 'parent' token type, which
        // will prevent the really-root job list from indenting.
        compute_indents_recursive(tree, start_node_idx, last_trailing_indent, symbol_job_list,
                                  &indents, &trailing_indent, &max_visited_node_idx);

        // We may have more to indent. The trailing indent becomes our current indent. Start at the
        // node after the last we visited.
        last_trailing_indent = trailing_indent;
        start_node_idx = max_visited_node_idx + 1;
    }

    // Handle comments. Each comment node has a parent (which is whatever the top of the symbol
    // stack was when the comment was encountered). So the source range of the comment has the same
    // indent as its parent.
    const size_t tree_size = tree.size();
    for (node_offset_t i = 0; i < tree_size; i++) {
        const parse_node_t &node = tree.at(i);
        if (node.type == parse_special_type_comment && node.has_source() &&
            node.parent < tree_size) {
            const parse_node_t &parent = tree.at(node.parent);
            if (parent.source_start != SOURCE_OFFSET_INVALID) {
                indents.at(node.source_start) = indents.at(parent.source_start);
            }
        }
    }

    // Now apply the indents. The indents array has -1 for places where the indent does not change,
    // so start at each value and extend it along the run of -1s.
    int last_indent = 0;
    for (size_t i = 0; i < src_size; i++) {
        int this_indent = indents.at(i);
        if (this_indent < 0) {
            indents.at(i) = last_indent;
        } else {
            // New indent level.
            last_indent = this_indent;
            // Make all whitespace before a token have the new level. This avoid using the wrong
            // indentation level if a new line starts with whitespace.
            size_t prev_char_idx = i;
            while (prev_char_idx--) {
                if (!wcschr(L" \n\t\r", src.at(prev_char_idx))) break;
                indents.at(prev_char_idx) = last_indent;
            }
        }
    }

    // Ensure trailing whitespace has the trailing indent. This makes sure a new line is correctly
    // indented even if it is empty.
    size_t suffix_idx = src_size;
    while (suffix_idx--) {
        if (!wcschr(L" \n\t\r", src.at(suffix_idx))) break;
        indents.at(suffix_idx) = last_trailing_indent;
    }

    return indents;
}

/// Append a syntax error to the given error list.
static bool append_syntax_error(parse_error_list_t *errors, size_t source_location,
                                const wchar_t *fmt, ...) {
    parse_error_t error;
    error.source_start = source_location;
    error.source_length = 0;
    error.code = parse_error_syntax;

    va_list va;
    va_start(va, fmt);
    error.text = vformat_string(fmt, va);
    va_end(va);

    errors->push_back(error);
    return true;
}

/// Returns 1 if the specified command is a builtin that may not be used in a pipeline.
static const wchar_t *const forbidden_pipe_commands[] = {L"exec", L"case", L"break", L"return",
                                                         L"continue"};
static int parser_is_pipe_forbidden(const wcstring &word) {
    return contains(forbidden_pipe_commands, word);
}

bool parse_util_argument_is_help(const wchar_t *s) {
    return wcscmp(L"-h", s) == 0 || wcscmp(L"--help", s) == 0;
}

/// Check if the first argument under the given node is --help.
static bool first_argument_is_help(tnode_t<grammar::plain_statement> statement,
                                   const wcstring &src) {
    bool is_help = false;
    auto arg_nodes = get_argument_nodes(statement.child<1>());
    if (!arg_nodes.empty()) {
        // Check the first argument only.
        wcstring first_arg_src = arg_nodes.front().get_source(src);
        is_help = parse_util_argument_is_help(first_arg_src.c_str());
    }
    return is_help;
}

/// If a var name or command is too long for error reporting, make it shorter.
static wcstring truncate_string(const wcstring &str) {
    const size_t max_len = 16;
    wcstring result(str, 0, max_len);
    if (str.size() > max_len) {
        // Truncate!
        if (ellipsis_char == L'\x2026') {
            result.at(max_len - 1) = ellipsis_char;
        } else {
            result.replace(max_len - 3, 3, L"...");
        }
    }
    return result;
}

/// Given a wide character immediately after a dollar sign, return the appropriate error message.
/// For example, if wc is @, then the variable name was $@ and we suggest $argv.
static const wchar_t *error_format_for_character(wchar_t wc) {
    switch (wc) {
        case L'?': {
            return ERROR_NOT_STATUS;
        }
        case L'#': {
            return ERROR_NOT_ARGV_COUNT;
        }
        case L'@': {
            return ERROR_NOT_ARGV_AT;
        }
        case L'*': {
            return ERROR_NOT_ARGV_STAR;
        }
        case L'$':
        case VARIABLE_EXPAND:
        case VARIABLE_EXPAND_SINGLE:
        case VARIABLE_EXPAND_EMPTY: {
            return ERROR_NOT_PID;
        }
        default: { return ERROR_BAD_VAR_CHAR1; }
    }
}

void parse_util_expand_variable_error(const wcstring &token, size_t global_token_pos,
                                      size_t dollar_pos, parse_error_list_t *errors) {
    // Note that dollar_pos is probably VARIABLE_EXPAND or VARIABLE_EXPAND_SINGLE, not a literal
    // dollar sign.
    assert(errors != NULL);
    assert(dollar_pos < token.size());
    const bool double_quotes = token.at(dollar_pos) == VARIABLE_EXPAND_SINGLE;
    const size_t start_error_count = errors->size();
    const size_t global_dollar_pos = global_token_pos + dollar_pos;
    const size_t global_after_dollar_pos = global_dollar_pos + 1;
    wchar_t char_after_dollar = dollar_pos + 1 >= token.size() ? 0 : token.at(dollar_pos + 1);

    switch (char_after_dollar) {
        case BRACE_BEGIN:
        case L'{': {
            // The BRACE_BEGIN is for unquoted, the { is for quoted. Anyways we have (possible
            // quoted) ${. See if we have a }, and the stuff in between is variable material. If so,
            // report a bracket error. Otherwise just complain about the ${.
            bool looks_like_variable = false;
            size_t closing_bracket =
                token.find(char_after_dollar == L'{' ? L'}' : wchar_t(BRACE_END), dollar_pos + 2);
            wcstring var_name;
            if (closing_bracket != wcstring::npos) {
                size_t var_start = dollar_pos + 2, var_end = closing_bracket;
                var_name = wcstring(token, var_start, var_end - var_start);
                looks_like_variable = valid_var_name(var_name);
            }
            if (looks_like_variable) {
                append_syntax_error(
                    errors, global_after_dollar_pos,
                    double_quotes ? ERROR_BRACKETED_VARIABLE_QUOTED1 : ERROR_BRACKETED_VARIABLE1,
                    truncate_string(var_name).c_str());
            } else {
                append_syntax_error(errors, global_after_dollar_pos, ERROR_BAD_VAR_CHAR1, L'{');
            }
            break;
        }
        case INTERNAL_SEPARATOR: {
            // e.g.: echo foo"$"baz
            // These are only ever quotes, not command substitutions. Command substitutions are
            // handled earlier.
            append_syntax_error(errors, global_dollar_pos, ERROR_NO_VAR_NAME);
            break;
        }
        case '(': {
            // e.g.: 'echo "foo$(bar)baz"
            // Try to determine what's in the parens.
            wcstring token_after_parens;
            wcstring paren_text;
            size_t open_parens = dollar_pos + 1, cmdsub_start = 0, cmdsub_end = 0;
            if (parse_util_locate_cmdsubst_range(token, &open_parens, &paren_text, &cmdsub_start,
                                                 &cmdsub_end, true) > 0) {
                token_after_parens = tok_first(paren_text);
            }

            // Make sure we always show something.
            if (token_after_parens.empty()) {
                token_after_parens = L"...";
            }

            append_syntax_error(errors, global_dollar_pos, ERROR_BAD_VAR_SUBCOMMAND1,
                                truncate_string(token_after_parens).c_str());
            break;
        }
        case L'\0': {
            append_syntax_error(errors, global_dollar_pos, ERROR_NO_VAR_NAME);
            break;
        }
        default: {
            wchar_t token_stop_char = char_after_dollar;
            // Unescape (see issue #50).
            if (token_stop_char == ANY_CHAR)
                token_stop_char = L'?';
            else if (token_stop_char == ANY_STRING || token_stop_char == ANY_STRING_RECURSIVE)
                token_stop_char = L'*';

            // Determine which error message to use. The format string may not consume all the
            // arguments we pass but that's harmless.
            const wchar_t *error_fmt = error_format_for_character(token_stop_char);

            append_syntax_error(errors, global_after_dollar_pos, error_fmt, token_stop_char);
            break;
        }
    }

    // We should have appended exactly one error.
    assert(errors->size() == start_error_count + 1);
}

/// Detect cases like $(abc). Given an arg like foo(bar), let arg_src be foo and cmdsubst_src be
/// bar. If arg ends with VARIABLE_EXPAND, then report an error.
static parser_test_error_bits_t detect_dollar_cmdsub_errors(size_t arg_src_offset,
                                                            const wcstring &arg_src,
                                                            const wcstring &cmdsubst_src,
                                                            parse_error_list_t *out_errors) {
    parser_test_error_bits_t result_bits = 0;
    wcstring unescaped_arg_src;

    if (!unescape_string(arg_src, &unescaped_arg_src, UNESCAPE_SPECIAL) ||
        unescaped_arg_src.empty()) {
        return result_bits;
    }

    wchar_t last = unescaped_arg_src.at(unescaped_arg_src.size() - 1);
    if (last == VARIABLE_EXPAND) {
        result_bits |= PARSER_TEST_ERROR;
        if (out_errors != NULL) {
            wcstring subcommand_first_token = tok_first(cmdsubst_src);
            if (subcommand_first_token.empty()) {
                // e.g. $(). Report somthing.
                subcommand_first_token = L"...";
            }
            append_syntax_error(
                out_errors,
                arg_src_offset + arg_src.size() - 1,  // global position of the dollar
                ERROR_BAD_VAR_SUBCOMMAND1, truncate_string(subcommand_first_token).c_str());
        }
    }

    return result_bits;
}

/// Test if this argument contains any errors. Detected errors include syntax errors in command
/// substitutions, improperly escaped characters and improper use of the variable expansion
/// operator.
parser_test_error_bits_t parse_util_detect_errors_in_argument(tnode_t<grammar::argument> node,
                                                              const wcstring &arg_src,
                                                              parse_error_list_t *out_errors) {
    assert(node.has_source() && "argument has no source");
    auto source_start = node.source_range()->start;
    int err = 0;
    wchar_t *paran_begin, *paran_end;
    int do_loop = 1;
    wcstring working_copy = arg_src;

    while (do_loop) {
        const wchar_t *working_copy_cstr = working_copy.c_str();
        switch (parse_util_locate_cmdsubst(working_copy_cstr, &paran_begin, &paran_end, false)) {
            case -1: {
                err = 1;
                if (out_errors) {
                    append_syntax_error(out_errors, source_start, L"Mismatched parenthesis");
                }
                return err;
            }
            case 0: {
                do_loop = 0;
                break;
            }
            case 1: {
                const wcstring subst(paran_begin + 1, paran_end);

                // Replace the command substitution with just INTERNAL_SEPARATOR.
                size_t cmd_sub_start = paran_begin - working_copy_cstr;
                size_t cmd_sub_len = paran_end + 1 - paran_begin;
                working_copy.replace(cmd_sub_start, cmd_sub_len, wcstring(1, INTERNAL_SEPARATOR));

                parse_error_list_t subst_errors;
                err |= parse_util_detect_errors(subst, &subst_errors,
                                                false /* do not accept incomplete */);

                // Our command substitution produced error offsets relative to its source. Tweak the
                // offsets of the errors in the command substitution to account for both its offset
                // within the string, and the offset of the node.
                size_t error_offset = cmd_sub_start + 1 + source_start;
                parse_error_offset_source_start(&subst_errors, error_offset);

                if (out_errors != NULL) {
                    out_errors->insert(out_errors->end(), subst_errors.begin(), subst_errors.end());

                    // Hackish. Take this opportunity to report $(...) errors. We do this because
                    // after we've replaced with internal separators, we can't distinguish between
                    // "" and (), and also we no longer have the source of the command substitution.
                    // As an optimization, this is only necessary if the last character is a $.
                    if (cmd_sub_start > 0 && working_copy.at(cmd_sub_start - 1) == L'$') {
                        err |= detect_dollar_cmdsub_errors(
                            source_start, working_copy.substr(0, cmd_sub_start), subst, out_errors);
                    }
                }
                break;
            }
            default: {
                DIE("unexpected parse_util_locate_cmdsubst() return value");
                break;
            }
        }
    }

    wcstring unesc;
    if (!unescape_string(working_copy, &unesc, UNESCAPE_SPECIAL)) {
        if (out_errors) {
            append_syntax_error(out_errors, source_start, L"Invalid token '%ls'",
                                working_copy.c_str());
        }
        return 1;
    }

    // Check for invalid variable expansions.
    const size_t unesc_size = unesc.size();
    for (size_t idx = 0; idx < unesc_size; idx++) {
        if (unesc.at(idx) != VARIABLE_EXPAND && unesc.at(idx) != VARIABLE_EXPAND_SINGLE) {
            continue;
        }

        wchar_t next_char = idx + 1 < unesc_size ? unesc.at(idx + 1) : L'\0';
        if (next_char != VARIABLE_EXPAND && next_char != VARIABLE_EXPAND_SINGLE &&
            !valid_var_name_char(next_char)) {
            err = 1;
            if (out_errors) {
                // We have something like $$$^....  Back up until we reach the first $.
                size_t first_dollar = idx;
                while (first_dollar > 0 && (unesc.at(first_dollar - 1) == VARIABLE_EXPAND ||
                                            unesc.at(first_dollar - 1) == VARIABLE_EXPAND_SINGLE)) {
                    first_dollar--;
                }
                parse_util_expand_variable_error(unesc, source_start, first_dollar, out_errors);
            }
        }
    }

    return err;
}

/// Given that the job given by node should be backgrounded, return true if we detect any errors.
static bool detect_errors_in_backgrounded_job(tnode_t<grammar::job> job,
                                              parse_error_list_t *parse_errors) {
    namespace g = grammar;
    auto source_range = job.source_range();
    if (!source_range) return false;

    bool errored = false;
    // Disallow background in the following cases:
    // foo & ; and bar
    // foo & ; or bar
    // if foo & ; end
    // while foo & ; end
    auto job_conj = job.try_get_parent<g::job_conjunction>();
    if (job_conj.try_get_parent<g::if_clause>()) {
        errored = append_syntax_error(parse_errors, source_range->start,
                                      BACKGROUND_IN_CONDITIONAL_ERROR_MSG);
    } else if (job_conj.try_get_parent<g::while_header>()) {
        errored = append_syntax_error(parse_errors, source_range->start,
                                      BACKGROUND_IN_CONDITIONAL_ERROR_MSG);
    } else if (auto jlist = job_conj.try_get_parent<g::job_list>()) {
        // This isn't very complete, e.g. we don't catch 'foo & ; not and bar'.
        // Fetch the job list and then advance it by one.
        auto first_jconj = jlist.next_in_list<g::job_conjunction>();
        assert(first_jconj == job.try_get_parent<g::job_conjunction>() &&
               "Expected first job to be the node we found");
        (void)first_jconj;

        // Try getting the next job's decorator.
        if (auto next_job_dec = jlist.next_in_list<g::job_decorator>()) {
            // The next job is indeed a boolean statement.
            parse_bool_statement_type_t bool_type = bool_statement_type(next_job_dec);
            if (bool_type == parse_bool_and) {
                errored = append_syntax_error(parse_errors, next_job_dec.source_range()->start,
                                              BOOL_AFTER_BACKGROUND_ERROR_MSG, L"and");
            } else if (bool_type == parse_bool_or) {
                errored = append_syntax_error(parse_errors, next_job_dec.source_range()->start,
                                              BOOL_AFTER_BACKGROUND_ERROR_MSG, L"or");
            }
        }
    }
    return errored;
}

static bool detect_errors_in_plain_statement(const wcstring &buff_src,
                                             const parse_node_tree_t &node_tree,
                                             tnode_t<grammar::plain_statement> pst,
                                             parse_error_list_t *parse_errors) {
    using namespace grammar;
    bool errored = false;
    auto source_start = pst.source_range()->start;

    // In a few places below, we want to know if we are in a pipeline.
    tnode_t<statement> st = pst.try_get_parent<decorated_statement>().try_get_parent<statement>();
    pipeline_position_t pipe_pos = get_pipeline_position(st);
    bool is_in_pipeline = (pipe_pos != pipeline_position_t::none);

    // We need to know the decoration.
    const enum parse_statement_decoration_t decoration = get_decoration(pst);

    // Check that we don't try to pipe through exec.
    if (is_in_pipeline && decoration == parse_statement_decoration_exec) {
        errored = append_syntax_error(parse_errors, source_start, EXEC_ERR_MSG, L"exec");
    }

    // This is a somewhat stale check that 'and' and 'or' are not in pipelines, except at the
    // beginning. We can't disallow them as commands entirely because we need to support 'and
    // --help', etc.
    if (pipe_pos == pipeline_position_t::subsequent) {
        // check if our command is 'and' or 'or'. This is very clumsy; we don't catch e.g. quoted
        // commands.
        wcstring command = pst.child<0>().get_source(buff_src);
        if (command == L"and" || command == L"or") {
            errored =
                append_syntax_error(parse_errors, source_start, EXEC_ERR_MSG, command.c_str());
        }
    }

    if (maybe_t<wcstring> unexp_command = command_for_plain_statement(pst, buff_src)) {
        wcstring command;
        // Check that we can expand the command.
        if (expand_to_command_and_args(*unexp_command, null_environment_t{}, &command, nullptr,
                                       parse_errors) == EXPAND_ERROR) {
            errored = true;
        }

        // Check that pipes are sound.
        if (!errored && parser_is_pipe_forbidden(command) && is_in_pipeline) {
            errored =
                append_syntax_error(parse_errors, source_start, EXEC_ERR_MSG, command.c_str());
        }

        // Check that we don't return from outside a function. But we allow it if it's
        // 'return --help'.
        if (!errored && command == L"return") {
            bool found_function = false;
            for (const parse_node_t *ancestor = pst.node(); ancestor != nullptr;
                 ancestor = node_tree.get_parent(*ancestor)) {
                auto fh = tnode_t<block_statement>::try_create(&node_tree, ancestor)
                              .child<0>()
                              .try_get_child<function_header, 0>();
                if (fh) {
                    found_function = true;
                    break;
                }
            }
            if (!found_function && !first_argument_is_help(pst, buff_src)) {
                errored = append_syntax_error(parse_errors, source_start, INVALID_RETURN_ERR_MSG);
            }
        }

        // Check that we don't break or continue from outside a loop.
        if (!errored && (command == L"break" || command == L"continue")) {
            // Walk up until we hit a 'for' or 'while' loop. If we hit a function first,
            // stop the search; we can't break an outer loop from inside a function.
            // This is a little funny because we can't tell if it's a 'for' or 'while'
            // loop from the ancestor alone; we need the header. That is, we hit a
            // block_statement, and have to check its header.
            bool found_loop = false;
            for (const parse_node_t *ancestor = pst.node(); ancestor != nullptr;
                 ancestor = node_tree.get_parent(*ancestor)) {
                tnode_t<block_header> bh =
                    tnode_t<block_statement>::try_create(&node_tree, ancestor).child<0>();
                if (bh.try_get_child<while_header, 0>() || bh.try_get_child<for_header, 0>()) {
                    // This is a loop header, so we can break or continue.
                    found_loop = true;
                    break;
                } else if (bh.try_get_child<function_header, 0>()) {
                    // This is a function header, so we cannot break or
                    // continue. We stop our search here.
                    found_loop = false;
                    break;
                }
            }

            if (!found_loop && !first_argument_is_help(pst, buff_src)) {
                errored = append_syntax_error(
                    parse_errors, source_start,
                    (command == L"break" ? INVALID_BREAK_ERR_MSG : INVALID_CONTINUE_ERR_MSG));
            }
        }

        // Check that we don't do an invalid builtin (issue #1252).
        if (!errored && decoration == parse_statement_decoration_builtin &&
            !builtin_exists(command)) {
            errored = append_syntax_error(parse_errors, source_start, UNKNOWN_BUILTIN_ERR_MSG,
                                          command.c_str());
        }
    }
    return errored;
}

parser_test_error_bits_t parse_util_detect_errors(const wcstring &buff_src,
                                                  parse_error_list_t *out_errors,
                                                  bool allow_incomplete,
                                                  parsed_source_ref_t *out_pstree) {
    namespace g = grammar;
    parse_node_tree_t node_tree;
    parse_error_list_t parse_errors;

    parser_test_error_bits_t res = 0;

    // Whether we encountered a parse error.
    bool errored = false;

    // Whether we encountered an unclosed block. We detect this via an 'end_command' block without
    // source.
    bool has_unclosed_block = false;

    // Whether we encounter a missing statement, i.e. a newline after a pipe. This is found by
    // detecting job_continuations that have source for pipes but not the statement.
    bool has_unclosed_pipe = false;

    // Whether there's an unclosed quote, and therefore unfinished. This is only set if
    // allow_incomplete is set.
    bool has_unclosed_quote = false;

    // Parse the input string into a parse tree. Some errors are detected here.
    bool parsed = parse_tree_from_string(
        buff_src, allow_incomplete ? parse_flag_leave_unterminated : parse_flag_none, &node_tree,
        &parse_errors);

    if (allow_incomplete) {
        size_t idx = parse_errors.size();
        while (idx--) {
            if (parse_errors.at(idx).code == parse_error_tokenizer_unterminated_quote) {
                // Remove this error, since we don't consider it a real error.
                has_unclosed_quote = true;
                parse_errors.erase(parse_errors.begin() + idx);
            }
        }
    }

    // Issue #1238: If the only error was unterminated quote, then consider this to have parsed
    // successfully. A better fix would be to have parse_tree_from_string return this information
    // directly (but it would be a shame to munge up its nice bool return).
    if (parse_errors.empty() && has_unclosed_quote) {
        parsed = true;
    }

    if (!parsed) {
        errored = true;
    }

    // has_unclosed_quote may only be set if allow_incomplete is true.
    assert(!has_unclosed_quote || allow_incomplete);

    // Expand all commands.
    // Verify 'or' and 'and' not used inside pipelines.
    // Verify pipes via parser_is_pipe_forbidden.
    // Verify return only within a function.
    // Verify no variable expansions.

    if (!errored) {
        for (const parse_node_t &node : node_tree) {
            if (node.type == symbol_end_command && !node.has_source()) {
                // An 'end' without source is an unclosed block.
                has_unclosed_block = true;
            } else if (node.type == symbol_statement && !node.has_source()) {
                // Check for a statement without source in a pipeline, i.e. unterminated pipeline.
                auto pipe_pos = get_pipeline_position({&node_tree, &node});
                if (pipe_pos != pipeline_position_t::none) {
                    has_unclosed_pipe = true;
                }
            } else if (node.type == symbol_argument) {
                tnode_t<g::argument> arg{&node_tree, &node};
                const wcstring arg_src = node.get_source(buff_src);
                res |= parse_util_detect_errors_in_argument(arg, arg_src, &parse_errors);
            } else if (node.type == symbol_job) {
                // Disallow background in the following cases:
                //
                // foo & ; and bar
                // foo & ; or bar
                // if foo & ; end
                // while foo & ; end
                // If it's not a background job, nothing to do.
                auto job = tnode_t<g::job>{&node_tree, &node};
                if (job_node_is_background(job)) {
                    errored |= detect_errors_in_backgrounded_job(job, &parse_errors);
                }
            } else if (node.type == symbol_arguments_or_redirections_list) {
                // verify no arguments to the end command of if, switch, begin (#986).
                auto list = tnode_t<g::arguments_or_redirections_list>{&node_tree, &node};
                if (list.try_get_parent<g::if_statement>() ||
                    list.try_get_parent<g::switch_statement>() ||
                    list.try_get_parent<g::block_statement>()) {
                    if (auto arg = list.next_in_list<g::argument>()) {
                        errored = append_syntax_error(&parse_errors, arg.source_range()->start,
                                                      END_ARG_ERR_MSG);
                    }
                }
            } else if (node.type == symbol_plain_statement) {
                tnode_t<grammar::plain_statement> pst{&node_tree, &node};
                errored |=
                    detect_errors_in_plain_statement(buff_src, node_tree, pst, &parse_errors);
            }
        }
    }

    if (errored) res |= PARSER_TEST_ERROR;

    if (has_unclosed_block || has_unclosed_quote || has_unclosed_pipe)
        res |= PARSER_TEST_INCOMPLETE;

    if (out_errors != NULL) {
        *out_errors = std::move(parse_errors);
    }

    if (out_pstree != NULL) {
        *out_pstree = std::make_shared<parsed_source_t>(buff_src, std::move(node_tree));
    }

    return res;
}
