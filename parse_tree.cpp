#include "parse_tree.h"
#include "tokenizer.h"
#include <vector>


class parse_command_t;

static wcstring token_type_description(parse_token_type_t type)
{
    switch (type)
    {
        case token_type_invalid: return L"invalid";
        
        case symbol_statement_list: return L"statement_list";
        case symbol_statement: return L"statement";
        case symbol_block_statement: return L"block_statement";
        case symbol_block_header: return L"block_header";
        case symbol_if_header: return L"if_header";
        case symbol_for_header: return L"for_header";
        case symbol_while_header: return L"while_header";
        case symbol_begin_header: return L"begin_header";
        case symbol_function_header: return L"function_header";
        case symbol_boolean_statement: return L"boolean_statement";
        case symbol_decorated_statement: return L"decorated_statement";
        case symbol_plain_statement: return L"plain_statement";
        case symbol_arguments_or_redirections_list: return L"arguments_or_redirections_list";
        case symbol_argument_or_redirection: return L"argument_or_redirection";
        
        case parse_token_type_string: return L"token_string";
        case parse_token_type_pipe: return L"token_pipe";
        case parse_token_type_redirection: return L"token_redirection";
        case parse_token_background: return L"token_background";
        case parse_token_type_end: return L"token_end";
        case parse_token_type_terminate: return L"token_terminate";
        
        default: return format_string(L"Unknown token type %ld", static_cast<long>(type));
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
};

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
            result.type = parse_token_background;
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
    
    append_format(*result, L"%2lu   ", *line);
    result->append(indent, L' ');;
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
        dump_tree_recursive(nodes, src, child_idx, indent + 2, result, line);
    }
}

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
    
    parse_stack_element_t(parse_token_type_t t) : type(t), keyword(parse_keyword_none), node_idx(-1)
    {
    }
    
    parse_stack_element_t(parse_keyword_t k) : type(parse_token_type_string), keyword(k), node_idx(-1)
    {
    }
};

class parse_ll_t
{
    friend class parse_t;
    
    std::vector<parse_stack_element_t> symbol_stack; // LL parser stack
    parse_node_tree_t nodes;
    bool errored;
    
    // Constructor
    parse_ll_t() : errored(false)
    {
        // initial node
        parse_stack_element_t elem = symbol_statement_list;
        elem.node_idx = 0;
        symbol_stack.push_back(elem); // goal token
        nodes.push_back(parse_node_t(symbol_statement_list));
    }
    
    bool top_node_match_token(parse_token_t token);
    
    // implementation of certain parser constructions
    void accept_token(parse_token_t token);
    void accept_token_statement_list(parse_token_t token);
    void accept_token_statement(parse_token_t token);
    void accept_token_block_header(parse_token_t token);
    void accept_token_boolean_statement(parse_token_t token);
    void accept_token_decorated_statement(parse_token_t token);
    void accept_token_plain_statement(parse_token_t token);
    void accept_token_arguments_or_redirections_list(parse_token_t token);
    void accept_token_argument_or_redirection(parse_token_t token);
    bool accept_token_string(parse_token_t token);
    
    void token_unhandled(parse_token_t token, const char *function);
    
