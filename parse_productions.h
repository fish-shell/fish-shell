/**\file parse_tree.h

    Programmatic representation of fish code.
*/

#ifndef FISH_PARSE_TREE_CONSTRUCTION_H
#define FISH_PARSE_TREE_CONSTRUCTION_H

#include "parse_tree.h"

/* Terrifying template black magic. */

/*

- Get info for symbol
- Resolve production from info
- Get productions for children
- Get symbols for productions

Production may be:

1. Single value
2. Sequence of values (possibly empty)
3. Options of Single / Sequence

Info to specify:

1. Number of different productions
2. Resolver function
3. Symbols for associated productions

Choice: should info be a class or a data?

data:

struct Symbol_t
{
    enum parse_token_type_t token_type;
    int (*resolver)(parse_token_type_t tok, parse_keyword_t key); //may be trivial
    production productions[5];
}

struct Production_t
{
    enum parse_token_type_t symbols[5];
}

*/

namespace parse_productions
{

#define MAX_PRODUCTIONS 5
#define MAX_SYMBOLS_PER_PRODUCTION 5



/* A production is an array of unsigned char. Symbols are encoded directly as their symbol value. Keywords are encoded with an offset of LAST_TOKEN_OR_SYMBOL + 1. So essentially we glom together keywords and symbols. */
typedef unsigned char Production_t[MAX_SYMBOLS_PER_PRODUCTION];

typedef Production_t ProductionList_t[MAX_PRODUCTIONS];

#define PRODUCE_KEYWORD(x) ((x) + LAST_TOKEN_OR_SYMBOL + 1)

struct Symbol_t
{
    enum parse_token_type_t token_type;
    int (*resolver)(parse_token_type_t tok, parse_keyword_t key);
    Production_t productions[MAX_PRODUCTIONS];
};



}

namespace parse_symbols
{

#define SYMBOL(x) static inline parse_token_type_t get_token() { return x; }

#define PRODUCE(X) static int production(parse_token_type_t tok, parse_keyword_t key) { return X; }

#define NO_PRODUCTION (-1)

struct Symbol
{
    typedef int magic_symbol_type_t;
};

template<parse_token_type_t WHICH>
struct Token : public Symbol
{
    SYMBOL(WHICH);
};

/* Placeholder */
typedef Token<token_type_invalid> none;

typedef Token<token_type_invalid> EMPTY;

template<typename T0, typename T1, typename T2 = none, typename T3 = none, typename T4 = none, typename T5 = none>
struct Seq
{
    typedef T0 t0;
    typedef T1 t1;
    typedef T2 t2;
    typedef T3 t3;
    typedef T4 t4;
    typedef T5 t5;

    typedef int magic_seq_type_t;
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

    typedef int magic_or_type_t;
};

template<parse_keyword_t WHICH>
struct Keyword : public Symbol
{
    static inline parse_keyword_t get_token()
    {
        return WHICH;
    }
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
struct optional_background;

/* A job_list is a list of jobs, separated by semicolons or newlines */
struct job_list : public Symbol
{
    typedef OR<
    EMPTY,
    Seq<job, job_list>,
    Seq<Token<parse_token_type_end>, job_list>
    > productions;

    SYMBOL(symbol_job_list)

    static int production(parse_token_type_t token_type, parse_keyword_t token_keyword)
    {
        switch (token_type)
        {
            case parse_token_type_string:
                // 'end' is special
                switch (token_keyword)
                {
                    case parse_keyword_end:
                    case parse_keyword_else:
                        // End this job list
                        return 0;

                    default:
                        // Normal string
                        return 1;
                }

            case parse_token_type_pipe:
            case parse_token_type_redirection:
            case parse_token_type_background:
                return 1;

            case parse_token_type_end:
                // Empty line
                return 2;

            case parse_token_type_terminate:
                // no more commands, just transition to empty
                return 0;
                break;

            default:
                return NO_PRODUCTION;
        }
    }

};

/* A job is a non-empty list of statements, separated by pipes. (Non-empty is useful for cases like if statements, where we require a command). To represent "non-empty", we require a statement, followed by a possibly empty job_continuation */
struct job : public Symbol
{
    typedef Seq<statement, job_continuation> sole_production;
    SYMBOL(symbol_job);
};

struct job_continuation : public Symbol
{
    typedef OR<
    EMPTY,
    Seq<Token<parse_token_type_pipe>, statement, job_continuation>
    > productions;

    SYMBOL(symbol_job_continuation);

