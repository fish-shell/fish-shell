#ifndef FISH_ENUM_MAP_H
#define FISH_ENUM_MAP_H

#include "config.h"  // IWYU pragma: keep

#include <cwchar>

/// Used for constructing mappings between enums and strings. The resulting array must be sorted
/// according to the `str` member since str_to_enum() does a binary search. Also the last entry must
/// have NULL for the `str` member and the default value for `val` to be returned if the string
/// isn't found.
template <typename T>
struct enum_map {
    T val;
    const wchar_t *const str;
};

/// Given a string return the matching enum. Return the sentinel enum if no match is made. The map
/// must be sorted by the `str` member. A binary search is twice as fast as a linear search with 16
/// elements in the map.
template <typename T>
T str_to_enum(const wchar_t *name, const enum_map<T> map[], int len) {
    // Ignore the sentinel value when searching as it is the "not found" value.
    size_t left = 0, right = len - 1;

    while (left < right) {
        size_t mid = left + (right - left) / 2;
        int cmp = std::wcscmp(name, map[mid].str);
        if (cmp < 0) {
            right = mid;  // name was smaller than mid
        } else if (cmp > 0) {
            left = mid + 1;  // name was larger than mid
        } else {
            return map[mid].val;  // found it
        }
    }
    return map[len - 1].val;  // return the sentinel value
}

/// Given an enum return the matching string.
template <typename T>
const wchar_t *enum_to_str(T enum_val, const enum_map<T> map[]) {
    for (const enum_map<T> *entry = map; entry->str; entry++) {
        if (enum_val == entry->val) {
            return entry->str;
        }
    }
    return nullptr;
};

#endif