    void parse_error(const wchar_t *expected, parse_token_t token);
    
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

    
    // Pop from the top of the symbol stack, then push, updating node counts. Note that these are pushed in reverse order, so the first argument will be on the top of the stack.
    inline void symbol_stack_pop_push(parse_stack_element_t tok1 = token_type_invalid, parse_stack_element_t tok2 = token_type_invalid, parse_stack_element_t tok3 = token_type_invalid, parse_stack_element_t tok4 = token_type_invalid, parse_stack_element_t tok5 = token_type_invalid)
    {
        // Get the node for the top symbol and tell it about its children
        size_t node_idx = symbol_stack.back().node_idx;
        parse_node_t &node = nodes.at(node_idx);
        
        // Should have no children yet
        PARSE_ASSERT(node.child_count == 0);
        
        // Tell the node where its children start
        node.child_start = nodes.size();
        
        // Add nodes for the children
        // Confusingly, we want our nodes to be in forwards order (last token last, so dumps look nice), but the symbols should be reverse order (last token first, so it's lowest on the stack)
        if (tok1.type != token_type_invalid) add_child_to_node(node_idx, &tok1);
        if (tok2.type != token_type_invalid) add_child_to_node(node_idx, &tok2);
        if (tok3.type != token_type_invalid) add_child_to_node(node_idx, &tok3);
        if (tok4.type != token_type_invalid) add_child_to_node(node_idx, &tok4);
        if (tok5.type != token_type_invalid) add_child_to_node(node_idx, &tok5);
        
        // The above set the node_idx. Now replace the top of the stack.
        symbol_stack.pop_back();
        if (tok5.type != token_type_invalid) symbol_stack.push_back(tok5);
        if (tok4.type != token_type_invalid) symbol_stack.push_back(tok4);
        if (tok3.type != token_type_invalid) symbol_stack.push_back(tok3);
        if (tok2.type != token_type_invalid) symbol_stack.push_back(tok2);
        if (tok1.type != token_type_invalid) symbol_stack.push_back(tok1);
    }
};

void parse_ll_t::token_unhandled(parse_token_t token, const char *function)
{
    fprintf(stderr, "Unhandled token with type %d in function %s\n", (int)token.type, function);
    PARSER_DIE();
}

void parse_ll_t::parse_error(const wchar_t *expected, parse_token_t token)
{
    fprintf(stderr, "Expected a %ls, instead got a token of type %d\n", expected, (int)token.type);
}

void parse_ll_t::accept_token_statement_list(parse_token_t token)
{
    PARSE_ASSERT(stack_top_type() == symbol_statement_list);
    switch (token.type)
    {
        case parse_token_type_string:
        case parse_token_type_pipe:
        case parse_token_type_redirection:
        case parse_token_background:
        case parse_token_type_end:
            symbol_stack_pop_push(symbol_statement, symbol_statement_list);
            break;
            
        case parse_token_type_terminate:
            // no more commands, just transition to empty
            symbol_stack_pop_push();
            break;
        
        default:
            token_unhandled(token, __FUNCTION__);
            break;
    }
}

void parse_ll_t::accept_token_statement(parse_token_t token)
{
    PARSE_ASSERT(stack_top_type() == symbol_statement);
    switch (token.type)
    {
        case parse_token_type_string:
            switch (token.keyword)
            {
                case parse_keyword_and:
                case parse_keyword_or:
                case parse_keyword_not:
                    symbol_stack_pop_push(symbol_boolean_statement);
                    break;

                case parse_keyword_if:
                case parse_keyword_else:
                case parse_keyword_for:
                case parse_keyword_in:
                case parse_keyword_while:
                case parse_keyword_begin:
                case parse_keyword_function:
                case parse_keyword_switch:
                    symbol_stack_pop_push(symbol_block_statement);
                    assert(0 && "Need assignment");
                    break;
                    
                case parse_keyword_end:
                    // TODO
                    break;
                    
                case parse_keyword_none:
                case parse_keyword_command:
                case parse_keyword_builtin:
                    symbol_stack_pop_push(symbol_decorated_statement);
                    break;
                    
            }
            break;
            
        case parse_token_type_pipe:
        case parse_token_type_redirection:
        case parse_token_background:
        case parse_token_type_end:
        case parse_token_type_terminate:
            parse_error(L"command", token);
            break;
                    
        default:
            token_unhandled(token, __FUNCTION__);
            break;
    }
}

