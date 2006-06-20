/** \file highlight.h
	Prototypes for functions for syntax highlighting
*/

#ifndef FISH_HIGHLIGHT_H
#define FISH_HIGHLIGHT_H

#include <wchar.h>

#include "util.h"

/**
   Internal value representing highlighting of normal text
*/
#define HIGHLIGHT_NORMAL 0x1
/**
   Internal value representing highlighting of an error
*/
#define HIGHLIGHT_ERROR 0x2
/**
   Internal value representing highlighting of a command
*/
#define HIGHLIGHT_COMMAND 0x4
/**
   Internal value representing highlighting of a process separator
*/
#define HIGHLIGHT_END 0x8
/**
   Internal value representing highlighting of a regular command parameter
*/
#define HIGHLIGHT_PARAM 0x10
/**
   Internal value representing highlighting of a comment
*/
#define HIGHLIGHT_COMMENT 0x20
/**
   Internal value representing highlighting of a matching parenteses, etc.
*/
#define HIGHLIGHT_MATCH 0x40
/**
   Internal value representing highlighting of a search match
*/
#define HIGHLIGHT_SEARCH_MATCH 0x80
/**
   Internal value representing highlighting of an operator
*/
#define HIGHLIGHT_OPERATOR 0x100
/**
   Internal value representing highlighting of an escape sequence
*/
#define HIGHLIGHT_ESCAPE 0x200
/**
   Internal value representing highlighting of a quoted string
*/
#define HIGHLIGHT_QUOTE 0x400
/**
   Internal value representing highlighting of an IO redirection
*/
#define HIGHLIGHT_REDIRECTION 0x800 
/**
   Internal value representing highlighting a potentially valid path
*/
#define HIGHLIGHT_VALID_PATH 0x1000

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
