#include "parse_productions.h"
#include "tokenizer.h"
#include <vector>

using namespace parse_productions;

wcstring parse_error_t::describe(const wcstring &src) const
{
    wcstring result = text;
    if (source_start < src.size() && source_start + source_length <= src.size())
    {
        // Locate the beginning of this line of source
        size_t line_start = 0;

        // Look for a newline prior to source_start. If we don't find one, start at the beginning of the string; otherwise start one past the newline
        size_t newline = src.find_last_of(L'\n', source_start);
        fprintf(stderr, "newline: %lu, source_start %lu, source_length %lu\n", newline, source_start, source_length);
        if (newline != wcstring::npos)
        {
            line_start = newline;// + 1;
        }

        size_t line_end = src.find(L'\n', source_start + source_length);
        if (line_end == wcstring::npos)
        {
            line_end = src.size();
        }
        assert(line_end >= line_start);
        fprintf(stderr, "source start: %lu, line start %lu\n", source_start, line_start);
        assert(source_start >= line_start);

        // Append the line of text
        result.push_back(L'\n');
        result.append(src, line_start, line_end - line_start);

        // Append the caret line
        result.push_back(L'\n');
        result.append(source_start - line_start, L' ');
        result.push_back(L'^');
    }
    return result;
}

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

        case symbol_argument_list_nonempty:
            return L"argument_list_nonempty";
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
        case symbol_optional_background:
            return L"optional_background";
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
        default:
            return format_string(L"Unknown keyword type %ld", static_cast<long>(k));
    }
}

wcstring parse_node_t::describe(void) const
{
    wcstring result = token_type_description(type);
    return result;
}

struct parse_token_t
{
    enum parse_token_type_t type; // The type of the token as represented by the parser
    enum token_type tokenizer_type; // The type of the token as represented by the tokenizer
    enum parse_keyword_t keyword; // Any keyword represented by this parser
    size_t source_start;
    size_t source_length;

    wcstring describe() const;
};

wcstring parse_token_t::describe(void) const
{
    wcstring result = token_type_description(type);
    if (keyword != parse_keyword_none)
    {
        append_format(result, L" <%ls>", keyword_description(keyword).c_str());
    }
    return result;
}

// Convert from tokenizer_t's token type to our token
static parse_token_t parse_token_from_tokenizer_token(enum token_type tokenizer_token_type)
{
    parse_token_t result = {};
    result.tokenizer_type = tokenizer_token_type;
    switch (tokenizer_token_type)
    {
        case TOK_STRING:
            result.type = parse_token_type_string;
            break;

        case TOK_PIPE:
            result.type = parse_token_type_pipe;
            break;

        case TOK_END:
            result.type = parse_token_type_end;
            break;

        case TOK_BACKGROUND:
            result.type = parse_token_type_background;
            break;
            
        case TOK_REDIRECT_OUT:
        case TOK_REDIRECT_APPEND:
        case TOK_REDIRECT_IN:
        case TOK_REDIRECT_FD:
        case TOK_REDIRECT_NOCLOB:
            result.type = parse_token_type_redirection;
            break;


        default:
            fprintf(stderr, "Bad token type %d passed to %s\n", (int)tokenizer_token_type, __FUNCTION__);
            assert(0);
            break;
    }
    return result;
}

static void dump_tree_recursive(const parse_node_tree_t &nodes, const wcstring &src, size_t start, size_t indent, wcstring *result, size_t *line)
{
    assert(start < nodes.size());
    const parse_node_t &node = nodes.at(start);

    const size_t spacesPerIndent = 2;

    // unindent statement lists by 1 to flatten them
    if (node.type == symbol_job_list || node.type == symbol_arguments_or_redirections_list)
    {
        if (indent > 0) indent -= 1;
    }

    append_format(*result, L"%2lu - %l2u  ", *line, start);
    result->append(indent * spacesPerIndent, L' ');;
    result->append(node.describe());
    if (node.child_count > 0)
    {
        append_format(*result, L" <%lu children>", node.child_count);
    }
    if (node.type == parse_token_type_string)
    {
        result->append(L": \"");
        result->append(src, node.source_start, node.source_length);
        result->append(L"\"");
    }
    result->push_back(L'\n');
    ++*line;
    for (size_t child_idx = node.child_start; child_idx < node.child_start + node.child_count; child_idx++)
    {
        dump_tree_recursive(nodes, src, child_idx, indent + 1, result, line);
    }
}

