#include "fish_indent_common.h"

#include "ast.h"
#include "common.h"
#include "env.h"
#include "expand.h"
#include "flog.h"
#include "global_safety.h"
#include "maybe.h"
#include "operation_context.h"
#include "parse_constants.h"
#include "parse_util.h"
#include "tokenizer.h"
#include "wcstringutil.h"
#if INCLUDE_RUST_HEADERS
#include "fish_indent.rs.h"
#endif

using namespace ast;

// The number of spaces per indent isn't supposed to be configurable.
// See discussion at https://github.com/fish-shell/fish-shell/pull/6790
#define SPACES_PER_INDENT 4

/// \return whether a character at a given index is escaped.
/// A character is escaped if it has an odd number of backslashes.
static bool char_is_escaped(const wcstring &text, size_t idx) {
    return count_preceding_backslashes(text, idx) % 2 == 1;
}

pretty_printer_t::pretty_printer_t(const wcstring &src, bool do_indent)
    : source(src),
      indents(do_indent ? parse_util_compute_indents(source) : std::vector<int>(src.size(), 0)),
      ast(ast_parse(src, parse_flags())),
      visitor(new_pretty_printer(*this)),
      do_indent(do_indent),
      gaps(compute_gaps()),
      preferred_semi_locations(compute_preferred_semi_locations()) {
    assert(indents.size() == source.size() && "indents and source should be same length");
}

pretty_printer_t::gap_flags_t pretty_printer_t::gap_text_flags_before_node(const node_t &node) {
    gap_flags_t result = default_flags;
    switch (node.typ()) {
        // Allow escaped newlines before leaf nodes that can be part of a long command.
        case type_t::argument:
        case type_t::redirection:
        case type_t::variable_assignment:
            result |= allow_escaped_newlines;
            break;

        case type_t::token_base:
            // Allow escaped newlines before && and ||, and also pipes.
            switch (node.token_type()) {
                case parse_token_type_t::andand:
                case parse_token_type_t::oror:
                case parse_token_type_t::pipe:
                    result |= allow_escaped_newlines;
                    break;
                case parse_token_type_t::string: {
                    // Allow escaped newlines before commands that follow a variable assignment
                    // since both can be long (#7955).
                    auto p = node.parent();
                    if (p->typ() != type_t::decorated_statement) break;
                    p = p->parent();
                    assert(p->typ() == type_t::statement);
                    p = p->parent();
                    if (auto *job = p->try_as_job_pipeline()) {
                        if (!job->variables().empty()) result |= allow_escaped_newlines;
                    } else if (auto *job_cnt = p->try_as_job_continuation()) {
                        if (!job_cnt->variables().empty()) result |= allow_escaped_newlines;
                    } else if (auto *not_stmt = p->try_as_not_statement()) {
                        if (!not_stmt->variables().empty()) result |= allow_escaped_newlines;
                    }
                    break;
                }
                default:
                    break;
            }
            break;

        default:
            break;
    }
    return result;
}

