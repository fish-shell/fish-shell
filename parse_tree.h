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
#define PARSER_DIE() assert(0)


class parse_ll_t;
class parse_sr_t;
class parse_t
{
    parse_ll_t * const parser;
    
    public:
    parse_t();
    void parse(const wcstring &str);
};

class parse_node_t;
typedef std::vector<parse_node_t> parse_node_tree_t;
typedef size_t node_offset_t;


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
    parse_keyword_end,
    parse_keyword_and,
    parse_keyword_or,
    parse_keyword_not,
    parse_keyword_command,
    parse_keyword_builtin
};

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
};



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

#endif
