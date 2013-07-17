/** \file parse_util.h

    Various mostly unrelated utility functions related to parsing,
    loading and evaluating fish code.
*/

#ifndef FISH_PARSE_UTIL_H
#define FISH_PARSE_UTIL_H

#include "autoload.h"
#include <wchar.h>
#include <map>
#include <set>

/**
   Find the beginning and end of the first subshell in the specified string.

   \param in the string to search for subshells
   \param begin the starting paranthesis of the subshell
   \param end the ending paranthesis of the subshell
   \param accept_incomplete whether to permit missing closing parenthesis
   \return -1 on syntax error, 0 if no subshells exist and 1 on sucess
*/

int parse_util_locate_cmdsubst(const wchar_t *in,
                               wchar_t **begin,
                               wchar_t **end,
                               bool accept_incomplete);

/**
   Find the beginning and end of the command substitution under the
   cursor. If no subshell is found, the entire string is returned. If
   the current command substitution is not ended, i.e. the closing
   parenthesis is missing, then the string from the beginning of the
   substitution to the end of the string is returned.

   \param buff the string to search for subshells
   \param cursor_pos the position of the cursor
   \param a the start of the searched string
   \param b the end of the searched string
*/
void parse_util_cmdsubst_extent(const wchar_t *buff,
                                size_t cursor_pos,
                                const wchar_t **a,
                                const wchar_t **b);

/**
   Find the beginning and end of the process definition under the cursor

   \param buff the string to search for subshells
   \param cursor_pos the position of the cursor
   \param a the start of the searched string
   \param b the end of the searched string
*/
void parse_util_process_extent(const wchar_t *buff,
                               size_t cursor_pos,
                               const wchar_t **a,
                               const wchar_t **b);


/**
   Find the beginning and end of the job definition under the cursor

   \param buff the string to search for subshells
   \param cursor_pos the position of the cursor
   \param a the start of the searched string
   \param b the end of the searched string
*/
void parse_util_job_extent(const wchar_t *buff,
                           size_t cursor_pos,
                           const wchar_t **a,
                           const wchar_t **b);

/**
   Find the beginning and end of the token under the cursor and the
   token before the current token. Any combination of tok_begin,
   tok_end, prev_begin and prev_end may be null.

   \param buff the string to search for subshells
   \param cursor_pos the position of the cursor
   \param tok_begin the start of the current token
   \param tok_end the end of the current token
   \param prev_begin the start o the token before the current token
   \param prev_end the end of the token before the current token
*/
void parse_util_token_extent(const wchar_t *buff,
                             size_t cursor_pos,
                             const wchar_t **tok_begin,
                             const wchar_t **tok_end,
                             const wchar_t **prev_begin,
                             const wchar_t **prev_end);


/**
   Get the linenumber at the specified character offset
*/
int parse_util_lineno(const wchar_t *str, size_t len);

/**
   Calculate the line number of the specified cursor position
 */
int parse_util_get_line_from_offset(const wcstring &str, size_t pos);

/**
   Get the offset of the first character on the specified line
 */
size_t parse_util_get_offset_from_line(const wcstring &str, int line);


/**
   Return the total offset of the buffer for the cursor position nearest to the specified poition
 */
size_t parse_util_get_offset(const wcstring &str, int line, long line_offset);

/**
   Set the argv environment variable to the specified null-terminated
   array of strings.
*/
void parse_util_set_argv(const wchar_t * const *argv, const wcstring_list_t &named_arguments);

/**
   Make a duplicate of the specified string, unescape wildcard
   characters but not performing any other character transformation.
*/
wchar_t *parse_util_unescape_wildcards(const wchar_t *in);

/**
   Calculates information on the parameter at the specified index.

   \param cmd The command to be analyzed
   \param pos An index in the string which is inside the parameter
   \param quote If not NULL, store the type of quote this parameter has, can be either ', " or \\0, meaning the string is not quoted.
   \param offset If not NULL, get_param will store the offset to the beginning of the parameter.
   \param type If not NULL, get_param will store the token type as returned by tok_last.
*/
void parse_util_get_parameter_info(const wcstring &cmd, const size_t pos, wchar_t *quote, size_t *offset, int *type);

/**
   Attempts to escape the string 'cmd' using the given quote type, as determined by the quote character. The quote can be a single quote or double quote, or L'\0' to indicate no quoting (and thus escaping should be with backslashes).
*/
wcstring parse_util_escape_string_with_quote(const wcstring &cmd, wchar_t quote);


#endif
