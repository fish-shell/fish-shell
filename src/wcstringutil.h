// Helper functions for working with wcstring.
#ifndef FISH_WCSTRINGUTIL_H
#define FISH_WCSTRINGUTIL_H

#include <string>
#include <utility>

#include "common.h"

/// @typedef wcstring_range represents a range in a wcstring.
/// The first element is the location, the second is the count.
typedef std::pair<wcstring::size_type, wcstring::size_type> wcstring_range;

/// wcstring equivalent of wcstok(). Supports NUL. For convenience and wcstok() compatibility, the
/// first character of each token separator is replaced with NUL.
/// @return Returns a pair of (pos, count).
///         This will be (npos, npos) when it's done. In the form of (pos, npos)
///         when the token is already known to be the final token.
/// @note The final token may not necessarily return (pos, npos).
wcstring_range wcstring_tok(wcstring& str, const wcstring& needle,
                            wcstring_range last = wcstring_range(0, 0));

#endif
