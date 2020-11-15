// The fish_indent program.
/*
Copyright (C) 2014 ridiculous_fish

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License version 2 as
published by the Free Software Foundation.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA
*/
#include "config.h"  // IWYU pragma: keep

#include <errno.h>
#include <getopt.h>
#include <locale.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <wctype.h>

#include <cstring>
#include <cwchar>
#include <memory>
#include <stack>
#include <string>
#include <tuple>
#include <vector>

#include "ast.h"
#include "color.h"
#include "common.h"
#include "env.h"
#include "expand.h"
#include "fish_version.h"
#include "flog.h"
#include "highlight.h"
#include "operation_context.h"
#include "output.h"
#include "parse_constants.h"
#include "parse_util.h"
#include "print_help.h"
#include "wutil.h"  // IWYU pragma: keep

// The number of spaces per indent isn't supposed to be configurable.
// See discussion at https://github.com/fish-shell/fish-shell/pull/6790
#define SPACES_PER_INDENT 4

static bool dump_parse_tree = false;
static int ret = 0;

// Read the entire contents of a file into the specified string.
static wcstring read_file(FILE *f) {
    wcstring result;
    while (true) {
        wint_t c = std::fgetwc(f);

        if (c == WEOF) {
            if (ferror(f)) {
                if (errno == EILSEQ) {
                    // Illegal byte sequence. Try to skip past it.
                    clearerr(f);
                    int ch = fgetc(f);  // for printing the warning, and seeks forward 1 byte.
                    FLOGF(warning, "%s (byte=%X)", std::strerror(errno), ch);
                    ret = 1;
                    continue;
                } else {
                    wperror(L"fgetwc");
                    exit(1);
                }
            }
            break;
        }
        result.push_back(static_cast<wchar_t>(c));
    }
    return result;
}

namespace {
/// From C++14.
template <bool B, typename T = void>
using enable_if_t = typename std::enable_if<B, T>::type;

/// \return the number of escaping backslashes before a character.
/// \p idx may be "one past the end."
size_t count_preceding_backslashes(const wcstring &text, size_t idx) {
    assert(idx <= text.size() && "Out of bounds");
    size_t backslashes = 0;
    while (backslashes < idx && text.at(idx - backslashes - 1) == L'\\') {
        backslashes++;
    }
    return backslashes;
}

/// \return whether a character at a given index is escaped.
/// A character is escaped if it has an odd number of backslashes.
bool char_is_escaped(const wcstring &text, size_t idx) {
    return count_preceding_backslashes(text, idx) % 2 == 1;
}

using namespace ast;
struct pretty_printer_t {
    // Note: this got somewhat more complicated after introducing the new AST, because that AST no
    // longer encodes detailed lexical information (e.g. every newline). This feels more complex
    // than necessary and would probably benefit from a more layered approach where we identify
    // certain runs, weight line breaks, have a cost model, etc.
    pretty_printer_t(const wcstring &src, bool do_indent)
        : source(src),
          indents(do_indent ? parse_util_compute_indents(source) : std::vector<int>(src.size(), 0)),
          ast(ast_t::parse(src, parse_flags())),
          do_indent(do_indent),
          gaps(compute_gaps()),
          preferred_semi_locations(compute_preferred_semi_locations()) {
        assert(indents.size() == source.size() && "indents and source should be same length");
    }

    // Original source.
    const wcstring &source;

    // The indents of our string.
    // This has the same length as 'source' and describes the indentation level.
    const std::vector<int> indents;

    // The parsed ast.
    const ast_t ast;

    // The prettifier output.
    wcstring output;

    // The indent of the source range which we are currently emitting.
    int current_indent{0};

    // Whether to indent, or just insert spaces.
    const bool do_indent;

    // Whether the next gap text should hide the first newline.
    bool gap_text_mask_newline{false};

    // The "gaps": a sorted set of ranges between tokens.
    // These contain whitespace, comments, semicolons, and other lexical elements which are not
    // present in the ast.
    const std::vector<source_range_t> gaps;

