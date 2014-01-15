#include "parse_productions.h"
#include "tokenizer.h"
#include "fallback.h"
#include "wutil.h"
#include "proc.h"
#include <vector>
#include <algorithm>

using namespace parse_productions;

static bool production_is_empty(const production_t *production)
{
    return (*production)[0] == token_type_invalid;
}

/** Returns a string description of this parse error */
wcstring parse_error_t::describe(const wcstring &src, bool skip_caret) const
{
    wcstring result = text;
    if (! skip_caret && source_start < src.size() && source_start + source_length <= src.size())
    {
        // Locate the beginning of this line of source
        size_t line_start = 0;

        // Look for a newline prior to source_start. If we don't find one, start at the beginning of the string; otherwise start one past the newline. Note that source_start may itself point at a newline; we want to find the newline before it.
        if (source_start > 0)
        {
            size_t newline = src.find_last_of(L'\n', source_start - 1);
            if (newline != wcstring::npos)
            {
                line_start = newline + 1;
            }
        }

        // Look for the newline after the source range. If the source range itself includes a newline, that's the one we want, so start just before the end of the range
        size_t last_char_in_range = (source_length == 0 ? source_start : source_start + source_length - 1);
        size_t line_end = src.find(L'\n', last_char_in_range);
        if (line_end == wcstring::npos)
        {
            line_end = src.size();
        }

        assert(line_end >= line_start);
        assert(source_start >= line_start);

        // Don't include the caret and line if we're interactive this is the first line, because then it's obvious
        bool skip_caret = (get_is_interactive() && source_start == 0);

        if (! skip_caret)
        {
            // Append the line of text.
            if (! result.empty())
            {
                result.push_back(L'\n');
            }
            result.append(src, line_start, line_end - line_start);


            // Append the caret line. The input source may include tabs; for that reason we construct a "caret line" that has tabs in corresponding positions
            wcstring caret_space_line;
            caret_space_line.reserve(source_start - line_start);
            for (size_t i=line_start; i < source_start; i++)
            {
                wchar_t wc = src.at(i);
                if (wc == L'\t')
                {
                    caret_space_line.push_back(L'\t');
                }
                else if (wc == L'\n')
                {
                    /* It's possible that the source_start points at a newline itself. In that case, pretend it's a space. We only expect this to be at the end of the string. */
                    caret_space_line.push_back(L' ');
                }
                else
                {
                    int width = fish_wcwidth(wc);
                    if (width > 0)
                    {
                        caret_space_line.append(static_cast<size_t>(width), L' ');
                    }
                }
            }
            result.push_back(L'\n');
            result.append(caret_space_line);
            result.push_back(L'^');
        }
    }
    return result;
}

wcstring parse_errors_description(const parse_error_list_t &errors, const wcstring &src, const wchar_t *prefix)
{
    wcstring target;
    for (size_t i=0; i < errors.size(); i++)
    {
        if (i > 0)
        {
            target.push_back(L'\n');
        }
        if (prefix != NULL)
        {
            target.append(prefix);
            target.append(L": ");
        }
        target.append(errors.at(i).describe(src));
    }
    return target;
}

/** Returns a string description of the given token type */
wcstring token_type_description(parse_token_type_t type)
{
    switch (type)
    {
        case token_type_invalid:
            return L"invalid";

        case symbol_job_list:
            return L"job_list";
        case symbol_job:
            return L"job";
        case symbol_job_continuation:
            return L"job_continuation";

        case symbol_statement:
            return L"statement";
        case symbol_block_statement:
            return L"block_statement";
        case symbol_block_header:
            return L"block_header";
        case symbol_for_header:
            return L"for_header";
        case symbol_while_header:
            return L"while_header";
        case symbol_begin_header:
            return L"begin_header";
        case symbol_function_header:
            return L"function_header";

        case symbol_if_statement:
            return L"if_statement";
        case symbol_if_clause:
            return L"if_clause";
        case symbol_else_clause:
            return L"else_clause";
        case symbol_else_continuation:
            return L"else_continuation";

        case symbol_switch_statement:
            return L"switch_statement";
        case symbol_case_item_list:
            return L"case_item_list";
        case symbol_case_item:
            return L"case_item";

        case symbol_argument_list:
            return L"argument_list";

        case symbol_boolean_statement:
            return L"boolean_statement";
        case symbol_decorated_statement:
            return L"decorated_statement";
        case symbol_plain_statement:
            return L"plain_statement";
        case symbol_arguments_or_redirections_list:
            return L"arguments_or_redirections_list";
        case symbol_argument_or_redirection:
            return L"argument_or_redirection";
        case symbol_argument:
            return L"symbol_argument";
        case symbol_redirection:
            return L"symbol_redirection";
        case symbol_optional_background:
            return L"optional_background";
        case symbol_end_command:
            return L"symbol_end_command";


        case parse_token_type_string:
            return L"token_string";
        case parse_token_type_pipe:
            return L"token_pipe";
        case parse_token_type_redirection:
            return L"token_redirection";
        case parse_token_type_background:
            return L"token_background";
        case parse_token_type_end:
            return L"token_end";
        case parse_token_type_terminate:
            return L"token_terminate";


        case parse_special_type_parse_error:
            return L"parse_error";
        case parse_special_type_tokenizer_error:
            return L"tokenizer_error";
        case parse_special_type_comment:
            return L"comment";

    }
    return format_string(L"Unknown token type %ld", static_cast<long>(type));
}

wcstring keyword_description(parse_keyword_t k)
{
    switch (k)
    {
        case parse_keyword_none:
            return L"none";
        case parse_keyword_if:
            return L"if";
        case parse_keyword_else:
            return L"else";
        case parse_keyword_for:
            return L"for";
        case parse_keyword_in:
            return L"in";
        case parse_keyword_while:
            return L"while";
        case parse_keyword_begin:
            return L"begin";
        case parse_keyword_function:
            return L"function";
        case parse_keyword_switch:
            return L"switch";
        case parse_keyword_case:
            return L"case";
        case parse_keyword_end:
            return L"end";
        case parse_keyword_and:
            return L"and";
        case parse_keyword_or:
            return L"or";
        case parse_keyword_not:
            return L"not";
        case parse_keyword_command:
            return L"command";
        case parse_keyword_builtin:
            return L"builtin";
    }
    return format_string(L"Unknown keyword type %ld", static_cast<long>(k));
}

