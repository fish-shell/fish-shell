/** \file input.h

Functions for reading a character of input from stdin, using the
inputrc information for key bindings.

*/

#ifndef FISH_INPUT_H
#define FISH_INPUT_H

#include <wchar.h>
#include "input_common.h"

/**
   Key codes for inputrc-style keyboard functions that are passed on
   to the caller of input_read()
*/
enum
{
	R_BEGINNING_OF_LINE = R_NULL+10,  /* This give input_common ten slots for lowlevel keycodes */
	R_END_OF_LINE,
	R_FORWARD_CHAR,
	R_BACKWARD_CHAR,
	R_FORWARD_WORD,
	R_BACKWARD_WORD,
	R_HISTORY_SEARCH_BACKWARD,
	R_HISTORY_SEARCH_FORWARD,
	R_DELETE_CHAR,
	R_BACKWARD_DELETE_CHAR,
	R_KILL_LINE,
	R_YANK,
	R_YANK_POP,
	R_COMPLETE,
	R_BEGINNING_OF_HISTORY,
	R_END_OF_HISTORY,
	R_BACKWARD_KILL_LINE,
	R_KILL_WHOLE_LINE,
	R_KILL_WORD,
	R_BACKWARD_KILL_WORD,
	R_DUMP_FUNCTIONS,
	R_HISTORY_TOKEN_SEARCH_BACKWARD,
	R_HISTORY_TOKEN_SEARCH_FORWARD,
	R_SELF_INSERT,
	R_VI_ARG_DIGIT,
	R_VI_DELETE_TO,
	R_EXECUTE,
	R_BEGINNING_OF_BUFFER,
	R_END_OF_BUFFER,
	R_REPAINT,
	R_UP_LINE,
	R_DOWN_LINE,
}
;

/**
   Initialize the terminal by calling setupterm, and set up arrays
   used by readch to detect escape sequences for special keys.

   Before calling input_init, terminfo is not initialized and MUST not be used
*/
int input_init();

/**
   free up memory used by terminal functions.
*/
void input_destroy();

/**
   Read a character from fd 0. Try to convert some escape sequences
   into character constants, but do not permanently block the escape
   character.

   This is performed in the same way vim does it, i.e. if an escape
   character is read, wait for more input for a short time (a few
   milliseconds). If more input is avaialable, it is assumed to be an
   escape sequence for a special character (such as an arrow key), and
   readch attempts to parse it. If no more input follows after the
   escape key, it is assumed to be an actual escape key press, and is
   returned as such.
*/
wint_t input_readch();

/**
   Push a character or a readline function onto the stack of unread
   characters that input_readch will return before actually reading from fd
   0.   
 */
void input_unreadch( wint_t ch );


/**
   Add a key mapping from the specified sequence to the specified command

   \param sequence the sequence to bind
   \param command an input function that will be run whenever the key sequence occurs
*/
void input_mapping_add( const wchar_t *sequence, const wchar_t *command );

/**
   Insert all mapping names into the specified array_list_t
 */
void input_mapping_get_names( array_list_t *list );

/**
   Erase binding for specified key sequence
 */
int input_mapping_erase( const wchar_t *sequence );

/**
   Return the command bound to the specified key sequence
 */
const wchar_t *input_mapping_get( const wchar_t *sequence );

/**
   Return the sequence for the terminfo variable of the specified name.

   If no terminfo variable of the specified name could be found, return 0 and set errno to ENOENT.
   If the terminfo variable does not have a value, return 0 and set errno to EILSEQ.
 */
const wchar_t *input_terminfo_get_sequence( const wchar_t *name );

/**
   Return the name of the terminfo variable with the specified sequence
 */
const wchar_t *input_terminfo_get_name( const wchar_t *seq );

/**
   Return a list of all known terminfo names
 */
void input_terminfo_get_names( array_list_t *lst, int skip_null );


/**
   Returns the input function code for the given input function name.
*/
wchar_t input_function_get_code( const wchar_t *name );

/**
   Returns a list of all existing input function names
 */
void input_function_get_names( array_list_t *lst );


#endif
