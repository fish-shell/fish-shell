/** \file output.h
	Generic output functions
*/
/**
   Constants for various character classifications. Each character of a command string can be classified as one of the following types.
*/

#ifndef FISH_OUTPUT_H
#define FISH_OUTPUT_H

#include <wchar.h>
#include "screen.h"
#include "color.h"

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
	FISH_COLOR_NORMAL,
	FISH_COLOR_IGNORE,
	FISH_COLOR_RESET
}
;

/**
   The value to send to set_color to tell it to use a bold font
*/
#define FISH_COLOR_BOLD 0x80
/**
   The value to send to set_color to tell it to underline the text
*/
#define FISH_COLOR_UNDERLINE 0x100

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


void set_color(rgb_color_t c, rgb_color_t c2);

/**
   Write specified multibyte string
 */
#define writembs( mbs )                         \
        {                                       \
                char *tmp = mbs;                \
                if( tmp )				\
                {					\
                        writembs_internal( tmp );			\
                }							\
                else							\
                {							\
                        debug( 0,					\
			       _(L"Tried to use terminfo string %s on line %d of %s, which is undefined in terminal of type \"%ls\". Please report this error to %s"), \
			       #mbs,					\
			       __LINE__,				\
			       __FILE__,				\
			       output_get_term(),			\
			       PACKAGE_BUGREPORT);			\
		}                                                       \
	}


/**
   Write a char * narrow string to FD 1, needed for the terminfo
   strings. This is usually just a wrapper aound tputs, using writeb
   as the sending function. But a weird bug on PPC Linux means that on
   this platform, write is instead used directly.
*/
int writembs_internal( char *str );

/**
   Write a wide character using the output method specified using output_set_writer().
*/
int writech( wint_t ch );

/**
   Write a wide character string to FD 1.
*/
void writestr( const wchar_t *str );

/**
   Write a wide character string to FD 1. If the string is wider than
   the specified maximum, truncate and ellipsize it.
*/
void writestr_ellipsis( const wchar_t *str, int max_width );

/**
   Escape and write a string to fd 1
*/
int write_escaped_str( const wchar_t *str, int max_len );

/**
   Return the internal color code representing the specified color
*/
int output_color_code( const wcstring &val, bool is_background );
rgb_color_t parse_color( const wcstring &val, bool is_background );

/**
   This is for writing process notification messages. Has to write to
   stdout, so clr_eol and such functions will work correctly. Not an
   issue since this function is only used in interactive mode anyway.
*/
int writeb( tputs_arg_t b );

/**
   Set the function used for writing in move_cursor, writespace and
   set_color and all other output functions in this library. By
   default, the write call is used to give completely unbuffered
   output to stdout.
*/
void output_set_writer( int (*writer)(char) );

/**
   Return the current output writer
 */
int (*output_get_writer())(char) ;

/** Set the terminal name */
void output_set_term( const wchar_t *term );

/** Return the terminal name */
const wchar_t *output_get_term();

/** Sets whether term256 colors are supported */
bool output_get_supports_term256();
void output_set_supports_term256(bool val);

#endif
