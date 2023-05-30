#ifndef FISH_KILL_H
#define FISH_KILL_H

#include "common.h"
#include "kill.rs.h"

/// Rotate the killring.
wcstring kill_yank_rotate();

/// Paste from the killring.
wcstring kill_yank();

/// Get copy of kill ring as vector of strings
std::vector<wcstring> kill_entries();

#endif