    // The sorted set of source offsets of nl_semi_t which should be set as semis, not newlines.
    // This is computed ahead of time for convenience.
    const std::vector<uint32_t> preferred_semi_locations;

    // Flags we support.
    using gap_flags_t = uint32_t;
    enum {
        default_flags = 0,

        // Whether to allow line splitting via escaped newlines.
        // For example, in argument lists:
        //
        //   echo a \
        //   b
        //
        // If this is not set, then split-lines will be joined.
        allow_escaped_newlines = 1 << 0,

        // Whether to require a space before this token.
        // This is used when emitting semis:
        //    echo a; echo b;
        // No space required between 'a' and ';', or 'b' and ';'.
        skip_space = 1 << 1,
    };

    // \return gap text flags for the gap text that comes *before* a given node type.
    static gap_flags_t gap_text_flags_before_node(const node_t &node) {
        gap_flags_t result = default_flags;
        switch (node.type) {
            // Allow escaped newlines in argument and redirection lists.
            case type_t::argument:
            case type_t::redirection:
                result |= allow_escaped_newlines;
                break;

            case type_t::token_base:
                // Allow escaped newlines before && and ||, and also pipes.
                switch (node.as<token_base_t>()->type) {
                    case parse_token_type_t::andand:
                    case parse_token_type_t::oror:
                    case parse_token_type_t::pipe:
                        result |= allow_escaped_newlines;
                        break;
                    default:
                        break;
                }
                break;

            default:
                break;
        }
        return result;
    }

    // \return whether we are at the start of a new line.
    bool at_line_start() const { return output.empty() || output.back() == L'\n'; }

    // \return whether we have a space before the output.
    // This ignores escaped spaces and escaped newlines.
    bool has_preceding_space() const {
        long idx = static_cast<long>(output.size()) - 1;
        // Skip escaped newlines.
        // This is historical. Example:
        //
        // cmd1 \
        // | cmd2
        //
        // we want the pipe to "see" the space after cmd1.
        // TODO: this is too tricky, we should factor this better.
        while (idx >= 0 && output.at(idx) == L'\n') {
            size_t backslashes = count_preceding_backslashes(source, idx);
            if (backslashes % 2 == 0) {
                // Not escaped.
                return false;
            }
            idx -= (1 + backslashes);
        }
        return idx >= 0 && output.at(idx) == L' ' && !char_is_escaped(output, idx);
    }

    // Entry point. Prettify our source code and return it.
    wcstring prettify() {
        output = wcstring{};
        node_visitor(*this).accept(ast.top());

        // Trailing gap text.
        emit_gap_text_before(source_range_t{(uint32_t)source.size(), 0}, default_flags);

        // Replace all trailing newlines with just a single one.
        while (!output.empty() && at_line_start()) {
            output.pop_back();
        }
        emit_newline();

        wcstring result = std::move(output);
        return result;
    }

    // \return a substring of source.
    wcstring substr(source_range_t r) const { return source.substr(r.start, r.length); }

    // Return the gap ranges from our ast.
    std::vector<source_range_t> compute_gaps() const {
        auto range_compare = [](source_range_t r1, source_range_t r2) {
            if (r1.start != r2.start) return r1.start < r2.start;
            return r1.length < r2.length;
        };
        // Collect the token ranges into a list.
        std::vector<source_range_t> tok_ranges;
        for (const node_t &node : ast) {
            if (node.category == category_t::leaf) {
                auto r = node.source_range();
                if (r.length > 0) tok_ranges.push_back(r);
            }
        }
        // Place a zero length range at end to aid in our inverting.
        tok_ranges.push_back(source_range_t{(uint32_t)source.size(), 0});

        // Our tokens should be sorted.
        assert(std::is_sorted(tok_ranges.begin(), tok_ranges.end(), range_compare));

        // For each range, add a gap range between the previous range and this range.
        std::vector<source_range_t> gaps;
        uint32_t prev_end = 0;
        for (source_range_t tok_range : tok_ranges) {
            assert(tok_range.start >= prev_end &&
                   "Token range should not overlap or be out of order");
            if (tok_range.start >= prev_end) {
                gaps.push_back(source_range_t{prev_end, tok_range.start - prev_end});
            }
            prev_end = tok_range.start + tok_range.length;
        }
        return gaps;
    }

