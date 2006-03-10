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
	R_BEGINNING_OF_LINE = R_NULL+1, 
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
	R_DELETE_LINE,
	R_BACKWARD_KILL_LINE,
	R_KILL_WHOLE_LINE,
	R_KILL_WORD,
	R_BACKWARD_KILL_WORD,
	R_DUMP_FUNCTIONS,
	R_CLEAR_SCREEN,
	R_EXIT,
	R_HISTORY_TOKEN_SEARCH_BACKWARD,
	R_HISTORY_TOKEN_SEARCH_FORWARD,
	R_SELF_INSERT,
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
   Add a key mapping from the specified sequence

   \param mode the name of the mapping mode to add this mapping to
   \param s the sequence
   \param d a description of the sequence
   \param cmd an input function that will be run whenever the key sequence occurs
*/
void add_mapping( const wchar_t *mode, const wchar_t *s, const wchar_t * d, const wchar_t *cmd );

/**
   Sets the mode keybindings. 
*/
void input_set_mode( wchar_t *name );

/**
   Sets the application keybindings
*/
void input_set_application( wchar_t *name );

/**
   Parse a single line of inputrc information. 
*/
void input_parse_inputrc_line( wchar_t *cmd );

/**
   Returns the function for the given function name.
*/
wchar_t input_get_code( wchar_t *name );

#endif
