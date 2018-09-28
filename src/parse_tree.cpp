// Programmatic representation of fish code.
#include "config.h"  // IWYU pragma: keep

#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>
#include <wchar.h>

#include <algorithm>
#include <string>
#include <type_traits>
#include <vector>

#include "common.h"
#include "fallback.h"
#include "parse_constants.h"
#include "parse_productions.h"
#include "parse_tree.h"
#include "proc.h"
#include "tnode.h"
#include "tokenizer.h"
#include "wutil.h"  // IWYU pragma: keep

using namespace parse_productions;

static bool production_is_empty(const production_element_t *production) {
    return *production == token_type_invalid;
}

static parse_error_code_t parse_error_from_tokenizer_error(tokenizer_error_t err) {
    switch (err) {
        case tokenizer_error_t::none:
            return parse_error_none;
        case tokenizer_error_t::unterminated_quote:
            return parse_error_tokenizer_unterminated_quote;
        case tokenizer_error_t::unterminated_subshell:
            return parse_error_tokenizer_unterminated_subshell;
        case tokenizer_error_t::unterminated_slice:
            return parse_error_tokenizer_unterminated_slice;
        case tokenizer_error_t::unterminated_escape:
            return parse_error_tokenizer_unterminated_escape;
        default:
            return parse_error_tokenizer_other;
    }
}

/// Returns a string description of this parse error.
wcstring parse_error_t::describe_with_prefix(const wcstring &src, const wcstring &prefix,
                                             bool is_interactive, bool skip_caret) const {
    if (skip_caret && this->text.empty()) return L"";

    wcstring result = prefix;
    result.append(this->text);
    if (skip_caret || source_start >= src.size() || source_start + source_length > src.size()) {
        return result;
    }

    // Locate the beginning of this line of source.
    size_t line_start = 0;

    // Look for a newline prior to source_start. If we don't find one, start at the beginning of
    // the string; otherwise start one past the newline. Note that source_start may itself point
    // at a newline; we want to find the newline before it.
    if (source_start > 0) {
        size_t newline = src.find_last_of(L'\n', source_start - 1);
        if (newline != wcstring::npos) {
            line_start = newline + 1;
        }
    }

    // Look for the newline after the source range. If the source range itself includes a
    // newline, that's the one we want, so start just before the end of the range.
    size_t last_char_in_range =
        (source_length == 0 ? source_start : source_start + source_length - 1);
    size_t line_end = src.find(L'\n', last_char_in_range);
    if (line_end == wcstring::npos) {
        line_end = src.size();
    }

    assert(line_end >= line_start);
    assert(source_start >= line_start);

    // Don't include the caret and line if we're interactive this is the first line, because
    // then it's obvious.
    bool interactive_skip_caret = is_interactive && source_start == 0;
    if (interactive_skip_caret) {
        return result;
    }

    // Append the line of text.
    if (!result.empty()) result.push_back(L'\n');
    result.append(src, line_start, line_end - line_start);

    // Append the caret line. The input source may include tabs; for that reason we
    // construct a "caret line" that has tabs in corresponding positions.
    wcstring caret_space_line;
    caret_space_line.reserve(source_start - line_start);
    for (size_t i = line_start; i < source_start; i++) {
        wchar_t wc = src.at(i);
        if (wc == L'\t') {
            caret_space_line.push_back(L'\t');
        } else if (wc == L'\n') {
            // It's possible that the source_start points at a newline itself. In that case,
            // pretend it's a space. We only expect this to be at the end of the string.
            caret_space_line.push_back(L' ');
        } else {
            int width = fish_wcwidth(wc);
            if (width > 0) {
                caret_space_line.append(static_cast<size_t>(width), L' ');
            }
        }
    }
    result.push_back(L'\n');
    result.append(caret_space_line);
    result.push_back(L'^');
    return result;
}

wcstring parse_error_t::describe(const wcstring &src) const {
    return this->describe_with_prefix(src, wcstring(), shell_is_interactive(), false);
}

void parse_error_offset_source_start(parse_error_list_t *errors, size_t amt) {
    assert(errors != NULL);
    if (amt > 0) {
        size_t i, max = errors->size();
        for (i = 0; i < max; i++) {
            parse_error_t *error = &errors->at(i);
            // Preserve the special meaning of -1 as 'unknown'.
            if (error->source_start != SOURCE_LOCATION_UNKNOWN) {
                error->source_start += amt;
            }
        }
    }
}

/// Returns a string description for the given token type.
const wchar_t *token_type_description(parse_token_type_t type) {
    const wchar_t *description = enum_to_str(type, token_enum_map);
    if (description) return description;
    return L"unknown_token_type";
}

const wchar_t *keyword_description(parse_keyword_t type) {
    const wchar_t *keyword = enum_to_str(type, keyword_enum_map);
    if (keyword) return keyword;
    return L"unknown_keyword";
}

static wcstring token_type_user_presentable_description(
    parse_token_type_t type, parse_keyword_t keyword = parse_keyword_none) {
    if (keyword != parse_keyword_none) {
        return format_string(L"keyword '%ls'", keyword_description(keyword));
    }

    switch (type) {
        // Hackish. We only support the following types.
        case symbol_statement:
            return L"a command";
        case symbol_argument:
            return L"an argument";
        case symbol_job:
        case symbol_job_list:
            return L"a job";
        case parse_token_type_string:
            return L"a string";
        case parse_token_type_pipe:
            return L"a pipe";
        case parse_token_type_redirection:
            return L"a redirection";
        case parse_token_type_background:
            return L"a '&'";
        case parse_token_type_andand:
            return L"'&&'";
        case parse_token_type_oror:
            return L"'||'";
        case parse_token_type_end:
            return L"end of the statement";
        case parse_token_type_terminate:
            return L"end of the input";
        default: { return format_string(L"a %ls", token_type_description(type)); }
    }
}

