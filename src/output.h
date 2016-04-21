/** \file output.h
  Generic output functions
*/
/**
   Constants for various character classifications. Each character of a command string can be classified as one of the following types.
*/
#ifndef FISH_OUTPUT_H
#define FISH_OUTPUT_H

#include <vector>
#include <stddef.h>
#include <stdbool.h>

#include "common.h"
#include "fallback.h"  // IWYU pragma: keep
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
    FISH_COLOR_NORMAL,  // the default fg color of the terminal
    FISH_COLOR_RESET
};

/**
   Sets the fg and bg color. May be called as often as you like, since
   if the new color is the same as the previous, nothing will be
   written. Negative values for set_color will also be ignored. Since
   the terminfo string this function emits can potentially cause the
   screen to flicker, the function takes care to write as little as
   possible.

   Possible values for color are any form the FISH_COLOR_* enum
   and FISH_COLOR_RESET. FISH_COLOR_RESET will perform an
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
void writembs_check(char *mbs, const char *mbs_name, const char *file, long line);
#define writembs(mbs) writembs_check((mbs), #mbs, __FILE__, __LINE__)

/**
   Write a wide character using the output method specified using output_set_writer().
*/
int writech(wint_t ch);

/**
   Write a wide character string to FD 1.
*/
void writestr(const wchar_t *str);

/**
   Return the internal color code representing the specified color
*/
rgb_color_t parse_color(const wcstring &val, bool is_background);

/**
   This is for writing process notification messages. Has to write to
   stdout, so clr_eol and such functions will work correctly. Not an
   issue since this function is only used in interactive mode anyway.
*/
int writeb(tputs_arg_t b);

/**
   Set the function used for writing in move_cursor, writespace and
   set_color and all other output functions in this library. By
   default, the write call is used to give completely unbuffered
   output to stdout.
*/
void output_set_writer(int (*writer)(char));

/**
   Return the current output writer
 */
int (*output_get_writer())(char) ;

/** Set the terminal name */
void output_set_term(const wcstring &term);

/** Return the terminal name */
const wchar_t *output_get_term();

/** Sets what colors are supported */
enum
{
    color_support_term256 = 1 << 0,
    color_support_term24bit = 1 << 1
};
typedef unsigned int color_support_t;
color_support_t output_get_color_support();
void output_set_color_support(color_support_t support);

/** Given a list of rgb_color_t, pick the "best" one, as determined by the color support. Returns rgb_color_t::none() if empty */
rgb_color_t best_color(const std::vector<rgb_color_t> &colors, color_support_t support);

/* Exported for builtin_set_color's usage only */
void write_color(rgb_color_t color, bool is_fg);
unsigned char index_for_color(rgb_color_t c);

#endif
