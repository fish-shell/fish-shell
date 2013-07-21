/**\file parse_tree.h

    Programmatic representation of fish code.
*/

#ifndef FISH_PARSE_TREE_H
#define FISH_PARSE_TREE_H

#include <wchar.h>

#include "config.h"
#include "util.h"
#include "common.h"
#include "tokenizer.h"
#include <vector>

#define PARSE_ASSERT(a) assert(a)
#define PARSER_DIE() exit_without_destructors(-1)

class parse_node_t;
class parse_node_tree_t;
typedef size_t node_offset_t;
#define NODE_OFFSET_INVALID (static_cast<node_offset_t>(-1))

struct parse_error_t
{
    /** Text of the error */
    wcstring text;
    
    /** Offset and length of the token in the source code that triggered this error */
    size_t source_start;
    size_t source_length;
    
    /** Return a string describing the error, suitable for presentation to the user */
    wcstring describe(const wcstring &src) const;
};
typedef std::vector<parse_error_t> parse_error_list_t;

class parse_ll_t;
class parse_t
{
    parse_ll_t * const parser;
    
    public:
    parse_t();
    bool parse(const wcstring &str, parse_node_tree_t *output, parse_error_list_t *errors);
};

enum parse_token_type_t
{
    token_type_invalid,
    
    // Non-terminal tokens
    symbol_job_list,
    symbol_job,
    symbol_job_continuation,
    symbol_statement,
    symbol_block_statement,
    symbol_block_header,
    symbol_for_header,
    symbol_while_header,
    symbol_begin_header,
    symbol_function_header,
    
    symbol_if_statement,
    symbol_if_clause,
    symbol_else_clause,
    symbol_else_continuation,
    
    symbol_switch_statement,
    symbol_case_item_list,
    symbol_case_item,
    
    symbol_boolean_statement,
    symbol_decorated_statement,
    symbol_plain_statement,
    symbol_arguments_or_redirections_list,
    symbol_argument_or_redirection,
    
    symbol_argument_list_nonempty,
    symbol_argument_list,

    // Terminal types
    parse_token_type_string,
    parse_token_type_pipe,
    parse_token_type_redirection,
    parse_token_background,
    parse_token_type_end,
    parse_token_type_terminate,
    
    FIRST_PARSE_TOKEN_TYPE = parse_token_type_string
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
    parse_keyword_case,
    parse_keyword_end,
    parse_keyword_and,
    parse_keyword_or,
    parse_keyword_not,
    parse_keyword_command,
    parse_keyword_builtin
};

wcstring token_type_description(parse_token_type_t type);
wcstring keyword_description(parse_keyword_t type);

/** Base class for nodes of a parse tree */
class parse_node_t
{
    public:
    
    /* Type of the node */
    enum parse_token_type_t type;
        
    /* Start in the source code */
    size_t source_start;
    
    /* Length of our range in the source code */
    size_t source_length;

    /* Children */
    node_offset_t child_start;
    node_offset_t child_count;
    
    /* Type-dependent data */
    uint32_t tag;
    
    /* Description */
    wcstring describe(void) const;
    
    /* Constructor */
    explicit parse_node_t(parse_token_type_t ty) : type(ty), source_start(0), source_length(0), child_start(0), child_count(0), tag(0)
    {
    }
    
    node_offset_t child_offset(node_offset_t which) const
    {
        PARSE_ASSERT(which < child_count);
        return child_start + which;
    }
};

class parse_node_tree_t : public std::vector<parse_node_t>
{
};

namespace parse_symbols
{
    
    #define SYMBOL(x) static inline parse_token_type_t get_token() { return x; }
    
    #define PRODUCE(X) static int production(parse_token_type_t tok, parse_keyword_t key) { return X; }
    
    #define NO_PRODUCTION (-1)
    
    
    template<parse_token_type_t WHICH>
    struct Token
    {
        SYMBOL(WHICH);
        
        typedef Token<WHICH> t0;
        typedef Token<token_type_invalid> t1;
        typedef Token<token_type_invalid> t2;
        typedef Token<token_type_invalid> t3;
        typedef Token<token_type_invalid> t4;
        typedef Token<token_type_invalid> t5;
    };
    
    /* Placeholder */
    typedef Token<token_type_invalid> none;
    
    struct EMPTY
    {
        typedef none t0;
        typedef none t1;
        typedef none t2;
        typedef none t3;
        typedef none t4;
        typedef none t5;
    };

    template<typename T0, typename T1, typename T2 = none, typename T3 = none, typename T4 = none, typename T5 = none>
    struct Seq
    {
        typedef T0 t0;
        typedef T1 t1;
        typedef T2 t2;
        typedef T3 t3;
        typedef T4 t4;
        typedef T5 t5;
    };
    
    template<typename P0, typename P1, typename P2 = none, typename P3 = none, typename P4 = none, typename P5 = none>
    struct OR
    {
        typedef P0 p0;
        typedef P1 p1;
        typedef P2 p2;
        typedef P3 p3;
        typedef P4 p4;
        typedef P5 p5;
    };
    