    // Return sorted list of semi-preferring semi_nl nodes.
    std::vector<uint32_t> compute_preferred_semi_locations() const {
        std::vector<uint32_t> result;
        auto mark_semi_from_input = [&](const optional_t<semi_nl_t> &n) {
            if (n && n->has_source() && substr(n->range) == L";") {
                result.push_back(n->range.start);
            }
        };

        // andor_job_lists get semis if the input uses semis.
        for (const auto &node : ast) {
            // See if we have a condition and an andor_job_list.
            const optional_t<semi_nl_t> *condition = nullptr;
            const andor_job_list_t *andors = nullptr;
            if (const auto *ifc = node.try_as<if_clause_t>()) {
                condition = &ifc->condition.semi_nl;
                andors = &ifc->andor_tail;
            } else if (const auto *wc = node.try_as<while_header_t>()) {
                condition = &wc->condition.semi_nl;
                andors = &wc->andor_tail;
            }

            // If there is no and-or tail then we always use a newline.
            if (andors && andors->count() > 0) {
                if (condition) mark_semi_from_input(*condition);
                // Mark all but last of the andor list.
                for (uint32_t i = 0; i + 1 < andors->count(); i++) {
                    mark_semi_from_input(andors->at(i)->job.semi_nl);
                }
            }
        }

        // `x ; and y` gets semis if it has them already, and they are on the same line.
        for (const auto &node : ast) {
            if (const auto *job_list = node.try_as<job_list_t>()) {
                const semi_nl_t *prev_job_semi_nl = nullptr;
                for (const job_conjunction_t &job : *job_list) {
                    // Set up prev_job_semi_nl for the next iteration to make control flow easier.
                    const semi_nl_t *prev = prev_job_semi_nl;
                    prev_job_semi_nl = job.semi_nl.contents.get();

                    // Is this an 'and' or 'or' job?
                    if (!job.decorator) continue;

                    // Now see if we want to mark 'prev' as allowing a semi.
                    // Did we have a previous semi_nl which was a newline?
                    if (!prev || substr(prev->range) != L";") continue;

                    // Is there a newline between them?
                    assert(prev->range.start <= job.decorator->range.start &&
                           "Ranges out of order");
                    auto start = source.begin() + prev->range.start;
                    auto end = source.begin() + job.decorator->range.end();
                    if (std::find(start, end, L'\n') == end) {
                        // We're going to allow the previous semi_nl to be a semi.
                        result.push_back(prev->range.start);
                    }
                }
            }
        }
        std::sort(result.begin(), result.end());
        return result;
    }

    // Emit a space or indent as necessary, depending on the previous output.
    void emit_space_or_indent(gap_flags_t flags = default_flags) {
        if (at_line_start()) {
            output.append(SPACES_PER_INDENT * current_indent, L' ');
        } else if (!(flags & skip_space) && !has_preceding_space()) {
            output.append(1, L' ');
        }
    }

