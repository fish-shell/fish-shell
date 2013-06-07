/**\file expression.h

    Programmatic representation of fish code.

*/

#ifndef FISH_EXPRESSION_H
#define FISH_EXPRESSION_H

#include <wchar.h>

#include "config.h"
#include "util.h"
#include "common.h"
#include "tokenizer.h"


class parse_ll_t;
class parse_sr_t;
class parse_t
{
    parse_ll_t * const parser;
    parse_t();
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


/* fish Shift-Reduce grammar:

   
   IF <- if_statement
   FOR <- for_statement
   
   

*/

#endif
