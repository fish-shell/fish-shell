// My own globbing implementation. Needed to implement this instead of using libs globbing to
// support tab-expansion of globbed parameters.
#ifndef FISH_WILDCARD_H
#define FISH_WILDCARD_H

#include <stddef.h>

#include "common.h"
#if INCLUDE_RUST_HEADERS
#include "wildcard.rs.h"
#endif

// Enumeration of all wildcard types.
enum {
    /// Character representing any character except '/' (slash).
    ANY_CHAR = WILDCARD_RESERVED_BASE,
    /// Character representing any character string not containing '/' (slash).
    ANY_STRING,
    /// Character representing any character string.
    ANY_STRING_RECURSIVE,
    /// This is a special pseudo-char that is not used other than to mark the
    /// end of the the special characters so we can sanity check the enum range.
    ANY_SENTINEL
};

#endif