static wcstring token_type_user_presentable_description(parse_token_type_t type, parse_keyword_t keyword)
{
    if (keyword != parse_keyword_none)
    {
        return format_string(L"keyword '%ls'", keyword_description(keyword).c_str());
    }

    switch (type)
    {
            /* Hackish. We only support the following types. */
        case symbol_statement:
            return L"a command";

        case parse_token_type_string:
            return L"a string";

        case parse_token_type_pipe:
            return L"a pipe";

        case parse_token_type_redirection:
            return L"a redirection";

        case parse_token_type_background:
            return L"a '&'";

        case parse_token_type_end:
            return L"end of the statement";

        default:
            return format_string(L"a %ls", token_type_description(type).c_str());
    }
}

/** Returns a string description of the given parse node */
wcstring parse_node_t::describe(void) const
{
    wcstring result = token_type_description(type);
    append_format(result, L" (prod %d)", this->production_idx);
    return result;
}


/** Returns a string description of the given parse token */
wcstring parse_token_t::describe() const
{
    wcstring result = token_type_description(type);
    if (keyword != parse_keyword_none)
    {
        append_format(result, L" <%ls>", keyword_description(keyword).c_str());
    }
    return result;
}

/** A string description appropriate for presentation to the user */
wcstring parse_token_t::user_presentable_description() const
{
    return token_type_user_presentable_description(type, keyword);
}

/* Convert from tokenizer_t's token type to a parse_token_t type */
static inline parse_token_type_t parse_token_type_from_tokenizer_token(enum token_type tokenizer_token_type)
{
    parse_token_type_t result = token_type_invalid;
    switch (tokenizer_token_type)
    {
        case TOK_STRING:
            result = parse_token_type_string;
            break;

        case TOK_PIPE:
            result = parse_token_type_pipe;
            break;

        case TOK_END:
            result = parse_token_type_end;
            break;

        case TOK_BACKGROUND:
            result = parse_token_type_background;
            break;

        case TOK_REDIRECT_OUT:
        case TOK_REDIRECT_APPEND:
        case TOK_REDIRECT_IN:
        case TOK_REDIRECT_FD:
        case TOK_REDIRECT_NOCLOB:
            result = parse_token_type_redirection;
            break;

        case TOK_ERROR:
            result = parse_special_type_tokenizer_error;
            break;

        case TOK_COMMENT:
            result = parse_special_type_comment;
            break;


        default:
            fprintf(stderr, "Bad token type %d passed to %s\n", (int)tokenizer_token_type, __FUNCTION__);
            assert(0);
            break;
    }
    return result;
}

/* Helper function for dump_tree */
static void dump_tree_recursive(const parse_node_tree_t &nodes, const wcstring &src, node_offset_t node_idx, size_t indent, wcstring *result, size_t *line, node_offset_t *inout_first_node_not_dumped)
{
    assert(node_idx < nodes.size());

    // Update first_node_not_dumped
    // This takes a bit of explanation. While it's true that a parse tree may be a "forest",  its individual trees are "compact," meaning they are not interleaved. Thus we keep track of the largest node index as we descend a tree. One past the largest is the start of the next tree.
    if (*inout_first_node_not_dumped <= node_idx)
    {
        *inout_first_node_not_dumped = node_idx + 1;
    }

    const parse_node_t &node = nodes.at(node_idx);

    const size_t spacesPerIndent = 2;

    // unindent statement lists by 1 to flatten them
    if (node.type == symbol_job_list || node.type == symbol_arguments_or_redirections_list)
    {
        if (indent > 0) indent -= 1;
    }

    append_format(*result, L"%2lu - %l2u  ", *line, node_idx);
    result->append(indent * spacesPerIndent, L' ');;
    result->append(node.describe());
    if (node.child_count > 0)
    {
        append_format(*result, L" <%lu children>", node.child_count);
    }

    if (node.has_source() && node.type == parse_token_type_string)
    {
        result->append(L": \"");
        result->append(src, node.source_start, node.source_length);
        result->append(L"\"");
    }

    if (node.type != parse_token_type_string)
    {
        if (node.has_source())
        {
            append_format(*result, L"  [%ld, %ld]", (long)node.source_start, (long)node.source_length);
        }
        else
        {
            append_format(*result, L"  [no src]", (long)node.source_start, (long)node.source_length);
        }
    }

    result->push_back(L'\n');
    ++*line;
    for (size_t child_idx = node.child_start; child_idx < node.child_start + node.child_count; child_idx++)
    {
        dump_tree_recursive(nodes, src, child_idx, indent + 1, result, line, inout_first_node_not_dumped);
    }
}

/* Gives a debugging textual description of a parse tree. Note that this supports "parse forests" too. That is, our tree may not really be a tree, but instead a collection of trees. */
wcstring parse_dump_tree(const parse_node_tree_t &nodes, const wcstring &src)
{
    if (nodes.empty())
        return L"(empty!)";

    node_offset_t first_node_not_dumped = 0;
    size_t line = 0;
    wcstring result;
    while (first_node_not_dumped < nodes.size())
    {
        if (first_node_not_dumped > 0)
        {
            result.append(L"---New Tree---\n");
        }
        dump_tree_recursive(nodes, src, first_node_not_dumped, 0, &result, &line, &first_node_not_dumped);
    }
    return result;
}

/* Struct representing elements of the symbol stack, used in the internal state of the LL parser */
struct parse_stack_element_t
{
    enum parse_token_type_t type;
    enum parse_keyword_t keyword;
    node_offset_t node_idx;

    explicit parse_stack_element_t(parse_token_type_t t, node_offset_t idx) : type(t), keyword(parse_keyword_none), node_idx(idx)
    {
    }