    // Emit "gap text:" newlines and comments from the original source.
    // Gap text may be a few things:
    //
    // 1. Just a space is common. We will trim the spaces to be empty.
    //
    // Here the gap text is the comment, followed by the newline:
    //
    //    echo abc # arg
    //    echo def
    //
    // 2. It may also be an escaped newline:
    // Here the gap text is a space, backslash, newline, space.
    //
    //     echo \
    //       hi
    //
    // 3. Lastly it may be an error, if there was an error token. Here the gap text is the pipe:
    //
    //   begin | stuff
    //
    //  We do not handle errors here - instead our caller does.
    bool emit_gap_text(const wcstring &gap_text, gap_flags_t flags) {
        // Common case: if we are only spaces, do nothing.
        if (gap_text.find_first_not_of(L' ') == wcstring::npos) return false;

        // Look to see if there is an escaped newline.
        // Emit it if either we allow it, or it comes before the first comment.
        // Note we do not have to be concerned with escaped backslashes or escaped #s. This is gap
        // text - we already know it has no semantic significance.
        size_t escaped_nl = gap_text.find(L"\\\n");
        bool have_line_continuation = false;
        if (escaped_nl != wcstring::npos) {
            size_t comment_idx = gap_text.find(L'#');
            if ((flags & allow_escaped_newlines) ||
                (comment_idx != wcstring::npos && escaped_nl < comment_idx)) {
                // Emit a space before the escaped newline.
                if (!at_line_start() && !has_preceding_space()) {
                    output.append(L" ");
                }
                output.append(L"\\\n");
                // Indent the line continuation and any comment before it (#7252).
                have_line_continuation = true;
                current_indent += 1;
                emit_space_or_indent();
            }
        }

        // It seems somewhat ambiguous whether we always get a newline after a comment. Ensure we
        // always emit one.
        bool needs_nl = false;

        tokenizer_t tokenizer(gap_text.c_str(), TOK_SHOW_COMMENTS | TOK_SHOW_BLANK_LINES);
        while (maybe_t<tok_t> tok = tokenizer.next()) {
            wcstring tok_text = tokenizer.text_of(*tok);

            if (needs_nl) {
                emit_newline();
                needs_nl = false;
                if (tok_text == L"\n") continue;
            } else if (gap_text_mask_newline) {
                // We only respect mask_newline the first time through the loop.
                gap_text_mask_newline = false;
                if (tok_text == L"\n") continue;
            }

            if (tok->type == token_type_t::comment) {
                emit_space_or_indent();
                output.append(tok_text);
                needs_nl = true;
            } else if (tok->type == token_type_t::end) {
                // This may be either a newline or semicolon.
                // Semicolons found here are not part of the ast and can simply be removed.
                // Newlines are preserved unless mask_newline is set.
                if (tok_text == L"\n") {
                    emit_newline();
                }
            } else {
                fprintf(stderr,
                        "Gap text should only have comments and newlines - instead found token "
                        "type %d with text: %ls\n",
                        (int)tok->type, tok_text.c_str());
                DIE("Gap text should only have comments and newlines");
            }
        }
        if (needs_nl) emit_newline();
        if (have_line_continuation) {
            emit_space_or_indent();
            current_indent -= 1;
        }
        return needs_nl;
    }

    /// \return the gap text ending at a given index into the string, or empty if none.
    source_range_t gap_text_to(uint32_t end) const {
        auto where = std::lower_bound(
            gaps.begin(), gaps.end(), end,
            [](source_range_t r, uint32_t end) { return r.start + r.length < end; });
        if (where == gaps.end() || where->start + where->length != end) {
            // Not found.
            return source_range_t{0, 0};
        } else {
            return *where;
        }
    }

    /// \return whether a range \p r overlaps an error range from our ast.
    bool range_contained_error(source_range_t r) const {
        const auto &errs = ast.extras().errors;
        auto range_is_before = [](source_range_t x, source_range_t y) {
            return x.start + x.length <= y.start;
        };
        assert(std::is_sorted(errs.begin(), errs.end(), range_is_before) &&
               "Error ranges should be sorted");
        return std::binary_search(errs.begin(), errs.end(), r, range_is_before);
    }

    // Emit the gap text before a source range.
    bool emit_gap_text_before(source_range_t r, gap_flags_t flags) {
        assert(r.start <= source.size() && "source out of bounds");
        bool added_newline = false;

        // Find the gap text which ends at start.
        source_range_t range = gap_text_to(r.start);
        if (range.length > 0) {
            // Set the indent from the beginning of this gap text.
            // For example:
            // begin
            //    cmd
            //    # comment
            // end
            // Here the comment is the gap text before the end, but we want the indent from the
            // command.
            if (range.start < indents.size()) current_indent = indents.at(range.start);

            // If this range contained an error, append the gap text without modification.
            // For example in: echo foo "
            // We don't want to mess with the quote.
            if (range_contained_error(range)) {
                output.append(substr(range));
            } else {
                added_newline = emit_gap_text(substr(range), flags);
            }
        }
        // Always clear gap_text_mask_newline after emitting even empty gap text.
        gap_text_mask_newline = false;
        return added_newline;
    }