__attribute__((unused))
static wcstring dump_tree(const parse_node_tree_t &nodes, const wcstring &src)
{
    if (nodes.empty())
        return L"(empty!)";

    size_t line = 0;
    wcstring result;
    dump_tree_recursive(nodes, src, 0, 0, &result, &line);
    return result;
}

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

};

class parse_ll_t
{
    friend class parse_t;

    std::vector<parse_stack_element_t> symbol_stack; // LL parser stack
    parse_node_tree_t nodes;

    bool fatal_errored;
    parse_error_list_t errors;

    // Constructor
    parse_ll_t() : fatal_errored(false)
    {
        // initial node
        symbol_stack.push_back(parse_stack_element_t(symbol_job_list, 0)); // goal token
        nodes.push_back(parse_node_t(symbol_job_list));
    }

    bool top_node_match_token(parse_token_t token);

    void accept_token(parse_token_t token, const wcstring &src);

    void token_unhandled(parse_token_t token, const char *function);

    void parse_error(const wchar_t *expected, parse_token_t token);
    void parse_error(parse_token_t token, const wchar_t *format, ...);
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

    void top_node_set_tag(uint32_t tag)
    {
        this->node_for_top_symbol().tag = tag;
    }

    inline void add_child_to_node(size_t parent_node_idx, parse_stack_element_t *tok)
    {
        PARSE_ASSERT(tok->type != token_type_invalid);
        tok->node_idx = nodes.size();
        nodes.push_back(parse_node_t(tok->type));
        nodes.at(parent_node_idx).child_count += 1;
    }

