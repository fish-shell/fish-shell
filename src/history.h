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

class history_search_type_t;
class HistorySharedPtr;
enum class PersistenceMode;

#endif  // INCLUDE_RUST_HEADERS

using history_item_t = HistoryItem;
using history_persistence_mode_t = PersistenceMode;

#endif

#if 0

/**
Fish supports multiple shells writing to history at once. Here is its strategy:

1. All history files are append-only. Data, once written, is never modified.

2. A history file may be re-written ("vacuumed"). This involves reading in the file and writing a
new one, while performing maintenance tasks: discarding items in an LRU fashion until we reach
the desired maximum count, removing duplicates, and sorting them by timestamp (eventually, not
implemented yet). The new file is atomically moved into place via rename().

3. History files are mapped in via mmap(). Before the file is mapped, the file takes a fcntl read
lock. The purpose of this lock is to avoid seeing a transient state where partial data has been
written to the file.

4. History is appended to under a fcntl write lock.

5. The chaos_mode boolean can be set to true to do things like lower buffer sizes which can
trigger race conditions. This is useful for testing.
*/
using history_search_type_t = SearchType;
using history_persistence_mode_t = PersistenceMode;

using history_item_t = HistoryItem;

/*
class history_item_t {
   public:
    /// Construct from a text, timestamp, and optional identifier.
    /// If \p persist_mode is ::ephemeral, then do not write this item to disk.
    explicit history_item_t(
        wcstring str = {}, time_t when = 0, history_identifier_t ident = 0,
        history_persistence_mode_t persist_mode = history_persistence_mode_t::Disk);

    /// \return the text as a string.
    const wcstring &str() const { return contents; }

    /// \return whether the text is empty.
    bool empty() const { return contents.empty(); }

    /// \return whether our contents matches a search term.
    bool matches_search(const wcstring &term, history_search_type_t type,
                        bool case_sensitive) const;

    /// \return the timestamp for creating this history item.
    time_t timestamp() const { return creation_timestamp; }

    /// \return whether this item should be persisted (written to disk).
    bool should_write_to_disk() const { return persist_mode == history_persistence_mode_t::Disk; }

    /// Get and set the list of arguments which referred to files.
    /// This is used for autosuggestion hinting.
    const path_list_t &get_required_paths() const { return required_paths; }
    void set_required_paths(path_list_t paths) { required_paths = std::move(paths); }

   private:
    /// Attempts to merge two compatible history items together.
    bool merge(const history_item_t &item);

    /// The actual contents of the entry.
    wcstring contents;

    /// Original creation time for the entry.
    time_t creation_timestamp;

    /// Paths that we require to be valid for this item to be autosuggested.
    path_list_t required_paths;

    /// Sometimes unique identifier used for hinting.
    history_identifier_t identifier;

    /// If set, do not write this item to disk.
    history_persistence_mode_t persist_mode;

    friend class history_t;
    friend struct history_impl_t;
    friend class history_lru_cache_t;
    friend class history_tests_t;
};
*/

using history_item_list_t = std::deque<history_item_t>;
using history_search_direction_t = SearchDirection;

using history_t = HistorySharedPtr;

/*
class history_t : noncopyable_t, nonmovable_t {
    friend class history_tests_t;
    struct impl_wrapper_t;
    const std::unique_ptr<impl_wrapper_t> wrap_;

    acquired_lock<history_impl_t> impl();
    acquired_lock<const history_impl_t> impl() const;

    /// Privately add an item. If pending, the item will not be returned by history searches until a
    /// call to resolve_pending. Any trailing ephemeral items are dropped.
    void add(history_item_t &&item, bool pending = false);

    /// Add a new history item with text \p str to the end of history.
    void add(wcstring str);

   public:
    explicit history_t(wcstring name);
    ~history_t();

    /// Whether we're in maximum chaos mode, useful for testing.
    /// This causes things like locks to fail.
    static bool chaos_mode;

    /// Whether to force the read path instead of mmap. This is useful for testing.
    static bool never_mmap;

    /// Returns history with the given name, creating it if necessary.
    static std::shared_ptr<history_t> with_name(const wcstring &name);

    /// Returns whether this is using the default name.
    bool is_default() const;

    /// Determines whether the history is empty. Unfortunately this cannot be const, since it may
    /// require populating the history.
    bool is_empty();

    /// Remove a history item.
    void remove(const wcstring &str);

    /// Remove any trailing ephemeral items.
    void remove_ephemeral_items();

    /// Add a new pending history item to the end, and then begin file detection on the items to
    /// determine which arguments are paths. Arguments may be expanded (e.g. with PWD and variables)
    /// using the given \p vars. The item has the given \p persist_mode.
    static void add_pending_with_file_detection(
        const std::shared_ptr<history_t> &self, const wcstring &str,
        const std::shared_ptr<environment_t> &vars,
        history_persistence_mode_t persist_mode = history_persistence_mode_t::disk);

    /// Resolves any pending history items, so that they may be returned in history searches.
    void resolve_pending();

    /// Saves history.
    void save();

    /// Searches history.
    bool search(history_search_type_t search_type, const std::vector<wcstring> &search_args,
                const wchar_t *show_time_format, size_t max_items, bool case_sensitive,
                bool null_terminate, bool reverse, const cancel_checker_t &cancel_check,
                io_streams_t &streams);

    /// Irreversibly clears history.
    void clear();

    /// Irreversibly clears history for the current session.
    void clear_session();

    /// Populates from older location (in config path, rather than data path).
    void populate_from_config_path();

    /// Populates from a bash history file.
    void populate_from_bash(FILE *f);

    /// Incorporates the history of other shells into this history.
    void incorporate_external_changes();

    /// Gets all the history into a list. This is intended for the $history environment variable.
    /// This may be long!
    void get_history(std::vector<wcstring> &result);

    /// Let indexes be a list of one-based indexes into the history, matching the interpretation of
    /// $history. That is, $history[1] is the most recently executed command. Values less than one
    /// are skipped. Return a mapping from index to history item text.
    std::unordered_map<long, wcstring> items_at_indexes(const std::vector<long> &idxs);

    /// Return the specified history at the specified index. 0 is the index of the current
    /// commandline. (So the most recent item is at index 1.)
    history_item_t item_at_index(size_t idx);

    /// Return the number of history entries.
    size_t size();
};
*/