static wcstring block_type_user_presentable_description(parse_token_type_t type) {
    switch (type) {
        case symbol_for_header: {
            return L"for loop";
        }
        case symbol_while_header: {
            return L"while loop";
        }
        case symbol_function_header: {
            return L"function definition";
        }
        case symbol_begin_header: {
            return L"begin";
        }
        case symbol_if_statement: {
            return L"if statement";
        }
        case symbol_switch_statement: {
            return L"switch statement";
        }
        default: { return token_type_description(type); }
    }
}

/// Returns a string description of the given parse node.
wcstring parse_node_t::describe() const {
    wcstring result = token_type_description(this->type);
    return result;
}

/// Returns a string description of the given parse token.
wcstring parse_token_t::describe() const {
    wcstring result = token_type_description(type);
    if (keyword != parse_keyword_none) {
        append_format(result, L" <%ls>", keyword_description(keyword));
    }
    return result;
}

/// A string description appropriate for presentation to the user.
wcstring parse_token_t::user_presentable_description() const {
    return token_type_user_presentable_description(type, keyword);
}

/// Convert from tokenizer_t's token type to a parse_token_t type.
static inline parse_token_type_t parse_token_type_from_tokenizer_token(
    enum token_type tokenizer_token_type) {
    switch (tokenizer_token_type) {
        case TOK_NONE:
            DIE("TOK_NONE passed to parse_token_type_from_tokenizer_token");
            return token_type_invalid;
        case TOK_STRING:
            return parse_token_type_string;
        case TOK_PIPE:
            return parse_token_type_pipe;
        case TOK_ANDAND:
            return parse_token_type_andand;
        case TOK_OROR:
            return parse_token_type_oror;
        case TOK_END:
            return parse_token_type_end;
        case TOK_BACKGROUND:
            return parse_token_type_background;
        case TOK_REDIRECT:
            return parse_token_type_redirection;
        case TOK_ERROR:
            return parse_special_type_tokenizer_error;
        case TOK_COMMENT:
            return parse_special_type_comment;
    }
    debug(0, "Bad token type %d passed to %s", (int)tokenizer_token_type, __FUNCTION__);
    DIE("bad token type");
    return token_type_invalid;
}

/// Helper function for parse_dump_tree().
static void dump_tree_recursive(const parse_node_tree_t &nodes, const wcstring &src,
                                node_offset_t node_idx, size_t indent, wcstring *result,
                                size_t *line, node_offset_t *inout_first_node_not_dumped) {
    assert(node_idx < nodes.size());

    // Update first_node_not_dumped. This takes a bit of explanation. While it's true that a parse
    // tree may be a "forest",  its individual trees are "compact," meaning they are not
    // interleaved. Thus we keep track of the largest node index as we descend a tree. One past the
    // largest is the start of the next tree.
    if (*inout_first_node_not_dumped <= node_idx) {
        *inout_first_node_not_dumped = node_idx + 1;
    }

    const parse_node_t &node = nodes.at(node_idx);

    const size_t spacesPerIndent = 2;

    // Unindent statement lists by 1 to flatten them.
    if (node.type == symbol_job_list || node.type == symbol_arguments_or_redirections_list) {
        if (indent > 0) indent -= 1;
    }

    append_format(*result, L"%2lu - %l2u  ", *line, node_idx);
    result->append(indent * spacesPerIndent, L' ');
    result->append(node.describe());
    if (node.child_count > 0) {
        append_format(*result, L" <%lu children>", node.child_count);
    }
    if (node.has_comments()) {
        append_format(*result, L" <has_comments>");
    }
    if (node.has_preceding_escaped_newline()) {
        append_format(*result, L" <preceding_esc_nl>");
    }

    if (node.has_source() && node.type == parse_token_type_string) {
        result->append(L": \"");
        result->append(src, node.source_start, node.source_length);
        result->append(L"\"");
    }

    if (node.type != parse_token_type_string) {
        if (node.has_source()) {
            append_format(*result, L"  [%ld, %ld]", (long)node.source_start,
                          (long)node.source_length);
        } else {
            append_format(*result, L"  [%ld, no src]", (long)node.source_start);
        }
    }

    result->push_back(L'\n');
    ++*line;
    for (node_offset_t child_idx = node.child_start;
         child_idx < node.child_start + node.child_count; child_idx++) {
        dump_tree_recursive(nodes, src, child_idx, indent + 1, result, line,
                            inout_first_node_not_dumped);
    }
}

/// Gives a debugging textual description of a parse tree. Note that this supports "parse forests"
/// too. That is, our tree may not really be a tree, but instead a collection of trees.
wcstring parse_dump_tree(const parse_node_tree_t &nodes, const wcstring &src) {
    if (nodes.empty()) return L"(empty!)";

    node_offset_t first_node_not_dumped = 0;
    size_t line = 0;
    wcstring result;
    while (first_node_not_dumped < nodes.size()) {
        if (first_node_not_dumped > 0) {
            result.append(L"---New Tree---\n");
        }
        dump_tree_recursive(nodes, src, first_node_not_dumped, 0, &result, &line,
                            &first_node_not_dumped);
    }
    return result;
}

/// Struct representing elements of the symbol stack, used in the internal state of the LL parser.
struct parse_stack_element_t {
    enum parse_token_type_t type;
    enum parse_keyword_t keyword;
    node_offset_t node_idx;

    explicit parse_stack_element_t(parse_token_type_t t, node_offset_t idx)
        : type(t), keyword(parse_keyword_none), node_idx(idx) {}

