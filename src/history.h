// Prototypes for history functions, part of the user interface.
#ifndef FISH_HISTORY_H
#define FISH_HISTORY_H

// IWYU pragma: no_include <cstddef>
#include <pthread.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <time.h>
#include <wctype.h>

#include <deque>
#include <memory>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include "common.h"
#include "wutil.h"  // IWYU pragma: keep

struct io_streams_t;
class env_stack_t;
class environment_t;

// Fish supports multiple shells writing to history at once. Here is its strategy:
//
// 1. All history files are append-only. Data, once written, is never modified.
//
// 2. A history file may be re-written ("vacuumed"). This involves reading in the file and writing a
// new one, while performing maintenance tasks: discarding items in an LRU fashion until we reach
// the desired maximum count, removing duplicates, and sorting them by timestamp (eventually, not
// implemented yet). The new file is atomically moved into place via rename().
//
// 3. History files are mapped in via mmap(). Before the file is mapped, the file takes a fcntl read
// lock. The purpose of this lock is to avoid seeing a transient state where partial data has been
// written to the file.
//
// 4. History is appended to under a fcntl write lock.
//
// 5. The chaos_mode boolean can be set to true to do things like lower buffer sizes which can
// trigger race conditions. This is useful for testing.

typedef std::vector<wcstring> path_list_t;

enum class history_search_type_t {
    // Search for commands exactly matching the given string.
    exact,
    // Search for commands containing the given string.
    contains,
    // Search for commands starting with the given string.
    prefix,
    // Search for commands containing the given glob pattern.
    contains_glob,
    // Search for commands starting with the given glob pattern.
    prefix_glob,
    // Matches everything.
    match_everything,
};

typedef uint64_t history_identifier_t;

class history_item_t {
    friend class history_t;
    friend struct history_impl_t;
    friend class history_lru_cache_t;
    friend class history_tests_t;

   private:
    // Attempts to merge two compatible history items together.
    bool merge(const history_item_t &item);

    // The actual contents of the entry.
    wcstring contents;

    // Original creation time for the entry.
    time_t creation_timestamp;

    // Sometimes unique identifier used for hinting.
    history_identifier_t identifier;

    // Paths that we require to be valid for this item to be autosuggested.
    path_list_t required_paths;

   public:
    explicit history_item_t(wcstring str = wcstring(), time_t when = 0,
                            history_identifier_t ident = 0);

    const wcstring &str() const { return contents; }

    bool empty() const { return contents.empty(); }

    // Whether our contents matches a search term.
    bool matches_search(const wcstring &term, enum history_search_type_t type,
                        bool case_sensitive) const;

    time_t timestamp() const { return creation_timestamp; }

    const path_list_t &get_required_paths() const { return required_paths; }
    void set_required_paths(const path_list_t &paths) { required_paths = paths; }

    bool operator==(const history_item_t &other) const {
        return contents == other.contents && creation_timestamp == other.creation_timestamp &&
               required_paths == other.required_paths;
    }
};

typedef std::deque<history_item_t> history_item_list_t;

class history_file_contents_t;
struct history_impl_t;

class history_t {
    friend class history_tests_t;
    struct impl_wrapper_t;
    const std::unique_ptr<impl_wrapper_t> wrap_;

    // No copying or moving.
    history_t() = delete;
    history_t(const history_t &) = delete;
    history_t(history_t &&) = delete;
    history_t &operator=(const history_t &) = delete;
    history_t &operator=(history_t &&) = delete;

    acquired_lock<history_impl_t> impl();
    acquired_lock<const history_impl_t> impl() const;

    // Privately add an item. If pending, the item will not be returned by history searches until a
    // call to resolve_pending.
    void add(const history_item_t &item, bool pending = false);

   public:
    explicit history_t(wcstring name);
    ~history_t();

    // Whether we're in maximum chaos mode, useful for testing.
    // This causes things like locks to fail.
    static bool chaos_mode;

    // Whether to force the read path instead of mmap. This is useful for testing.
    static bool never_mmap;

    // Returns history with the given name, creating it if necessary.
    static history_t &history_with_name(const wcstring &name);

    /// Returns whether this is using the default name.
    bool is_default() const;

    // Determines whether the history is empty. Unfortunately this cannot be const, since it may
    // require populating the history.
    bool is_empty();

    // Add a new history item to the end. If pending is set, the item will not be returned by
    // item_at_index until a call to resolve_pending(). Pending items are tracked with an offset
    // into the array of new items, so adding a non-pending item has the effect of resolving all
    // pending items.
    void add(const wcstring &str, history_identifier_t ident = 0, bool pending = false);