void parse_ll_t::accept_token_block_header(parse_token_t token)
{
    PARSE_ASSERT(stack_top_type() == symbol_block_header);
    switch (token.type)
    {
        case parse_token_type_string:
            switch (token.keyword)
            {
                case parse_keyword_if:
                    symbol_stack_pop_push(symbol_if_header);
                    break;
                    
                case parse_keyword_else:
                    //todo
                    break;
                    
                case parse_keyword_for:
                    symbol_stack_pop_push(symbol_for_header);
                    break;
                    
                    
                case parse_keyword_while:
                    symbol_stack_pop_push(symbol_while_header);
                    break;
                    
                case parse_keyword_begin:
                    symbol_stack_pop_push(symbol_begin_header);
                    break;
                    
                case parse_keyword_function:
                    symbol_stack_pop_push(symbol_function_header);
                    break;
                                        
                default:
                    token_unhandled(token, __FUNCTION__);
                    break;
                    
            }
            break;
                    
        default:
            token_unhandled(token, __FUNCTION__);
            break;
    }
}

void parse_ll_t::accept_token_boolean_statement(parse_token_t token)
{
    PARSE_ASSERT(stack_top_type() == symbol_boolean_statement);
    switch (token.type)
    {
        case parse_token_type_string:
            switch (token.keyword)
            {
                case parse_keyword_and:
                case parse_keyword_or:
                case parse_keyword_not:
                    top_node_set_tag(token.keyword);
                    symbol_stack_pop_push(token.keyword, symbol_statement);
                    break;
                    
                default:
                    token_unhandled(token, __FUNCTION__);
                    break;
            }
            
        default:
            token_unhandled(token, __FUNCTION__);
            break;
    }
}

void parse_ll_t::accept_token_decorated_statement(parse_token_t token)
{
    PARSE_ASSERT(stack_top_type() == symbol_decorated_statement);
    switch (token.type)
    {
        case parse_token_type_string:
            switch (token.keyword)
            {
                case parse_keyword_command:
                    top_node_set_tag(parse_keyword_command);
                    symbol_stack_pop_push(parse_keyword_command, symbol_plain_statement);
                    break;
                    
                case parse_keyword_builtin:
                    top_node_set_tag(parse_keyword_builtin);
                    symbol_stack_pop_push(parse_keyword_builtin, symbol_plain_statement);
                    break;
                    
                default:
                    top_node_set_tag(parse_keyword_none);
                    symbol_stack_pop_push(symbol_plain_statement);
                    break;
            }
            break;
            
        default:
            token_unhandled(token, __FUNCTION__);
            break;
    }
}

void parse_ll_t::accept_token_plain_statement(parse_token_t token)
{
    PARSE_ASSERT(stack_top_type() == symbol_plain_statement);
    symbol_stack_pop_push(parse_token_type_string, symbol_arguments_or_redirections_list, parse_token_type_end);
}

void parse_ll_t::accept_token_arguments_or_redirections_list(parse_token_t token)
{
    PARSE_ASSERT(stack_top_type() == symbol_arguments_or_redirections_list);
    switch (token.type)
    {
        case parse_token_type_string:
        case parse_token_type_redirection:
            symbol_stack_pop_push(symbol_argument_or_redirection, symbol_arguments_or_redirections_list);
            break;
            
        default:
            // Some other token, end of list
            symbol_stack_pop_push();
            break;
    }
}

void parse_ll_t::accept_token_argument_or_redirection(parse_token_t token)
{
    PARSE_ASSERT(stack_top_type() == symbol_argument_or_redirection);
    switch (token.type)
    {
        case parse_token_type_string:
            symbol_stack_pop_push(parse_token_type_string);
            // Got an argument
            break;
            
        case parse_token_type_redirection:
            symbol_stack_pop_push(parse_token_type_redirection);
            // Got a redirection
            break;
            
        default:
            token_unhandled(token, __FUNCTION__);
            break;
    }
}

bool parse_ll_t::accept_token_string(parse_token_t token)
{
    PARSE_ASSERT(stack_top_type() == parse_token_type_string);
    bool result = false;
    switch (token.type)
    {
        case parse_token_type_string:
            // Got our string
            symbol_stack_pop_push();
            result = true;
            break;
            
        default:
            token_unhandled(token, __FUNCTION__);
            break;
    }
    return result;
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
    }
    return result;
}