    explicit parse_stack_element_t(production_element_t e, node_offset_t idx)
        : type(production_element_type(e)), keyword(production_element_keyword(e)), node_idx(idx) {}

    wcstring describe() const {
        wcstring result = token_type_description(type);
        if (keyword != parse_keyword_none) {
            append_format(result, L" <%ls>", keyword_description(keyword));
        }
        return result;
    }

    /// Returns a name that we can show to the user, e.g. "a command".
    wcstring user_presentable_description() const {
        return token_type_user_presentable_description(type, keyword);
    }
};

/// The parser itself, private implementation of class parse_t. This is a hand-coded table-driven LL
/// parser. Most hand-coded LL parsers are recursive descent, but recursive descent parsers are
/// difficult to "pause", unlike table-driven parsers.
class parse_ll_t {
    // Traditional symbol stack of the LL parser.
    std::vector<parse_stack_element_t> symbol_stack;
    // Parser output. This is a parse tree, but stored in an array.
    parse_node_tree_t nodes;
    // Whether we ran into a fatal error, including parse errors or tokenizer errors.
    bool fatal_errored;
    // Whether we should collect error messages or not.
    bool should_generate_error_messages;
    // List of errors we have encountered.
    parse_error_list_t errors;
    // The symbol stack can contain terminal types or symbols. Symbols go on to do productions, but
    // terminal types are just matched against input tokens.
    bool top_node_handle_terminal_types(const parse_token_t &token);

    void parse_error_unexpected_token(const wchar_t *expected, parse_token_t token);
    void parse_error(parse_token_t token, parse_error_code_t code, const wchar_t *format, ...);
    void parse_error_at_location(size_t source_start, size_t source_length, size_t error_location,
                                 parse_error_code_t code, const wchar_t *format, ...);
    void parse_error_failed_production(struct parse_stack_element_t &elem, parse_token_t token);
    void parse_error_unbalancing_token(parse_token_t token);

    // Reports an error for an unclosed block, e.g. 'begin;'. Returns true on success, false on
    // failure (e.g. it is not an unclosed block).
    bool report_error_for_unclosed_block();

    // void dump_stack(void) const;

    /// Get the node corresponding to the top element of the stack.
    parse_node_t &node_for_top_symbol() {
        PARSE_ASSERT(!symbol_stack.empty());  //!OCLINT(multiple unary operator)
        const parse_stack_element_t &top_symbol = symbol_stack.back();
        PARSE_ASSERT(top_symbol.node_idx != NODE_OFFSET_INVALID);
        PARSE_ASSERT(top_symbol.node_idx < nodes.size());
        return nodes.at(top_symbol.node_idx);
    }

    /// Pop from the top of the symbol stack, then push the given production, updating node counts.
    /// Note that production_element_t has type "pointer to array" so some care is required.
    inline void symbol_stack_pop_push_production(const production_element_t *production) {
        bool logit = false;
        if (logit) {
            int count = 0;
            fwprintf(stderr, L"Applying production:\n");
            for (int i = 0;; i++) {
                production_element_t elem = production[i];
                if (!production_element_is_valid(elem)) break;  // all done, bail out
                parse_token_type_t type = production_element_type(elem);
                parse_keyword_t keyword = production_element_keyword(elem);
                fwprintf(stderr, L"\t%ls <%ls>\n", token_type_description(type),
                         keyword_description(keyword));
                count++;
            }
            if (!count) fwprintf(stderr, L"\t<empty>\n");
        }

        // Get the parent index. But we can't get the parent parse node yet, since it may be made
        // invalid by adding children.
        const node_offset_t parent_node_idx = symbol_stack.back().node_idx;

        // Add the children. Confusingly, we want our nodes to be in forwards order (last token
        // last, so dumps look nice), but the symbols should be reverse order (last token first, so
        // it's lowest on the stack)
        const size_t child_start_big = nodes.size();
        assert(child_start_big < NODE_OFFSET_INVALID);
        node_offset_t child_start = static_cast<node_offset_t>(child_start_big);

        // To avoid constructing multiple nodes, we make a single one that we modify.
        parse_node_t representative_child(token_type_invalid);
        representative_child.parent = parent_node_idx;

        node_offset_t child_count = 0;
        for (int i = 0;; i++) {
            production_element_t elem = production[i];
            if (!production_element_is_valid(elem)) break;  // all done, bail out
            // Append the parse node.
            representative_child.type = production_element_type(elem);
            nodes.push_back(representative_child);
            child_count++;
        }

        // Update the parent.
        parse_node_t &parent_node = nodes.at(parent_node_idx);

        // Should have no children yet.
        PARSE_ASSERT(parent_node.child_count == 0);

        // Tell the node about its children.
        parent_node.child_start = child_start;
        parent_node.child_count = child_count;

        // Replace the top of the stack with new stack elements corresponding to our new nodes. Note
        // that these go in reverse order.
        symbol_stack.pop_back();
        symbol_stack.reserve(symbol_stack.size() + child_count);
        node_offset_t idx = child_count;
        while (idx--) {
            production_element_t elem = production[idx];
            PARSE_ASSERT(production_element_is_valid(elem));
            symbol_stack.push_back(parse_stack_element_t(elem, child_start + idx));
        }
    }

   public:
    // Constructor
    explicit parse_ll_t(enum parse_token_type_t goal)
        : fatal_errored(false), should_generate_error_messages(true) {
        this->symbol_stack.reserve(16);
        this->nodes.reserve(64);
        this->reset_symbols_and_nodes(goal);
    }

    // Input
    void accept_tokens(parse_token_t token1, parse_token_t token2);

    /// Report tokenizer errors.
    void report_tokenizer_error(const tok_t &tok);

