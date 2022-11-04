// SPDX-FileCopyrightText: © 2006 Axel Liljencrantz
// SPDX-FileCopyrightText: © 2009 fish-shell contributors
// SPDX-FileCopyrightText: © 2022 fish-shell contributors
//
// SPDX-License-Identifier: GPL-2.0-only

// Various mostly unrelated utility functions related to parsing, loading and evaluating fish code.
//
// This library can be seen as a 'toolbox' for functions that are used in many places in fish and
// that are somehow related to parsing the code.
#include "config.h"  // IWYU pragma: keep

#include "parse_util.h"

#include <stdarg.h>

#include <algorithm>
#include <cwchar>
#include <cwctype>
#include <memory>
#include <string>
#include <utility>

#include "ast.h"
#include "builtin.h"
#include "common.h"
#include "expand.h"
#include "fallback.h"  // IWYU pragma: keep
#include "future_feature_flags.h"
#include "operation_context.h"
#include "parse_constants.h"
#include "parse_tree.h"
#include "tokenizer.h"
#include "wcstringutil.h"
#include "wildcard.h"
#include "wutil.h"  // IWYU pragma: keep

/// Error message for use of backgrounded commands before and/or.
#define BOOL_AFTER_BACKGROUND_ERROR_MSG \
    _(L"The '%ls' command can not be used immediately after a backgrounded job")

/// Error message for backgrounded commands as conditionals.
#define BACKGROUND_IN_CONDITIONAL_ERROR_MSG \
    _(L"Backgrounded commands can not be used as conditionals")

/// Error message for arguments to 'end'
#define END_ARG_ERR_MSG _(L"'end' does not take arguments. Did you forget a ';'?")

/// Error message when 'time' is in a pipeline.
#define TIME_IN_PIPELINE_ERR_MSG _(L"The 'time' command may only be at the beginning of a pipeline")

/// Maximum length of a variable name to show in error reports before truncation
static constexpr int var_err_len = 16;

int parse_util_lineno(const wcstring &str, size_t offset) {
    // Return the line number of position offset, starting with 1.
    if (str.empty()) {
        return 1;
    }

    auto end = offset > str.length() ? str.end() : str.begin() + offset;
    return std::count(str.begin(), end, L'\n') + 1;
}

int parse_util_get_line_from_offset(const wcstring &str, size_t pos) {
    // Return the line pos is on, or -1 if it's after the end.
    if (pos > str.length()) return -1;
    return std::count(str.begin(), str.begin() + pos, L'\n');
}

size_t parse_util_get_offset_from_line(const wcstring &str, int line) {
    // Return the first position on line X, counting from 0.
    if (line < 0) return static_cast<size_t>(-1);
    if (line == 0) return 0;

    size_t pos = -1;
    int count = 0;
    while ((pos = str.find(L'\n', pos + 1)) != wcstring::npos) {
        count++;
        if (count == line) return pos + 1;
    }
    return static_cast<size_t>(-1);
}

size_t parse_util_get_offset(const wcstring &str, int line, long line_offset) {
    size_t off = parse_util_get_offset_from_line(str, line);
    size_t off2 = parse_util_get_offset_from_line(str, line + 1);

    if (off == static_cast<size_t>(-1)) return static_cast<size_t>(-1);
    if (off2 == static_cast<size_t>(-1)) off2 = str.length() + 1;
    if (line_offset < 0) line_offset = 0;  //!OCLINT(parameter reassignment)

    if (static_cast<size_t>(line_offset) >= off2 - off - 1) {
        line_offset = off2 - off - 1;  //!OCLINT(parameter reassignment)
    }

    return off + line_offset;
}

static int parse_util_locate_cmdsub(const wchar_t *in, const wchar_t **begin, const wchar_t **end,
                                    bool allow_incomplete, bool *inout_is_quoted,
                                    bool *out_has_dollar) {
    bool escaped = false;
    bool is_token_begin = true;
    bool syntax_error = false;
    int paran_count = 0;
    std::vector<int> quoted_cmdsubs;

    const wchar_t *paran_begin = nullptr, *paran_end = nullptr;

    assert(in && "null parameter");

    const wchar_t *pos = in;
    const wchar_t *last_dollar = nullptr;
    auto process_opening_quote = [&](wchar_t quote) -> bool /* ok */ {
        const wchar_t *q_end = quote_end(pos, quote);
        if (!q_end) return false;
        if (*q_end == L'$') {
            last_dollar = q_end;
            quoted_cmdsubs.push_back(paran_count);
        }
        // We want to report whether the outermost command substitution between
        // paran_begin..paran_end is quoted.
        if (paran_count == 0 && inout_is_quoted) {
            *inout_is_quoted = *q_end == L'$';
        }
        pos = q_end;
        return true;
    };

    if (inout_is_quoted && *inout_is_quoted && *pos) {
        if (!process_opening_quote(L'"')) pos += std::wcslen(pos);
    }

    for (; *pos; pos++) {
        if (!escaped) {
            if (*pos == L'\'' || *pos == L'"') {
                if (!process_opening_quote(*pos)) break;
            } else if (*pos == L'\\') {
                escaped = true;
            } else if (*pos == L'#' && is_token_begin) {
                pos = comment_end(pos) - 1;
            } else if (*pos == L'$') {
                last_dollar = pos;
            } else {
                if (*pos == L'(') {
                    if ((paran_count == 0) && (paran_begin == nullptr)) {
                        paran_begin = pos;
                        if (out_has_dollar) {
                            *out_has_dollar = last_dollar == pos - 1;
                        }
                    }

                    paran_count++;
                } else if (*pos == L')') {
                    paran_count--;

                    if ((paran_count == 0) && (paran_end == nullptr)) {
                        paran_end = pos;
                        break;
                    }

                    if (paran_count < 0) {
                        syntax_error = true;
                        break;
                    }

                    // Check if the ) did complete a quoted command substitution.
                    if (!quoted_cmdsubs.empty() && quoted_cmdsubs.back() == paran_count) {
                        quoted_cmdsubs.pop_back();
                        // Quoted command substitutions temporarily close double quotes.
                        // In "foo$(bar)baz$(qux)"
                        // We are here ^
                        // After the ) in a quoted command substitution, we need to act as if
                        // there was an invisible double quote.
                        const wchar_t *q_end = quote_end(pos, L'"');
                        if (q_end && *q_end) {  // Found a valid closing quote.
                            // Stop at $(qux), which is another quoted command substitution.
                            if (*q_end == L'$') quoted_cmdsubs.push_back(paran_count);
                            pos = q_end;
                        } else {
                            break;
                        }
                    }
                }
            }
            is_token_begin = is_token_delimiter(pos[0], pos[1]);
        } else {
            escaped = false;
            is_token_begin = false;
        }
    }

    syntax_error |= (paran_count < 0);
    syntax_error |= ((paran_count > 0) && (!allow_incomplete));

    if (syntax_error) {
        return -1;
    }

    if (paran_begin == nullptr) {
        return 0;
    }

    if (begin) {
        *begin = paran_begin;
    }

    if (end) {
        *end = paran_count ? in + std::wcslen(in) : paran_end;
    }

    return 1;
}

