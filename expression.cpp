#include "expression.h"
#include <stack>
#include <vector>

/* Fish grammar:

# A statement_list is a list of statements, separated by semicolons or newlines

    statement_list = <empty> |
                     statement statement_list

# A statement is a normal job, or an if / while / and etc.

    statement = boolean_statement | block_statement | decorated_statement
    
# A block is a conditional, loop, or begin/end

    block_statement = block_header statement_list END arguments_or_redirections_list
    block_header = if_header | for_header | while_header | function_header | begin_header
    if_header = IF statement
    for_header = FOR var_name IN arguments_or_redirections_list STATEMENT_TERMINATOR
    while_header = WHILE statement
    begin_header = BEGIN STATEMENT_TERMINATOR
    function_header = FUNCTION arguments_or_redirections_list STATEMENT_TERMINATOR
    
# A boolean statement is AND or OR or NOT

    boolean_statement = AND statement | OR statement | NOT statement
 
# A decorated_statement is a command with a list of arguments_or_redirections, possibly with "builtin" or "command"

    decorated_statement = COMMAND plain_statement | BUILTIN plain_statement | plain_statement
    plain_statement = command arguments_or_redirections_list terminator

    arguments_or_redirections_list = <empty> |
                                     argument_or_redirection arguments_or_redirections_list
    argument_or_redirection = redirection | <TOK_STRING>
    redirection = <TOK_REDIRECTION>
    
    terminator = <TOK_END> | <TOK_BACKGROUND>
 
*/

#define PARSE_ASSERT(a) assert(a)

#define PARSER_DIE() assert(0)

#if 1
class parse_command_t;

enum parse_token_type_t
{
    token_type_invalid,
    
    // Non-terminal tokens
    symbol_statement_list,
    symbol_statement,
    symbol_block_statement,
    symbol_block_header,
    symbol_if_header,
    symbol_for_header,
    symbol_while_header,
    symbol_begin_header,
    symbol_function_header,
    symbol_boolean_statement,
    symbol_decorated_statement,
    symbol_plain_statement,
    symbol_arguments_or_redirections_list,
    symbol_argument_or_redirection,
    

    // Terminal types
    parse_token_type_string,
    parse_token_type_pipe,
    parse_token_type_redirection,
    parse_token_background,
    parse_token_type_end,
    parse_token_type_terminate,
};

enum parse_keyword_t
{
    parse_keyword_none,
    parse_keyword_if,
    parse_keyword_else,
    parse_keyword_for,
    parse_keyword_in,
    parse_keyword_while,
    parse_keyword_begin,
    parse_keyword_function,
    parse_keyword_switch,
    parse_keyword_end,
    parse_keyword_and,
    parse_keyword_or,
    parse_keyword_not,
    parse_keyword_command,
    parse_keyword_builtin
};


struct parse_token_t
{
    enum parse_token_type_t type; // The type of the token as represnted by the parser
    enum token_type tokenizer_type; // The type of the token as represented by the tokenizer
    enum parse_keyword_t keyword; // Any keyword represented by this parser
    size_t source_start;
    size_t source_end;
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

/** Root of a parse tree */
class parse_statement_list_t;
class parse_tree_t
{
    friend class parse_ll_t;
    
    parse_statement_list_t *root;
};

/** Base class for nodes of a parse tree */
class parse_node_base_t
{
    /* Type of the node */
    const enum parse_token_type_t type;
    
    /* Start in the source code */
    const unsigned int source_start;
    
    /* Length of our range in the source code */
    const unsigned int source_length;
    
    protected:
    /* Index of the production used */
    unsigned char branch;
    
    public:
    parse_node_base_t(parse_token_type_t ty) : type(ty), source_start(0), source_length(0)
    {
    }
    
    virtual ~parse_node_base_t()
    {
    }
};

class parse_node_t : public parse_node_base_t
{
    public:
    parse_node_t *p1;
    parse_node_t *p2;
    uint32_t c1;
    
    parse_node_t(parse_token_type_t ty) : parse_node_base_t(ty), p1(NULL), p2(NULL), c1(0)
    {
    }
};

class parse_statement_t;
class parse_statement_list_t : public parse_node_base_t
{
    public:
    parse_statement_t *statement;
    parse_statement_list_t *next;
    parse_statement_list_t() : parse_node_base_t(symbol_statement_list), statement(NULL), next(NULL)
    {
    }
};

class parse_statement_t : public parse_node_base_t
{
    // abstract class
    public:
    parse_statement_t(parse_token_type_t ty) : parse_node_base_t(ty)
    {
    }
};

class parse_abstract_statement_t : public parse_statement_t
{
    public:
    parse_statement_t *subject;
    parse_abstract_statement_t() : parse_statement_t(symbol_statement), subject(NULL)
    {
    }
};

class parse_boolean_statement_t : public parse_statement_t
{
    public:
    enum {
        boolean_invalid,
        boolean_and,
        boolean_or,
        boolean_not
    } condition;
    