    /// Indicate if we hit a fatal error.
    bool has_fatal_error() const { return this->fatal_errored; }

    /// Indicate whether we want to generate error messages.
    void set_should_generate_error_messages(bool flag) {
        this->should_generate_error_messages = flag;
    }

    /// Clear the parse symbol stack (but not the node tree). Add a node of the given type as the
    /// goal node. This is called from the constructor.
    void reset_symbols(enum parse_token_type_t goal);

    /// Clear the parse symbol stack and the node tree. Add a node of the given type as the goal
    /// node. This is called from the constructor.
    void reset_symbols_and_nodes(enum parse_token_type_t goal);

    /// Once parsing is complete, determine the ranges of intermediate nodes.
    void determine_node_ranges();

    /// Acquire output after parsing. This transfers directly from within self.
    void acquire_output(parse_node_tree_t *output, parse_error_list_t *errors);
};

#if 0
void parse_ll_t::dump_stack(void) const {
    // Walk backwards from the top, looking for parents.
    wcstring_list_t stack_lines;
    if (symbol_stack.empty()) {
        stack_lines.push_back(L"(empty)");
    } else {
        node_offset_t child = symbol_stack.back().node_idx;
        node_offset_t cursor = child;
        stack_lines.push_back(nodes.at(cursor).describe());
        while (cursor--) {
            const parse_node_t &node = nodes.at(cursor);
            if (node.child_start <= child && node.child_start + node.child_count > child) {
                stack_lines.push_back(node.describe());
                child = cursor;
            }
        }
    }

    fwprintf(stderr, L"Stack dump (%zu elements):\n", symbol_stack.size());
    for (size_t idx = 0; idx < stack_lines.size(); idx++) {
        fwprintf(stderr, L"    %ls\n", stack_lines.at(idx).c_str());
    }
}
#endif

// Give each node a source range equal to the union of the ranges of its children. Terminal nodes
// already have source ranges (and no children). Since children always appear after their parents,
// we can implement this very simply by walking backwards. We then do a second pass to give empty
// nodes an empty source range (but with a valid offset). We do this by walking forward. If a child
// of a node has an invalid source range, we set it equal to the end of the source range of its
// previous child.
void parse_ll_t::determine_node_ranges() {
    size_t idx = nodes.size();
    while (idx--) {
        parse_node_t *parent = &nodes[idx];

        // Skip nodes that already have a source range. These are terminal nodes.
        if (parent->source_start != SOURCE_OFFSET_INVALID) continue;

        // Ok, this node needs a source range. Get all of its children, and then set its range.
        source_offset_t min_start = SOURCE_OFFSET_INVALID,
                        max_end = 0;  // note SOURCE_OFFSET_INVALID is huge
        for (node_offset_t i = 0; i < parent->child_count; i++) {
            const parse_node_t &child = nodes.at(parent->child_offset(i));
            if (child.has_source()) {
                min_start = std::min(min_start, child.source_start);
                max_end = std::max(max_end, child.source_start + child.source_length);
            }
        }

        if (min_start != SOURCE_OFFSET_INVALID) {
            assert(max_end >= min_start);
            parent->source_start = min_start;
            parent->source_length = max_end - min_start;
        }
    }

    // Forward pass.
    size_t size = nodes.size();
    for (idx = 0; idx < size; idx++) {
        // Since we populate the source range based on the sibling node, it's simpler to walk over
        // the children of each node. We keep a running "child_source_cursor" which is meant to be
        // the end of the child's source range. It's initially set to the beginning of the parent'
        // source range.
        parse_node_t *parent = &nodes[idx];
        // If the parent doesn't have a valid source range, then none of its children will either;
        // skip it entirely.
        if (parent->source_start == SOURCE_OFFSET_INVALID) {
            continue;
        }
        source_offset_t child_source_cursor = parent->source_start;
        for (size_t child_idx = 0; child_idx < parent->child_count; child_idx++) {
            parse_node_t *child = &nodes[parent->child_start + child_idx];
            if (child->source_start == SOURCE_OFFSET_INVALID) {
                child->source_start = child_source_cursor;
            }
            child_source_cursor = child->source_start + child->source_length;
        }
    }
}

void parse_ll_t::acquire_output(parse_node_tree_t *output, parse_error_list_t *errors) {
    if (output != NULL) {
        *output = std::move(this->nodes);
    }
    if (errors != NULL) {
        *errors = std::move(this->errors);
    }
}

void parse_ll_t::parse_error(parse_token_t token, parse_error_code_t code, const wchar_t *fmt,
                             ...) {
    this->fatal_errored = true;
    if (this->should_generate_error_messages) {
        // this->dump_stack();
        parse_error_t err;

        va_list va;
        va_start(va, fmt);
        err.text = vformat_string(fmt, va);
        err.code = code;
        va_end(va);

        err.source_start = token.source_start;
        err.source_length = token.source_length;
        this->errors.push_back(err);
    }
}

void parse_ll_t::parse_error_at_location(size_t source_start, size_t source_length,
                                         size_t error_location, parse_error_code_t code,
                                         const wchar_t *fmt, ...) {
    (void)error_location;
    this->fatal_errored = true;
    if (this->should_generate_error_messages) {
        // this->dump_stack();
        parse_error_t err;

        va_list va;
        va_start(va, fmt);
        err.text = vformat_string(fmt, va);
        err.code = code;
        va_end(va);

        err.source_start = source_start;
        err.source_length = source_length;
        this->errors.push_back(err);
    }
}

