// SPDX-FileCopyrightText: © 2005 Axel Liljencrantz
// SPDX-FileCopyrightText: © 2009 fish-shell contributors
// SPDX-FileCopyrightText: © 2022 fish-shell contributors
//
// SPDX-License-Identifier: GPL-2.0-only

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

/// Get copy of kill ring as vector of strings
wcstring_list_t kill_entries();

#endif