    /// Given a string \p input, remove unnecessary quotes, etc.
    wcstring clean_text(const wcstring &input) {
        // Unescape the string - this leaves special markers around if there are any
        // expansions or anything. We specifically tell it to not compute backslash-escapes
        // like \U or \x, because we want to leave them intact.
        wcstring unescaped = input;
        unescape_string_in_place(&unescaped, UNESCAPE_SPECIAL | UNESCAPE_NO_BACKSLASHES);

        // Remove INTERNAL_SEPARATOR because that's a quote.
        auto quote = [](wchar_t ch) { return ch == INTERNAL_SEPARATOR; };
        unescaped.erase(std::remove_if(unescaped.begin(), unescaped.end(), quote), unescaped.end());

        // If no non-"good" char is left, use the unescaped version.
        // This can be extended to other characters, but giving the precise list is tough,
        // can change over time (see "^", "%" and "?", in some cases "{}") and it just makes
        // people feel more at ease.
        auto goodchars = [](wchar_t ch) {
            return fish_iswalnum(ch) || ch == L'_' || ch == L'-' || ch == L'/';
        };
        if (std::find_if_not(unescaped.begin(), unescaped.end(), goodchars) == unescaped.end() &&
            !unescaped.empty()) {
            return unescaped;
        } else {
            return input;
        }
    }

    // Emit a range of original text. This indents as needed, and also inserts preceding gap text.
    // If \p tolerate_line_splitting is set, then permit escaped newlines; otherwise collapse such
    // lines.
    void emit_text(source_range_t r, gap_flags_t flags) {
        emit_gap_text_before(r, flags);
        current_indent = indents.at(r.start);
        if (r.length > 0) {
            emit_space_or_indent(flags);
            output.append(clean_text(substr(r)));
        }
    }

    template <type_t Type>
    void emit_node_text(const leaf_t<Type> &node) {
        emit_text(node.range, gap_text_flags_before_node(node));
    }

    // Emit one newline.
    void emit_newline() { output.push_back(L'\n'); }

    // Emit a semicolon.
    void emit_semi() { output.push_back(L';'); }

    // For branch and list nodes, default is to visit their children.
    template <typename Node>
    enable_if_t<Node::Category == category_t::branch> visit(const Node &node) {
        node_visitor(*this).accept_children_of(node);
    }

    template <typename Node>
    enable_if_t<Node::Category == ast::category_t::list> visit(const Node &node) {
        node_visitor(*this).accept_children_of(node);
    }

    // Leaf nodes we just visit their text.
    void visit(const keyword_base_t &node) { emit_node_text(node); }
    void visit(const token_base_t &node) { emit_node_text(node); }
    void visit(const argument_t &node) { emit_node_text(node); }
    void visit(const variable_assignment_t &node) { emit_node_text(node); }

    void visit(const semi_nl_t &node) {
        // These are semicolons or newlines which are part of the ast. That means it includes e.g.
        // ones terminating a job or 'if' header, but not random semis in job lists. We respect
        // preferred_semi_locations to decide whether or not these should stay as newlines or
        // become semicolons.

        // Check if we should prefer a semicolon.
        bool prefer_semi = node.range.length > 0 &&
                           std::binary_search(preferred_semi_locations.begin(),
                                              preferred_semi_locations.end(), node.range.start);
        emit_gap_text_before(node.range, gap_text_flags_before_node(node));

        // Don't emit anything if the gap text put us on a newline (because it had a comment).
        if (!at_line_start()) {
            prefer_semi ? emit_semi() : emit_newline();

            // If it was a semi but we emitted a newline, swallow a subsequent newline.
            if (!prefer_semi && substr(node.range) == L";") {
                gap_text_mask_newline = true;
            }
        }
    }

    void visit(const redirection_t &node) {
        // No space between a redirection operator and its target (#2899).
        emit_text(node.oper.range, default_flags);
        emit_text(node.target.range, skip_space);
    }