    parse_boolean_statement_t() : parse_statement_t(symbol_boolean_statement), condition(boolean_invalid)
    {
#if 0
        switch (keyword)
        {
            case parse_keyword_and:
                condition = boolean_and;
                break;
            
            case parse_keyword_or:
                condition = boolean_or;
                break;
            
            case parse_keyword_not:
                condition = boolean_not;
                break;
                
            default:
                PARSE_ASSERT(0 && "Unknown keyword");
                break;
        }
#endif
    }
};

class parse_plain_statement_t;
class parse_decorated_statement_t : public parse_statement_t
{
public:
    enum {
        decoration_none,
        decoration_command,
        decoration_builtin
    } decoration;
    
    parse_plain_statement_t *subject;
    
    parse_decorated_statement_t() : parse_statement_t(symbol_decorated_statement), subject(NULL), decoration(decoration_none)
    {
    }

};

class parse_string_t;
class parse_plain_statement_t : public parse_statement_t
{
    parse_string_t *command;
    parse_arguments_or_redirection_list_t *arguments_or_redirections_list;
    
    public:
    parse_plain_statement_t() : parse_statement_t(symbol_plain_statement)
    {
    }
};

class parse_block_statement_t : public parse_statement_t
{
    // abstract class
    parse_block_statement_t(parse_tree_t *t, parse_token_type_t ty) : parse_statement_t(ty)
    {
    }
};

class parse_string_t : public parse_node_base_t
{
};

class parse_arguments_or_redirection_list_t : public parse_node_base_t
{
};


struct parse_stack_element_t
{
    enum parse_token_type_t type;
    enum parse_keyword_t keyword;
    parse_node_base_t *node;
    
    private:
    void allocate_node(void)
    {
        assert(node == NULL);
        switch (type)
        {
            // Set up our node
            case symbol_statement_list:
                node = new parse_statement_list_t();
                break;
                
            case symbol_statement:
                node = new parse_abstract_statement_t();
                break;
                
            case symbol_block_statement:
            case symbol_block_header:
            case symbol_if_header:
            case symbol_for_header:
            case symbol_while_header:
            case symbol_begin_header:
            case symbol_function_header:
                break;
            
            case symbol_boolean_statement:
                node = new parse_boolean_statement_t();
                break;
                
            case symbol_decorated_statement:
                node = new parse_decorated_statement_t();
                break;
                
            case symbol_plain_statement:
                node = new parse_plain_statement_t();
                break;
                
            case symbol_arguments_or_redirections_list:
            case symbol_argument_or_redirection:
            
            default:
            ;
                // nothing
        }
    }

    
    public:
    
    // Construct a token type, with no keyword
    parse_stack_element_t(enum parse_token_type_t t) : type(t), keyword(parse_keyword_none)
    {
        allocate_node();
    }
    
    // Construct a string type from a keyword
    parse_stack_element_t(enum parse_keyword_t k) : type(parse_token_type_string), keyword(k), node(NULL)
    {
        allocate_node();
    }
};

template<typename T>
static T* cast_node(parse_node_base_t *node)
{
    return static_cast<T*>(node);
}

class parse_ll_t
{
    friend class parse_t;
    
    std::vector<parse_stack_element_t> symbol_stack; // LL parser stack
    parse_tree_t *tree; //tree we are constructing
    
    // Constructor
    parse_ll_t()
    {
        this->tree = new parse_tree_t();

        symbol_stack.push_back(symbol_statement_list); // goal token
        tree->root = stack_get_node_cast<parse_statement_list_t>(0);
    }
    
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
    
    void token_unhandled(parse_token_t token, const char *function);
    
    void parse_error(const wchar_t *expected, parse_token_t token);
    
    parse_token_type_t stack_top_type() const
    {
        return symbol_stack.back().type;
    }
    
    template<typename T>
    T* stack_get_node_cast(unsigned int idx)
    {
        assert(idx < symbol_stack.size());
        parse_node_base_t *base_node = symbol_stack.at(symbol_stack.size() - idx - 1).node;
        return static_cast<T *>(base_node);

    }
    
    parse_node_base_t *stack_get_node(unsigned int idx) const
    {
        assert(idx < symbol_stack.size());
        return symbol_stack.at(symbol_stack.size() - idx - 1).node;
    }
    
