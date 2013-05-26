/**\file expression.h

    Programmatic representation of fish code.

*/

#ifndef FISH_EXPRESSION_H
#define FISH_EXPRESSION_H

#include <wchar.h>

#include "config.h"
#include "util.h"
#include "common.h"


/* Fish grammar:

# A statement_list is a list of statements, separated by semicolons or newlines

    statement_list = <empty> | statement | statement statement_list

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

    arguments_or_redirections_list = <empty> | argument_or_redirection 
    argument_or_redirection = redirection | <STRING>
    redirection = REDIRECTION
 
*/


class parse_command_t;

/** Root of a parse tree */
class parse_tree_t
{
    /** Literal source code */
    wcstring source;
    
    /** Initial node */
    parse_command_list_t *child;
};

/** Base class for nodes of a parse tree */
class parse_node_base_t
{
    /* Backreference to the tree */
    parse_tree_t * const tree;
    
    /* Start in the source code */
    const unsigned int source_start;
    
    /* Length of our range in the source code */
    const unsigned int source_length;
};

class parse_statement_list_t : public parse_node_base_t
{
    std::vector<parse_statement_t *> statements;
};

class parse_statement_t : public parse_node_base_t
{

};

class parse_boolean_statement_t : public parse_statement_t
{

};

class parse_plain_statement_t : public parse_statement_t
{
    
};

class parse_block_statement_t : public parse_statement_t
{
    
};

#endif