long parse_util_slice_length(const wchar_t *in) {
    assert(in && "null parameter");
    const wchar_t openc = L'[';
    const wchar_t closec = L']';
    bool escaped = false;

    // Check for initial opening [
    if (*in != openc) return 0;
    int bracket_count = 1;

    assert(in && "null parameter");
    for (const wchar_t *pos = in + 1; *pos; pos++) {
        if (!escaped) {
            if (*pos == L'\'' || *pos == L'"') {
                const wchar_t *q_end = quote_end(pos, *pos);
                if (q_end && *q_end) {
                    pos = q_end;
                } else {
                    break;
                }
            } else {
                if (*pos == openc) {
                    bracket_count++;
                } else if (*pos == closec) {
                    bracket_count--;
                    if (bracket_count == 0) {
                        // pos points at the closing ], so add 1.
                        return pos - in + 1;
                    }
                }
            }
        }
        if (*pos == '\\') {
            escaped = !escaped;
        } else {
            escaped = false;
        }
    }
    assert(bracket_count > 0 && "Should have unclosed brackets");
    return -1;
}

int parse_util_locate_cmdsubst_range(const wcstring &str, size_t *inout_cursor_offset,
                                     wcstring *out_contents, size_t *out_start, size_t *out_end,
                                     bool accept_incomplete, bool *inout_is_quoted,
                                     bool *out_has_dollar) {
    // Clear the return values.
    if (out_contents != nullptr) out_contents->clear();
    *out_start = 0;
    *out_end = str.size();

    // Nothing to do if the offset is at or past the end of the string.
    if (*inout_cursor_offset >= str.size()) return 0;

    // Defer to the wonky version.
    const wchar_t *const buff = str.c_str();
    const wchar_t *const valid_range_start = buff + *inout_cursor_offset,
                         *valid_range_end = buff + str.size();
    const wchar_t *bracket_range_begin = nullptr;
    const wchar_t *bracket_range_end = nullptr;

    int ret = parse_util_locate_cmdsub(valid_range_start, &bracket_range_begin, &bracket_range_end,
                                       accept_incomplete, inout_is_quoted, out_has_dollar);
    if (ret <= 0) {
        return ret;
    }

    // The command substitutions must not be NULL and must be in the valid pointer range, and
    // the end must be bigger than the beginning.
    assert(bracket_range_begin != nullptr && bracket_range_begin >= valid_range_start &&
           bracket_range_begin <= valid_range_end);
    assert(bracket_range_end != nullptr && bracket_range_end > bracket_range_begin &&
           bracket_range_end >= valid_range_start && bracket_range_end <= valid_range_end);

    // Assign the substring to the out_contents.
    const wchar_t *interior_begin = bracket_range_begin + 1;
    if (out_contents != nullptr) {
        out_contents->assign(interior_begin, bracket_range_end - interior_begin);
    }

    // Return the start and end.
    *out_start = bracket_range_begin - buff;
    *out_end = bracket_range_end - buff;

    // Update the inout_cursor_offset. Note this may cause it to exceed str.size(), though
    // overflow is not likely.
    *inout_cursor_offset = 1 + *out_end;

    return ret;
}