    explicit parse_stack_element_t(production_element_t e, node_offset_t idx) : type(production_element_type(e)), keyword(production_element_keyword(e)), node_idx(idx)
    {
    }

    wcstring describe(void) const
    {
        wcstring result = token_type_description(type);
        if (keyword != parse_keyword_none)
        {
            append_format(result, L" <%ls>", keyword_description(keyword).c_str());
        }
        return result;
    }

    /* Returns a name that we can show to the user, e.g. "a command" */
    wcstring user_presentable_description(void) const
    {
        return token_type_user_presentable_description(type, keyword);
    }
};

/* The parser itself, private implementation of class parse_t. This is a hand-coded table-driven LL parser. Most hand-coded LL parsers are recursive descent, but recursive descent parsers are difficult to "pause", unlike table-driven parsers. */
class parse_ll_t
{
    /* Traditional symbol stack of the LL parser */
    std::vector<parse_stack_element_t> symbol_stack;

    /* Parser output. This is a parse tree, but stored in an array. */
    parse_node_tree_t nodes;

    /* Whether we ran into a fatal error, including parse errors or tokenizer errors */
    bool fatal_errored;

    /* Whether we should collect error messages or not */
    bool should_generate_error_messages;

    /* List of errors we have encountered */
    parse_error_list_t errors;

    /* The symbol stack can contain terminal types or symbols. Symbols go on to do productions, but terminal types are just matched against input tokens. */
    bool top_node_handle_terminal_types(parse_token_t token);

    void parse_error(const wchar_t *expected, parse_token_t token);
    void parse_error(parse_token_t token, parse_error_code_t code, const wchar_t *format, ...);
    void parse_error_failed_production(struct parse_stack_element_t &elem, parse_token_t token);
    void parse_error_unbalancing_token(parse_token_t token);
    void append_error_callout(wcstring &error_message, parse_token_t token);

    void dump_stack(void) const;

    // Get the node corresponding to the top element of the stack
    parse_node_t &node_for_top_symbol()
    {
        PARSE_ASSERT(! symbol_stack.empty());
        const parse_stack_element_t &top_symbol = symbol_stack.back();
        PARSE_ASSERT(top_symbol.node_idx != -1);
        PARSE_ASSERT(top_symbol.node_idx < nodes.size());
        return nodes.at(top_symbol.node_idx);
    }

    parse_token_type_t stack_top_type() const
    {
        return symbol_stack.back().type;
    }

    // Pop from the top of the symbol stack, then push the given production, updating node counts. Note that production_t has type "pointer to array" so some care is required.
    inline void symbol_stack_pop_push_production(const production_t *production)
    {
        bool logit = false;
        if (logit)
        {
            size_t count = 0;
            fprintf(stderr, "Applying production:\n");
            for (size_t i=0; i < MAX_SYMBOLS_PER_PRODUCTION; i++)
            {
                production_element_t elem = (*production)[i];
                if (production_element_is_valid(elem))
                {
                    parse_token_type_t type = production_element_type(elem);
                    parse_keyword_t keyword = production_element_keyword(elem);
                    fprintf(stderr, "\t%ls <%ls>\n", token_type_description(type).c_str(), keyword_description(keyword).c_str());
                    count++;
                }
            }
            if (! count) fprintf(stderr, "\t<empty>\n");
        }

        // Get the parent index. But we can't get the parent parse node yet, since it may be made invalid by adding children
        const size_t parent_node_idx = symbol_stack.back().node_idx;

        // Add the children. Confusingly, we want our nodes to be in forwards order (last token last, so dumps look nice), but the symbols should be reverse order (last token first, so it's lowest on the stack)
        const size_t child_start = nodes.size();
        size_t child_count = 0;
        for (size_t i=0; i < MAX_SYMBOLS_PER_PRODUCTION; i++)
        {
            production_element_t elem = (*production)[i];
            if (!production_element_is_valid(elem))
            {
                // All done, bail out
                break;
            }
            else
            {
                // Generate the parse node.
                parse_token_type_t child_type = production_element_type(elem);
                parse_node_t child = parse_node_t(child_type);
                child.parent = parent_node_idx;
                nodes.push_back(child);
                child_count++;
            }
        }

        // Update the parent
        parse_node_t &parent_node = nodes.at(parent_node_idx);

        // Should have no children yet
        PARSE_ASSERT(parent_node.child_count == 0);

        // Tell the node about its children
        parent_node.child_start = child_start;
        parent_node.child_count = child_count;

        // Replace the top of the stack with new stack elements corresponding to our new nodes. Note that these go in reverse order.
        symbol_stack.pop_back();
        symbol_stack.reserve(symbol_stack.size() + child_count);
        size_t idx = child_count;
        while (idx--)
        {
            production_element_t elem = (*production)[idx];
            PARSE_ASSERT(production_element_is_valid(elem));
            symbol_stack.push_back(parse_stack_element_t(elem, child_start + idx));
        }
    }

public:

    /* Constructor */
    parse_ll_t() : fatal_errored(false), should_generate_error_messages(true)
    {
        this->symbol_stack.reserve(16);
        this->nodes.reserve(64);
        this->reset_symbols_and_nodes();
    }

    /* Input */
    void accept_tokens(parse_token_t token1, parse_token_t token2);

    /* Report tokenizer errors */
    void report_tokenizer_error(parse_token_t token, int tok_err, const wchar_t *tok_error);

    /* Indicate if we hit a fatal error */
    bool has_fatal_error(void) const
    {
        return this->fatal_errored;
    }

    /* Indicate whether we want to generate error messages */
    void set_should_generate_error_messages(bool flag)
    {
        this->should_generate_error_messages = flag;
    }

    /* Clear the parse symbol stack (but not the node tree). Add a new job_list_t goal node. This is called from the constructor */
    void reset_symbols(void);

    /* Clear the parse symbol stack and the node tree. Add a new job_list_t goal node. This is called from the constructor. */
    void reset_symbols_and_nodes(void);

    /* Once parsing is complete, determine the ranges of intermediate nodes */
    void determine_node_ranges();