// Unbalancing token. This includes 'else' or 'case' or 'end' outside of the appropriate block
// This essentially duplicates some logic from resolving the production for symbol_statement_list -
// yuck.
void parse_ll_t::parse_error_unbalancing_token(parse_token_t token) {
    this->fatal_errored = true;
    if (this->should_generate_error_messages) {
        switch (token.keyword) {
            case parse_keyword_end: {
                this->parse_error(token, parse_error_unbalancing_end, L"'end' outside of a block");
                break;
            }
            case parse_keyword_else: {
                this->parse_error(token, parse_error_unbalancing_else,
                                  L"'else' builtin not inside of if block");
                break;
            }
            case parse_keyword_case: {
                this->parse_error(token, parse_error_unbalancing_case,
                                  L"'case' builtin not inside of switch block");
                break;
            }
            default: {
                // At the moment, this case should only be hit if you parse a
                // freestanding_argument_list. For example, 'complete -c foo -a 'one & three'.
                // Hackish error message for that case.
                if (!symbol_stack.empty() &&
                    symbol_stack.back().type == symbol_freestanding_argument_list) {
                    this->parse_error(
                        token, parse_error_generic, L"Expected %ls, but found %ls",
                        token_type_user_presentable_description(symbol_argument).c_str(),
                        token.user_presentable_description().c_str());
                } else {
                    this->parse_error(token, parse_error_generic, L"Did not expect %ls",
                                      token.user_presentable_description().c_str());
                }
                break;
            }
        }
    }
}

/// This is a 'generic' parse error when we can't match the top of the stack element.
void parse_ll_t::parse_error_failed_production(struct parse_stack_element_t &stack_elem,
                                               parse_token_t token) {
    fatal_errored = true;
    if (this->should_generate_error_messages) {
        const wcstring expected = stack_elem.user_presentable_description();
        this->parse_error_unexpected_token(expected.c_str(), token);
    }
}

void parse_ll_t::report_tokenizer_error(const tok_t &tok) {
    parse_error_code_t parse_error_code = parse_error_from_tokenizer_error(tok.error);
    this->parse_error_at_location(tok.offset, tok.length, tok.offset + tok.error_offset,
                                  parse_error_code, L"%ls",
                                  tokenizer_get_error_message(tok.error).c_str());
}

void parse_ll_t::parse_error_unexpected_token(const wchar_t *expected, parse_token_t token) {
    fatal_errored = true;
    if (this->should_generate_error_messages) {
        this->parse_error(token, parse_error_generic, L"Expected %ls, but instead found %ls",
                          expected, token.user_presentable_description().c_str());
    }
}

void parse_ll_t::reset_symbols(enum parse_token_type_t goal) {
    // Add a new goal node, and then reset our symbol list to point at it.
    node_offset_t where = static_cast<node_offset_t>(nodes.size());
    nodes.push_back(parse_node_t(goal));

    symbol_stack.clear();
    symbol_stack.push_back(parse_stack_element_t(goal, where));  // goal token
    this->fatal_errored = false;
}

/// Reset both symbols and nodes.
void parse_ll_t::reset_symbols_and_nodes(enum parse_token_type_t goal) {
    nodes.clear();
    this->reset_symbols(goal);
}

static bool type_is_terminal_type(parse_token_type_t type) {
    switch (type) {
        case parse_token_type_string:
        case parse_token_type_pipe:
        case parse_token_type_redirection:
        case parse_token_type_background:
        case parse_token_type_end:
        case parse_token_type_andand:
        case parse_token_type_oror:
        case parse_token_type_terminate: {
            return true;
        }
        default: { return false; }
    }
}

bool parse_ll_t::report_error_for_unclosed_block() {
    bool reported_error = false;
    // Unclosed block, for example, 'while true ; '. We want to show the block node that opened it.
    const parse_node_t &top_node = this->node_for_top_symbol();

    // Hacktastic. We want to point at the source location of the block, but our block doesn't have
    // a source range yet - only the terminal tokens do. So get the block statement corresponding to
    // this end command. In general this block may be of a variety of types: if_statement,
    // switch_statement, etc., each with different node structures. But keep descending the first
    // child and eventually you hit a keyword: begin, if, etc. That's the keyword we care about.
    const parse_node_t *end_command = this->nodes.get_parent(top_node, symbol_end_command);
    const parse_node_t *block_node = end_command ? this->nodes.get_parent(*end_command) : NULL;

    if (block_node && block_node->type == symbol_block_statement) {
        // Get the header.
        block_node = this->nodes.get_child(*block_node, 0, symbol_block_header);
        block_node = this->nodes.get_child(*block_node, 0);  // specific statement
    }
    if (block_node == NULL) {
        return reported_error;
    }

    // block_node is now an if_statement, switch_statement, for_header, while_header,
    // function_header, or begin_header.
    //
    // Hackish: descend down the first node until we reach the bottom. This will be a keyword
    // node like SWITCH, which will have the source range. Ordinarily the source range would be
    // known by the parent node too, but we haven't completed parsing yet, so we haven't yet
    // propagated source ranges.
    const parse_node_t *cursor = block_node;
    while (cursor->child_count > 0) {
        cursor = this->nodes.get_child(*cursor, 0);
        assert(cursor != NULL);
    }
    if (cursor->source_start != NODE_OFFSET_INVALID) {
        const wcstring node_desc = block_type_user_presentable_description(block_node->type);
        this->parse_error_at_location(cursor->source_start, 0, cursor->source_start,
                                      parse_error_generic, L"Missing end to balance this %ls",
                                      node_desc.c_str());
        reported_error = true;
    }
    return reported_error;
}

