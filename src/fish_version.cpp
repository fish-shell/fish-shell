/** \file fish_version.c Fish version receiver.

  This file has a specific purpose of shortening compilation times when
  the only change is different `git describe` version.
*/

#include "fish_version.h"

/**
 * Return fish shell version.
 */
const char *get_fish_version() {
    return FISH_BUILD_VERSION;
}