    template<parse_keyword_t WHICH>
    struct Keyword
    {
        static inline parse_keyword_t get_token() { return WHICH; }
    };
    
    struct job;
    struct statement;
    struct job_continuation;
    struct boolean_statement;
    struct block_statement;
    struct if_statement;
    struct if_clause;
    struct else_clause;
    struct else_continuation;
    struct switch_statement;
    struct decorated_statement;
    struct switch_statement;
    struct case_item_list;
    struct case_item;
    struct argument_list_nonempty;
    struct argument_list;
    struct block_statement;
    struct block_header;
    struct for_header;
    struct while_header;
    struct begin_header;
    struct function_header;
    struct boolean_statement;
    struct decorated_statement;
    struct plain_statement;
    struct arguments_or_redirections_list;
    struct argument_or_redirection;
    struct redirection;
    struct statement_terminator;
    
    /* A job_list is a list of jobs, separated by semicolons or newlines */
    struct job_list : OR<
            EMPTY,
            Seq<job, job_list>,
            Seq<Token<parse_token_type_end>, job_list>
            >
    {
        SYMBOL(symbol_job_list)
    };
    
    /* A job is a non-empty list of statements, separated by pipes. (Non-empty is useful for cases like if statements, where we require a command). To represent "non-empty", we require a statement, followed by a possibly empty job_continuation */
    struct job : Seq<statement, job_continuation>
    {
        SYMBOL(symbol_job);
    };
    
    struct job_continuation : OR<
            EMPTY,
            Seq<Token<parse_token_type_pipe>, statement, job_continuation>
        >
    {
        SYMBOL(symbol_job_continuation);
    };
    
    /* A statement is a normal command, or an if / while / and etc */
    struct statement : OR<
            boolean_statement,
            block_statement,
            if_statement,
            switch_statement,
            decorated_statement
    >
    {
        SYMBOL(symbol_statement);
    };
    
    struct if_statement : Seq<if_clause, else_clause, Keyword<parse_keyword_end> >
    {
        SYMBOL(symbol_if_statement);
        PRODUCE(0)
    };
    
    struct if_clause : Seq<Keyword<parse_keyword_if>, job, statement_terminator, job_list>
    {
        SYMBOL(symbol_if_clause);
        PRODUCE(0)
    };
    
    struct else_clause : OR<
        EMPTY,
        Keyword<parse_keyword_else>, else_continuation
    >
    {
        SYMBOL(symbol_else_clause);
        
        static int production(parse_token_type_t tok, parse_keyword_t key)
        {
            switch (key)
            {
                case parse_keyword_else: return 1;
                default: return 0;
            }
        }
    };

    struct else_continuation : OR<
        Seq<if_clause, else_clause>,
        Seq<statement_terminator, job_list>
    >
    {
        SYMBOL(symbol_else_continuation);
        
        static int production(parse_token_type_t tok, parse_keyword_t key)
        {
            switch (key)
            {
                case parse_keyword_if: return 0;
                default: return 1;
            }
        }
    };
    
    struct switch_statement : Seq<Keyword<parse_keyword_switch>, Token<parse_token_type_string>, statement_terminator, case_item_list, Keyword<parse_keyword_end>
    >
    {
        SYMBOL(symbol_switch_statement);
    };

    struct case_item_list : OR
    <
        EMPTY,
        case_item, case_item_list
    >
    {
        SYMBOL(symbol_case_item_list);
        
        static int production(parse_token_type_t tok, parse_keyword_t key)
        {
            switch (key)
            {
                case parse_keyword_case: return 1;
                default: return 0;
            }
        }
    };
    
    struct case_item : Seq<Keyword<parse_keyword_case>, argument_list, statement_terminator, job_list>
    {
        SYMBOL(symbol_case_item);
    };
    
    struct argument_list_nonempty : Seq<Token<parse_token_type_string>, argument_list>
    {
        SYMBOL(symbol_argument_list_nonempty);
    };
    
    struct argument_list : OR<EMPTY, argument_list_nonempty>
    {
        SYMBOL(symbol_argument_list);
        static int production(parse_token_type_t tok, parse_keyword_t key)
        {
            switch (tok)
            {
                case parse_token_type_string: return 1;
                default: return 0;
            }
        }
    };

    struct block_statement : Seq<block_header, statement_terminator, job_list, Keyword<parse_keyword_end>, arguments_or_redirections_list>
    {
        SYMBOL(symbol_block_statement);
        PRODUCE(0)
    };
    
    struct block_header : OR<for_header, while_header, function_header, begin_header>
    {
        SYMBOL(symbol_block_header);
    };

    struct for_header : Seq<Keyword<parse_keyword_for>, Token<parse_token_type_string>, Keyword<parse_keyword_in>, arguments_or_redirections_list>
    {
        SYMBOL(symbol_for_header);
    };

    struct while_header : Seq<Keyword<parse_keyword_while>, statement>
    {
        SYMBOL(symbol_while_header);
    };
    
    struct begin_header : Keyword<parse_keyword_begin>
    {
        SYMBOL(symbol_begin_header);
    };