    static int production(parse_token_type_t token_type, parse_keyword_t token_keyword)
    {
        switch (token_type)
        {
            case parse_token_type_pipe:
                // Pipe, continuation
                return 1;

            default:
                // Not a pipe, no job continuation
                return 0;
        }

    }
};

/* A statement is a normal command, or an if / while / and etc */
struct statement : public Symbol
{
    typedef OR<
    boolean_statement,
    block_statement,
    if_statement,
    switch_statement,
    decorated_statement
    > productions;

    SYMBOL(symbol_statement);

    static int production(parse_token_type_t token_type, parse_keyword_t token_keyword)
    {
        switch (token_type)
        {
            case parse_token_type_string:
                switch (token_keyword)
                {
                    case parse_keyword_and:
                    case parse_keyword_or:
                    case parse_keyword_not:
                        return 0;

                    case parse_keyword_for:
                    case parse_keyword_while:
                    case parse_keyword_function:
                    case parse_keyword_begin:
                        return 1;

                    case parse_keyword_if:
                        return 2;

                    case parse_keyword_else:
                        //symbol_stack_pop();
                        return NO_PRODUCTION;

                    case parse_keyword_switch:
                        return 3;

                    case parse_keyword_end:
                        PARSER_DIE(); //todo
                        return NO_PRODUCTION;

                        // 'in' is only special within a for_header
                    case parse_keyword_in:
                    case parse_keyword_none:
                    case parse_keyword_command:
                    case parse_keyword_builtin:
                    case parse_keyword_case:
                        return 4;
                }
                break;

            case parse_token_type_pipe:
            case parse_token_type_redirection:
            case parse_token_type_background:
            case parse_token_type_terminate:
                return NO_PRODUCTION;
                //parse_error(L"statement", token);

            default:
                return NO_PRODUCTION;
        }
    }

};

struct if_statement : public Symbol
{
    typedef Seq<if_clause, else_clause, Keyword<parse_keyword_end>, arguments_or_redirections_list> sole_production;
    SYMBOL(symbol_if_statement);
};

struct if_clause : public Symbol
{
    typedef Seq<Keyword<parse_keyword_if>, job, statement_terminator, job_list> sole_production;
    SYMBOL(symbol_if_clause);
};

struct else_clause : public Symbol
{
    typedef OR<
    EMPTY,
    Seq<Keyword<parse_keyword_else>, else_continuation>
    > productions;

    SYMBOL(symbol_else_clause);

    static int production(parse_token_type_t tok, parse_keyword_t key)
    {
        switch (key)
        {
            case parse_keyword_else:
                return 1;
            default:
                return 0;
        }
    }
};

struct else_continuation : public Symbol
{
    typedef OR<
    Seq<if_clause, else_clause>,
        Seq<statement_terminator, job_list>
        > productions;

    SYMBOL(symbol_else_continuation);

    static int production(parse_token_type_t tok, parse_keyword_t key)
    {
        switch (key)
        {
            case parse_keyword_if:
                return 0;
            default:
                return 1;
        }
    }
};

struct switch_statement : public Symbol
{
    typedef Seq<Keyword<parse_keyword_switch>,
                Token<parse_token_type_string>,
                statement_terminator,
                case_item_list,
                Keyword<parse_keyword_end>
                > sole_production;

    SYMBOL(symbol_switch_statement);
};

struct case_item_list : public Symbol
{
    typedef OR
    <
    EMPTY,
    Seq<case_item, case_item_list>,
    Seq<Token<parse_token_type_end>, case_item_list>
    > productions;

    SYMBOL(symbol_case_item_list);

    static int production(parse_token_type_t tok, parse_keyword_t key)
    {
        switch (key)
        {
            case parse_keyword_case: return 1;
            
            default:
                if (tok == parse_token_type_end)
                {
                    /* empty line */
                    return 2;
                }
                else
                {
                    return 0;
                }
                
        }
    }
};

struct case_item : public Symbol
{
    typedef Seq<Keyword<parse_keyword_case>, argument_list, statement_terminator, job_list> sole_production;

    SYMBOL(symbol_case_item);
};

struct argument_list_nonempty : public Symbol
{
    typedef Seq<Token<parse_token_type_string>, argument_list> sole_production;
    SYMBOL(symbol_argument_list_nonempty);
};

struct argument_list : public Symbol
{
    typedef OR<EMPTY, argument_list_nonempty> productions;

