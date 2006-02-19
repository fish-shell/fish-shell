/** \file parse_util.h

    Various mostly unrelated utility functions related to parsing,
    loading and evaluating fish code.
*/

#ifndef FISH_PARSE_UTIL_H
#define FISH_PARSE_UTIL_H

#include <wchar.h>

/**
   Find the beginning and end of the first subshell in the specified string.
   
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

/**
   Find the beginning and end of the command substitution under the cursor

   \param buff the string to search for subshells
   \param cursor_pos the position of the cursor
   \param a the start of the searched string
   \param b the end of the searched string
*/
void parse_util_cmdsubst_extent( const wchar_t *buff,
								 int cursor_pos,
								 const wchar_t **a, 
								 const wchar_t **b );

/**
   Find the beginning and end of the process definition under the cursor

   \param buff the string to search for subshells
   \param cursor_pos the position of the cursor
   \param a the start of the searched string
   \param b the end of the searched string
*/
void parse_util_process_extent( const wchar_t *buff,
								int cursor_pos,
								const wchar_t **a, 
								const wchar_t **b );


/**
   Find the beginning and end of the job definition under the cursor

   \param buff the string to search for subshells
   \param cursor_pos the position of the cursor
   \param a the start of the searched string
   \param b the end of the searched string
*/
void parse_util_job_extent( const wchar_t *buff,
							int cursor_pos,
							const wchar_t **a, 
							const wchar_t **b );

/**
   Find the beginning and end of the token under the cursor

   \param buff the string to search for subshells
   \param cursor_pos the position of the cursor
   \param a the start of the searched string
   \param b the end of the searched string
*/
void parse_util_token_extent( const wchar_t *buff,
							  int cursor_pos,
							  const wchar_t **tok_begin,
							  const wchar_t **tok_end,
							  const wchar_t **prev_begin, 
							  const wchar_t **prev_end );


/**
   Get the linenumber at the specified character offset
*/
int parse_util_lineno( const wchar_t *str, int len );

/**
   Autoload the specified file, if it exists in the specified path. Do
   not load it multiple times unless it's timestamp changes.

   \param cmd the filename to search for. The suffix '.fish' is always added to this name
   \param path_var_name the name of an environment variable containing a search path
   \param on_load a callback function to run if a suitable file is found, which has not already been run
   \param reload wheter to recheck file timestamps on already loaded files
*/
int parse_util_load( const wchar_t *cmd,
					 const wchar_t *path_var_name,
					 void (*on_load)(const wchar_t *cmd),
					 int reload );

/**
   Reset the loader for the specified path variable
*/
void parse_util_load_reset( const wchar_t *path_var );

/**
   Set the argv environment variable to the specified null-terminated
   array of strings. 
*/
void parse_util_set_argv( wchar_t **argv );

/**
   Make a duplicate of the specified string, unescape wildcard
   characters but not performing any other character transformation.
*/
wchar_t *parse_util_unescape_wildcards( const wchar_t *in );

#endif