    void visit(const maybe_newlines_t &node) {
        // Our newlines may have comments embedded in them, example:
        //    cmd |
        //    # something
        //    cmd2
        // Treat it as gap text.
        if (node.range.length > 0) {
            auto flags = gap_text_flags_before_node(node);
            current_indent = indents.at(node.range.start);
            bool added_newline = emit_gap_text_before(node.range, flags);
            wcstring text = source.substr(node.range.start, node.range.length);
            if (added_newline && !text.empty() && text.front() == L'\n') {
                text = text.substr(strlen("\n"));
            }
            emit_gap_text(text, flags);
        }
    }

    void visit(const begin_header_t &node) {
        // 'begin' does not require a newline after it, but we insert one.
        node_visitor(*this).accept_children_of(node);
        if (!at_line_start()) {
            emit_newline();
        }
    }

    // The flags we use to parse.
    static parse_tree_flags_t parse_flags() {
        return parse_flag_continue_after_error | parse_flag_include_comments |
               parse_flag_leave_unterminated | parse_flag_show_blank_lines;
    }
};
}  // namespace

static const char *highlight_role_to_string(highlight_role_t role) {
#define TEST_ROLE(x)          \
    case highlight_role_t::x: \
        return #x;
    switch (role) {
        TEST_ROLE(normal)
        TEST_ROLE(error)
        TEST_ROLE(command)
        TEST_ROLE(statement_terminator)
        TEST_ROLE(param)
        TEST_ROLE(comment)
        TEST_ROLE(search_match)
        TEST_ROLE(operat)
        TEST_ROLE(escape)
        TEST_ROLE(quote)
        TEST_ROLE(redirection)
        TEST_ROLE(autosuggestion)
        TEST_ROLE(selection)
        TEST_ROLE(pager_progress)
        TEST_ROLE(pager_background)
        TEST_ROLE(pager_prefix)
        TEST_ROLE(pager_completion)
        TEST_ROLE(pager_description)
        TEST_ROLE(pager_secondary_background)
        TEST_ROLE(pager_secondary_prefix)
        TEST_ROLE(pager_secondary_completion)
        TEST_ROLE(pager_secondary_description)
        TEST_ROLE(pager_selected_background)
        TEST_ROLE(pager_selected_prefix)
        TEST_ROLE(pager_selected_completion)
        TEST_ROLE(pager_selected_description)
        default:
            DIE("UNKNOWN ROLE");
    }
#undef TEST_ROLE
}

// Entry point for Pygments CSV output.
// Our output is a newline-separated string.
// Each line is of the form `start,end,role`
// start and end is the half-open token range, value is a string from highlight_role_t.
// Example:
// 3,7,command
static std::string make_pygments_csv(const wcstring &src) {
    const size_t len = src.size();
    std::vector<highlight_spec_t> colors;
    highlight_shell(src, colors, operation_context_t::globals());
    assert(colors.size() == len && "Colors and src should have same size");

    struct token_range_t {
        unsigned long start;
        unsigned long end;
        highlight_role_t role;
    };

    std::vector<token_range_t> token_ranges;
    for (size_t i = 0; i < len; i++) {
        highlight_role_t role = colors.at(i).foreground;
        // See if we can extend the last range.
        if (!token_ranges.empty()) {
            auto &last = token_ranges.back();
            if (last.role == role && last.end == i) {
                last.end = i + 1;
                continue;
            }
        }
        // We need a new range.
        token_ranges.push_back(token_range_t{i, i + 1, role});
    }

    // Now render these to a string.
    std::string result;
    for (const auto &range : token_ranges) {
        char buff[128];
        snprintf(buff, sizeof buff, "%lu,%lu,%s\n", range.start, range.end,
                 highlight_role_to_string(range.role));
        result.append(buff);
    }
    return result;
}

// Entry point for prettification.
static wcstring prettify(const wcstring &src, bool do_indent) {
    if (dump_parse_tree) {
        auto ast =
            ast::ast_t::parse(src, parse_flag_leave_unterminated | parse_flag_include_comments |
                                       parse_flag_show_extra_semis);
        wcstring ast_dump = ast.dump(src);
        std::fwprintf(stderr, L"%ls\n", ast_dump.c_str());
    }

    pretty_printer_t printer{src, do_indent};
    wcstring output = printer.prettify();
    return output;
}

