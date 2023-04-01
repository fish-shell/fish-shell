// Fish version receiver.
//
// This file has a specific purpose of shortening compilation times when
// the only change is different `git describe` version.
#include "ghoti_version.h"

#ifndef FISH_BUILD_VERSION
// The contents of FISH-BUILD-VERSION-FILE looks like:
// FISH_BUILD_VERSION="2.7.1-62-gc0480092-dirty"
// Arrange for it to become a variable.
static const char *const
#include "FISH-BUILD-VERSION-FILE"
    ;
#endif

/// Return ghoti shell version.
const char *get_ghoti_version() { return FISH_BUILD_VERSION; }
