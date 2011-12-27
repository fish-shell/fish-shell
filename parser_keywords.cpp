/** \file parser_keywords.c

Functions having to do with parser keywords, like testing if a function is a block command.
*/

#include "config.h"


#include <stdlib.h>
#include <stdio.h>

#include "fallback.h"
#include "common.h"
#include "parser_keywords.h"


int parser_keywords_is_switch( const wchar_t *cmd )
{
	if( wcscmp( cmd, L"--" ) == 0 )
		return ARG_SKIP;
	else 
		return cmd[0] == L'-';
}

int parser_keywords_skip_arguments( const wchar_t *cmd )
{
	return contains( cmd,
					 L"else",
					 L"begin" );
}


int parser_keywords_is_subcommand( const wchar_t *cmd )
{

	return parser_keywords_skip_arguments( cmd ) ||
		contains( cmd,
				  L"command",
				  L"builtin",
				  L"while",
				  L"exec",
				  L"if",
				  L"and",
				  L"or",
				  L"not" );
	
}

int parser_keywords_is_block( const wchar_t *word)
{
	return contains( word,
					 L"for",
					 L"while",
					 L"if",
					 L"function",
					 L"switch",
					 L"begin" );
}

int parser_keywords_is_reserved( const wchar_t *word)
{
	return parser_keywords_is_block(word) ||
		parser_keywords_is_subcommand( word ) ||
		contains( word,
				  L"end",
				  L"case",
				  L"else",
				  L"return",
				  L"continue",
				  L"break" );
}

