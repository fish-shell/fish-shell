// Helper functions for working with wcstring.
#ifndef FISH_WCSTRINGUTIL_H
#define FISH_WCSTRINGUTIL_H

#include <algorithm>
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

/// Given iterators into a string (forward or reverse), splits the haystack iterators
/// about the needle sequence, up to max times. Inserts splits into the output array.
/// If the iterators are forward, this does the normal thing.
/// If the iterators are backward, this returns reversed strings, in reversed order!
/// If the needle is empty, split on individual elements (characters).
/// Max output entries will be max + 1 (after max splits)
template <typename ITER>
void split_about(ITER haystack_start, ITER haystack_end, ITER needle_start, ITER needle_end,
                 wcstring_list_t* output, long max = LONG_MAX, bool no_empty = false) {
    long remaining = max;
    ITER haystack_cursor = haystack_start;
    while (remaining > 0 && haystack_cursor != haystack_end) {
        ITER split_point;
        if (needle_start == needle_end) {  // empty needle, we split on individual elements
            split_point = haystack_cursor + 1;
        } else {
            split_point = std::search(haystack_cursor, haystack_end, needle_start, needle_end);
        }
        if (split_point == haystack_end) {  // not found
            break;
        }
        if (!no_empty || haystack_cursor != split_point) {
            output->emplace_back(haystack_cursor, split_point);
        }
        remaining--;
        // Need to skip over the needle for the next search note that the needle may be empty.
        haystack_cursor = split_point + std::distance(needle_start, needle_end);
    }
    // Trailing component, possibly empty.
    if (!no_empty || haystack_cursor != haystack_end) {
        output->emplace_back(haystack_cursor, haystack_end);
    }
}

enum class ellipsis_type {
    None,
    //Prefer niceness over minimalness
    Prettiest,
    //Make every character count ($ instead of ...)
    Shortest,
};

wcstring truncate(const wcstring &input, int max_len, ellipsis_type etype = ellipsis_type::Prettiest);
wcstring trim(const wcstring &input);
wcstring trim(const wcstring &input, const wchar_t *any_of);

#endif