    /* Acquire output after parsing. This transfers directly from within self */
    void acquire_output(parse_node_tree_t *output, parse_error_list_t *errors);
};

void parse_ll_t::dump_stack(void) const
{
    // Walk backwards from the top, looking for parents
    wcstring_list_t lines;
    if (symbol_stack.empty())
    {
        lines.push_back(L"(empty)");
    }
    else
    {
        node_offset_t child = symbol_stack.back().node_idx;
        node_offset_t cursor = child;
        lines.push_back(nodes.at(cursor).describe());
        while (cursor--)
        {
            const parse_node_t &node = nodes.at(cursor);
            if (node.child_start <= child && node.child_start + node.child_count > child)
            {
                lines.push_back(node.describe());
                child = cursor;
            }
        }
    }

    fprintf(stderr, "Stack dump (%lu elements):\n", symbol_stack.size());
    for (size_t idx = 0; idx < lines.size(); idx++)
    {
        fprintf(stderr, "    %ls\n", lines.at(idx).c_str());
    }
}

// Give each node a source range equal to the union of the ranges of its children
// Terminal nodes already have source ranges (and no children)
// Since children always appear after their parents, we can implement this very simply by walking backwards
void parse_ll_t::determine_node_ranges(void)
{
    const size_t source_start_invalid = -1;
    size_t idx = nodes.size();
    while (idx--)
    {
        parse_node_t *parent = &nodes.at(idx);

        // Skip nodes that already have a source range. These are terminal nodes.
        if (parent->source_start != source_start_invalid)
            continue;

        // Ok, this node needs a source range. Get all of its children, and then set its range.
        size_t min_start = source_start_invalid, max_end = 0; //note source_start_invalid is huge
        for (node_offset_t i=0; i < parent->child_count; i++)
        {
            const parse_node_t &child = nodes.at(parent->child_offset(i));
            if (child.has_source())
            {
                min_start = std::min(min_start, child.source_start);
                max_end = std::max(max_end, child.source_start + child.source_length);
            }
        }

        if (min_start != source_start_invalid)
        {
            assert(max_end >= min_start);
            parent->source_start = min_start;
            parent->source_length = max_end - min_start;
        }
    }
}

void parse_ll_t::acquire_output(parse_node_tree_t *output, parse_error_list_t *errors)
{
    if (output != NULL)
    {
        std::swap(*output, this->nodes);
    }
    this->nodes.clear();

    if (errors != NULL)
    {
        std::swap(*errors, this->errors);
    }
    this->errors.clear();
    this->symbol_stack.clear();
}