    // Pop from the top of the symbol stack, then push. Note that these are pushed in reverse order, so the first argument will be on the top of the stack
    inline void symbol_stack_pop_push(parse_stack_element_t tok1 = token_type_invalid, parse_stack_element_t tok2 = token_type_invalid, parse_stack_element_t tok3 = token_type_invalid, parse_stack_element_t tok4 = token_type_invalid, parse_stack_element_t tok5 = token_type_invalid)
    {
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
    parse_statement_list_t *list = stack_get_node_cast<parse_statement_list_t>(0);
    switch (token.type)
    {
        case parse_token_type_string:
        case parse_token_type_pipe:
        case parse_token_type_redirection:
        case parse_token_background:
        case parse_token_type_end:
            symbol_stack_pop_push(symbol_statement, symbol_statement_list);
            list->next = stack_get_node_cast<parse_statement_list_t>(0);
            list->statement = stack_get_node_cast<parse_statement_t>(1);
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
    parse_abstract_statement_t *statement = stack_get_node_cast<parse_abstract_statement_t>(0);
    switch (token.type)
    {
        case parse_token_type_string:
            switch (token.keyword)
            {
                case parse_keyword_and:
                case parse_keyword_or:
                case parse_keyword_not:
                    symbol_stack_pop_push(symbol_boolean_statement);
                    statement->subject = stack_get_node_cast<parse_boolean_statement_t>(0);
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
                    statement->subject = stack_get_node_cast<parse_decorated_statement_t>(0);
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
    parse_boolean_statement_t *statement = stack_get_node_cast<parse_boolean_statement_t>(0);
    switch (token.type)
    {
        case parse_token_type_string:
            switch (token.keyword)
            {
                case parse_keyword_and:
                    statement->condition = parse_boolean_statement_t::boolean_and;
                    symbol_stack_pop_push(parse_keyword_and, symbol_statement);
                    break;
                case parse_keyword_or:
                    statement->condition = parse_boolean_statement_t::boolean_or;
                    symbol_stack_pop_push(parse_keyword_or, symbol_statement);
                    break;
                case parse_keyword_not:
                    statement->condition = parse_boolean_statement_t::boolean_not;
                    symbol_stack_pop_push(parse_keyword_not, symbol_statement);
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
    parse_decorated_statement_t *statement = stack_get_node_cast<parse_decorated_statement_t>(0);
    switch (token.type)
    {
        case parse_token_type_string:
            switch (token.keyword)
            {
                case parse_keyword_command:
                    symbol_stack_pop_push(parse_keyword_command, symbol_plain_statement);
                    statement->subject = stack_get_node_cast<parse_plain_statement_t>(0);
                    statement->decoration = parse_decorated_statement_t::decoration_command;

                    break;
                    
                case parse_keyword_builtin:
                    symbol_stack_pop_push(parse_keyword_builtin, symbol_plain_statement);
                    statement->subject = stack_get_node_cast<parse_plain_statement_t>(0);
                    statement->decoration = parse_decorated_statement_t::decoration_builtin;
                    break;
                    
                default:
                    symbol_stack_pop_push(symbol_plain_statement);
                    statement->subject = stack_get_node_cast<parse_plain_statement_t>(0);
                    statement->decoration = parse_decorated_statement_t::decoration_none;
                    break;
            }
            
        default:
            token_unhandled(token, __FUNCTION__);
            break;
    }
}

void parse_ll_t::accept_token_plain_statement(parse_token_t token)
{
    PARSE_ASSERT(stack_top_type() == symbol_decorated_statement);
    parse_plain_statement_t *statement = stack_get_node_cast<parse_plain_statement_t>(0);
    symbol_stack_pop_push(parse_token_type_string, symbol_arguments_or_redirections_list, parse_token_type_end);
    statement->
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
            symbol_stack_pop_push();
            // Got an argument
            break;
            
        case parse_token_type_redirection:
            symbol_stack_pop_push();
            // Got a redirection
            break;
            
        default:
            token_unhandled(token, __FUNCTION__);
            break;
    }
}

void parse_ll_t::accept_token(parse_token_t token)
{
    assert(! symbol_stack.empty());
    switch (stack_top_type())
    {
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
    }
}
#endif

#if 0
class parse_sr_t
{
    friend class parse_t;
    
    std::vector<parse_node_base_t *> node_stack;
    void accept_token(parse_token_t token);
    
    void accept_token_string(parse_token_t token);
};

void parse_sr_t::accept_token_string(parse_token_t token)
{
    assert(token.type == parse_token_type_string);
}

void parse_sr_t::accept_token(parse_token_t token)
{
    // We are a SR parser. Our action depends on a combination of the top element(s) of our node stack and the token type.
    // Switch on the token type to make progress
    switch (token.type)
    {
        case parse_token_type_string:
            accept_token_string(token);
            break;
            
        case parse_token_type_pipe:
        case parse_token_type_redirection:
        case parse_token_background:
        case parse_token_type_end:
        case parse_token_type_terminate:
    }
}

#endif

parse_t::parse_t() : parser(new parse_ll_t())
{
}
