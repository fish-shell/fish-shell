// Directory utilities. This library contains functions for locating configuration directories, for
// testing if a command with a given name can be found in the PATH, and various other path-related
// issues.
#ifndef FISH_PATH_H
#define FISH_PATH_H

#include <string>

#include "common.h"
#include "wutil.h"

/// Appends a path component, with a / if necessary.
void append_path_component(wcstring &path, const wcstring &component);

#endif
