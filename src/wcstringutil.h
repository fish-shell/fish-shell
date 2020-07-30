// Helper functions for working with wcstring.
#ifndef FISH_WCSTRINGUTIL_H
#define FISH_WCSTRINGUTIL_H

#include <algorithm>
#include <cstring>
#include <string>
#include <utility>

#include "common.h"
#include "expand.h"

/// Test if a string prefixes another. Returns true if a is a prefix of b.
bool string_prefixes_string(const wcstring &proposed_prefix, const wcstring &value);
bool string_prefixes_string(const wchar_t *proposed_prefix, const wcstring &value);
bool string_prefixes_string(const wchar_t *proposed_prefix, const wchar_t *value);
bool string_prefixes_string(const char *proposed_prefix, const std::string &value);
bool string_prefixes_string(const char *proposed_prefix, const char *value);

/// Test if a string is a suffix of another.
bool string_suffixes_string(const wcstring &proposed_suffix, const wcstring &value);
bool string_suffixes_string(const wchar_t *proposed_suffix, const wcstring &value);
bool string_suffixes_string_case_insensitive(const wcstring &proposed_suffix,
                                             const wcstring &value);

/// Test if a string prefixes another without regard to case. Returns true if a is a prefix of b.
bool string_prefixes_string_case_insensitive(const wcstring &proposed_prefix,
                                             const wcstring &value);

/// Case-insensitive string search, modeled after std::string::find().
/// \param fuzzy indicates this is being used for fuzzy matching and case insensitivity is
/// expanded to include symbolic characters (#3584).
/// \return the offset of the first case-insensitive matching instance of `needle` within
/// `haystack`, or `string::npos()` if no results were found.
size_t ifind(const wcstring &haystack, const wcstring &needle, bool fuzzy = false);
size_t ifind(const std::string &haystack, const std::string &needle, bool fuzzy = false);

/// Split a string by a separator character.
wcstring_list_t split_string(const wcstring &val, wchar_t sep);

/// Join a list of strings by a separator character.
wcstring join_strings(const wcstring_list_t &vals, wchar_t sep);

inline wcstring to_string(long x) {
    wchar_t buff[64];
    format_long_safe(buff, x);
    return wcstring(buff);
}

inline wcstring to_string(unsigned long long x) {
    wchar_t buff[64];
    format_ullong_safe(buff, x);
    return wcstring(buff);
}

inline wcstring to_string(int x) { return to_string(static_cast<long>(x)); }

inline wcstring to_string(size_t x) { return to_string(static_cast<unsigned long long>(x)); }

inline bool bool_from_string(const std::string &x) {
    if (x.empty()) return false;
    switch (x.front()) {
        case 'Y':
        case 'T':
        case 'y':
        case 't':
        case '1':
            return true;
        default:
            return false;
    }
}

inline bool bool_from_string(const wcstring &x) {
    return !x.empty() && std::wcschr(L"YTyt1", x.at(0));
}

/// @typedef wcstring_range represents a range in a wcstring.
/// The first element is the location, the second is the count.
typedef std::pair<wcstring::size_type, wcstring::size_type> wcstring_range;

/// wcstring equivalent of wcstok(). Supports NUL. For convenience and wcstok() compatibility, the
/// first character of each token separator is replaced with NUL.
/// @return Returns a pair of (pos, count).
///         This will be (npos, npos) when it's done. In the form of (pos, npos)
///         when the token is already known to be the final token.
/// @note The final token may not necessarily return (pos, npos).
wcstring_range wcstring_tok(wcstring &str, const wcstring &needle,
                            wcstring_range last = wcstring_range(0, 0));

/// Given iterators into a string (forward or reverse), splits the haystack iterators
/// about the needle sequence, up to max times. Inserts splits into the output array.
/// If the iterators are forward, this does the normal thing.
/// If the iterators are backward, this returns reversed strings, in reversed order!
/// If the needle is empty, split on individual elements (characters).
/// Max output entries will be max + 1 (after max splits)
template <typename ITER>
void split_about(ITER haystack_start, ITER haystack_end, ITER needle_start, ITER needle_end,
                 wcstring_list_t *output, long max = LONG_MAX, bool no_empty = false) {
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
    // Prefer niceness over minimalness
    Prettiest,
    // Make every character count ($ instead of ...)
    Shortest,
};

wcstring truncate(const wcstring &input, int max_len,
                  ellipsis_type etype = ellipsis_type::Prettiest);
wcstring trim(wcstring input);
wcstring trim(wcstring input, const wchar_t *any_of);

/// Converts a string to lowercase.
wcstring wcstolower(wcstring input);

// Out-of-line helper for wcs2string_callback.
void wcs2string_bad_char(wchar_t);

/// Implementation of wcs2string that accepts a callback.
/// This invokes \p func with (const char*, size_t) pairs.
/// If \p func returns false, it stops; otherwise it continues.
/// \return false if the callback returned false, otherwise true.
template <typename Func>
bool wcs2string_callback(const wchar_t *input, size_t len, const Func &func) {
    mbstate_t state = {};
    char converted[MB_LEN_MAX];

    for (size_t i = 0; i < len; i++) {
        wchar_t wc = input[i];
        // TODO: this doesn't seem sound.
        if (wc == INTERNAL_SEPARATOR) {
            // do nothing
        } else if (wc >= ENCODE_DIRECT_BASE && wc < ENCODE_DIRECT_BASE + 256) {
            converted[0] = wc - ENCODE_DIRECT_BASE;
            if (!func(converted, 1)) return false;
        } else if (MB_CUR_MAX == 1) {  // single-byte locale (C/POSIX/ISO-8859)
            // If `wc` contains a wide character we emit a question-mark.
            if (wc & ~0xFF) {
                wc = '?';
            }
            converted[0] = wc;
            if (!func(converted, 1)) return false;
        } else {
            std::memset(converted, 0, sizeof converted);
            size_t len = std::wcrtomb(converted, wc, &state);
            if (len == static_cast<size_t>(-1)) {
                wcs2string_bad_char(wc);
                std::memset(&state, 0, sizeof(state));
            } else {
                if (!func(converted, len)) return false;
            }
        }
    }
    return true;
}

/// Support for iterating over a newline-separated string.
template <typename Collection>
class line_iterator_t {
    // Storage for each line.
    Collection storage;

    // The collection we're iterating. Note we hold this by reference.
    const Collection &coll;

    // The current location in the iteration.
    typename Collection::const_iterator current;

   public:
    /// Construct from a collection (presumably std::string or std::wcstring).
    line_iterator_t(const Collection &coll) : coll(coll), current(coll.cbegin()) {}

    /// Access the storage in which the last line was stored.
    const Collection &line() const { return storage; }

    /// Advances to the next line. \return true on success, false if we have exhausted the string.
    bool next() {
        if (current == coll.end()) return false;
        auto newline_or_end = std::find(current, coll.cend(), '\n');
        storage.assign(current, newline_or_end);
        current = newline_or_end;

        // Skip the newline.
        if (current != coll.cend()) ++current;
        return true;
    }
};

#endif