    inline void symbol_stack_pop()
    {
        symbol_stack.pop_back();
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

    
        // Add the children. Confusingly, we want our nodes to be in forwards order (last token last, so dumps look nice), but the symbols should be reverse order (last token first, so it's lowest on the stack)
        const size_t child_start = nodes.size();
        size_t child_count = 0;
        for (size_t i=0; i < MAX_SYMBOLS_PER_PRODUCTION; i++)
        {
            production_element_t elem = (*production)[i];
            if (production_element_is_valid(elem))
            {
                // Generate the parse node. Note that this push_back may invalidate node.
               parse_token_type_t child_type = production_element_type(elem);
               nodes.push_back(parse_node_t(child_type));
               child_count++;
            }
        }
        
        // Update the parent
        const size_t parent_node_idx = symbol_stack.back().node_idx;
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

void parse_ll_t::token_unhandled(parse_token_t token, const char *function)
{
    fprintf(stderr, "Unhandled token with type %ls in function %s\n", token_type_description(token.type).c_str(), function);
    this->dump_stack();
    parse_error_t err;
    err.text = format_string(L"Unhandled token with type %ls in function %s", token_type_description(token.type).c_str(), function);
    err.source_start = token.source_start;
    err.source_length = token.source_length;
    this->errors.push_back(err);
    this->fatal_errored = true;
}

void parse_ll_t::parse_error(parse_token_t token, const wchar_t *fmt, ...)
{
    this->dump_stack();
    parse_error_t err;
    
    va_list va;
    va_start(va, fmt);
    err.text = vformat_string(fmt, va);
    va_end(va);

    err.source_start = token.source_start;
    err.source_length = token.source_length;
    this->errors.push_back(err);
    this->fatal_errored = true;
}


void parse_ll_t::parse_error(const wchar_t *expected, parse_token_t token)
{
    wcstring desc = token_type_description(token.type);
    parse_error_t error;
    error.text = format_string(L"Expected a %ls, instead got a token of type %ls", expected, desc.c_str());
    error.source_start = token.source_start;
    error.source_start = token.source_length;
    errors.push_back(error);
    fatal_errored = true;
}

bool parse_ll_t::top_node_match_token(parse_token_t token)
{
    PARSE_ASSERT(! symbol_stack.empty());
    PARSE_ASSERT(token.type >= FIRST_PARSE_TOKEN_TYPE);
    bool result = false;
    parse_stack_element_t &stack_top = symbol_stack.back();
    if (stack_top.type == token.type)
    {
        // So far so good. See if we need a particular keyword.
        if (stack_top.keyword == parse_keyword_none || stack_top.keyword == token.keyword)
        {
            // Success. Tell the node that it matched this token
            parse_node_t &node = node_for_top_symbol();
            node.source_start = token.source_start;
            node.source_length = token.source_length;

            // We consumed this symbol
            symbol_stack.pop_back();
            result = true;
        }
        else if (token.type == parse_token_type_pipe)
        {
            // Pipes are primitive
            symbol_stack.pop_back();
            result = true;
        }
    }
    return result;
}

void parse_ll_t::accept_token(parse_token_t token, const wcstring &src)
{
    bool logit = false;
    if (logit)
    {
        const wcstring txt = wcstring(src, token.source_start, token.source_length);
        fprintf(stderr, "Accept token %ls\n", token.describe().c_str());
    }
    PARSE_ASSERT(token.type >= FIRST_PARSE_TOKEN_TYPE);
    PARSE_ASSERT(! symbol_stack.empty());
    bool consumed = false;
    while (! consumed && ! this->fatal_errored)
    {
        if (top_node_match_token(token))
        {
            if (logit)
            {
                fprintf(stderr, "Consumed token %ls\n", token.describe().c_str());
            }
            consumed = true;
            break;
        }
        
        // Get the production for the top of the stack
        parse_stack_element_t &stack_elem = symbol_stack.back();
        parse_node_t &node = nodes.at(stack_elem.node_idx);
        const production_t *production = production_for_token(stack_elem.type, token.type, token.keyword, &node.production_idx, &node.tag);
        PARSE_ASSERT(production != NULL);
        
        // Manipulate the symbol stack.
        // Note that stack_elem is invalidated by popping the stack.
        symbol_stack_pop_push_production(production);
    }
}

parse_t::parse_t() : parser(new parse_ll_t())
{
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

bool parse_t::parse(const wcstring &str, parse_node_tree_t *output, parse_error_list_t *errors)
{
    tokenizer_t tok = tokenizer_t(str.c_str(), 0);
    for (; tok_has_next(&tok) && ! this->parser->fatal_errored; tok_next(&tok))
    {
        token_type tok_type = static_cast<token_type>(tok_last_type(&tok));
        const wchar_t *tok_txt = tok_last(&tok);
        int tok_start = tok_get_pos(&tok);
        size_t tok_extent = tok_get_extent(&tok);

        if (tok_type == TOK_ERROR)
        {
            fprintf(stderr, "Tokenizer error\n");
            break;
        }

        parse_token_t token = parse_token_from_tokenizer_token(tok_type);
        token.tokenizer_type = tok_type;
        token.source_start = (size_t)tok_start;
        token.source_length = tok_extent;
        token.keyword = keyword_for_token(tok_type, tok_txt);
        this->parser->accept_token(token, str);
        
        if (this->parser->fatal_errored)
            break;
    }

    wcstring result = L"";//dump_tree(this->parser->nodes, str);
    fprintf(stderr, "Tree (%ld nodes):\n%ls", this->parser->nodes.size(), result.c_str());
    fprintf(stderr, "%lu nodes, node size %lu, %lu bytes\n", this->parser->nodes.size(), sizeof(parse_node_t), this->parser->nodes.size() * sizeof(parse_node_t));

    if (output != NULL)
    {
        output->swap(this->parser->nodes);
        this->parser->nodes.clear();
    }

    if (errors != NULL)
    {
        errors->swap(this->parser->errors);
        this->parser->errors.clear();
    }

    return ! this->parser->fatal_errored;
}
