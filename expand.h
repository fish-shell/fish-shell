/**\file expand.h

   Prototypes for string expantion functions. These functions perform
   several kinds of parameter expantion. There are a lot of issues
   with regards to memory allocation. Overall, these functions would
   benefit from using a more clever memory allocation scheme, perhaps
   an evil combination of talloc, string buffers and reference
   counting.

*/

/**
   Flag specifying that subshell expantion should be skipped
*/
#define EXPAND_SKIP_SUBSHELL 1

/**
   Flag specifying that variable expantion should be skipped
*/
#define EXPAND_SKIP_VARIABLES 2

/**
   Flag specifying that wildcard expantion should be skipped
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

/** Character represeting a home directory */
#define HOME_DIRECTORY 0xfffffffc

/** Character represeting process expantion */
#define PROCESS_EXPAND 0xfffffffb

/** Character representing variable expantion */
#define VARIABLE_EXPAND 0xfffffffa

/** Character representing the start of a bracket expantion */
#define BRACKET_BEGIN 0xfffffff9

/** Character representing the end of a bracket expantion */
#define BRACKET_END 0xfffffff8

/** Character representing separation between two bracket elements */
#define BRACKET_SEP 0xfffffff7

/** Character for separating two array elements. We use 30, i.e. the ascii record separator since that seems logical. */
#define ARRAY_SEP 0x1e

/** String containing the character for separating two array elements */
#define ARRAY_SEP_STR L"\x1e"

/**
   Separate subtokens in a token with this character. 
*/
#define INTERNAL_SEPARATOR 0xfffffff0



/**
   Perform various forms of expansion on in, such as tilde expansion
   (~USER becomes the users home directory), variable expansion
   ($VAR_NAME becomes the value of the environment variable VAR_NAME),
   subshell expantion and wildcard expansion. The results are inserted
   into the list out.
   
   If the parameter does not need expansion, it is copied into the list
   out. If expansion is performed, the original parameter is freed and
   newly allocated strings are inserted into the list out.
   
   \param in The parameter to expand
   \param flag Specifies if any expantion pass should be skipped. Legal values are any combination of EXPAND_SKIP_SUBSHELL EXPAND_SKIP_VARIABLES and EXPAND_SKIP_WILDCARDS
   \param out The list to which the result will be appended.
*/
int expand_string( wchar_t *in, array_list_t *out, int flag );

/**
   expand_one is identical to expand_string, except it will fail if in
   expands to more than one string. This is used for expanding command
   names.

   \param in The parameter to expand
   \param flag Specifies if any expantion pass should be skipped. Legal values are any combination of EXPAND_SKIP_SUBSHELL EXPAND_SKIP_VARIABLES and EXPAND_SKIP_WILDCARDS
   \return The expanded parameter, or 0 on failiure
*/
wchar_t *expand_one( wchar_t *in, int flag );

/**
   Replace special characters with escape sequences. Newline is
   replaced with \n, etc. 

   \param in The string to be escaped
   \param escape_all Whether all characters wich hold special meaning in fish (Pipe, semicolon, etc,) should be escaped, or only unprintable characters
   \return The escaped string, or 0 if there is not enough memory
*/
wchar_t *expand_escape( wchar_t *in, int escape_all );

/**
   Convert the variable value to a human readable form, i.e. escape things, handle arrays, etc. Suitable for pretty-printing.
*/
wchar_t *expand_escape_variable( const wchar_t *in );


/**
   Perform tilde expantion and nothing else on the specified string.

   If tilde expantion is needed, the original string is freed and a
   new string, allocated using malloc, is returned.
*/
wchar_t *expand_tilde(wchar_t *in);

/**
   Locate the last subshell in the specified string.
   
   \param in the string to search for subshells
   \param begin the starting paranthesis of the subshell
   \param end the ending paranthesis of the subshell
   \param flags set this variable to ACCEPT_INCOMPLETE if in tab_completion mode
   \return -1 on syntax error, 0 if no subshells exist and 1 on sucess
*/
int expand_locate_subshell( wchar_t *in, 
							wchar_t **begin, 
							wchar_t **end,
							int flags );


/**
   Expand backslashed escapes and substitute them with their unescaped
   counterparts. Also optionally change the wildcards, the tilde
   character and a few more into constants which are defined to be
   outside of the valid character space, but still inside the valid
   space for a wchar_t. This assumes that a wchar_t is at least 32
   bits long AND that the characterset is UCS4 or some other 31-bit
   character set.

   Since removing the escape sequences can never lengthen the string,
   the specified string is modified instead of allocating a new one.

*/
wchar_t *expand_backslash( wchar_t * in, int escape_special );


/**
   Tokenize the specified string into the specified array_list_t.
   Each new element is allocated using malloc and must be freed by the
   caller.
   
   \param val the input string. The contents of this string is not changed.
   \param out the list in which to place the elements. 
*/
void expand_variable_array( const wchar_t *val, array_list_t *out );