bool pretty_printer_t::has_preceding_space() const {
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

wcstring pretty_printer_t::prettify() {
    output = wcstring{};
    visitor->visit(*ast->top());

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

std::vector<source_range_t> pretty_printer_t::compute_gaps() const {
    auto range_compare = [](source_range_t r1, source_range_t r2) {
        if (r1.start != r2.start) return r1.start < r2.start;
        return r1.length < r2.length;
    };
    // Collect the token ranges into a list.
    std::vector<source_range_t> tok_ranges;
    for (auto ast_traversal = new_ast_traversal(*ast->top());;) {
        auto node = ast_traversal->next();
        if (!node->has_value()) break;
        if (node->category() == category_t::leaf) {
            auto r = node->source_range();
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
        assert(tok_range.start >= prev_end && "Token range should not overlap or be out of order");
        if (tok_range.start >= prev_end) {
            gaps.push_back(source_range_t{prev_end, tok_range.start - prev_end});
        }
        prev_end = tok_range.start + tok_range.length;
    }
    return gaps;
}

void pretty_printer_t::visit_begin_header() {
    if (!at_line_start()) {
        emit_newline();
    }
}

void pretty_printer_t::visit_maybe_newlines(const void *node_) {
    const auto &node = *static_cast<const maybe_newlines_t *>(node_);
    // Our newlines may have comments embedded in them, example:
    //    cmd |
    //    # something
    //    cmd2
    // Treat it as gap text.
    if (node.range().length > 0) {
        auto flags = gap_text_flags_before_node(*node.ptr());
        current_indent = indents.at(node.range().start);
        bool added_newline = emit_gap_text_before(node.range(), flags);
        source_range_t gap_range = node.range();
        if (added_newline && gap_range.length > 0 && source.at(gap_range.start) == L'\n') {
            gap_range.start++;
        }
        emit_gap_text(gap_range, flags);
    }
}

void pretty_printer_t::visit_redirection(const void *node_) {
    const auto &node = *static_cast<const redirection_t *>(node_);
    // No space between a redirection operator and its target (#2899).
    emit_text(node.oper().range(), default_flags);
    emit_text(node.target().range(), skip_space);
}

void pretty_printer_t::visit_semi_nl(const void *node_) {
    // These are semicolons or newlines which are part of the ast. That means it includes e.g.
    // ones terminating a job or 'if' header, but not random semis in job lists. We respect
    // preferred_semi_locations to decide whether or not these should stay as newlines or
    // become semicolons.
    const auto &node = *static_cast<const node_t *>(node_);
    auto range = node.source_range();

    // Check if we should prefer a semicolon.
    bool prefer_semi =
        range.length > 0 && std::binary_search(preferred_semi_locations.begin(),
                                               preferred_semi_locations.end(), range.start);
    emit_gap_text_before(range, gap_text_flags_before_node(*node.ptr()));

    // Don't emit anything if the gap text put us on a newline (because it had a comment).
    if (!at_line_start()) {
        prefer_semi ? emit_semi() : emit_newline();

        // If it was a semi but we emitted a newline, swallow a subsequent newline.
        if (!prefer_semi && substr(range) == L";") {
            gap_text_mask_newline = true;
        }
    }
}

void pretty_printer_t::emit_node_text(const void *node_) {
    const auto &node = *static_cast<const node_t *>(node_);
    source_range_t range = node.source_range();

    // Weird special-case: a token may end in an escaped newline. Notably, the newline is
    // not part of the following gap text, handle indentation here (#8197).
    bool ends_with_escaped_nl = range.length >= 2 && source.at(range.end() - 2) == L'\\' &&
                                source.at(range.end() - 1) == L'\n';
    if (ends_with_escaped_nl) {
        range = {range.start, range.length - 2};
    }

    emit_text(range, gap_text_flags_before_node(node));

    if (ends_with_escaped_nl) {
        // By convention, escaped newlines are preceded with a space.
        output.append(L" \\\n");
        // TODO Maybe check "allow_escaped_newlines" and use the precomputed indents.
        // The cases where this matters are probably very rare.
        current_indent++;
        emit_space_or_indent();
        current_indent--;
    }
}

void pretty_printer_t::emit_text(source_range_t r, gap_flags_t flags) {
    emit_gap_text_before(r, flags);
    current_indent = indents.at(r.start);
    if (r.length > 0) {
        emit_space_or_indent(flags);
        output.append(clean_text(substr(r)));
    }
}

wcstring pretty_printer_t::clean_text(const wcstring &input) {
    // Unescape the string - this leaves special markers around if there are any
    // expansions or anything. We specifically tell it to not compute backslash-escapes
    // like \U or \x, because we want to leave them intact.
    wcstring unescaped =
        *unescape_string(input.c_str(), input.size(), UNESCAPE_SPECIAL | UNESCAPE_NO_BACKSLASHES,
                         STRING_STYLE_SCRIPT);

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

bool pretty_printer_t::emit_gap_text_before(source_range_t r, gap_flags_t flags) {
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
            added_newline = emit_gap_text(range, flags);
        }
    }
    // Always clear gap_text_mask_newline after emitting even empty gap text.
    gap_text_mask_newline = false;
    return added_newline;
}

bool pretty_printer_t::range_contained_error(source_range_t r) const {
    const auto &errs = ast->extras()->errors();
    auto range_is_before = [](source_range_t x, source_range_t y) {
        return x.start + x.length <= y.start;
    };
    assert(std::is_sorted(errs.begin(), errs.end(), range_is_before) &&
           "Error ranges should be sorted");
    return std::binary_search(errs.begin(), errs.end(), r, range_is_before);
}

source_range_t pretty_printer_t::gap_text_to(uint32_t end) const {
    auto where =
        std::lower_bound(gaps.begin(), gaps.end(), end,
                         [](source_range_t r, uint32_t end) { return r.start + r.length < end; });
    if (where == gaps.end() || where->start + where->length != end) {
        // Not found.
        return source_range_t{0, 0};
    } else {
        return *where;
    }
}

bool pretty_printer_t::emit_gap_text(source_range_t range, gap_flags_t flags) {
    wcstring gap_text = substr(range);
    // Common case: if we are only spaces, do nothing.
    if (gap_text.find_first_not_of(L' ') == wcstring::npos) return false;

    // Look to see if there is an escaped newline.
    // Emit it if either we allow it, or it comes before the first comment.
    // Note we do not have to be concerned with escaped backslashes or escaped #s. This is gap
    // text - we already know it has no semantic significance.
    size_t escaped_nl = gap_text.find(L"\\\n");
    if (escaped_nl != wcstring::npos) {
        size_t comment_idx = gap_text.find(L'#');
        if ((flags & allow_escaped_newlines) ||
            (comment_idx != wcstring::npos && escaped_nl < comment_idx)) {
            // Emit a space before the escaped newline.
            if (!at_line_start() && !has_preceding_space()) {
                output.append(L" ");
            }
            output.append(L"\\\n");
            // Indent the continuation line and any leading comments (#7252).
            // Use the indentation level of the next newline.
            current_indent = indents.at(range.start + escaped_nl + 1);
            emit_space_or_indent();
        }
    }

    // It seems somewhat ambiguous whether we always get a newline after a comment. Ensure we
    // always emit one.
    bool needs_nl = false;

    auto tokenizer = new_tokenizer(gap_text.c_str(), TOK_SHOW_COMMENTS | TOK_SHOW_BLANK_LINES);
    while (auto tok = tokenizer->next()) {
        wcstring tok_text = *tokenizer->text_of(*tok);

        if (needs_nl) {
            emit_newline();
            needs_nl = false;
            if (tok_text == L"\n") continue;
        } else if (gap_text_mask_newline) {
            // We only respect mask_newline the first time through the loop.
            gap_text_mask_newline = false;
            if (tok_text == L"\n") continue;
        }

        if (tok->type_ == token_type_t::comment) {
            emit_space_or_indent();
            output.append(tok_text);
            needs_nl = true;
        } else if (tok->type_ == token_type_t::end) {
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
                    (int)tok->type_, tok_text.c_str());
            DIE("Gap text should only have comments and newlines");
        }
    }
    if (needs_nl) emit_newline();
    return needs_nl;
}

void pretty_printer_t::emit_space_or_indent(gap_flags_t flags) {
    if (at_line_start()) {
        output.append(SPACES_PER_INDENT * current_indent, L' ');
    } else if (!(flags & skip_space) && !has_preceding_space()) {
        output.append(1, L' ');
    }
}

std::vector<uint32_t> pretty_printer_t::compute_preferred_semi_locations() const {
    std::vector<uint32_t> result;
    auto mark_semi_from_input = [&](const semi_nl_t &n) {
        if (n.ptr()->has_source() && substr(n.range()) == L";") {
            result.push_back(n.range().start);
        }
    };

    // andor_job_lists get semis if the input uses semis.
    for (auto ast_traversal = new_ast_traversal(*ast->top());;) {
        auto node = ast_traversal->next();
        if (!node->has_value()) break;
        // See if we have a condition and an andor_job_list.
        const semi_nl_t *condition = nullptr;
        const andor_job_list_t *andors = nullptr;
        if (const auto *ifc = node->try_as_if_clause()) {
            if (ifc->condition().has_semi_nl()) {
                condition = &ifc->condition().semi_nl();
            }
            andors = &ifc->andor_tail();
        } else if (const auto *wc = node->try_as_while_header()) {
            if (wc->condition().has_semi_nl()) {
                condition = &wc->condition().semi_nl();
            }
            andors = &wc->andor_tail();
        }

        // If there is no and-or tail then we always use a newline.
        if (andors && andors->count() > 0) {
            if (condition) mark_semi_from_input(*condition);
            // Mark all but last of the andor list.
            for (uint32_t i = 0; i + 1 < andors->count(); i++) {
                mark_semi_from_input(andors->at(i)->job().semi_nl());
            }
        }
    }

    // `x ; and y` gets semis if it has them already, and they are on the same line.
    for (auto ast_traversal = new_ast_traversal(*ast->top());;) {
        auto node = ast_traversal->next();
        if (!node->has_value()) break;
        if (const auto *job_list = node->try_as_job_list()) {
            const semi_nl_t *prev_job_semi_nl = nullptr;
            for (size_t i = 0; i < job_list->count(); i++) {
                const job_conjunction_t &job = *job_list->at(i);
                // Set up prev_job_semi_nl for the next iteration to make control flow easier.
                const semi_nl_t *prev = prev_job_semi_nl;
                prev_job_semi_nl = job.has_semi_nl() ? &job.semi_nl() : nullptr;

                // Is this an 'and' or 'or' job?
                if (!job.has_decorator()) continue;

                // Now see if we want to mark 'prev' as allowing a semi.
                // Did we have a previous semi_nl which was a newline?
                if (!prev || substr(prev->range()) != L";") continue;

                // Is there a newline between them?
                assert(prev->range().start <= job.decorator().range().start &&
                       "Ranges out of order");
                auto start = source.begin() + prev->range().start;
                auto end = source.begin() + job.decorator().range().end();
                if (std::find(start, end, L'\n') == end) {
                    // We're going to allow the previous semi_nl to be a semi.
                    result.push_back(prev->range().start);
                }
            }
        }
    }
    std::sort(result.begin(), result.end());
    return result;
}
