/**
   Constants for various character classifications. Each character of a command string can be classified as one of the following types.
*/

#ifndef FISH_OUTPUT_H
#define FISH_OUTPUT_H

#include <wchar.h>

enum
{
	HIGHLIGHT_NORMAL,
	HIGHLIGHT_COMMAND,
	HIGHLIGHT_SUBSHELL,
	HIGHLIGHT_REDIRECTION, 
	HIGHLIGHT_END, 
	HIGHLIGHT_ERROR,
	HIGHLIGHT_PARAM,
	HIGHLIGHT_COMMENT,
	HIGHLIGHT_MATCH,
	HIGHLIGHT_SEARCH_MATCH,
}
	;

/**
   Constants for various colors as used by the set_color function. 
*/
enum
{
	FISH_COLOR_BLACK,
	FISH_COLOR_RED,
	FISH_COLOR_GREEN,
	FISH_COLOR_YELLOW,
	FISH_COLOR_BLUE,
	FISH_COLOR_MAGENTA,
	FISH_COLOR_CYAN,
	FISH_COLOR_WHITE,
	/** The default fg color of the terminal */
	FISH_COLOR_NORMAL
}
;
 

/**
   Sets the fg and bg color. May be called as often as you like, since
   if the new color is the same as the previous, nothing will be
   written. Negative values for set_color will also be ignored. Since
   the terminfo string this function emits can potentially cause the
   screen to flicker, the function takes care to write as little as
   possible.

   Possible values for color are any form the FISH_COLOR_* enum,
   FISH_COLOR_IGNORE and FISH_COLOR_RESET. FISH_COLOR_IGNORE will
   leave the color unchanged, and FISH_COLOR_RESET will perform an
   exit_attribute_mode, even if set_color thinks it is already in
   FISH_COLOR_NORMAL mode.

   In order to set the color to normal, three terminfo strings may
   have to be written.

   - First a string to set the color, such as set_a_foreground. This
   is needed because otherwise the previous strings colors might be
   removed as well.

   - After that we write the exit_attribute_mode string to reset all
   color attributes.

   - Lastly we may need to write set_a_background or set_a_foreground
   to set the other half of the color pair to what it should be.

   \param c Foreground color.
   \param c2 Background color.
*/


void set_color( int c, int c2 );

/**
   Write a char * narrow string to FD 1, needed for the terminfo
   strings.
*/
int writembs( char *str );

int writech( wint_t ch );

void writestr( const wchar_t *str );

void writestr_ellipsis( const wchar_t *str, int max_width );

int write_escaped_str( const wchar_t *str, int max_len );

int writespace( int c );

int output_color_code( const wchar_t *val );

void output_init();

void output_destroy();

#endif