/// Flags for history searching.
enum {
    /// If set, ignore case.
    history_search_ignore_case = 1 << 0,

    /// If set, do not deduplicate, which can help performance.
    history_search_no_dedup = 1 << 1
};
using history_search_flags_t = uint32_t;

using history_search_t = HistorySearch;

/*
/// Support for searching a history backwards.
/// Note this does NOT de-duplicate; it is the caller's responsibility to do so.
class history_search_t {
   private:
    /// The history in which we are searching.
    /// TODO: this should be a shared_ptr.
    history_t *history_;

    /// The original search term.
    wcstring orig_term_;

    /// The (possibly lowercased) search term.
    wcstring canon_term_;

    /// Our search type.
    enum history_search_type_t search_type_ { history_search_type_t::contains };

    /// Our flags.
    history_search_flags_t flags_{0};

    /// The current history item.
    maybe_t<history_item_t> current_item_;

    /// Index of the current history item.
    size_t current_index_{0};

    /// If deduping, the items we've seen.
    std::unordered_set<wcstring> deduper_;

    /// return whether we deduplicate items.
    bool dedup() const { return !(flags_ & history_search_no_dedup); }

   public:
    /// Gets the original search term.
    const wcstring &original_term() const { return orig_term_; }

    // Finds the next search result. Returns true if one was found.
    bool go_to_next_match(history_search_direction_t direction);

    /// Returns the current search result item. asserts if there is no current item.
    const history_item_t &current_item() const;

    /// Returns the current search result item contents. asserts if there is no current item.
    const wcstring &current_string() const;

    /// Returns the index of the current history item.
    size_t current_index() const;

    /// return whether we are case insensitive.
    bool ignores_case() const { return flags_ & history_search_ignore_case; }

    /// Construct from a history pointer; the caller is responsible for ensuring the history stays
    /// alive.
    history_search_t(history_t *hist, const wcstring &str,
                     enum history_search_type_t type = history_search_type_t::contains,
                     history_search_flags_t flags = 0, size_t starting_index = 0)
        : history_(hist),
          orig_term_(str),
          canon_term_(str),
          search_type_(type),
          flags_(flags),
          current_index_(starting_index) {
        if (ignores_case()) {
            std::transform(canon_term_.begin(), canon_term_.end(), canon_term_.begin(), towlower);
        }
    }

    /// Construct from a shared_ptr. TODO: this should be the only constructor.
    history_search_t(const std::shared_ptr<history_t> &hist, const wcstring &str,
                     enum history_search_type_t type = history_search_type_t::contains,
                     history_search_flags_t flags = 0, size_t starting_index = 0)
        : history_search_t(hist.get(), str, type, flags, starting_index) {}

    /// Default constructor.
    history_search_t() = default;
};
*/

/** Return the prefix for the files to be used for command and read history. */
inline wcstring history_session_id(const environment_t &vars) { return *rust_session_id(vars); }

/**
    Given a list of proposed paths and a context, perform variable and home directory expansion,
    and detect if the result expands to a value which is also the path to a file.
    Wildcard expansions are suppressed - see implementation comments for why.
    This is used for autosuggestion hinting. If we add an item to history, and one of its arguments
    refers to a file, then we only want to suggest it if there is a valid file there.
    This does disk I/O and may only be called in a background thread.
*/
path_list_t expand_and_detect_paths(const path_list_t &paths, const environment_t &vars);

/**
    Given a list of proposed paths and a context, expand each one and see if it refers to a file.
    Wildcard expansions are suppressed.
    \return true if \p paths is empty or every path is valid.
*/
inline bool all_paths_are_valid(const path_list_t &paths, const operation_context_t &ctx) {
    return rust_all_paths_are_valid(paths, ctx);
}

#endif  // FISH_HISTORY_H
