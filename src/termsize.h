// Support for exposing the terminal size.

#include "config.h"  // IWYU pragma: keep
#ifndef FISH_TERMSIZE_H
#define FISH_TERMSIZE_H

#if INCLUDE_RUST_HEADERS
#include "termsize.rs.h"
#else
struct termsize_t;
#endif

#endif  // FISH_TERMSIZE_H