    struct function_header : Keyword<parse_keyword_function>
    {
        SYMBOL(symbol_function_header);
    };
    
    /* A boolean statement is AND or OR or NOT */
    struct boolean_statement : OR<
        Seq<Keyword<parse_keyword_and>, statement>,
        Seq<Keyword<parse_keyword_or>, statement>,
        Seq<Keyword<parse_keyword_not>, statement>
    >
    {
        SYMBOL(symbol_boolean_statement);
        
        static int production(parse_token_type_t tok, parse_keyword_t key)
        {
            switch (key)
            {
                case parse_keyword_and: return 0;
                case parse_keyword_or: return 1;
                case parse_keyword_not: return 2;
                default: return NO_PRODUCTION;
            }
        }
    };
    
    /* A decorated_statement is a command with a list of arguments_or_redirections, possibly with "builtin" or "command" */
    struct decorated_statement : OR<
        Seq<Keyword<parse_keyword_command>, plain_statement>,
        Seq<Keyword<parse_keyword_builtin>, plain_statement>,
        plain_statement
    >
    {
        SYMBOL(symbol_decorated_statement);
        
        static int production(parse_token_type_t tok, parse_keyword_t key)
        {
            switch (key)
            {
                case parse_keyword_command: return 0;
                case parse_keyword_builtin: return 1;
                default: return 2;
            }
        }
    };
    
    struct plain_statement : Seq<Token<parse_token_type_string>, arguments_or_redirections_list>
    {
        SYMBOL(symbol_plain_statement);
        
        static int production(parse_token_type_t tok, parse_keyword_t key)
        {
            return 0;
        }

    };
    
    struct arguments_or_redirections_list : OR<
        EMPTY,
        Seq<argument_or_redirection, arguments_or_redirections_list> >
    {
        SYMBOL(symbol_arguments_or_redirections_list);
        
        static int production(parse_token_type_t tok, parse_keyword_t key)
        {
            switch (tok)
            {
                case parse_token_type_string:
                case parse_token_type_redirection:
                    return 1;
                default:
                    return 0;
            }
        }
    };
    
    struct argument_or_redirection : OR<
        Token<parse_token_type_string>,
        redirection
        >
    {
        SYMBOL(symbol_argument_or_redirection);
        
        static int production(parse_token_type_t tok, parse_keyword_t key)
        {
            switch (tok)
            {
                case parse_token_type_string: return 0;
                case parse_token_type_redirection: return 1;
                default: return NO_PRODUCTION;
            }
        }
    };

    struct redirection : Token<parse_token_type_redirection>
    {
        SYMBOL(parse_token_type_redirection);
    };
    
    struct statement_terminator : Token<parse_token_type_end>
    {
        SYMBOL(parse_token_type_end);
    };
}


/* Fish grammar:

# A job_list is a list of jobs, separated by semicolons or newlines

    job_list = <empty> |
                <TOK_END> job_list |
                job job_list

# A job is a non-empty list of statements, separated by pipes. (Non-empty is useful for cases like if statements, where we require a command). To represent "non-empty", we require a statement, followed by a possibly empty job_continuation

    job = statement job_continuation
    job_continuation = <empty> | 
                       <TOK_PIPE> statement job_continuation

# A statement is a normal command, or an if / while / and etc

    statement = boolean_statement | block_statement | if_statement | switch_statement | decorated_statement
    
# A block is a conditional, loop, or begin/end

    if_statement = if_clause else_clause <END>
    if_clause = <IF> job STATEMENT_TERMINATOR job_list
    else_clause = <empty> |
                 <ELSE> else_continuation
    else_continuation = if_clause else_clause |
                        STATEMENT_TERMINATOR job_list
                        
    switch_statement = SWITCH <TOK_STRING> STATEMENT_TERMINATOR case_item_list <END>
    case_item_list = <empty> |
                    case_item case_item_list
    case_item = CASE argument_list STATEMENT_TERMINATOR job_list
    
    argument_list_nonempty = <TOK_STRING> argument_list
    argument_list = <empty> | argument_list_nonempty

    block_statement = block_header STATEMENT_TERMINATOR job_list <END> arguments_or_redirections_list
    block_header = for_header | while_header | function_header | begin_header
    for_header = FOR var_name IN arguments_or_redirections_list
    while_header = WHILE statement
    begin_header = BEGIN
    function_header = FUNCTION function_name argument_list
 
# A boolean statement is AND or OR or NOT

    boolean_statement = AND statement | OR statement | NOT statement
 
# A decorated_statement is a command with a list of arguments_or_redirections, possibly with "builtin" or "command"

    decorated_statement = COMMAND plain_statement | BUILTIN plain_statement | plain_statement
    plain_statement = COMMAND arguments_or_redirections_list 

    arguments_or_redirections_list = <empty> |
                                     argument_or_redirection arguments_or_redirections_list
    argument_or_redirection = redirection | <TOK_STRING>
    redirection = <TOK_REDIRECTION>
    
    terminator = <TOK_END> | <TOK_BACKGROUND>
 
*/

#endif