void parse_util_cmdsubst_extent(const wchar_t *buff, size_t cursor_pos, const wchar_t **a,
                                const wchar_t **b) {
    assert(buff && "Null buffer");
    const wchar_t *const cursor = buff + cursor_pos;

    const size_t bufflen = std::wcslen(buff);
    assert(cursor_pos <= bufflen);

    // ap and bp are the beginning and end of the tightest command substitution found so far.
    const wchar_t *ap = buff, *bp = buff + bufflen;
    const wchar_t *pos = buff;
    for (;;) {
        const wchar_t *begin = nullptr, *end = nullptr;
        if (parse_util_locate_cmdsub(pos, &begin, &end, true, nullptr, nullptr) <= 0) {
            // No subshell found, all done.
            break;
        }
        // Interpret NULL to mean the end.
        if (end == nullptr) {
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

    if (a != nullptr) *a = ap;
    if (b != nullptr) *b = bp;
}

/// Get the beginning and end of the job or process definition under the cursor.
static void job_or_process_extent(bool process, const wchar_t *buff, size_t cursor_pos,
                                  const wchar_t **a, const wchar_t **b,
                                  std::vector<tok_t> *tokens) {
    assert(buff && "Null buffer");
    const wchar_t *begin = nullptr, *end = nullptr;
    int finished = 0;

    if (a) *a = nullptr;
    if (b) *b = nullptr;
    parse_util_cmdsubst_extent(buff, cursor_pos, &begin, &end);
    if (!end || !begin) {
        return;
    }

    assert(cursor_pos >= static_cast<size_t>(begin - buff));
    const size_t pos = cursor_pos - (begin - buff);

    if (a) *a = begin;
    if (b) *b = end;

    const wcstring buffcpy(begin, end);
    tokenizer_t tok(buffcpy.c_str(), TOK_ACCEPT_UNFINISHED);
    maybe_t<tok_t> token{};
    while ((token = tok.next()) && !finished) {
        size_t tok_begin = token->offset;

        switch (token->type) {
            case token_type_t::pipe: {
                if (!process) {
                    break;
                }
                __fallthrough__
            }
            case token_type_t::end:
            case token_type_t::background:
            case token_type_t::andand:
            case token_type_t::oror:
            case token_type_t::comment: {
                if (tok_begin >= pos) {
                    finished = 1;
                    if (b) *b = const_cast<wchar_t *>(begin) + tok_begin;
                } else {
                    // Statement at cursor might start after this token.
                    if (a) *a = const_cast<wchar_t *>(begin) + tok_begin + token->length;
                    if (tokens) tokens->clear();
                }
                continue;  // Do not add this to tokens
            }
            default: {
                break;
            }
        }
        if (tokens) tokens->push_back(*token);
    }
}

void parse_util_process_extent(const wchar_t *buff, size_t pos, const wchar_t **a,
                               const wchar_t **b, std::vector<tok_t> *tokens) {
    job_or_process_extent(true, buff, pos, a, b, tokens);
}

void parse_util_job_extent(const wchar_t *buff, size_t pos, const wchar_t **a, const wchar_t **b) {
    job_or_process_extent(false, buff, pos, a, b, nullptr);
}

void parse_util_token_extent(const wchar_t *buff, size_t cursor_pos, const wchar_t **tok_begin,
                             const wchar_t **tok_end, const wchar_t **prev_begin,
                             const wchar_t **prev_end) {
    assert(buff && "Null buffer");
    const wchar_t *a = nullptr, *b = nullptr, *pa = nullptr, *pb = nullptr;

    const wchar_t *cmdsubst_begin, *cmdsubst_end;
    parse_util_cmdsubst_extent(buff, cursor_pos, &cmdsubst_begin, &cmdsubst_end);

    if (!cmdsubst_end || !cmdsubst_begin) {
        return;
    }

    // pos is equivalent to cursor_pos within the range of the command substitution {begin, end}.
    size_t offset_within_cmdsubst = cursor_pos - (cmdsubst_begin - buff);

    size_t bufflen = std::wcslen(buff);

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
    while (maybe_t<tok_t> token = tok.next()) {
        size_t tok_begin = token->offset;
        size_t tok_end = tok_begin;

        // Calculate end of token.
        if (token->type == token_type_t::string) {
            tok_end += token->length;
        }

        // Cursor was before beginning of this token, means that the cursor is between two tokens,
        // so we set it to a zero element string and break.
        if (tok_begin > offset_within_cmdsubst) {
            a = b = cmdsubst_begin + offset_within_cmdsubst;
            break;
        }

        // If cursor is inside the token, this is the token we are looking for. If so, set a and b
        // and break.
        if (token->type == token_type_t::string && tok_end >= offset_within_cmdsubst) {
            a = cmdsubst_begin + token->offset;
            b = a + token->length;
            break;
        }

        // Remember previous string token.
        if (token->type == token_type_t::string) {
            pa = cmdsubst_begin + token->offset;
            pb = pa + token->length;
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
    const wchar_t *cmd = cmd_str.c_str();

    while (true) {
        if (!cmd[i]) break;

        if (cmd[i] == L'\\') {
            i++;
            if (!cmd[i]) break;
            i++;
        } else {
            if (cmd[i] == L'\'' || cmd[i] == L'\"') {
                const wchar_t *end = quote_end(&cmd[i], cmd[i]);
                if ((end == nullptr) || (!*end) || (end > cmd + len)) {
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

wchar_t parse_util_get_quote_type(const wcstring &cmd, size_t pos) {
    tokenizer_t tok(cmd.c_str(), TOK_ACCEPT_UNFINISHED);
    while (auto token = tok.next()) {
        if (token->type == token_type_t::string &&
            token->location_in_or_at_end_of_source_range(pos)) {
            return get_quote(tok.text_of(*token), pos - token->offset);
        }
    }
    return L'\0';
}

wcstring parse_util_escape_string_with_quote(const wcstring &cmd, wchar_t quote, bool no_tilde) {
    wcstring result;
    if (quote == L'\0') {
        escape_flags_t flags = ESCAPE_NO_QUOTED | (no_tilde ? ESCAPE_NO_TILDE : 0);
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
                case L'$':
                    if (quote == L'"') result.push_back(L'\\');
                    result.push_back(L'$');
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

std::vector<int> parse_util_compute_indents(const wcstring &src) {
    // Make a vector the same size as the input string, which contains the indents. Initialize them
    // to 0.
    const size_t src_size = src.size();
    std::vector<int> indents(src_size, 0);

    // Simple trick: if our source does not contain a newline, then all indents are 0.
    if (src.find('\n') == wcstring::npos) {
        return indents;
    }

    // Parse the string. We pass continue_after_error to produce a forest; the trailing indent of
    // the last node we visited becomes the input indent of the next. I.e. in the case of 'switch
    // foo ; cas', we get an invalid parse tree (since 'cas' is not valid) but we indent it as if it
    // were a case item list.
    using namespace ast;
    auto ast =
        ast_t::parse(src, parse_flag_continue_after_error | parse_flag_include_comments |
                              parse_flag_accept_incomplete_tokens | parse_flag_leave_unterminated);

    // Visit all of our nodes. When we get a job_list or case_item_list, increment indent while
    // visiting its children.
    struct indent_visitor_t {
        indent_visitor_t(const wcstring &src, std::vector<int> &indents)
            : src(src), indents(indents) {}

        void visit(const node_t &node) {
            int inc = 0;
            int dec = 0;
            switch (node.type) {
                case type_t::job_list:
                case type_t::andor_job_list:
                    // Job lists are never unwound.
                    inc = 1;
                    dec = 1;
                    break;

                // Increment indents for conditions in headers (#1665).
                case type_t::job_conjunction:
                    if (node.parent->type == type_t::while_header ||
                        node.parent->type == type_t::if_clause) {
                        inc = 1;
                        dec = 1;
                    }
                    break;

                // Increment indents for job_continuation_t if it contains a newline.
                // This is a bit of a hack - it indents cases like:
                //    cmd1 |
                //    ....cmd2
                // but avoids "double indenting" if there's no newline:
                //   cmd1 | while cmd2
                //   ....cmd3
                //   end
                // See #7252.
                case type_t::job_continuation:
                    if (has_newline(node.as<job_continuation_t>()->newlines)) {
                        inc = 1;
                        dec = 1;
                    }
                    break;

                // Likewise for && and ||.
                case type_t::job_conjunction_continuation:
                    if (has_newline(node.as<job_conjunction_continuation_t>()->newlines)) {
                        inc = 1;
                        dec = 1;
                    }
                    break;

                case type_t::case_item_list:
                    // Here's a hack. Consider:
                    // switch abc
                    //    cas
                    //
                    // fish will see that 'cas' is not valid inside a switch statement because it is
                    // not "case". It will then unwind back to the top level job list, producing a
                    // parse tree like:
                    //
                    //   job_list
                    //      switch_job
                    //         <err>
                    //      normal_job
                    //         cas
                    //
                    // And so we will think that the 'cas' job is at the same level as the switch.
                    // To address this, if we see that the switch statement was not closed, do not
                    // decrement the indent afterwards.
                    inc = 1;
                    dec = node.parent->as<switch_statement_t>()->end.unsourced ? 0 : 1;
                    break;
                case type_t::token_base: {
                    auto tok = node.as<token_base_t>();
                    if (node.parent->type == type_t::begin_header &&
                        tok->type == parse_token_type_t::end) {
                        // The newline after "begin" is optional, so it is part of the header.
                        // The header is not in the indented block, so indent the newline here.
                        if (node.source(src) == L"\n") {
                            inc = 1;
                            dec = 1;
                        }
                    }
                    break;
                }
                default:
                    break;
            }

            auto range = node.source_range();
            if (range.length > 0 && node.category == category_t::leaf) {
                record_line_continuations_until(range.start);
                std::fill(indents.begin() + last_leaf_end, indents.begin() + range.start,
                          last_indent);
            }

            indent += inc;

            // If we increased the indentation, apply it to the remainder of the string, even if the
            // list is empty. For example (where _ represents the cursor):
            //
            //    if foo
            //       _
            //
            // we want to indent the newline.
            if (inc) {
                last_indent = indent;
            }

            // If this is a leaf node, apply the current indentation.
            if (node.category == category_t::leaf && range.length > 0) {
                std::fill(indents.begin() + range.start, indents.begin() + range.end(), indent);
                last_leaf_end = range.start + range.length;
                last_indent = indent;
            }

            node_visitor(*this).accept_children_of(&node);
            indent -= dec;
        }

        /// \return whether a maybe_newlines node contains at least one newline.
        bool has_newline(const maybe_newlines_t &nls) const {
            return nls.source(src).find(L'\n') != wcstring::npos;
        }

        void record_line_continuations_until(size_t offset) {
            wcstring gap_text = src.substr(last_leaf_end, offset - last_leaf_end);
            size_t escaped_nl = gap_text.find(L"\\\n");
            if (escaped_nl == wcstring::npos) return;
            auto line_end = gap_text.begin() + escaped_nl;
            if (std::find(gap_text.begin(), line_end, L'#') != line_end) return;
            auto end = src.begin() + offset;
            auto newline = src.begin() + last_leaf_end + escaped_nl + 1;
            // The gap text might contain multiple newlines if there are multiple lines that
            // don't contain an AST node, for example, comment lines, or lines containing only
            // the escaped newline.
            do {
                line_continuations.push_back(newline - src.begin());
                newline = std::find(newline + 1, end, L'\n');
            } while (newline != end);
        }

        // The one-past-the-last index of the most recently encountered leaf node.
        // We use this to populate the indents even if there's no tokens in the range.
        size_t last_leaf_end{0};

        // The last indent which we assigned.
        int last_indent{-1};

        // The source we are indenting.
        const wcstring &src;

        // List of indents, which we populate.
        std::vector<int> &indents;

        // Initialize our starting indent to -1, as our top-level node is a job list which
        // will immediately increment it.
        int indent{-1};

        // List of locations of escaped newline characters.
        std::vector<size_t> line_continuations;
    };

    indent_visitor_t iv(src, indents);
    node_visitor(iv).accept(ast.top());
    iv.record_line_continuations_until(indents.size());
    std::fill(indents.begin() + iv.last_leaf_end, indents.end(), iv.last_indent);

    // All newlines now get the *next* indent.
    // For example, in this code:
    //    if true
    //       stuff
    // the newline "belongs" to the if statement as it ends its job.
    // But when rendered, it visually belongs to the job list.

    size_t idx = src_size;
    int next_indent = iv.last_indent;
    while (idx--) {
        if (src.at(idx) == L'\n') {
            bool empty_middle_line = idx + 1 < src_size && src.at(idx + 1) == L'\n';
            if (!empty_middle_line) {
                indents.at(idx) = next_indent;
            }
        } else {
            next_indent = indents.at(idx);
        }
    }
    // Add an extra level of indentation to continuation lines.
    for (size_t idx : iv.line_continuations) {
        do {
            indents.at(idx)++;
        } while (++idx < src_size && src.at(idx) != L'\n');
    }

    return indents;
}

/// Append a syntax error to the given error list.
static bool append_syntax_error(parse_error_list_t *errors, size_t source_location,
                                size_t source_length, const wchar_t *fmt, ...) {
    if (!errors) return true;
    parse_error_t error;
    error.source_start = source_location;
    error.source_length = source_length;
    error.code = parse_error_syntax;

    va_list va;
    va_start(va, fmt);
    error.text = vformat_string(fmt, va);
    va_end(va);

    errors->push_back(std::move(error));
    return true;
}

/// Returns true if the specified command is a builtin that may not be used in a pipeline.
static const wchar_t *const forbidden_pipe_commands[] = {L"exec", L"case", L"break", L"return",
                                                         L"continue"};
static bool parser_is_pipe_forbidden(const wcstring &word) {
    return contains(forbidden_pipe_commands, word);
}

bool parse_util_argument_is_help(const wcstring &s) { return s == L"-h" || s == L"--help"; }

// \return a pointer to the first argument node of an argument_or_redirection_list_t, or nullptr if
// there are no arguments.
static const ast::argument_t *get_first_arg(const ast::argument_or_redirection_list_t &list) {
    for (const ast::argument_or_redirection_t &v : list) {
        if (v.is_argument()) return &v.argument();
    }
    return nullptr;
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
        default: {
            return ERROR_BAD_VAR_CHAR1;
        }
    }
}

void parse_util_expand_variable_error(const wcstring &token, size_t global_token_pos,
                                      size_t dollar_pos, parse_error_list_t *errors) {
    // Note that dollar_pos is probably VARIABLE_EXPAND or VARIABLE_EXPAND_SINGLE, not a literal
    // dollar sign.
    assert(errors != nullptr);
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
                    errors, global_after_dollar_pos, 1,
                    double_quotes ? ERROR_BRACKETED_VARIABLE_QUOTED1 : ERROR_BRACKETED_VARIABLE1,
                    truncate(var_name, var_err_len).c_str());
            } else {
                append_syntax_error(errors, global_after_dollar_pos, 1, ERROR_BAD_VAR_CHAR1, L'{');
            }
            break;
        }
        case INTERNAL_SEPARATOR: {
            // e.g.: echo foo"$"baz
            // These are only ever quotes, not command substitutions. Command substitutions are
            // handled earlier.
            append_syntax_error(errors, global_dollar_pos, 1, ERROR_NO_VAR_NAME);
            break;
        }
        case L'\0': {
            append_syntax_error(errors, global_dollar_pos, 1, ERROR_NO_VAR_NAME);
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

            append_syntax_error(errors, global_after_dollar_pos, 1, error_fmt, token_stop_char);
            break;
        }
    }

    // We should have appended exactly one error.
    assert(errors->size() == start_error_count + 1);
}

/// Test if this argument contains any errors. Detected errors include syntax errors in command
/// substitutions, improperly escaped characters and improper use of the variable expansion
/// operator.
parser_test_error_bits_t parse_util_detect_errors_in_argument(const ast::argument_t &arg,
                                                              const wcstring &arg_src,
                                                              parse_error_list_t *out_errors) {
    maybe_t<source_range_t> source_range = arg.try_source_range();
    if (!source_range.has_value()) return 0;

    size_t source_start = source_range->start;
    parser_test_error_bits_t err = 0;

    auto check_subtoken = [&arg_src, &out_errors, source_start](size_t begin, size_t end) -> int {
        wcstring unesc;
        if (!unescape_string(arg_src.c_str() + begin, end - begin, &unesc, UNESCAPE_SPECIAL)) {
            if (out_errors) {
                const wchar_t *fmt = L"Invalid token '%ls'";
                if (arg_src.length() == 2 && arg_src[0] == L'\\' &&
                        (arg_src[1] == L'c' || towlower(arg_src[1]) == L'u'
                        || towlower(arg_src[1]) == L'x')) {
                    fmt = L"Incomplete escape sequence '%ls'";
                }

                append_syntax_error(out_errors, source_start + begin, end - begin,
                                    fmt, arg_src.c_str());
            }
            return 1;
        }

        parser_test_error_bits_t err = 0;
        // Check for invalid variable expansions.
        const size_t unesc_size = unesc.size();
        for (size_t idx = 0; idx < unesc_size; idx++) {
            if (unesc.at(idx) != VARIABLE_EXPAND && unesc.at(idx) != VARIABLE_EXPAND_SINGLE) {
                continue;
            }

            wchar_t next_char = idx + 1 < unesc_size ? unesc.at(idx + 1) : L'\0';
            if (next_char != VARIABLE_EXPAND && next_char != VARIABLE_EXPAND_SINGLE &&
                next_char != '(' && !valid_var_name_char(next_char)) {
                err = 1;
                if (out_errors) {
                    // We have something like $$$^....  Back up until we reach the first $.
                    size_t first_dollar = idx;
                    while (first_dollar > 0 &&
                           (unesc.at(first_dollar - 1) == VARIABLE_EXPAND ||
                            unesc.at(first_dollar - 1) == VARIABLE_EXPAND_SINGLE)) {
                        first_dollar--;
                    }
                    parse_util_expand_variable_error(unesc, source_start, first_dollar, out_errors);
                }
            }
        }

        return err;
    };

    size_t cursor = 0;
    size_t checked = 0;
    wcstring subst;

    bool do_loop = true;
    bool is_quoted = false;
    while (do_loop) {
        size_t paren_begin = 0;
        size_t paren_end = 0;
        bool has_dollar = false;
        switch (parse_util_locate_cmdsubst_range(arg_src, &cursor, &subst, &paren_begin, &paren_end,
                                                 false, &is_quoted, &has_dollar)) {
            case -1: {
                err |= PARSER_TEST_ERROR;
                if (out_errors) {
                    append_syntax_error(out_errors, source_start, 1, L"Mismatched parenthesis");
                }
                return err;
            }
            case 0: {
                do_loop = false;
                break;
            }
            case 1: {
                err |= check_subtoken(checked, paren_begin - has_dollar);

                assert(paren_begin < paren_end && "Parens out of order?");
                parse_error_list_t subst_errors;
                err |= parse_util_detect_errors(subst, &subst_errors);

                // Our command substitution produced error offsets relative to its source. Tweak the
                // offsets of the errors in the command substitution to account for both its offset
                // within the string, and the offset of the node.
                size_t error_offset = paren_begin + 1 + source_start;
                parse_error_offset_source_start(&subst_errors, error_offset);

                if (out_errors != nullptr) {
                    out_errors->insert(out_errors->end(), subst_errors.begin(), subst_errors.end());
                }

                checked = paren_end + 1;
                break;
            }
            default: {
                DIE("unexpected parse_util_locate_cmdsubst() return value");
            }
        }
    }
    err |= check_subtoken(checked, arg_src.size());

    return err;
}

/// Given that the job given by node should be backgrounded, return true if we detect any errors.
static bool detect_errors_in_backgrounded_job(const ast::job_t &job,
                                              parse_error_list_t *parse_errors) {
    using namespace ast;
    auto source_range = job.try_source_range();
    if (!source_range) return false;

    bool errored = false;
    // Disallow background in the following cases:
    // foo & ; and bar
    // foo & ; or bar
    // if foo & ; end
    // while foo & ; end
    const job_conjunction_t *job_conj = job.parent->try_as<job_conjunction_t>();
    if (!job_conj) return false;

    if (job_conj->parent->try_as<if_clause_t>()) {
        errored = append_syntax_error(parse_errors, source_range->start, source_range->length,
                                      BACKGROUND_IN_CONDITIONAL_ERROR_MSG);
    } else if (job_conj->parent->try_as<while_header_t>()) {
        errored = append_syntax_error(parse_errors, source_range->start, source_range->length,
                                      BACKGROUND_IN_CONDITIONAL_ERROR_MSG);
    } else if (const ast::job_list_t *jlist = job_conj->parent->try_as<ast::job_list_t>()) {
        // This isn't very complete, e.g. we don't catch 'foo & ; not and bar'.
        // Find the index of ourselves in the job list.
        size_t index;
        for (index = 0; index < jlist->count(); index++) {
            if (jlist->at(index) == job_conj) break;
        }
        assert(index < jlist->count() && "Should have found the job in the list");

        // Try getting the next job and check its decorator.
        if (const job_conjunction_t *next = jlist->at(index + 1)) {
            if (const keyword_base_t *deco = next->decorator.contents.get()) {
                assert(
                    (deco->kw == parse_keyword_t::kw_and || deco->kw == parse_keyword_t::kw_or) &&
                    "Unexpected decorator keyword");
                const wchar_t *deco_name = (deco->kw == parse_keyword_t::kw_and ? L"and" : L"or");
                errored = append_syntax_error(parse_errors, deco->source_range().start,
                                              deco->source_range().length,
                                              BOOL_AFTER_BACKGROUND_ERROR_MSG, deco_name);
            }
        }
    }
    return errored;
}

/// Given a source buffer \p buff_src and decorated statement \p dst within it, return true if there
/// is an error and false if not. \p storage may be used to reduce allocations.
static bool detect_errors_in_decorated_statement(const wcstring &buff_src,
                                                 const ast::decorated_statement_t &dst,
                                                 wcstring *storage,
                                                 parse_error_list_t *parse_errors) {
    using namespace ast;
    bool errored = false;
    auto source_start = dst.source_range().start;
    auto source_length = dst.source_range().length;
    const statement_decoration_t decoration = dst.decoration();

    // Determine if the first argument is help.
    bool first_arg_is_help = false;
    if (const auto *arg = get_first_arg(dst.args_or_redirs)) {
        const wcstring &arg_src = arg->source(buff_src, storage);
        first_arg_is_help = parse_util_argument_is_help(arg_src);
    }

    // Get the statement we are part of.
    const statement_t *st = dst.parent->as<statement_t>();

    // Walk up to the job.
    const ast::job_t *job = nullptr;
    for (const node_t *cursor = st; job == nullptr; cursor = cursor->parent) {
        assert(cursor && "Reached root without finding a job");
        job = cursor->try_as<ast::job_t>();
    }
    assert(job && "Should have found the job");

    // Check our pipeline position.
    pipeline_position_t pipe_pos;
    if (job->continuation.empty()) {
        pipe_pos = pipeline_position_t::none;
    } else if (&job->statement == st) {
        pipe_pos = pipeline_position_t::first;
    } else {
        pipe_pos = pipeline_position_t::subsequent;
    }

    // Check that we don't try to pipe through exec.
    bool is_in_pipeline = (pipe_pos != pipeline_position_t::none);
    if (is_in_pipeline && decoration == statement_decoration_t::exec) {
        errored = append_syntax_error(parse_errors, source_start, source_length,
                                      INVALID_PIPELINE_CMD_ERR_MSG, L"exec");
    }

    // This is a somewhat stale check that 'and' and 'or' are not in pipelines, except at the
    // beginning. We can't disallow them as commands entirely because we need to support 'and
    // --help', etc.
    if (pipe_pos == pipeline_position_t::subsequent) {
        // check if our command is 'and' or 'or'. This is very clumsy; we don't catch e.g. quoted
        // commands.
        const wcstring &command = dst.command.source(buff_src, storage);
        if (command == L"and" || command == L"or") {
            errored = append_syntax_error(parse_errors, source_start, source_length,
                                          INVALID_PIPELINE_CMD_ERR_MSG, command.c_str());
        }

        // Similarly for time (#8841).
        if (command == L"time") {
            errored = append_syntax_error(parse_errors, source_start, source_length,
                                          TIME_IN_PIPELINE_ERR_MSG);
        }
    }

    // $status specifically is invalid as a command,
    // to avoid people trying `if $status`.
    // We see this surprisingly regularly.
    const wcstring &com = dst.command.source(buff_src, storage);
    if (com == L"$status") {
        errored =
            append_syntax_error(parse_errors, source_start, source_length,
                                _(L"$status is not valid as a command. See `help conditions`"));
    }

    const wcstring &unexp_command = dst.command.source(buff_src, storage);
    if (!unexp_command.empty()) {
        // Check that we can expand the command.
        // Make a new error list so we can fix the offset for just those, then append later.
        wcstring command;
        parse_error_list_t new_errors;
        if (expand_to_command_and_args(unexp_command, operation_context_t::empty(), &command,
                                       nullptr, &new_errors,
                                       true /* skip wildcards */) == expand_result_t::error) {
            errored = true;
        }

        // Check that pipes are sound.
        if (!errored && parser_is_pipe_forbidden(command) && is_in_pipeline) {
            errored = append_syntax_error(parse_errors, source_start, source_length,
                                          INVALID_PIPELINE_CMD_ERR_MSG, command.c_str());
        }

        // Check that we don't break or continue from outside a loop.
        if (!errored && (command == L"break" || command == L"continue") && !first_arg_is_help) {
            // Walk up until we hit a 'for' or 'while' loop. If we hit a function first,
            // stop the search; we can't break an outer loop from inside a function.
            // This is a little funny because we can't tell if it's a 'for' or 'while'
            // loop from the ancestor alone; we need the header. That is, we hit a
            // block_statement, and have to check its header.
            bool found_loop = false;
            for (const node_t *ancestor = &dst; ancestor != nullptr; ancestor = ancestor->parent) {
                const auto *block = ancestor->try_as<block_statement_t>();
                if (!block) continue;
                if (block->header->type == type_t::for_header ||
                    block->header->type == type_t::while_header) {
                    // This is a loop header, so we can break or continue.
                    found_loop = true;
                    break;
                } else if (block->header->type == type_t::function_header) {
                    // This is a function header, so we cannot break or
                    // continue. We stop our search here.
                    found_loop = false;
                    break;
                }
            }

            if (!found_loop) {
                errored = append_syntax_error(
                    parse_errors, source_start, source_length,
                    (command == L"break" ? INVALID_BREAK_ERR_MSG : INVALID_CONTINUE_ERR_MSG));
            }
        }

        // Check that we don't do an invalid builtin (issue #1252).
        if (!errored && decoration == statement_decoration_t::builtin) {
            wcstring command = unexp_command;
            if (expand_one(command, expand_flag::skip_cmdsubst, operation_context_t::empty(),
                           parse_errors) &&
                !builtin_exists(unexp_command)) {
                errored = append_syntax_error(parse_errors, source_start, source_length,
                                              UNKNOWN_BUILTIN_ERR_MSG, unexp_command.c_str());
            }
        }

        if (parse_errors) {
            // The expansion errors here go from the *command* onwards,
            // so we need to offset them by the *command* offset,
            // excluding the decoration.
            parse_error_offset_source_start(&new_errors, dst.command.source_range().start);
            vec_append(*parse_errors, std::move(new_errors));
        }
    }
    return errored;
}

// Given we have a trailing argument_or_redirection_list, like `begin; end > /dev/null`, verify that
// there are no arguments in the list.
static bool detect_errors_in_block_redirection_list(
    const ast::argument_or_redirection_list_t &args_or_redirs, parse_error_list_t *out_errors) {
    if (const auto *first_arg = get_first_arg(args_or_redirs)) {
        return append_syntax_error(out_errors, first_arg->source_range().start,
                                   first_arg->source_range().length, END_ARG_ERR_MSG);
    }
    return false;
}

parser_test_error_bits_t parse_util_detect_errors(const ast::ast_t &ast, const wcstring &buff_src,
                                                  parse_error_list_t *out_errors) {
    using namespace ast;
    parser_test_error_bits_t res = 0;

    // Whether we encountered a parse error.
    bool errored = false;

    // Whether we encountered an unclosed block. We detect this via an 'end_command' block without
    // source.
    bool has_unclosed_block = false;

    // Whether we encounter a missing statement, i.e. a newline after a pipe. This is found by
    // detecting job_continuations that have source for pipes but not the statement.
    bool has_unclosed_pipe = false;

    // Whether we encounter a missing job, i.e. a newline after && or ||. This is found by
    // detecting job_conjunction_continuations that have source for && or || but not the job.
    bool has_unclosed_conjunction = false;

    // Expand all commands.
    // Verify 'or' and 'and' not used inside pipelines.
    // Verify return only within a function.
    // Verify no variable expansions.
    wcstring storage;

    for (const node_t &node : ast) {
        if (const job_continuation_t *jc = node.try_as<job_continuation_t>()) {
            // Somewhat clumsy way of checking for a statement without source in a pipeline.
            // See if our pipe has source but our statement does not.
            if (!jc->pipe.unsourced && !jc->statement.try_source_range().has_value()) {
                has_unclosed_pipe = true;
            }
        } else if (const auto *jcc = node.try_as<job_conjunction_continuation_t>()) {
            // Somewhat clumsy way of checking for a job without source in a conjunction.
            // See if our conjunction operator (&& or ||) has source but our job does not.
            if (!jcc->conjunction.unsourced && !jcc->job.try_source_range().has_value()) {
                has_unclosed_conjunction = true;
            }
        } else if (const argument_t *arg = node.try_as<argument_t>()) {
            const wcstring &arg_src = arg->source(buff_src, &storage);
            res |= parse_util_detect_errors_in_argument(*arg, arg_src, out_errors);
        } else if (const ast::job_t *job = node.try_as<ast::job_t>()) {
            // Disallow background in the following cases:
            //
            // foo & ; and bar
            // foo & ; or bar
            // if foo & ; end
            // while foo & ; end
            // If it's not a background job, nothing to do.
            if (job->bg) {
                errored |= detect_errors_in_backgrounded_job(*job, out_errors);
            }
        } else if (const ast::decorated_statement_t *stmt = node.try_as<decorated_statement_t>()) {
            errored |= detect_errors_in_decorated_statement(buff_src, *stmt, &storage, out_errors);
        } else if (const auto *block = node.try_as<block_statement_t>()) {
            // If our 'end' had no source, we are unsourced.
            if (block->end.unsourced) has_unclosed_block = true;
            errored |= detect_errors_in_block_redirection_list(block->args_or_redirs, out_errors);
        } else if (const auto *ifs = node.try_as<if_statement_t>()) {
            // If our 'end' had no source, we are unsourced.
            if (ifs->end.unsourced) has_unclosed_block = true;
            errored |= detect_errors_in_block_redirection_list(ifs->args_or_redirs, out_errors);
        } else if (const auto *switchs = node.try_as<switch_statement_t>()) {
            // If our 'end' had no source, we are unsourced.
            if (switchs->end.unsourced) has_unclosed_block = true;
            errored |= detect_errors_in_block_redirection_list(switchs->args_or_redirs, out_errors);
        }
    }

    if (errored) res |= PARSER_TEST_ERROR;

    if (has_unclosed_block || has_unclosed_pipe || has_unclosed_conjunction)
        res |= PARSER_TEST_INCOMPLETE;

    return res;
}

parser_test_error_bits_t parse_util_detect_errors(const wcstring &buff_src,
                                                  parse_error_list_t *out_errors,
                                                  bool allow_incomplete) {
    // Whether there's an unclosed quote or subshell, and therefore unfinished. This is only set if
    // allow_incomplete is set.
    bool has_unclosed_quote_or_subshell = false;

    const parse_tree_flags_t parse_flags =
        allow_incomplete ? parse_flag_leave_unterminated : parse_flag_none;

    // Parse the input string into an ast. Some errors are detected here.
    using namespace ast;
    parse_error_list_t parse_errors;
    auto ast = ast_t::parse(buff_src, parse_flags, &parse_errors);
    if (allow_incomplete) {
        // Issue #1238: If the only error was unterminated quote, then consider this to have parsed
        // successfully.
        size_t idx = parse_errors.size();
        while (idx--) {
            if (parse_errors.at(idx).code == parse_error_tokenizer_unterminated_quote ||
                parse_errors.at(idx).code == parse_error_tokenizer_unterminated_subshell) {
                // Remove this error, since we don't consider it a real error.
                has_unclosed_quote_or_subshell = true;
                parse_errors.erase(parse_errors.begin() + idx);
            }
        }
    }

    // has_unclosed_quote_or_subshell may only be set if allow_incomplete is true.
    assert(!has_unclosed_quote_or_subshell || allow_incomplete);
    if (has_unclosed_quote_or_subshell) {
        // We do not bother to validate the rest of the tree in this case.
        return PARSER_TEST_INCOMPLETE;
    }

    // Early parse error, stop here.
    if (!parse_errors.empty()) {
        if (out_errors) vec_append(*out_errors, std::move(parse_errors));
        return PARSER_TEST_ERROR;
    }

    // Defer to the tree-walking version.
    return parse_util_detect_errors(ast, buff_src, out_errors);
}

maybe_t<wcstring> parse_util_detect_errors_in_argument_list(const wcstring &arg_list_src,
                                                            const wcstring &prefix) {
    // Helper to return a description of the first error.
    auto get_error_text = [&](const parse_error_list_t &errors) {
        assert(!errors.empty() && "Expected an error");
        return errors.at(0).describe_with_prefix(arg_list_src, prefix, false /* not interactive */,
                                                 false /* don't skip caret */);
    };

    // Parse the string as a freestanding argument list.
    using namespace ast;
    parse_error_list_t errors;
    auto ast = ast_t::parse_argument_list(arg_list_src, parse_flag_none, &errors);
    if (!errors.empty()) {
        return get_error_text(errors);
    }

    // Get the root argument list and extract arguments from it.
    // Test each of these.
    for (const argument_t &arg : ast.top()->as<freestanding_argument_list_t>()->arguments) {
        const wcstring arg_src = arg.source(arg_list_src);
        if (parse_util_detect_errors_in_argument(arg, arg_src, &errors)) {
            return get_error_text(errors);
        }
    }
    return none();
}
