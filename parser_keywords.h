/** \file parser_keywords.h

Functions having to do with parser keywords, like testing if a function is a block command.
*/

#ifndef FISH_PARSER_KEYWORD_H
#define FISH_PARSER_KEYWORD_H

/**
   Return valuse for parser_keywords_is_switch()
*/
enum 
{
	ARG_NON_SWITCH,
	ARG_SWITCH,
	ARG_SKIP
}
	;



/**
   Check if the specified argument is a switch. Return ARG_SWITCH if yes,
   ARG_NON_SWITCH if no and ARG_SKIP if the argument is '--'
*/
int parser_keywords_is_switch( const wchar_t *cmd );


/**
   Tests if the specified commands parameters should be interpreted as another command, which will be true if the command is either 'command', 'exec', 'if', 'while' or 'builtin'.  

   \param cmd The command name to test
   \return 1 of the command parameter is a command, 0 otherwise
*/

int parser_keywords_is_subcommand( const wchar_t *cmd );

/**
   Tests if the specified command is a reserved word, i.e. if it is
   the name of one of the builtin functions that change the block or
   command scope, like 'for', 'end' or 'command' or 'exec'. These
   functions may not be overloaded, so their names are reserved.

   \param word The command name to test
   \return 1 of the command parameter is a command, 0 otherwise
*/
int parser_keywords_is_reserved( const wchar_t *word );

/**
   Test if the specified string is command that opens a new block
*/

int parser_keywords_is_block( const wchar_t *word);

/**
   Check if the specified command is one of the builtins that cannot
   have arguments, any followin argument is interpreted as a new
   command
*/
int parser_keywords_skip_arguments( const wchar_t *cmd );


#endif
