/** \file common.h
	Prototypes for various functions, mostly string utilities, that are used by most parts of fish.
*/

#ifndef FISH_COMMON_H
/**
   Header guard
*/
#define FISH_COMMON_H

#include <stdlib.h>
#include <stdio.h>
#include <wchar.h>
#include <termios.h>

#include "util.h"

/**
   Maximum number of bytes used by a single utf-8 character
*/
#define MAX_UTF8_BYTES 6

/**
   This is in the unicode private use area.
*/
#define ENCODE_DIRECT_BASE 0xf100

/**
  Highest legal ascii value
*/
#define ASCII_MAX 127u

/**
  Highest legal 16-bit unicode value
*/
#define UCS2_MAX 0xffffu

/**
  Highest legal byte value
*/
#define BYTE_MAX 0xffu

/**
  Escape special fish syntax characters like the semicolon
 */
#define UNESCAPE_SPECIAL 1

/**
  Allow incomplete escape sequences
 */
#define UNESCAPE_INCOMPLETE 2

/**
   Escape all characters, including magic characters like the semicolon
 */
#define ESCAPE_ALL 1
/**
   Do not try to use 'simplified' quoted escapes, and do not use empty quotes as the empty string
 */
#define ESCAPE_NO_QUOTED 2


/** 
	Save the shell mode on startup so we can restore them on exit
*/
extern struct termios shell_modes;      

/**
   The character to use where the text has been truncated. Is an
   ellipsis on unicode system and a $ on other systems.
*/
extern wchar_t ellipsis_char;

/**
   The verbosity level of fish. If a call to debug has a severity
   level higher than \c debug_level, it will not be printed.
*/
extern int debug_level;

/**
   Profiling flag. True if commands should be profiled.
*/
extern char *profile;

/**
   Name of the current program. Should be set at startup. Used by the
   debug function.
*/
extern wchar_t *program_name;