bool parse_ll_t::top_node_handle_terminal_types(const parse_token_t &token) {
    PARSE_ASSERT(!symbol_stack.empty());  //!OCLINT(multiple unary operator)
    PARSE_ASSERT(token.type >= FIRST_PARSE_TOKEN_TYPE);
    parse_stack_element_t &stack_top = symbol_stack.back();

    if (!type_is_terminal_type(stack_top.type)) {
        return false;  // was not handled
    }

    // The top of the stack is terminal. We are going to handle this (because we can't produce
    // from a terminal type).

    // Now see if we actually matched
    bool matched = false;
    if (stack_top.type == token.type) {
        if (stack_top.type == parse_token_type_string) {
            // We matched if the keywords match, or no keyword was required.
            matched =
                (stack_top.keyword == parse_keyword_none || stack_top.keyword == token.keyword);
        } else {
            // For other types, we only require that the types match.
            matched = true;
        }
    }

    if (matched) {
        // Success. Tell the node that it matched this token, and what its source range is in
        // the parse phase, we only set source ranges for terminal types. We propagate ranges to
        // parent nodes afterwards.
        parse_node_t &node = node_for_top_symbol();
        node.keyword = token.keyword;
        node.source_start = token.source_start;
        node.source_length = token.source_length;
        if (token.preceding_escaped_nl) node.flags |= parse_node_flag_preceding_escaped_nl;
    } else {
        // Failure
        if (stack_top.type == parse_token_type_string && token.type == parse_token_type_string) {
            // Keyword failure. We should unify this with the 'matched' computation above.
            assert(stack_top.keyword != parse_keyword_none && stack_top.keyword != token.keyword);

            // Check to see which keyword we got which was considered wrong.
            switch (token.keyword) {
                // Some keywords are only valid in certain contexts. If this cascaded all the
                // way down through the outermost job_list, it was not in a valid context.
                case parse_keyword_case:
                case parse_keyword_end:
                case parse_keyword_else: {
                    this->parse_error_unbalancing_token(token);
                    break;
                }
                case parse_keyword_none: {
                    // This is a random other string (not a keyword).
                    const wcstring expected = keyword_description(stack_top.keyword);
                    this->parse_error(token, parse_error_generic, L"Expected keyword '%ls'",
                                      expected.c_str());
                    break;
                }
                default: {
                    // Got a real keyword we can report.
                    const wcstring actual =
                        (token.keyword == parse_keyword_none ? token.describe()
                                                             : keyword_description(token.keyword));
                    const wcstring expected = keyword_description(stack_top.keyword);
                    this->parse_error(token, parse_error_generic,
                                      L"Expected keyword '%ls', instead got keyword '%ls'",
                                      expected.c_str(), actual.c_str());
                    break;
                }
            }
        } else if (stack_top.keyword == parse_keyword_end &&
                   token.type == parse_token_type_terminate &&
                   this->report_error_for_unclosed_block()) {
            ;  // handled by report_error_for_unclosed_block
        } else {
            const wcstring expected = stack_top.user_presentable_description();
            this->parse_error_unexpected_token(expected.c_str(), token);
        }
    }

    // We handled the token, so pop the symbol stack.
    symbol_stack.pop_back();
    return true;
}

void parse_ll_t::accept_tokens(parse_token_t token1, parse_token_t token2) {
    PARSE_ASSERT(token1.type >= FIRST_PARSE_TOKEN_TYPE);

    // Handle special types specially. Note that these are the only types that can be pushed if the
    // symbol stack is empty.
    if (token1.type == parse_special_type_parse_error ||
        token1.type == parse_special_type_tokenizer_error ||
        token1.type == parse_special_type_comment) {
        // We set the special node's parent to the top of the stack. This means that we have an
        // asymmetric relationship: the special node has a parent (which is the node we were trying
        // to generate when we encountered the special node), but the parent node does not have the
        // special node as a child. This means for example that parents don't have to worry about
        // tracking any comment nodes, but we can still recover the parent from the comment.
        parse_node_t special_node(token1.type);
        special_node.parent = symbol_stack.back().node_idx;
        special_node.source_start = token1.source_start;
        special_node.source_length = token1.source_length;
        if (token1.preceding_escaped_nl) special_node.flags |= parse_node_flag_preceding_escaped_nl;
        nodes.push_back(special_node);

        // Mark special flags.
        if (token1.type == parse_special_type_comment) {
            this->node_for_top_symbol().flags |= parse_node_flag_has_comments;
        }

        // Tokenizer errors are fatal.
        if (token1.type == parse_special_type_tokenizer_error) this->fatal_errored = true;
        return;
    }

    // It's not a special type.
    while (!this->fatal_errored) {
        PARSE_ASSERT(!symbol_stack.empty());  //!OCLINT(multiple unary operator)

        if (top_node_handle_terminal_types(token1)) {
            break;
        }

        // top_node_match_token may indicate an error if our stack is empty.
        if (this->fatal_errored) break;

        // Get the production for the top of the stack.
        parse_stack_element_t &stack_elem = symbol_stack.back();
        parse_node_t &node = nodes.at(stack_elem.node_idx);
        parse_node_tag_t tag = 0;
        const production_element_t *production =
            production_for_token(stack_elem.type, token1, token2, &tag);
        node.tag = tag;
        if (production == NULL) {
            parse_error_failed_production(stack_elem, token1);
            // The above sets fatal_errored, which ends the loop.
        } else {
            bool is_terminate = (token1.type == parse_token_type_terminate);

            // When a job_list encounters something like 'else', it returns an empty production to
            // return control to the outer block. But if it's unbalanced, then we'll end up with an
            // empty stack! So make sure that doesn't happen. This is the primary mechanism by which
            // we detect e.g. unbalanced end. However, if we get a true terminate token, then we
            // allow (expect) this to empty the stack.
            if (symbol_stack.size() == 1 && production_is_empty(production) && !is_terminate) {
                this->parse_error_unbalancing_token(token1);
                break;
            }

            // Manipulate the symbol stack. Note that stack_elem is invalidated by popping the
            // stack.
            symbol_stack_pop_push_production(production);

            // Expect to not have an empty stack, unless this was the terminate type. Note we may
            // not have an empty stack with the terminate type (i.e. incomplete input).
            assert(is_terminate || !symbol_stack.empty());

            if (symbol_stack.empty()) {
                break;
            }
        }
    }
}

