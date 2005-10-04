/** \file highlight.h
	Prototypes for functions for syntax highlighting
*/

#ifndef FISH_HIGHLIGHT_H
#define FISH_HIGHLIGHT_H

#include <wchar.h>

#include "util.h"

/**
   Perform syntax highlighting for the shell commands in buff. The result is
   stored in the color array as a color_code from the HIGHLIGHT_ enum
   for each character in buff.

   \param buff The buffer on which to perform syntax highlighting
   \param color The array in wchich to store the color codes. The first 8 bits are used for fg color, the next 8 bits for bg color. 
   \param pos the cursor position. Used for quote matching, etc.
   \param error a list in which a description of each error will be inserted. May be 0, in whcich case no error descriptions will be generated.
*/
void highlight_shell( wchar_t * buff, int *color, int pos, array_list_t *error );

/**
   Perform syntax highlighting for the text in buff. Matching quotes and paranthesis are highlighted. The result is
   stored in the color array as a color_code from the HIGHLIGHT_ enum
   for each character in buff.

   \param buff The buffer on which to perform syntax highlighting
   \param color The array in wchich to store the color codes. The first 8 bits are used for fg color, the next 8 bits for bg color. 
   \param pos the cursor position. Used for quote matching, etc.
   \param error a list in which a description of each error will be inserted. May be 0, in whcich case no error descriptions will be generated.
*/
void highlight_universal( wchar_t * buff, int *color, int pos, array_list_t *error );

/**
   Translate from HIGHLIGHT_* to FISH_COLOR_* according to environment
   variables. Defaults to FISH_COLOR_NORMAL.

   Example: 

   If the environment variable FISH_FISH_COLOR_ERROR is set to 'red', a
   call to highlight_get_color( HIGHLIGHT_ERROR) will return
   FISH_COLOR_RED.
*/
int highlight_get_color( int highlight );

#endif
