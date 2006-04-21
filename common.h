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
   Color code for set_color. Does not update the color.
*/

#define FISH_COLOR_IGNORE -1

/**
   Color code for set_color. Sets the default color.
*/
#define FISH_COLOR_RESET -2

/**
   This is in the unicode private use area.
*/
#define ENCODE_DIRECT_BASE 0xf100

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
   The maximum number of charset convertion errors to report
*/
extern int error_max;

/**
   The verbosity of fish
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
   Take an array_list_t containing wide strings and converts them to a
   single null-terminated wchar_t **. The array is allocated using
   halloc, and uses the \c context parameter as context. If \c context
   is not noll, all elements of the \c array_list_t are also
   registered to \c context using \c halloc_register().
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
   Sorts a list of wide strings according to the wcsfilecmp-function
   from the util library
*/
void sort_list( array_list_t *comp );

/**
   Returns a newly allocated wide character string equivalent of the
   specified multibyte character string
*/
wchar_t *str2wcs( const char *in );
wchar_t *str2wcs_internal( const char *in, wchar_t *out );

/**
   Returns a newly allocated multibyte character string equivalent of
   the specified wide character string
*/
char *wcs2str( const wchar_t *in );

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
   character strings
*/
wchar_t *wcsdupcat( const wchar_t *a, const wchar_t *b );

/**
   Returns a newly allocated concatenation of the specified wide
   character strings. The last argument must be a null pointer.
*/
wchar_t *wcsdupcat2( const wchar_t *a, ... );

/**
   Test if the given string is a valid variable name
*/


/**
   Test if the given string is a valid variable name. 

   \return null if this is a valid name, and a pointer to the first invalid character otherwise
*/

wchar_t *wcsvarname( wchar_t *str );


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
const wchar_t *quote_end( const wchar_t *in );

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
   Checks if \c needle is included in the list of strings specified

   \param needle the string to search for in the list 
*/
int contains_str( const wchar_t *needle, ... );

/**
   Call read while blocking the SIGCHLD signal. Should only be called
   if you _know_ there is data available for reading.
*/
int read_blocked(int fd, void *buf, size_t count);

/**
   Exit program at once, leaving an error message about running out of memory
*/
void die_mem();

/**
  Create global_context using halloc
*/
void common_init();

/**
   Free global_context using halloc_free
*/
void common_destroy();

/**
   Issue a debug message with printf-style string formating and
   automatic line breaking. The string will begin with the string \c
   program_name, followed by a colon and a whitespace.
   
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
	it.

   Only works if common_handle_winch is registered to handle winch signals.
*/
int common_get_width();
/**
   Returns the height of the terminal window, so that not all
   functions that use these values continually have to keep track of
   it.

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
   Write paragraph of output to screen. Ignore newlines in message and
   perform internal line-breaking.
*/
void write_screen( const wchar_t *msg );


#endif

