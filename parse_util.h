/** \file parse_util.h

    Various utility functions for parsing a command
*/

#ifndef FISH_PARSE_UTIL_H
#define FISH_PARSE_UTIL_H

#include <wchar.h>

/**
   Locate the first subshell in the specified string.
   
   \param in the string to search for subshells
   \param begin the starting paranthesis of the subshell
   \param end the ending paranthesis of the subshell
   \param flags set this variable to ACCEPT_INCOMPLETE if in tab_completion mode
   \return -1 on syntax error, 0 if no subshells exist and 1 on sucess
*/

int parse_util_locate_cmdsubst( const wchar_t *in, 
								const wchar_t **begin, 
								const wchar_t **end,
								int allow_incomplete );


void parse_util_cmdsubst_extent( const wchar_t *buff,
								 int cursor_pos,
								 const wchar_t **a, 
								 const wchar_t **b );

void parse_util_process_extent( const wchar_t *buff,
								int pos,
								const wchar_t **a, 
								const wchar_t **b );


void parse_util_job_extent( const wchar_t *buff,
							int pos,
							const wchar_t **a, 
							const wchar_t **b );

void parse_util_token_extent( const wchar_t *buff,
							  int cursor_pos,
							  const wchar_t **tok_begin,
							  const wchar_t **tok_end,
							  const wchar_t **prev_begin, 
							  const wchar_t **prev_end );

int parse_util_lineno( const wchar_t *str, int len );


#endif