// Given an expanded string, returns any keyword it matches.
static inline parse_keyword_t keyword_with_name(const wchar_t *name) {
    return str_to_enum(name, keyword_enum_map, keyword_enum_map_len);
}

static bool is_keyword_char(wchar_t c) {
    return (c >= L'a' && c <= L'z') || (c >= L'A' && c <= L'Z') || (c >= L'0' && c <= L'9') ||
           c == L'\'' || c == L'"' || c == L'\\' || c == '\n' || c == L'!';
}

/// Given a token, returns the keyword it matches, or parse_keyword_none.
static parse_keyword_t keyword_for_token(token_type tok, const wcstring &token) {
    /* Only strings can be keywords */
    if (tok != TOK_STRING) {
        return parse_keyword_none;
    }

    // If tok_txt is clean (which most are), we can compare it directly. Otherwise we have to expand
    // it. We only expand quotes, and we don't want to do expensive expansions like tilde
    // expansions. So we do our own "cleanliness" check; if we find a character not in our allowed
    // set we know it's not a keyword, and if we never find a quote we don't have to expand! Note
    // that this lowercase set could be shrunk to be just the characters that are in keywords.
    parse_keyword_t result = parse_keyword_none;
    bool needs_expand = false, all_chars_valid = true;
    const wchar_t *tok_txt = token.c_str();
    for (size_t i = 0; tok_txt[i] != L'\0'; i++) {
        wchar_t c = tok_txt[i];
        if (!is_keyword_char(c)) {
            all_chars_valid = false;
            break;
        }
        // If we encounter a quote, we need expansion.
        needs_expand = needs_expand || c == L'"' || c == L'\'' || c == L'\\';
    }

    if (all_chars_valid) {
        // Expand if necessary.
        if (!needs_expand) {
            result = keyword_with_name(tok_txt);
        } else {
            wcstring storage;
            if (unescape_string(tok_txt, &storage, 0)) {
                result = keyword_with_name(storage.c_str());
            }
        }
    }
    return result;
}

/// Placeholder invalid token.
static constexpr parse_token_t kInvalidToken{token_type_invalid};

/// Terminal token.
static constexpr parse_token_t kTerminalToken = {parse_token_type_terminate};

static inline bool is_help_argument(const wcstring &txt) {
    return txt == L"-h" || txt == L"--help";
}

/// Return a new parse token, advancing the tokenizer.
static inline parse_token_t next_parse_token(tokenizer_t *tok, tok_t *token, wcstring *storage) {
    if (!tok->next(token)) {
        return kTerminalToken;
    }

    // Set the type, keyword, and whether there's a dash prefix. Note that this is quite sketchy,
    // because it ignores quotes. This is the historical behavior. For example, `builtin --names`
    // lists builtins, but `builtin "--names"` attempts to run --names as a command. Amazingly as of
    // this writing (10/12/13) nobody seems to have noticed this. Squint at it really hard and it
    // even starts to look like a feature.
    parse_token_t result{parse_token_type_from_tokenizer_token(token->type)};
    const wcstring &text = tok->copy_text_of(*token, storage);
    result.keyword = keyword_for_token(token->type, text);
    result.has_dash_prefix = !text.empty() && text.at(0) == L'-';
    result.is_help_argument = result.has_dash_prefix && is_help_argument(text);
    result.is_newline = (result.type == parse_token_type_end && text == L"\n");
    result.preceding_escaped_nl = token->preceding_escaped_nl;

    // These assertions are totally bogus. Basically our tokenizer works in size_t but we work in
    // uint32_t to save some space. If we have a source file larger than 4 GB, we'll probably just
    // crash.
    assert(token->offset < SOURCE_OFFSET_INVALID);
    result.source_start = (source_offset_t)token->offset;

    assert(token->length <= SOURCE_OFFSET_INVALID);
    result.source_length = (source_offset_t)token->length;

    return result;
}

