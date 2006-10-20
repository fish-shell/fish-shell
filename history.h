/** \file history.h
  Prototypes for history functions, part of the user interface.
*/

#ifndef FISH_HISTORY_H
#define FISH_HISTORY_H

#include <wchar.h>

/**
   Init history library. The history file won't actually be loaded
   until the first time a history search is performed.
*/
void history_init();

/**
   Saves the new history to disc and frees all memory used by the history.
*/
void history_destroy();

/**
  Add a new history item to the bottom of the history, containing a
  copy of str. Remove any duplicates. Moves the current item past the
  end of the history list.
*/
void history_add( const wchar_t *str );

/**
  Find previous history item starting with str. If this moves before
  the start of the history, str is returned.
*/
const wchar_t *history_prev_match( const wchar_t *str );

/**
   Return the specified history at the specified index, or 0 if out of bounds. 0 is the index of the current commandline.
*/
wchar_t *history_get( int idx );


/**
   Move to first history item
*/
void history_first();

/**
   Make current point to last history item
*/
void history_reset();


/**
  Find next history item starting with str. If this moves past
  the end of the history, str is returned.  
*/
const wchar_t *history_next_match( const wchar_t *str);


/**
   Set the current mode name for history. Each application that uses
   the history has it's own mode. This must be called prior to any use
   of the history.
*/

void history_set_mode( const wchar_t *name );


/**
   Perform sanity checks
*/
void history_sanity_check();

#endif