void parse_ll_t::parse_error(parse_token_t token, parse_error_code_t code, const wchar_t *fmt, ...)
{
    this->fatal_errored = true;
    if (this->should_generate_error_messages)
    {
        //this->dump_stack();
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

// Unbalancing token. This includes 'else' or 'case' or 'end' outside of the appropriate block
// This essentially duplicates some logic from resolving the production for symbol_statement_list - yuck
void parse_ll_t::parse_error_unbalancing_token(parse_token_t token)
{
    this->fatal_errored = true;
    if (this->should_generate_error_messages)
    {
        assert(token.type == parse_token_type_string);
        assert(token.keyword == parse_keyword_end || token.keyword == parse_keyword_else || token.keyword == parse_keyword_case);
        switch (token.keyword)
        {
            case parse_keyword_end:
                this->parse_error(token, parse_error_unbalancing_end, L"'end' outside of a block");
                break;

            case parse_keyword_else:
                this->parse_error(token, parse_error_unbalancing_else, L"'else' builtin not inside of if block");
                break;

            case parse_keyword_case:
                this->parse_error(token, parse_error_unbalancing_case, L"'case' builtin not inside of switch block");
                break;

            default:
                fprintf(stderr, "Unexpected token %ls passed to %s\n", token.describe().c_str(), __FUNCTION__);
                PARSER_DIE();
                break;
        }
    }
}

// This is a 'generic' parse error when we can't match the top of the stack element
void parse_ll_t::parse_error_failed_production(struct parse_stack_element_t &stack_elem, parse_token_t token)
{
    fatal_errored = true;
    if (this->should_generate_error_messages)
    {
        bool done = false;

        /* Check for || */
        if (token.type == parse_token_type_pipe && token.source_start > 0)
        {
            /* Here we wanted a statement and instead got a pipe. See if this is a double pipe: foo || bar. If so, we have a special error message. */
            const parse_node_t *prev_pipe = nodes.find_node_matching_source_location(parse_token_type_pipe, token.source_start - 1, NULL);
            if (prev_pipe != NULL)
            {
                /* The pipe of the previous job abuts our current token. So we have ||. */
                this->parse_error(token, parse_error_double_pipe, CMD_OR_ERR_MSG);
                done = true;
            }
        }

        /* Check for && */
        if (! done && token.type == parse_token_type_background && token.source_start > 0)
        {
            /* Check to see if there was a previous token_background */
            const parse_node_t *prev_background = nodes.find_node_matching_source_location(parse_token_type_background, token.source_start - 1, NULL);
            if (prev_background != NULL)
            {
                /* We have &&. */
                this->parse_error(token, parse_error_double_background, CMD_AND_ERR_MSG);
                done = true;
            }
        }

        if (! done)
        {
            const wcstring expected = stack_elem.user_presentable_description();
            this->parse_error(expected.c_str(), token);
        }
    }
}

void parse_ll_t::report_tokenizer_error(parse_token_t token, int tok_err_code, const wchar_t *tok_error)
{
    assert(tok_error != NULL);
    parse_error_code_t parse_error_code;
    switch (tok_err_code)
    {
        case TOK_UNTERMINATED_QUOTE:
            parse_error_code = parse_error_tokenizer_unterminated_quote;
            break;

        case TOK_UNTERMINATED_SUBSHELL:
            parse_error_code = parse_error_tokenizer_unterminated_subshell;
            break;

        case TOK_UNTERMINATED_ESCAPE:
            parse_error_code = parse_error_tokenizer_unterminated_escape;
            break;

        case TOK_OTHER:
        default:
            parse_error_code = parse_error_tokenizer_other;
            break;

    }
    this->parse_error(token, parse_error_code, L"%ls", tok_error);
}

void parse_ll_t::parse_error(const wchar_t *expected, parse_token_t token)
{
    fatal_errored = true;
    if (this->should_generate_error_messages)
    {
        this->parse_error(token, parse_error_generic, L"Expected %ls, but instead found %ls", expected, token.user_presentable_description().c_str());
    }
}

void parse_ll_t::reset_symbols(void)
{
    /* Add a new job_list node, and then reset our symbol list to point at it */
    node_offset_t where = nodes.size();
    nodes.push_back(parse_node_t(symbol_job_list));

    symbol_stack.clear();
    symbol_stack.push_back(parse_stack_element_t(symbol_job_list, where)); // goal token
    this->fatal_errored = false;
}

/* Reset both symbols and nodes */
void parse_ll_t::reset_symbols_and_nodes(void)
{
    nodes.clear();
    this->reset_symbols();
}

static bool type_is_terminal_type(parse_token_type_t type)
{
    switch (type)
    {
        case parse_token_type_string:
        case parse_token_type_pipe:
        case parse_token_type_redirection:
        case parse_token_type_background:
        case parse_token_type_end:
        case parse_token_type_terminate:
            return true;

        default:
            return false;
    }
}

bool parse_ll_t::top_node_handle_terminal_types(parse_token_t token)
{
    PARSE_ASSERT(! symbol_stack.empty());
    PARSE_ASSERT(token.type >= FIRST_PARSE_TOKEN_TYPE);
    bool handled = false;
    parse_stack_element_t &stack_top = symbol_stack.back();
    if (type_is_terminal_type(stack_top.type))
    {
        // The top of the stack is terminal. We are going to handle this (because we can't produce from a terminal type)
        handled = true;

        // Now see if we actually matched
        bool matched = false;
        if (stack_top.type == token.type)
        {
            switch (stack_top.type)
            {
                case parse_token_type_string:
                    // We matched if the keywords match, or no keyword was required
                    matched = (stack_top.keyword == parse_keyword_none || stack_top.keyword == token.keyword);
                    break;

                default:
                    // For other types, we only require that the types match
                    matched = true;
                    break;
            }
        }

        if (matched)
        {
            // Success. Tell the node that it matched this token, and what its source range is
            // In the parse phase, we only set source ranges for terminal types. We propagate ranges to parent nodes afterwards.
            parse_node_t &node = node_for_top_symbol();
            node.source_start = token.source_start;
            node.source_length = token.source_length;
        }
        else
        {
            // Failure
            if (stack_top.type == parse_token_type_string && token.type == parse_token_type_string)
            {
                // Keyword failure. We should unify this with the 'matched' computation above.
                assert(stack_top.keyword != parse_keyword_none && stack_top.keyword != token.keyword);

                // Check to see which keyword we got which was considered wrong
                switch (token.keyword)
                {
                        // Some keywords are only valid in certain contexts. If this cascaded all the way down through the outermost job_list, it was not in a valid context.
                    case parse_keyword_case:
                    case parse_keyword_end:
                    case parse_keyword_else:
                        this->parse_error_unbalancing_token(token);
                        break;

                    case parse_keyword_none:
                    {
                        // This is a random other string (not a keyword)
                        const wcstring expected = keyword_description(stack_top.keyword);
                        this->parse_error(token, parse_error_generic, L"Expected keyword '%ls'", expected.c_str());
                        break;
                    }


                    default:
                    {
                        // Got a real keyword we can report
                        const wcstring actual = (token.keyword == parse_keyword_none ? token.describe() : keyword_description(token.keyword));
                        const wcstring expected = keyword_description(stack_top.keyword);
                        this->parse_error(token, parse_error_generic, L"Expected keyword '%ls', instead got keyword '%ls'", expected.c_str(), actual.c_str());
                        break;
                    }
                }
            }
            else
            {
                const wcstring expected = stack_top.user_presentable_description();
                this->parse_error(expected.c_str(), token);
            }
        }

        // We handled the token, so pop the symbol stack
        symbol_stack.pop_back();
    }
    return handled;
}

void parse_ll_t::accept_tokens(parse_token_t token1, parse_token_t token2)
{
    bool logit = false;
    if (logit)
    {
        fprintf(stderr, "Accept token %ls\n", token1.describe().c_str());
    }
    PARSE_ASSERT(token1.type >= FIRST_PARSE_TOKEN_TYPE);

    bool consumed = false;

    // Handle special types specially. Note that these are the only types that can be pushed if the symbol stack is empty.
    if (token1.type == parse_special_type_parse_error || token1.type == parse_special_type_tokenizer_error || token1.type == parse_special_type_comment)
    {
        parse_node_t err_node(token1.type);
        err_node.source_start = token1.source_start;
        err_node.source_length = token1.source_length;
        nodes.push_back(err_node);
        consumed = true;

        /* tokenizer errors are fatal */
        if (token1.type == parse_special_type_tokenizer_error)
            this->fatal_errored = true;
    }

    while (! consumed && ! this->fatal_errored)
    {
        PARSE_ASSERT(! symbol_stack.empty());

        if (top_node_handle_terminal_types(token1))
        {
            if (logit)
            {
                fprintf(stderr, "Consumed token %ls\n", token1.describe().c_str());
            }
            consumed = true;
            break;
        }

        // top_node_match_token may indicate an error if our stack is empty
        if (this->fatal_errored)
            break;

        // Get the production for the top of the stack
        parse_stack_element_t &stack_elem = symbol_stack.back();
        parse_node_t &node = nodes.at(stack_elem.node_idx);
        const production_t *production = production_for_token(stack_elem.type, token1, token2, &node.production_idx, NULL /* error text */);
        if (production == NULL)
        {
            parse_error_failed_production(stack_elem, token1);
            // the above sets fatal_errored, which ends the loop
        }
        else
        {
            bool is_terminate = (token1.type == parse_token_type_terminate);

            // When a job_list encounters something like 'else', it returns an empty production to return control to the outer block. But if it's unbalanced, then we'll end up with an empty stack! So make sure that doesn't happen. This is the primary mechanism by which we detect e.g. unbalanced end. However, if we get a true terminate token, then we allow (expect) this to empty the stack
            if (symbol_stack.size() == 1 && production_is_empty(production) && ! is_terminate)
            {
                this->parse_error_unbalancing_token(token1);
                break;
            }

            // Manipulate the symbol stack.
            // Note that stack_elem is invalidated by popping the stack.
            symbol_stack_pop_push_production(production);

            // Expect to not have an empty stack, unless this was the terminate type
            // Note we may not have an empty stack with the terminate type (i.e. incomplete input)
            assert(is_terminate || ! symbol_stack.empty());

            if (symbol_stack.empty())
            {
                break;
            }
        }
    }
}

static parse_keyword_t keyword_for_token(token_type tok, const wchar_t *tok_txt)
{
    parse_keyword_t result = parse_keyword_none;
    if (tok == TOK_STRING)
    {

        const struct
        {
            const wchar_t *txt;
            parse_keyword_t keyword;
        } keywords[] =
        {
            {L"if", parse_keyword_if},
            {L"else", parse_keyword_else},
            {L"for", parse_keyword_for},
            {L"in", parse_keyword_in},
            {L"while", parse_keyword_while},
            {L"begin", parse_keyword_begin},
            {L"function", parse_keyword_function},
            {L"switch", parse_keyword_switch},
            {L"case", parse_keyword_case},
            {L"end", parse_keyword_end},
            {L"and", parse_keyword_and},
            {L"or", parse_keyword_or},
            {L"not", parse_keyword_not},
            {L"command", parse_keyword_command},
            {L"builtin", parse_keyword_builtin}
        };

        for (size_t i=0; i < sizeof keywords / sizeof *keywords; i++)
        {
            if (! wcscmp(keywords[i].txt, tok_txt))
            {
                result = keywords[i].keyword;
                break;
            }
        }
    }
    return result;
}

/* Placeholder invalid token */
static const parse_token_t kInvalidToken = {token_type_invalid, parse_keyword_none, false, false, -1, -1};

/* Terminal token */
static const parse_token_t kTerminalToken = {parse_token_type_terminate, parse_keyword_none, false, false, -1, -1};

static inline bool is_help_argument(const wchar_t *txt)
{
    return ! wcscmp(txt, L"-h") || ! wcscmp(txt, L"--help");
}

/* Return a new parse token, advancing the tokenizer */
static inline parse_token_t next_parse_token(tokenizer_t *tok)
{
    if (! tok_has_next(tok))
    {
        return kTerminalToken;
    }

    token_type tok_type = static_cast<token_type>(tok_last_type(tok));
    int tok_start = tok_get_pos(tok);
    size_t tok_extent = tok_get_extent(tok);
    assert(tok_extent < 10000000); //paranoia
    const wchar_t *tok_txt = tok_last(tok);

    parse_token_t result;

    /* Set the type, keyword, and whether there's a dash prefix. Note that this is quite sketchy, because it ignores quotes. This is the historical behavior. For example, `builtin --names` lists builtins, but `builtin "--names"` attempts to run --names as a command. Amazingly as of this writing (10/12/13) nobody seems to have noticed this. Squint at it really hard and it even starts to look like a feature. */
    result.type = parse_token_type_from_tokenizer_token(tok_type);
    result.keyword = keyword_for_token(tok_type, tok_txt);
    result.has_dash_prefix = (tok_txt[0] == L'-');
    result.is_help_argument = result.has_dash_prefix && is_help_argument(tok_txt);
    result.source_start = (size_t)tok_start;
    result.source_length = tok_extent;

    tok_next(tok);
    return result;
}

bool parse_tree_from_string(const wcstring &str, parse_tree_flags_t parse_flags, parse_node_tree_t *output, parse_error_list_t *errors)
{
    parse_ll_t parser;
    parser.set_should_generate_error_messages(errors != NULL);

    /* Construct the tokenizer */
    tok_flags_t tok_options = 0;
    if (parse_flags & parse_flag_include_comments)
        tok_options |= TOK_SHOW_COMMENTS;

    if (parse_flags & parse_flag_accept_incomplete_tokens)
        tok_options |= TOK_ACCEPT_UNFINISHED;

    if (errors == NULL)
        tok_options |= TOK_SQUASH_ERRORS;

    tokenizer_t tok = tokenizer_t(str.c_str(), tok_options);

    /* We are an LL(2) parser. We pass two tokens at a time. New tokens come in at index 1. Seed our queue with an initial token at index 1. */
    parse_token_t queue[2] = {kInvalidToken, kInvalidToken};

    /* Loop until we have a terminal token. */
    for (size_t token_count = 0; queue[0].type != parse_token_type_terminate; token_count++)
    {
        /* Push a new token onto the queue */
        queue[0] = queue[1];
        queue[1] = next_parse_token(&tok);

        /* If we are leaving things unterminated, then don't pass parse_token_type_terminate */
        if (queue[0].type == parse_token_type_terminate && (parse_flags & parse_flag_leave_unterminated))
        {
            break;
        }

        /* Pass these two tokens, unless we're still loading the queue. We know that queue[0] is valid; queue[1] may be invalid. */
        if (token_count > 0)
        {
            parser.accept_tokens(queue[0], queue[1]);
        }

        /* Handle tokenizer errors. This is a hack because really the parser should report this for itself; but it has no way of getting the tokenizer message */
        if (queue[1].type == parse_special_type_tokenizer_error)
        {
            parser.report_tokenizer_error(queue[1], tok_get_error(&tok), tok_last(&tok));
        }

        /* Handle errors */
        if (parser.has_fatal_error())
        {
            if (parse_flags & parse_flag_continue_after_error)
            {
                /* Hack hack hack. Typically the parse error is due to the first token. However, if it's a tokenizer error, then has_fatal_error was set due to the check above; in that case the second token is what matters. */
                size_t error_token_idx = (queue[1].type == parse_special_type_tokenizer_error ? 1 : 0);

                /* Mark a special error token, and then keep going */
                const parse_token_t token = {parse_special_type_parse_error, parse_keyword_none, false, false, queue[error_token_idx].source_start, queue[error_token_idx].source_length};
                parser.accept_tokens(token, kInvalidToken);
                parser.reset_symbols();
            }
            else
            {
                /* Bail out */
                break;
            }
        }
    }

    // Teach each node where its source range is
    parser.determine_node_ranges();

    // Acquire the output from the parser
    parser.acquire_output(output, errors);

#if 0
    //wcstring result = dump_tree(this->parser->nodes, str);
    //fprintf(stderr, "Tree (%ld nodes):\n%ls", this->parser->nodes.size(), result.c_str());
    fprintf(stderr, "%lu nodes, node size %lu, %lu bytes\n", output->size(), sizeof(parse_node_t), output->size() * sizeof(parse_node_t));
#endif

    // Indicate if we had a fatal error
    return ! parser.has_fatal_error();
}

const parse_node_t *parse_node_tree_t::get_child(const parse_node_t &parent, node_offset_t which, parse_token_type_t expected_type) const
{
    const parse_node_t *result = NULL;

    /* We may get nodes with no children if we had an imcomplete parse. Don't consider than an error */
    if (parent.child_count > 0)
    {
        PARSE_ASSERT(which < parent.child_count);
        node_offset_t child_offset = parent.child_offset(which);
        if (child_offset < this->size())
        {
            result = &this->at(child_offset);

            /* If we are given an expected type, then the node must be null or that type */
            assert(expected_type == token_type_invalid || expected_type == result->type);
        }
    }

    return result;
}

const parse_node_t &parse_node_tree_t::find_child(const parse_node_t &parent, parse_token_type_t type) const
{
    for (size_t i=0; i < parent.child_count; i++)
    {
        const parse_node_t *child = this->get_child(parent, i);
        if (child->type == type)
        {
            return *child;
        }
    }
    PARSE_ASSERT(0);
    return *(parse_node_t *)(NULL); //unreachable
}

const parse_node_t *parse_node_tree_t::get_parent(const parse_node_t &node, parse_token_type_t expected_type) const
{
    const parse_node_t *result = NULL;
    if (node.parent != NODE_OFFSET_INVALID)
    {
        PARSE_ASSERT(node.parent < this->size());
        const parse_node_t &parent = this->at(node.parent);
        if (expected_type == token_type_invalid || expected_type == parent.type)
        {
            // The type matches (or no type was requested)
            result = &parent;
        }
    }
    return result;
}

const parse_node_t *parse_node_tree_t::get_first_ancestor_of_type(const parse_node_t &node, parse_token_type_t desired_type) const
{
    const parse_node_t *ancestor = &node;
    while ((ancestor = this->get_parent(*ancestor)))
    {
        if (ancestor->type == desired_type)
        {
            break;
        }
    }
    return ancestor;
}

static void find_nodes_recursive(const parse_node_tree_t &tree, const parse_node_t &parent, parse_token_type_t type, parse_node_tree_t::parse_node_list_t *result, size_t max_count)
{
    if (result->size() < max_count)
    {
        if (parent.type == type) result->push_back(&parent);
        for (size_t i=0; i < parent.child_count; i++)
        {
            const parse_node_t *child = tree.get_child(parent, i);
            assert(child != NULL);
            find_nodes_recursive(tree, *child, type, result, max_count);
        }
    }
}

parse_node_tree_t::parse_node_list_t parse_node_tree_t::find_nodes(const parse_node_t &parent, parse_token_type_t type, size_t max_count) const
{
    parse_node_list_t result;
    find_nodes_recursive(*this, parent, type, &result, max_count);
    return result;
}

/* Return true if the given node has the proposed ancestor as an ancestor (or is itself that ancestor) */
static bool node_has_ancestor(const parse_node_tree_t &tree, const parse_node_t &node, const parse_node_t &proposed_ancestor)
{
    if (&node == &proposed_ancestor)
    {
        /* Found it */
        return true;
    }
    else if (node.parent == NODE_OFFSET_INVALID)
    {
        /* No more parents */
        return false;
    }
    else
    {
        /* Recurse to the parent */
        return node_has_ancestor(tree, tree.at(node.parent), proposed_ancestor);
    }
}

const parse_node_t *parse_node_tree_t::find_last_node_of_type(parse_token_type_t type, const parse_node_t *parent) const
{
    const parse_node_t *result = NULL;
    // Find nodes of the given type in the tree, working backwards
    size_t idx = this->size();
    while (idx--)
    {
        const parse_node_t &node = this->at(idx);
        if (node.type == type)
        {
            // Types match. Check if it has the right parent
            if (parent == NULL || node_has_ancestor(*this, node, *parent))
            {
                // Success
                result = &node;
                break;
            }
        }
    }
    return result;
}

const parse_node_t *parse_node_tree_t::find_node_matching_source_location(parse_token_type_t type, size_t source_loc, const parse_node_t *parent) const
{
    const parse_node_t *result = NULL;
    // Find nodes of the given type in the tree, working backwards
    const size_t len = this->size();
    for (size_t idx=0; idx < len; idx++)
    {
        const parse_node_t &node = this->at(idx);

        /* Types must match */
        if (node.type != type)
            continue;

        /* Must contain source location */
        if (! node.location_in_or_at_end_of_source_range(source_loc))
            continue;

        /* If a parent is given, it must be an ancestor */
        if (parent != NULL && node_has_ancestor(*this, node, *parent))
            continue;

        /* Found it */
        result = &node;
        break;
    }
    return result;
}


bool parse_node_tree_t::argument_list_is_root(const parse_node_t &node) const
{
    bool result = true;
    assert(node.type == symbol_argument_list || node.type == symbol_arguments_or_redirections_list);
    const parse_node_t *parent = this->get_parent(node);
    if (parent != NULL)
    {
        /* We have a parent - check to make sure it's not another list! */
        result = parent->type != symbol_arguments_or_redirections_list && parent->type != symbol_argument_list;
    }
    return result;
}

enum parse_statement_decoration_t parse_node_tree_t::decoration_for_plain_statement(const parse_node_t &node) const
{
    assert(node.type == symbol_plain_statement);
    enum parse_statement_decoration_t decoration = parse_statement_decoration_none;
    const parse_node_t *decorated_statement = this->get_parent(node, symbol_decorated_statement);
    if (decorated_statement != NULL)
    {
        decoration = static_cast<enum parse_statement_decoration_t>(decorated_statement->production_idx);
    }
    return decoration;
}

bool parse_node_tree_t::command_for_plain_statement(const parse_node_t &node, const wcstring &src, wcstring *out_cmd) const
{
    bool result = false;
    assert(node.type == symbol_plain_statement);
    const parse_node_t *cmd_node = this->get_child(node, 0, parse_token_type_string);
    if (cmd_node != NULL && cmd_node->has_source())
    {
        out_cmd->assign(src, cmd_node->source_start, cmd_node->source_length);
        result = true;
    }
    else
    {
        out_cmd->clear();
    }
    return result;
}

bool parse_node_tree_t::statement_is_in_pipeline(const parse_node_t &node, bool include_first) const
{
    // Moderately nasty hack! Walk up our ancestor chain and see if we are in a job_continuation. This checks if we are in the second or greater element in a pipeline; if we are the first element we treat this as false
    // This accepts a few statement types
    bool result = false;
    const parse_node_t *ancestor = &node;

    // If we're given a plain statement, try to get its decorated statement parent
    if (ancestor && ancestor->type == symbol_plain_statement)
        ancestor = this->get_parent(*ancestor, symbol_decorated_statement);
    if (ancestor)
        ancestor = this->get_parent(*ancestor, symbol_statement);
    if (ancestor)
        ancestor = this->get_parent(*ancestor);

    if (ancestor)
    {
        if (ancestor->type == symbol_job_continuation)
        {
            // Second or more in a pipeline
            result = true;
        }
        else if (ancestor->type == symbol_job && include_first)
        {
            // Check to see if we have a job continuation that's not empty
            const parse_node_t *continuation = this->get_child(*ancestor, 1, symbol_job_continuation);
            result = (continuation != NULL && continuation->child_count > 0);
        }
    }

    return result;
}

enum token_type parse_node_tree_t::type_for_redirection(const parse_node_t &redirection_node, const wcstring &src, int *out_fd, wcstring *out_target) const
{
    assert(redirection_node.type == symbol_redirection);
    enum token_type result = TOK_NONE;
    const parse_node_t *redirection_primitive = this->get_child(redirection_node, 0, parse_token_type_redirection); //like 2>
    const parse_node_t *redirection_target = this->get_child(redirection_node, 1, parse_token_type_string); //like &1 or file path

    if (redirection_primitive != NULL && redirection_primitive->has_source())
    {
        result = redirection_type_for_string(redirection_primitive->get_source(src), out_fd);
    }
    if (out_target != NULL)
    {
        *out_target = redirection_target ? redirection_target->get_source(src) : L"";
    }
    return result;
}

const parse_node_t *parse_node_tree_t::header_node_for_block_statement(const parse_node_t &node) const
{
    const parse_node_t *result = NULL;
    if (node.type == symbol_block_statement)
    {
        const parse_node_t *block_header = this->get_child(node, 0, symbol_block_header);
        if (block_header != NULL)
        {
            result = this->get_child(*block_header, 0);
        }
    }
    return result;
}

parse_node_tree_t::parse_node_list_t parse_node_tree_t::specific_statements_for_job(const parse_node_t &job) const
{
    assert(job.type == symbol_job);
    parse_node_list_t result;

    /* Initial statement (non-specific) */
    result.push_back(get_child(job, 0, symbol_statement));

    /* Our cursor variable. Walk over the list of continuations. */
    const parse_node_t *continuation = get_child(job, 1, symbol_job_continuation);
    while (continuation != NULL && continuation->child_count > 0)
    {
        result.push_back(get_child(*continuation, 1, symbol_statement));
        continuation = get_child(*continuation, 2, symbol_job_continuation);
    }

    /* Result now contains a list of statements. But we want a list of specific statements e.g. symbol_switch_statement. So replace them in-place in the vector. */
    for (size_t i=0; i < result.size(); i++)
    {
        const parse_node_t *statement = result.at(i);
        assert(statement->type == symbol_statement);
        result.at(i) = this->get_child(*statement, 0);
    }

    return result;
}

const parse_node_t *parse_node_tree_t::next_node_in_node_list(const parse_node_t &node_list, parse_token_type_t entry_type, const parse_node_t **out_list_tail) const
{
    parse_token_type_t list_type = node_list.type;

    /* Paranoia - it doesn't make sense for a list type to contain itself */
    assert(list_type != entry_type);

    const parse_node_t *list_cursor = &node_list;
    const parse_node_t *list_entry = NULL;

    /* Loop while we don't have an item but do have a list. Note that not every node in the list may contain an in item that we care about - e.g. job_list contains blank lines as a production */
    while (list_entry == NULL && list_cursor != NULL)
    {
        const parse_node_t *next_cursor = NULL;

        /* Walk through the children */
        for (size_t i=0; i < list_cursor->child_count; i++)
        {
            const parse_node_t *child = this->get_child(*list_cursor, i);
            if (child->type == entry_type)
            {
                /* This is the list entry */
                list_entry = child;
            }
            else if (child->type == list_type)
            {
                /* This is the next in the list */
                next_cursor = child;
            }
        }
        /* Go to the next entry, even if it's NULL */
        list_cursor = next_cursor;
    }

    /* Return what we got */
    assert(list_cursor == NULL || list_cursor->type == list_type);
    assert(list_entry == NULL || list_entry->type == entry_type);
    if (out_list_tail != NULL)
        *out_list_tail = list_cursor;
    return list_entry;
}