void parse_ll_t::accept_token(parse_token_t token)
{
    PARSE_ASSERT(token.type >= FIRST_PARSE_TOKEN_TYPE);
    PARSE_ASSERT(! symbol_stack.empty());
    bool consumed = false;
    while (! consumed && ! this->errored)
    {
        if (top_node_match_token(token))
        {
            consumed = true;
            break;
        }
        
        switch (stack_top_type())
        {
            /* Symbols */
            case symbol_statement_list:
                accept_token_statement_list(token);
                break;
                
            case symbol_statement:
                accept_token_statement(token);
                break;
                
            case symbol_block_statement:
                symbol_stack_pop_push(symbol_block_header, symbol_statement_list, parse_keyword_end, symbol_arguments_or_redirections_list);
                break;
                
            case symbol_block_header:
                accept_token_block_header(token);
                break;
                
            case symbol_if_header:
                break;
                
            case symbol_for_header:
                symbol_stack_pop_push(parse_keyword_for, parse_token_type_string, parse_keyword_in, symbol_arguments_or_redirections_list, parse_token_type_end);
                break;
                
            case symbol_while_header:
                symbol_stack_pop_push(parse_keyword_while, symbol_statement);
                break;
                
            case symbol_begin_header:
                symbol_stack_pop_push(parse_keyword_begin, parse_token_type_end);
                break;
                
            case symbol_function_header:
                symbol_stack_pop_push(parse_keyword_function, symbol_arguments_or_redirections_list, parse_token_type_end);
                break;
                
            case symbol_boolean_statement:
                accept_token_boolean_statement(token);
                break;
                
            case symbol_decorated_statement:
                accept_token_decorated_statement(token);
                break;
                
            case symbol_plain_statement:
                accept_token_plain_statement(token);
                break;
                
            case symbol_arguments_or_redirections_list:
                accept_token_arguments_or_redirections_list(token);
                break;
                
            case symbol_argument_or_redirection:
                accept_token_argument_or_redirection(token);
                break;
                
            /* Tokens */
            case parse_token_type_string:
                consumed = accept_token_string(token);
                break;
                
            default:
                fprintf(stderr, "Bailing with token type %d\n", (int)token.type);
                break;
        }
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
    
        const struct {
            const wchar_t *txt;
            parse_keyword_t keyword;
        } keywords[] = {
            {L"if", parse_keyword_if},
            {L"else", parse_keyword_else},
            {L"for", parse_keyword_for},
            {L"in", parse_keyword_in},
            {L"while", parse_keyword_while},
            {L"begin", parse_keyword_begin},
            {L"function", parse_keyword_function},
            {L"switch", parse_keyword_switch},
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

void parse_t::parse(const wcstring &str, parse_node_tree_t *output)
{
    tokenizer_t tok = tokenizer_t(str.c_str(), 0);
    for (; tok_has_next(&tok); tok_next(&tok))
    {
        token_type tok_type = static_cast<token_type>(tok_last_type(&tok));
        const wchar_t *tok_txt = tok_last(&tok);
        int tok_start = tok_get_pos(&tok);
        
        if (tok_type == TOK_ERROR)
        {
            fprintf(stderr, "Tokenizer error\n");
            break;
        }
        
        parse_token_t token = parse_token_from_tokenizer_token(tok_type);
        token.tokenizer_type = tok_type;
        token.source_start = (size_t)tok_start;
        token.source_length = wcslen(tok_txt);
        token.keyword = keyword_for_token(tok_type, tok_txt);
        this->parser->accept_token(token);
    }
    wcstring result = dump_tree(this->parser->nodes, str);
    fprintf(stderr, "Tree (%ld nodes):\n%ls", this->parser->nodes.size(), result.c_str());
    fprintf(stderr, "%lu nodes, node size %lu, %lu bytes\n", this->parser->nodes.size(), sizeof(parse_node_t), this->parser->nodes.size() * sizeof(parse_node_t));
    
    if (output != NULL)
    {
        output->swap(this->parser->nodes);
        this->parser->nodes.clear();
    }
}
