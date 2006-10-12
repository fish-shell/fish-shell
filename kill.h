/** \file kill.h
	Prototypes for the killring.

	Works like the killring in emacs and readline. The killring is cut and paste whith a memory of previous cuts.
*/

#ifndef FISH_KILL_H
#define FISH_KILL_H

#include <wchar.h>

/**
   Replace the specified string in the killring
*/
void kill_replace( wchar_t *old, wchar_t *new );


/**
   Add a string to the top of the killring
*/
void kill_add( wchar_t *str );
/**
   Rotate the killring
*/
wchar_t *kill_yank_rotate();
/**
  Paste from the killring
*/
wchar_t *kill_yank();
/**
   Sanity check
*/
void kill_sanity_check();
/**
   Initialize the killring
*/
void kill_init();
/**
   Destroy the killring
*/
void kill_destroy();

#endif