/// Given a string and list of colors of the same size, return the string with HTML span elements
/// for the various colors.
static const wchar_t *html_class_name_for_color(highlight_spec_t spec) {
#define P(x) L"fish_color_" #x
    switch (spec.foreground) {
        case highlight_role_t::normal: {
            return P(normal);
        }
        case highlight_role_t::error: {
            return P(error);
        }
        case highlight_role_t::command: {
            return P(command);
        }
        case highlight_role_t::statement_terminator: {
            return P(statement_terminator);
        }
        case highlight_role_t::param: {
            return P(param);
        }
        case highlight_role_t::comment: {
            return P(comment);
        }
        case highlight_role_t::search_match: {
            return P(search_match);
        }
        case highlight_role_t::operat: {
            return P(operator);
        }
        case highlight_role_t::escape: {
            return P(escape);
        }
        case highlight_role_t::quote: {
            return P(quote);
        }
        case highlight_role_t::redirection: {
            return P(redirection);
        }
        case highlight_role_t::autosuggestion: {
            return P(autosuggestion);
        }
        case highlight_role_t::selection: {
            return P(selection);
        }
        default: {
            return P(other);
        }
    }
}

static std::string html_colorize(const wcstring &text,
                                 const std::vector<highlight_spec_t> &colors) {
    if (text.empty()) {
        return "";
    }

    assert(colors.size() == text.size());
    wcstring html = L"<pre><code>";
    highlight_spec_t last_color = highlight_role_t::normal;
    for (size_t i = 0; i < text.size(); i++) {
        // Handle colors.
        highlight_spec_t color = colors.at(i);
        if (i > 0 && color != last_color) {
            html.append(L"</span>");
        }
        if (i == 0 || color != last_color) {
            append_format(html, L"<span class=\"%ls\">", html_class_name_for_color(color));
        }
        last_color = color;

        // Handle text.
        wchar_t wc = text.at(i);
        switch (wc) {
            case L'&': {
                html.append(L"&amp;");
                break;
            }
            case L'\'': {
                html.append(L"&apos;");
                break;
            }
            case L'"': {
                html.append(L"&quot;");
                break;
            }
            case L'<': {
                html.append(L"&lt;");
                break;
            }
            case L'>': {
                html.append(L"&gt;");
                break;
            }
            default: {
                html.push_back(wc);
                break;
            }
        }
    }
    html.append(L"</span></code></pre>");
    return wcs2string(html);
}

static std::string no_colorize(const wcstring &text) { return wcs2string(text); }