    // Remove a history item.
    void remove(const wcstring &str);

    // Add a new pending history item to the end, and then begin file detection on the items to
    // determine which arguments are paths
    void add_pending_with_file_detection(const wcstring &str, const wcstring &working_dir_slash);

    // Resolves any pending history items, so that they may be returned in history searches.
    void resolve_pending();

    // Saves history.
    void save();

    // Searches history.
    bool search(history_search_type_t search_type, const wcstring_list_t &search_args,
                const wchar_t *show_time_format, size_t max_items, bool case_sensitive,
                bool null_terminate, bool reverse, const cancel_checker_t &cancel_check,
                io_streams_t &streams);

    // Irreversibly clears history.
    void clear();

    // Populates from older location (in config path, rather than data path).
    void populate_from_config_path();

    // Populates from a bash history file.
    void populate_from_bash(FILE *f);

    // Incorporates the history of other shells into this history.
    void incorporate_external_changes();

    // Gets all the history into a list. This is intended for the $history environment variable.
    // This may be long!
    void get_history(wcstring_list_t &result);

    // Let indexes be a list of one-based indexes into the history, matching the interpretation of
    // $history. That is, $history[1] is the most recently executed command. Values less than one
    // are skipped. Return a mapping from index to history item text.
    std::unordered_map<long, wcstring> items_at_indexes(const std::vector<long> &idxs);

    // Return the specified history at the specified index. 0 is the index of the current
    // commandline. (So the most recent item is at index 1.)
    history_item_t item_at_index(size_t idx);

    // Return the number of history entries.
    size_t size();
};

/// Flags for history searching.
enum {
    // If set, ignore case.
    history_search_ignore_case = 1 << 0,

    // If set, do not deduplicate, which can help performance.
    history_search_no_dedup = 1 << 1
};
using history_search_flags_t = uint32_t;

/// Support for searching a history backwards.
/// Note this does NOT de-duplicate; it is the caller's responsibility to do so.
class history_search_t {
   private:
    // The history in which we are searching.
    history_t *history_;

    // The original search term.
    wcstring orig_term_;

    // The (possibly lowercased) search term.
    wcstring canon_term_;

    // Our search type.
    enum history_search_type_t search_type_ { history_search_type_t::contains };

    // Our flags.
    history_search_flags_t flags_{0};

    // The current history item.
    maybe_t<history_item_t> current_item_;

    // Index of the current history item.
    size_t current_index_{0};

    // If deduping, the items we've seen.
    std::unordered_set<wcstring> deduper_;

    // return whether we are case insensitive.
    bool ignores_case() const { return flags_ & history_search_ignore_case; }

    // return whether we deduplicate items.
    bool dedup() const { return !(flags_ & history_search_no_dedup); }

   public:
    // Gets the original search term.
    const wcstring &original_term() const { return orig_term_; }

    // Finds the previous search result (backwards in time). Returns true if one was found.
    bool go_backwards();

    // Returns the current search result item. asserts if there is no current item.
    const history_item_t &current_item() const;

    // Returns the current search result item contents. asserts if there is no current item.
    const wcstring &current_string() const;

    // Constructor.
    history_search_t(history_t &hist, const wcstring &str,
                     enum history_search_type_t type = history_search_type_t::contains,
                     history_search_flags_t flags = 0)
        : history_(&hist), orig_term_(str), canon_term_(str), search_type_(type), flags_(flags) {
        if (ignores_case()) {
            std::transform(canon_term_.begin(), canon_term_.end(), canon_term_.begin(), towlower);
        }
    }

    // Default constructor.
    history_search_t() = default;
};

/// Saves the new history to disk.
void history_save_all();

/// Return the prefix for the files to be used for command and read history.
wcstring history_session_id(const environment_t &vars);

/// Given a list of paths and a working directory, return the paths that are valid
/// This does disk I/O and may only be called in a background thread
path_list_t valid_paths(const path_list_t &paths, const wcstring &working_directory);

/// Given a list of paths and a working directory,
/// return true if all paths in the list are valid
/// Returns true for if paths is empty
bool all_paths_are_valid(const path_list_t &paths, const wcstring &working_directory);

/// Sets private mode on. Once in private mode, it cannot be turned off.
void start_private_mode(env_stack_t &vars);
/// Queries private mode status.
bool in_private_mode();

#endif