bool parse_tree_from_string(const wcstring &str, parse_tree_flags_t parse_flags,
                            parse_node_tree_t *output, parse_error_list_t *errors,
                            parse_token_type_t goal) {
    parse_ll_t parser(goal);
    parser.set_should_generate_error_messages(errors != NULL);

    // A string whose storage we reuse.
    wcstring storage;

    // Construct the tokenizer.
    tok_flags_t tok_options = 0;
    if (parse_flags & parse_flag_include_comments) tok_options |= TOK_SHOW_COMMENTS;

    if (parse_flags & parse_flag_accept_incomplete_tokens) tok_options |= TOK_ACCEPT_UNFINISHED;

    if (parse_flags & parse_flag_show_blank_lines) tok_options |= TOK_SHOW_BLANK_LINES;

    tokenizer_t tok(str.c_str(), tok_options);

    // We are an LL(2) parser. We pass two tokens at a time. New tokens come in at index 1. Seed our
    // queue with an initial token at index 1.
    parse_token_t queue[2] = {kInvalidToken, kInvalidToken};

    // Loop until we have a terminal token.
    tok_t tokenizer_token;
    for (size_t token_count = 0; queue[0].type != parse_token_type_terminate; token_count++) {
        // Push a new token onto the queue.
        queue[0] = queue[1];
        queue[1] = next_parse_token(&tok, &tokenizer_token, &storage);

        // If we are leaving things unterminated, then don't pass parse_token_type_terminate.
        if (queue[0].type == parse_token_type_terminate &&
            (parse_flags & parse_flag_leave_unterminated)) {
            break;
        }

        // Pass these two tokens, unless we're still loading the queue. We know that queue[0] is
        // valid; queue[1] may be invalid.
        if (token_count > 0) {
            parser.accept_tokens(queue[0], queue[1]);
        }

        // Handle tokenizer errors. This is a hack because really the parser should report this for
        // itself; but it has no way of getting the tokenizer message.
        if (queue[1].type == parse_special_type_tokenizer_error) {
            parser.report_tokenizer_error(tokenizer_token);
        }

        if (!parser.has_fatal_error()) {
            continue;
        }

        // Handle errors.
        if (!(parse_flags & parse_flag_continue_after_error)) {
            break;  // bail out
        }
        // Hack. Typically the parse error is due to the first token. However, if it's a
        // tokenizer error, then has_fatal_error was set due to the check above; in that
        // case the second token is what matters.
        size_t error_token_idx = 0;
        if (queue[1].type == parse_special_type_tokenizer_error) {
            error_token_idx = (queue[1].type == parse_special_type_tokenizer_error ? 1 : 0);
            token_count = -1;  // so that it will be 0 after incrementing, and our tokenizer
                               // error will be ignored
        }

        // Mark a special error token, and then keep going.
        parse_token_t token = {parse_special_type_parse_error};
        token.source_start = queue[error_token_idx].source_start;
        token.source_length = queue[error_token_idx].source_length;
        parser.accept_tokens(token, kInvalidToken);
        parser.reset_symbols(goal);
    }

    // Teach each node where its source range is.
    parser.determine_node_ranges();

    // Acquire the output from the parser.
    parser.acquire_output(output, errors);

    // Indicate if we had a fatal error.
    return !parser.has_fatal_error();
}

const parse_node_t *parse_node_tree_t::get_child(const parse_node_t &parent, node_offset_t which,
                                                 parse_token_type_t expected_type) const {
    const parse_node_t *result = NULL;

    // We may get nodes with no children if we had an incomplete parse. Don't consider than an
    // error.
    if (parent.child_count > 0) {
        PARSE_ASSERT(which < parent.child_count);
        node_offset_t child_offset = parent.child_offset(which);
        if (child_offset < this->size()) {
            result = &this->at(child_offset);

            // If we are given an expected type, then the node must be null or that type.
            assert(expected_type == token_type_invalid || expected_type == result->type);
        }
    }

    return result;
}

parsed_source_ref_t parse_source(wcstring src, parse_tree_flags_t flags, parse_error_list_t *errors,
                                 parse_token_type_t goal) {
    parse_node_tree_t tree;
    if (!parse_tree_from_string(src, flags, &tree, errors, goal)) return {};
    return std::make_shared<parsed_source_t>(std::move(src), std::move(tree));
}

const parse_node_t &parse_node_tree_t::find_child(const parse_node_t &parent,
                                                  parse_token_type_t type) const {
    for (node_offset_t i = 0; i < parent.child_count; i++) {
        const parse_node_t *child = this->get_child(parent, i);
        if (child->type == type) {
            return *child;
        }
    }
    DIE("failed to find child node");
}

const parse_node_t *parse_node_tree_t::get_parent(const parse_node_t &node,
                                                  parse_token_type_t expected_type) const {
    const parse_node_t *result = NULL;
    if (node.parent != NODE_OFFSET_INVALID) {
        PARSE_ASSERT(node.parent < this->size());
        const parse_node_t &parent = this->at(node.parent);
        if (expected_type == token_type_invalid || expected_type == parent.type) {
            // The type matches (or no type was requested).
            result = &parent;
        }
    }
    return result;
}

/// Return true if the given node has the proposed ancestor as an ancestor (or is itself that
/// ancestor).
static bool node_has_ancestor(const parse_node_tree_t &tree, const parse_node_t &node,
                              const parse_node_t &proposed_ancestor) {
    if (&node == &proposed_ancestor) {
        return true;  // found it
    } else if (node.parent == NODE_OFFSET_INVALID) {
        return false;  // no more parents
    }

    // Recurse to the parent.
    return node_has_ancestor(tree, tree.at(node.parent), proposed_ancestor);
}

const parse_node_t *parse_node_tree_t::find_node_matching_source_location(
    parse_token_type_t type, size_t source_loc, const parse_node_t *parent) const {
    const parse_node_t *result = NULL;
    // Find nodes of the given type in the tree, working backwards.
    const size_t len = this->size();
    for (size_t idx = 0; idx < len && result == NULL; idx++) {
        const parse_node_t &node = this->at(idx);

        // Types must match.
        if (node.type != type) continue;

        // Must contain source location.
        if (!node.location_in_or_at_end_of_source_range(source_loc)) continue;

        // If a parent is given, it must be an ancestor.
        if (parent != NULL && !node_has_ancestor(*this, node, *parent)) continue;

        // Found it.
        result = &node;
    }

    return result;
}

const parse_node_t *parse_node_tree_t::find_last_node_of_type(parse_token_type_t type,
                                                              const parse_node_t *parent) const {
    const parse_node_t *result = NULL;
    // Find nodes of the given type in the tree, working backwards.
    size_t idx = this->size();
    while (idx--) {
        const parse_node_t &node = this->at(idx);
        bool expected_type = (node.type == type);
        if (expected_type && (parent == NULL || node_has_ancestor(*this, node, *parent))) {
            // The types match and it has the right parent.
            result = &node;
            break;
        }
    }
    return result;
}