int main(int argc, char *argv[]) {
    program_name = L"fish_indent";
    set_main_thread();
    setup_fork_guards();
    // Using the user's default locale could be a problem if it doesn't use UTF-8 encoding. That's
    // because the fish project assumes Unicode UTF-8 encoding in all of its scripts.
    //
    // TODO: Auto-detect the encoding of the script. We should look for a vim style comment
    // (e.g., "# vim: set fileencoding=<encoding-name>:") or an emacs style comment
    // (e.g., "# -*- coding: <encoding-name> -*-").
    setlocale(LC_ALL, "");
    env_init();

    // Types of output we support.
    enum {
        output_type_plain_text,
        output_type_file,
        output_type_ansi,
        output_type_pygments_csv,
        output_type_check,
        output_type_html
    } output_type = output_type_plain_text;
    const char *output_location = "";
    bool do_indent = true;

    const char *short_opts = "+d:hvwicD:";
    const struct option long_opts[] = {{"debug-level", required_argument, nullptr, 'd'},
                                       {"debug-stack-frames", required_argument, nullptr, 'D'},
                                       {"dump-parse-tree", no_argument, nullptr, 'P'},
                                       {"no-indent", no_argument, nullptr, 'i'},
                                       {"help", no_argument, nullptr, 'h'},
                                       {"version", no_argument, nullptr, 'v'},
                                       {"write", no_argument, nullptr, 'w'},
                                       {"html", no_argument, nullptr, 1},
                                       {"ansi", no_argument, nullptr, 2},
                                       {"pygments", no_argument, nullptr, 3},
                                       {"check", no_argument, nullptr, 'c'},
                                       {nullptr, 0, nullptr, 0}};

    int opt;
    while ((opt = getopt_long(argc, argv, short_opts, long_opts, nullptr)) != -1) {
        switch (opt) {
            case 'P': {
                dump_parse_tree = true;
                break;
            }
            case 'h': {
                print_help("fish_indent", 1);
                exit(0);
            }
            case 'v': {
                std::fwprintf(stdout, _(L"%ls, version %s\n"), program_name, get_fish_version());
                exit(0);
            }
            case 'w': {
                output_type = output_type_file;
                break;
            }
            case 'i': {
                do_indent = false;
                break;
            }
            case 1: {
                output_type = output_type_html;
                break;
            }
            case 2: {
                output_type = output_type_ansi;
                break;
            }
            case 3: {
                output_type = output_type_pygments_csv;
                break;
            }
            case 'c': {
                output_type = output_type_check;
                break;
            }
            case 'd': {
                char *end;
                long tmp;

                errno = 0;
                tmp = strtol(optarg, &end, 10);

                if (tmp >= 0 && tmp <= 10 && !*end && !errno) {
                    debug_level = static_cast<int>(tmp);
                } else {
                    std::fwprintf(stderr, _(L"Invalid value '%s' for debug-level flag"), optarg);
                    exit(1);
                }
                break;
            }
            case 'D': {
                // TODO: Option is currently useless.
                // Either remove it or make it work with FLOG.
                break;
            }
            default: {
                // We assume getopt_long() has already emitted a diagnostic msg.
                exit(1);
            }
        }
    }

    argc -= optind;
    argv += optind;

    int retval = 0;

    wcstring src;
    for (int i = 0; i < argc || (argc == 0 && i == 0); i++) {
        if (argc == 0 && i == 0) {
            if (output_type == output_type_file) {
                std::fwprintf(
                    stderr, _(L"Expected file path to read/write for -w:\n\n $ %ls -w foo.fish\n"),
                    program_name);
                exit(1);
            }
            src = read_file(stdin);
        } else {
            FILE *fh = fopen(argv[i], "r");
            if (fh) {
                src = read_file(fh);
                fclose(fh);
                output_location = argv[i];
            } else {
                std::fwprintf(stderr, _(L"Opening \"%s\" failed: %s\n"), argv[i],
                              std::strerror(errno));
                exit(1);
            }
        }

        if (output_type == output_type_pygments_csv) {
            std::string output = make_pygments_csv(src);
            fputs(output.c_str(), stdout);
            continue;
        }

        const wcstring output_wtext = prettify(src, do_indent);

        // Maybe colorize.
        std::vector<highlight_spec_t> colors;
        if (output_type != output_type_plain_text) {
            highlight_shell(output_wtext, colors, operation_context_t::globals());
        }

        std::string colored_output;
        switch (output_type) {
            case output_type_plain_text: {
                colored_output = no_colorize(output_wtext);
                break;
            }
            case output_type_file: {
                FILE *fh = fopen(output_location, "w");
                if (fh) {
                    std::fputws(output_wtext.c_str(), fh);
                    fclose(fh);
                } else {
                    std::fwprintf(stderr, _(L"Opening \"%s\" failed: %s\n"), output_location,
                                  std::strerror(errno));
                    exit(1);
                }
                break;
            }
            case output_type_ansi: {
                colored_output = colorize(output_wtext, colors, env_stack_t::globals());
                break;
            }
            case output_type_html: {
                colored_output = html_colorize(output_wtext, colors);
                break;
            }
            case output_type_pygments_csv: {
                DIE("pygments_csv should have been handled above");
            }
            case output_type_check: {
                if (output_wtext != src) {
                    if (argc) {
                        std::fwprintf(stderr, _(L"%s\n"), argv[i]);
                    }
                    retval++;
                }
                break;
            }
        }

        std::fputws(str2wcstring(colored_output).c_str(), stdout);
    }
    return retval;
}
