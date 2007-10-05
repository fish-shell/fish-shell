/** \file reader.h 

    Prototypes for functions for reading data from stdin and passing
	to the parser. If stdin is a keyboard, it supplies a killring,
	history, syntax highlighting, tab-completion and various other
	features.
*/

#ifndef FISH_READER_H
#define FISH_READER_H

#include <wchar.h>

#include "util.h"
#include "io.h"

/**
  Read commands from \c fd until encountering EOF
*/
int reader_read( int fd, io_data_t *io);

/**
  Tell the shell that it should exit after the currently running command finishes.
*/
void reader_exit( int do_exit, int force );

/**
   Check that the reader is in a sane state
*/
void reader_sanity_check();

/**
   Initialize the reader
*/
void reader_init();

/**
   Destroy and free resources used by the reader
*/
void reader_destroy();

/**
   Returns the filename of the file currently read
*/
wchar_t *reader_current_filename();

/**
   Push a new filename on the stack of read files
   
   \param fn The fileanme to push
*/
void reader_push_current_filename( const wchar_t *fn );
/**
   Pop the current filename from the stack of read files
 */
wchar_t *reader_pop_current_filename();

/**
   Write the title to the titlebar. This function is called just
   before a new application starts executing and just after it
   finishes.
*/
void reader_write_title();

/**
   Call this function to tell the reader that a repaint is needed, and
   should be performed when possible.
 */
void reader_repaint_needed();

/**
   Run the specified command with the correct terminal modes, and
   while taking care to perform job notification, set the title, etc.
*/
void reader_run_command( const wchar_t *buff );

/**
   Get the string of character currently entered into the command
   buffer, or 0 if interactive mode is uninitialized.
*/
wchar_t *reader_get_buffer();

/**
   Set the string of characters in the command buffer, as well as the cursor position.

   \param b the new buffer value
   \param p the cursor position. If \c p is less than zero, the cursor is placed on the last character.
*/
void reader_set_buffer( wchar_t *b, int p );

/**
   Get the current cursor position in the command line. If interactive
   mode is uninitialized, return -1.
*/
int reader_get_cursor_pos();

/**
   Return the value of the interrupted flag, which is set by the sigint
   handler, and clear it if it was set.
*/
int reader_interrupted();

/**
   Read one line of input. Before calling this function, reader_push()
   must have been called in order to set up a valid reader
   environment.
*/
wchar_t *reader_readline();

/**
   Push a new reader environment. 
*/
void reader_push( wchar_t *name );

/**
   Return to previous reader environment
*/
void reader_pop();

/**
   Specify function to use for finding possible tab completions. The function must take these arguments: 

   - The command to be completed as a null terminated array of wchar_t
   - An array_list_t in which completions will be inserted.
*/
void reader_set_complete_function( void (*f)( const wchar_t *, array_list_t * ) );

/**
   Specify function for syntax highlighting. The function must take these arguments:

   - The command to be highlighted as a null terminated array of wchar_t
   - The color code of each character as an array of ints
   - The cursor position
   - An array_list_t used for storing error messages
*/
void reader_set_highlight_function( void (*f)( wchar_t *, int *, int, array_list_t * ) );

/**
   Specify function for testing if the command buffer contains syntax
   errors that must be corrected before returning.
*/
void reader_set_test_function( int (*f)( wchar_t * ) );

/**
   Specify string of shell commands to be run in order to generate the
   prompt.
*/
void reader_set_prompt( wchar_t *prompt );

/**
   Returns true if the shell is exiting, 0 otherwise. 
*/
int exit_status();

/**
   Replace the current token with the specified string
*/
void reader_replace_current_token( wchar_t *new_token );

/**
   The readers interrupt signal handler. Cancels all currently running blocks.
*/
void reader_handle_int( int signal );

/**
   This function returns true if fish is exiting by force, i.e. because stdin died
*/
int reader_exit_forced();

/**
   Test if the given shell command contains errors. Uses parser_test
   for testing. Suitable for reader_set_test_function().
*/
int reader_shell_test( wchar_t *b );

/**
   Test whether the interactive reader is in search mode.

   \return o if not in search mode, 1 if in search mode and -1 if not in interactive mode
 */
int reader_search_mode();


#endif
