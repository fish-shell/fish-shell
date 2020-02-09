// Prototypes for the killring.
//
// Works like the killring in emacs and readline. The killring is cut and paste with a memory of
// previous cuts.
#ifndef FISH_KILL_H
#define FISH_KILL_H

#include "common.h"

/// Replace the specified string in the killring.
void kill_replace(const wcstring &old, const wcstring &newv);

/// Add a string to the top of the killring.
void kill_add(wcstring str);

/// Rotate the killring.
wcstring kill_yank_rotate();

/// Paste from the killring.
wcstring kill_yank();

#endif
