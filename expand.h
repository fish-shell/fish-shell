/**\file expand.h

   Prototypes for string expansion functions. These functions perform
   several kinds of parameter expansion. There are a lot of issues
   with regards to memory allocation. Overall, these functions would
   benefit from using a more clever memory allocation scheme, perhaps
   an evil combination of talloc, string buffers and reference
   counting.
   
*/

#ifndef FISH_EXPAND_H
/**
   Header guard
*/
#define FISH_EXPAND_H

#include <wchar.h>

#include "util.h"

/**
   Flag specifying that subshell expansion should be skipped
*/
#define EXPAND_SKIP_SUBSHELL 1

/**
   Flag specifying that variable expansion should be skipped
*/
#define EXPAND_SKIP_VARIABLES 2

/**
   Flag specifying that wildcard expansion should be skipped
*/
#define EXPAND_SKIP_WILDCARDS 4

/**
   Incomplete matches in the last segment are ok (for tab
   completion). An incomplete match is a wildcard that matches a
   prefix of the filename. If accept_incomplete is true, only the
   remainder of the string is returned.
*/
#define ACCEPT_INCOMPLETE 8

/**
   Only match files that are executable by the current user. Only applicable together with ACCEPT_INCOMPLETE.
*/

#define EXECUTABLES_ONLY 16

/**
   Only match directories. Only applicable together with ACCEPT_INCOMPLETE.
*/

#define DIRECTORIES_ONLY 32

/**
  Use unencoded private-use keycodes for internal characters
*/
#define EXPAND_RESERVED 0xf000

enum
{
	/** Character represeting a home directory */
	HOME_DIRECTORY = EXPAND_RESERVED,

	/** Character represeting process expansion */
	PROCESS_EXPAND,
	
	/** Character representing variable expansion */
	VARIABLE_EXPAND,

	/** Character rpresenting variable expansion into a single element*/
	VARIABLE_EXPAND_SINGLE,

	/** Character representing the start of a bracket expansion */
	BRACKET_BEGIN,

	/** Character representing the end of a bracket expansion */
	BRACKET_END,

	/** Character representing separation between two bracket elements */
	BRACKET_SEP,
	/**
	   Separate subtokens in a token with this character. 
	*/
	INTERNAL_SEPARATOR,

}
	;


/**
   These are the possible return values for expand_string
*/
enum
{
	/** Error */
	EXPAND_ERROR,
	/** Ok */
	EXPAND_OK,
	/** Ok, a wildcard in the string matched no files */
	EXPAND_WILDCARD_NO_MATCH,
	/* Ok, a wildcard in the string matched a file */
	EXPAND_WILDCARD_MATCH
}
	;

/** Character for separating two array elements. We use 30, i.e. the ascii record separator since that seems logical. */
#define ARRAY_SEP 0x1e

/** String containing the character for separating two array elements */
#define ARRAY_SEP_STR L"\x1e"

/**
   Error issued on invalid variable name
*/
#define COMPLETE_VAR_DESC _( L"The '$' character begins a variable name. The character '%lc', which directly followed a '$', is not allowed as a part of a variable name, and variable names may not be zero characters long. To learn more about variable expansion in fish, type 'help expand-variable'.")

/**
   Error issued on invalid variable name
*/
#define COMPLETE_VAR_NULL_DESC _( L"The '$' begins a variable name. It was given at the end of an argument. Variable names may not be zero characters long. To learn more about variable expansion in fish, type 'help expand-variable'.")

/**
   Error issued on invalid variable name
*/
#define COMPLETE_VAR_BRACKET_DESC _( L"Did you mean {$VARIABLE}? The '$' character begins a variable name. A bracket, which directly followed a '$', is not allowed as a part of a variable name, and variable names may not be zero characters long. To learn more about variable expansion in fish, type 'help expand-variable'." )

/**
   Error issued on invalid variable name
*/
#define COMPLETE_VAR_PARAN_DESC _( L"Did you mean (COMMAND)? In fish, the '$' character is only used for accessing variables. To learn more about command substitution in fish, type 'help expand-command-substitution'.")

/**
   Error issued on array out of bounds
*/
#define ARRAY_BOUNDS_ERR _(L"Array index out of bounds")


/**
   Perform various forms of expansion on in, such as tilde expansion
   (\~USER becomes the users home directory), variable expansion
   (\$VAR_NAME becomes the value of the environment variable VAR_NAME),
   subshell expansion and wildcard expansion. The results are inserted
   into the list out.
   
   If the parameter does not need expansion, it is copied into the list
   out. If expansion is performed, the original parameter is freed and
   newly allocated strings are inserted into the list out.

   If \c context is non-null, all the strings contained in the
   array_list_t \c out will be registered to be free'd when context is
   free'd.
 
   \param context the halloc context to use for automatic deallocation
   \param in The parameter to expand
   \param flag Specifies if any expansion pass should be skipped. Legal values are any combination of EXPAND_SKIP_SUBSHELL EXPAND_SKIP_VARIABLES and EXPAND_SKIP_WILDCARDS
   \param out The list to which the result will be appended.
   \return One of EXPAND_OK, EXPAND_ERROR, EXPAND_WILDCARD_MATCH and EXPAND_WILDCARD_NO_MATCH. EXPAND_WILDCARD_NO_MATCH and EXPAND_WILDCARD_MATCH are normal exit conditions used only on strings containing wildcards to tell if the wildcard produced any matches.
*/
int expand_string( void *context, wchar_t *in, array_list_t *out, int flag );

/**
   expand_one is identical to expand_string, except it will fail if in
   expands to more than one string. This is used for expanding command
   names.

   If \c context is non-null, the returning string ill be registered
   to be free'd when context is free'd.
 
   \param context the halloc context to use for automatic deallocation   
   \param in The parameter to expand
   \param flag Specifies if any expansion pass should be skipped. Legal values are any combination of EXPAND_SKIP_SUBSHELL EXPAND_SKIP_VARIABLES and EXPAND_SKIP_WILDCARDS
   \return The expanded parameter, or 0 on failiure
*/
wchar_t *expand_one( void *context, wchar_t *in, int flag );

/**
   Convert the variable value to a human readable form, i.e. escape things, handle arrays, etc. Suitable for pretty-printing.

   \param in the value to escape
*/
wchar_t *expand_escape_variable( const wchar_t *in );

/**
   Perform tilde expansion and nothing else on the specified string.

   If tilde expansion is needed, the original string is freed and a
   new string, allocated using malloc, is returned.

   \param in the string to tilde expand
*/
wchar_t *expand_tilde(wchar_t *in);


/**
   Test if the specified argument is clean, i.e. it does not contain
   any tokens which need to be expanded or otherwise altered. Clean
   strings can be passed through expand_string and expand_one without
   changing them. About two thirds of all strings are clean, so
   skipping expansion on them actually does save a small amount of
   time, since it avoids multiple memory allocations during the
   expansion process.

   \param in the string to test
*/
int expand_is_clean( const wchar_t *in );

#endif