/**
   This macro is used to check that an input argument is not null. It
   is a bit lika a non-fatal form of assert. Instead of exit-ing on
   failiure, the current function is ended at once. The second
   parameter is the return value of the current function on failiure.
*/
#define CHECK( arg, retval )											\
	if( !(arg) )														\
	{																	\
		debug( 0,														\
			   _( L"function %s called with null value for argument %s. " ), \
			   __func__,												\
			   #arg );													\
		bugreport();													\
		show_stackframe();												\
		return retval;													\
	}

/**
   Pause for input, then exit the program. If supported, print a backtrace first.
*/
#define FATAL_EXIT()											\
	{															\
		int exit_read_count;char exit_read_buff;				\
		show_stackframe();										\
		exit_read_count=read( 0, &exit_read_buff, 1 );			\
		exit( 1 );												\
	}															\
		

/**
   Exit program at once, leaving an error message about running out of memory.
*/
#define DIE_MEM()														\
	{																	\
		fwprintf( stderr,												\
				  L"fish: Out of memory on line %d of file %s, shutting down fish\n", \
				  __LINE__,												\
				  __FILE__ );											\
		FATAL_EXIT();														\
	}

/**
   Check if signals are blocked. If so, print an error message and
   return from the function performing this check.
*/
#define CHECK_BLOCK( retval )											\
	if( signal_is_blocked() )											\
	{																	\
		debug( 0,														\
			   _( L"function %s called while blocking signals. " ),		\
			   __func__);												\
		bugreport();													\
		show_stackframe();												\
		return retval;													\
	}
		
/**
   Shorthand for wgettext call
*/
#define _(wstr) wgettext(wstr)

/**
   Noop, used to tell xgettext that a string should be translated,
   even though it is not directly sent to wgettext. 
*/
#define N_(wstr) wstr

/**
   Check if the specified stringelement is a part of the specified string list
 */
#define contains( str,... ) contains_internal( str, __VA_ARGS__, (void *)0 )
/**
   Concatenate all the specified strings into a single newly allocated one
 */
#define wcsdupcat( str,... ) wcsdupcat_internal( str, __VA_ARGS__, (void *)0 )

/**
  Print a stack trace to stderr
*/
void show_stackframe();

/**
   Take an array_list_t containing wide strings and converts them to a
   single null-terminated wchar_t **. The array is allocated using
   malloc, and needs to be fred's by the caller.
*/
wchar_t **list_to_char_arr( array_list_t *l );

/**
   Read a line from the stream f into the buffer buff of length len. If
   buff is to small, it will be reallocated, and both buff and len will
   be updated to reflect this. Returns the number of bytes read or -1
   on failiure. 

   If the carriage return character is encountered, it is
   ignored. fgetws() considers the line to end if reading the file
   results in either a newline (L'\n') character, the null (L'\\0')
   character or the end of file (WEOF) character.
*/
int fgetws2( wchar_t **buff, int *len, FILE *f );

/**
   Sorts an array_list of wide strings according to the
   wcsfilecmp-function from the util library
*/
void sort_list( array_list_t *comp );

/**
   Returns a newly allocated wide character string equivalent of the
   specified multibyte character string

   This function encodes illegal character sequences in a reversible
   way using the private use area.
*/
wchar_t *str2wcs( const char *in );

/**
   Converts the narrow character string \c in into it's wide
   equivalent, stored in \c out. \c out must have enough space to fit
   the entire string.

   This function encodes illegal character sequences in a reversible
   way using the private use area.
*/
wchar_t *str2wcs_internal( const char *in, wchar_t *out );

/**
   Returns a newly allocated multibyte character string equivalent of
   the specified wide character string

   This function decodes illegal character sequences in a reversible
   way using the private use area.
*/
char *wcs2str( const wchar_t *in );

/**
   Converts the wide character string \c in into it's narrow
   equivalent, stored in \c out. \c out must have enough space to fit
   the entire string.

   This function decodes illegal character sequences in a reversible
   way using the private use area.
*/
char *wcs2str_internal( const wchar_t *in, char *out );

/**
   Returns a newly allocated wide character string array equivalent of
   the specified multibyte character string array
*/
char **wcsv2strv( const wchar_t **in );

/**
   Returns a newly allocated multibyte character string array equivalent of the specified wide character string array
*/
wchar_t **strv2wcsv( const char **in );


/**
   Returns a newly allocated concatenation of the specified wide
   character strings. The last argument must be a null pointer.
*/
__sentinel wchar_t *wcsdupcat_internal( const wchar_t *a, ... );

/**
   Test if the given string is a valid variable name
*/


/**
   Test if the given string is a valid variable name. 

   \return null if this is a valid name, and a pointer to the first invalid character otherwise
*/

wchar_t *wcsvarname( const wchar_t *str );


/**
   Test if the given string is a valid function name. 

   \return null if this is a valid name, and a pointer to the first invalid character otherwise
*/

wchar_t *wcsfuncname( const wchar_t *str );

/**
   Test if the given string is valid in a variable name 

   \return 1 if this is a valid name, 0 otherwise
*/

int wcsvarchr( wchar_t chr );


/**
   A wcswidth workalike. Fish uses this since the regular wcswidth seems flaky.
*/
int my_wcswidth( const wchar_t *c );

/**
   This functions returns the end of the quoted substring beginning at
   \c in. The type of quoting character is detemrined by examining \c
   in. Returns 0 on error.

   \param in the position of the opening quote
*/
wchar_t *quote_end( const wchar_t *in );

/**
   A call to this function will reset the error counter. Some
   functions print out non-critical error messages. These should check
   the error_count before, and skip printing the message if
   MAX_ERROR_COUNT messages have been printed. The error_reset()
   should be called after each interactive command executes, to allow
   new messages to be printed.
*/
void error_reset();

/**
   This function behaves exactly like a wide character equivalent of
   the C function setlocale, except that it will also try to detect if
   the user is using a Unicode character set, and if so, use the
   unicode ellipsis character as ellipsis, instead of '$'.      
*/
const wchar_t *wsetlocale( int category, const wchar_t *locale );

/**
   Checks if \c needle is included in the list of strings specified. A warning is printed if needle is zero.

   \param needle the string to search for in the list 

   \return zero if needle is not found, of if needle is null, non-zero otherwise
*/
__sentinel int contains_internal( const wchar_t *needle, ... );

/**
   Call read while blocking the SIGCHLD signal. Should only be called
   if you _know_ there is data available for reading, or the program
   will hang until there is data.
*/
int read_blocked(int fd, void *buf, size_t count);

/**
   Loop a write request while failiure is non-critical. Return -1 and set errno
   in case of critical error.
 */
ssize_t write_loop(int fd, char *buff, size_t count);


/**
   Issue a debug message with printf-style string formating and
   automatic line breaking. The string will begin with the string \c
   program_name, followed by a colon and a whitespace.

   Because debug is often called to tell the user about an error,
   before using wperror to give a specific error message, debug will
   never ever modify the value of errno.
   
   \param level the priority of the message. Lower number means higher priority. Messages with a priority_number higher than \c debug_level will be ignored..
   \param msg the message format string. 

   Example:

   <code>debug( 1, L"Pi = %.3f", M_PI );</code>

   will print the string 'fish: Pi = 3.141', given that debug_level is 1 or higher, and that program_name is 'fish'.
*/
void debug( int level, const wchar_t *msg, ... );

/**
   Replace special characters with backslash escape sequences. Newline is
   replaced with \n, etc. 

   \param in The string to be escaped
   \param escape_all Whether all characters wich hold special meaning in fish (Pipe, semicolon, etc,) should be escaped, or only unprintable characters
   \return The escaped string, or 0 if there is not enough memory
*/

wchar_t *escape( const wchar_t *in, int escape_all );

/**
   Expand backslashed escapes and substitute them with their unescaped
   counterparts. Also optionally change the wildcards, the tilde
   character and a few more into constants which are defined in a
   private use area of Unicode. This assumes wchar_t is a unicode
   character set.

   The result must be free()d. The original string is not modified. If
   an invalid sequence is specified, 0 is returned.

*/
wchar_t *unescape( const wchar_t * in, 
				   int escape_special );

/**
   Attempt to acquire a lock based on a lockfile, waiting LOCKPOLLINTERVAL 
   milliseconds between polls and timing out after timeout seconds, 
   thereafter forcibly attempting to obtain the lock if force is non-zero.
   Returns 1 on success, 0 on failure.
   To release the lock the lockfile must be unlinked.
   A unique temporary file named by appending characters to the lockfile name 
   is used; any pre-existing file of the same name is subject to deletion.
*/
int acquire_lock_file( const char *lockfile, const int timeout, int force );

/** 
    Returns the width of the terminal window, so that not all
    functions that use these values continually have to keep track of
    it separately.

    Only works if common_handle_winch is registered to handle winch signals.
*/
int common_get_width();
/**
   Returns the height of the terminal window, so that not all
   functions that use these values continually have to keep track of
   it separatly.

   Only works if common_handle_winch is registered to handle winch signals.
*/
int common_get_height();

/**
  Handle a window change event by looking up the new window size and
  saving it in an internal variable used by common_get_wisth and
  common_get_height().
*/
void common_handle_winch( int signal );

/**
   Write paragraph of output to the specified stringbuffer, and redo
   the linebreaks to fit the current screen.
*/
void write_screen( const wchar_t *msg, string_buffer_t *buff );

/**
   Tokenize the specified string into the specified array_list_t.
   Each new element is allocated using malloc and must be freed by the
   caller.
   
   \param val the input string. The contents of this string is not changed.
   \param out the list in which to place the elements. 
*/
void tokenize_variable_array( const wchar_t *val, array_list_t *out );

/**
   Make sure the specified direcotry exists. If needed, try to create
   it and any currently not existing parent directories..

   \return 0 if, at the time of function return the directory exists, -1 otherwise.
*/
int create_directory( wchar_t *d );

/**
   Print a short message about how to file a bug report to stderr
*/
void bugreport();

/**
   Format the specified size (in bytes, kilobytes, etc.) into the specified stringbuffer.
*/
void sb_format_size( string_buffer_t *sb,
		     long long sz );

/**
   Return the number of seconds from the UNIX epoch, with subsecond
   precision. This function uses the gettimeofday function, and will
   have the same precision as that function.

   If an error occurs, NAN is returned.
 */
double timef();


#endif