    SYMBOL(symbol_argument_list);
    static int production(parse_token_type_t tok, parse_keyword_t key)
    {
        switch (tok)
        {
            case parse_token_type_string:
                return 1;
            default:
                return 0;
        }
    }
};

struct block_statement : public Symbol
{
    typedef Seq<block_header, statement_terminator, job_list, Keyword<parse_keyword_end>, arguments_or_redirections_list> sole_production;

    SYMBOL(symbol_block_statement);
};

struct block_header : public Symbol
{
    typedef OR<for_header, while_header, function_header, begin_header> productions;

    SYMBOL(symbol_block_header);

    static int production(parse_token_type_t tok, parse_keyword_t key)
    {
        switch (key)
        {
                // todo
            case parse_keyword_else:
                return NO_PRODUCTION;
            case parse_keyword_for:
                return 0;
            case parse_keyword_while:
                return 1;
            case parse_keyword_function:
                return 2;
            case parse_keyword_begin:
                return 3;
            default:
                return NO_PRODUCTION;
        }
    }
};

struct for_header : public Symbol
{
    typedef Seq<Keyword<parse_keyword_for>, Token<parse_token_type_string>, Keyword<parse_keyword_in>, arguments_or_redirections_list> sole_production;

    SYMBOL(symbol_for_header);
};

struct while_header : public Symbol
{
    typedef Seq<Keyword<parse_keyword_while>, statement> sole_production;

    SYMBOL(symbol_while_header);
};

struct begin_header : public Symbol
{
    typedef Keyword<parse_keyword_begin> sole_production;
    SYMBOL(symbol_begin_header);
};

struct function_header : public Symbol
{
    typedef Seq< Keyword<parse_keyword_function>, Token<parse_token_type_string>, argument_list> sole_production;
    SYMBOL(symbol_function_header);
};

/* A boolean statement is AND or OR or NOT */
struct boolean_statement : public Symbol
{
    typedef OR<
    Seq<Keyword<parse_keyword_and>, statement>,
        Seq<Keyword<parse_keyword_or>, statement>,
        Seq<Keyword<parse_keyword_not>, statement>
        > productions;

    SYMBOL(symbol_boolean_statement);

    static int production(parse_token_type_t tok, parse_keyword_t key)
    {
        switch (key)
        {
            case parse_keyword_and:
                return 0;
            case parse_keyword_or:
                return 1;
            case parse_keyword_not:
                return 2;
            default:
                return NO_PRODUCTION;
        }
    }
};

/* A decorated_statement is a command with a list of arguments_or_redirections, possibly with "builtin" or "command" */
struct decorated_statement : public Symbol
{

    typedef OR<
    Seq<Keyword<parse_keyword_command>, plain_statement>,
        Seq<Keyword<parse_keyword_builtin>, plain_statement>,
        plain_statement
        > productions;

    SYMBOL(symbol_decorated_statement);

    static int production(parse_token_type_t tok, parse_keyword_t key)
    {
        switch (key)
        {
            case parse_keyword_command:
                return 0;
            case parse_keyword_builtin:
                return 1;
            default:
                return 2;
        }
    }
};

struct plain_statement : public Symbol
{

    typedef Seq<Token<parse_token_type_string>, arguments_or_redirections_list, optional_background> sole_production;

    SYMBOL(symbol_plain_statement);

};

struct arguments_or_redirections_list : public Symbol
{
    typedef OR<
    EMPTY,
    Seq<argument_or_redirection, arguments_or_redirections_list> >
    productions;

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

struct argument_or_redirection : public Symbol
{
    typedef OR<
    Token<parse_token_type_string>,
          redirection
          > productions;


    SYMBOL(symbol_argument_or_redirection);

    static int production(parse_token_type_t tok, parse_keyword_t key)
    {
        switch (tok)
        {
            case parse_token_type_string:
                return 0;
            case parse_token_type_redirection:
                return 1;
            default:
                return NO_PRODUCTION;
        }
    }
};

struct redirection : public Symbol
{
    typedef Token<parse_token_type_redirection> production;
    SYMBOL(parse_token_type_redirection);
};

struct statement_terminator : public Symbol
{
    typedef Token<parse_token_type_end> production;
    SYMBOL(parse_token_type_end);
};

struct optional_background : public Symbol
{
    typedef OR<
        EMPTY,
        Token<parse_token_type_background>
    > productions;
    
    SYMBOL(symbol_optional_background);
    
    static int production(parse_token_type_t tok, parse_keyword_t key)
    {
        switch (tok)
        {
            case parse_token_type_background:
                return 1;
            default:
                return 0;
        }
    }
};

}

#endif
