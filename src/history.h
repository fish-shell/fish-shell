// Prototypes for history functions, part of the user interface.
#ifndef FISH_HISTORY_H
#define FISH_HISTORY_H

#include <stdint.h>
#include <stdio.h>
#include <time.h>
#include <wctype.h>

#include <algorithm>
#include <deque>
#include <memory>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include "common.h"
#include "maybe.h"
#include "wutil.h"  // IWYU pragma: keep

using path_list_t = std::vector<wcstring>;
using history_identifier_t = uint64_t;

#if INCLUDE_RUST_HEADERS

#include "history.rs.h"

#else

struct HistoryItem;

class HistorySharedPtr;
enum class PersistenceMode;
enum class SearchDirection;
enum class SearchType;

#endif  // INCLUDE_RUST_HEADERS

using history_item_t = HistoryItem;
using history_persistence_mode_t = PersistenceMode;
using history_search_direction_t = SearchDirection;
using history_search_type_t = SearchType;
using history_search_flags_t = uint32_t;

/// Flags for history searching.
enum {
    /// If set, ignore case.
    history_search_ignore_case = 1 << 0,

    /// If set, do not deduplicate, which can help performance.
    history_search_no_dedup = 1 << 1
};
using history_search_flags_t = uint32_t;

#endif
