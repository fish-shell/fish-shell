// History functions, part of the user interface.
#include "config.h"  // IWYU pragma: keep

#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>

#include <cstdint>
#include <cstring>
// We need the sys/file.h for the flock() declaration on Linux but not OS X.
#include <sys/file.h>  // IWYU pragma: keep
#include <sys/stat.h>
#include <unistd.h>
#include <wctype.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cwchar>
#include <functional>
#include <iterator>
#include <map>
#include <numeric>
#include <random>
#include <type_traits>
#include <unordered_set>

#include "ast.h"
#include "common.h"
#include "env.h"
#include "fallback.h"  // IWYU pragma: keep
#include "flog.h"
#include "global_safety.h"
#include "history.h"
#include "history_file.h"
#include "io.h"
#include "iothread.h"
#include "lru.h"
#include "parse_constants.h"
#include "parse_util.h"
#include "parser.h"
#include "path.h"
#include "reader.h"
#include "wcstringutil.h"
#include "wildcard.h"  // IWYU pragma: keep
#include "wutil.h"     // IWYU pragma: keep

// Our history format is intended to be valid YAML. Here it is:
//
//   - cmd: ssh blah blah blah
//     when: 2348237
//     paths:
//       - /path/to/something
//       - /path/to/something_else
//
//   Newlines are replaced by \n. Backslashes are replaced by \\.

// This is the history session ID we use by default if the user has not set env var fish_history.
#define DFLT_FISH_HISTORY_SESSION_ID L"fish"

// When we rewrite the history, the number of items we keep.
#define HISTORY_SAVE_MAX (1024 * 256)

// Default buffer size for flushing to the history file.
#define HISTORY_OUTPUT_BUFFER_SIZE (64 * 1024)

// The file access mode we use for creating history files
static constexpr int history_file_mode = 0600;

// How many times we retry to save
// Saving may fail if the file is modified in between our opening
// the file and taking the lock
static constexpr int max_save_tries = 1024;

namespace {

/// If the size of \p buffer is at least \p min_size, output the contents of a string \p str to \p
/// fd, and clear the string. \return 0 on success, an error code on failure.
int flush_to_fd(std::string *buffer, int fd, size_t min_size) {
    if (buffer->empty() || buffer->size() < min_size) {
        return 0;
    }
    if (write_loop(fd, buffer->data(), buffer->size()) < 0) {
        return errno;
    }
    buffer->clear();
    return 0;
}

class time_profiler_t {
    const char *what;
    double start;

   public:
    explicit time_profiler_t(const char *w) {
        what = w;
        start = timef();
    }

    ~time_profiler_t() {
        double end = timef();
        FLOGF(profile_history, "%s: %.0f ms", what, (end - start) * 1000);
    }
};

/// \return the path for the history file for the given \p session_id, or none() if it could not be
/// loaded. If suffix is provided, append that suffix to the path; this is used for temporary files.
maybe_t<wcstring> history_filename(const wcstring &session_id, const wcstring &suffix = {}) {
    if (session_id.empty()) return none();

    wcstring result;
    if (!path_get_data(result)) return none();

    result.append(L"/");
    result.append(session_id);
    result.append(L"_history");
    result.append(suffix);
    return result;
}

/// Lock the history file.
/// Returns true on success, false on failure.
bool history_file_lock(int fd, int lock_type) {
    static std::atomic<bool> do_locking(true);
    if (!do_locking) return false;

    double start_time = timef();
    int retval = flock(fd, lock_type);
    double duration = timef() - start_time;
    if (duration > 0.25) {
        FLOGF(warning, _(L"Locking the history file took too long (%.3f seconds)."), duration);
        // we've decided to stop doing any locking behavior
        // but make sure we don't leave the file locked!
        if (retval == 0 && lock_type != LOCK_UN) {
            flock(fd, LOCK_UN);
        }
        do_locking = false;
        return false;
    }
    return retval != -1;
}

}  // anonymous namespace

class history_lru_cache_t : public lru_cache_t<history_lru_cache_t, history_item_t> {
   public:
    explicit history_lru_cache_t(size_t max)
        : lru_cache_t<history_lru_cache_t, history_item_t>(max) {}

    /// Function to add a history item.
    void add_item(history_item_t item) {
        // Skip empty items.
        if (item.empty()) return;

        // See if it's in the cache. If it is, update the timestamp. If not, we create a new node
        // and add it. Note that calling get_node promotes the node to the front.
        wcstring key = item.str();
        history_item_t *node = this->get(key);
        if (node == nullptr) {
            this->insert(std::move(key), std::move(item));
        } else {
            node->creation_timestamp = std::max(node->timestamp(), item.timestamp());
            // What to do about paths here? Let's just ignore them.
        }
    }
};

/// We can merge two items if they are the same command. We use the more recent timestamp, more
/// recent identifier, and the longer list of required paths.
bool history_item_t::merge(const history_item_t &item) {
    bool result = false;
    if (this->contents == item.contents) {
        this->creation_timestamp = std::max(this->creation_timestamp, item.creation_timestamp);
        if (this->required_paths.size() < item.required_paths.size()) {
            this->required_paths = item.required_paths;
        }
        if (this->identifier < item.identifier) {
            this->identifier = item.identifier;
        }
        result = true;
    }
    return result;
}

history_item_t::history_item_t(wcstring str, time_t when, history_identifier_t ident)
    : contents(trim(std::move(str))), creation_timestamp(when), identifier(ident) {}

bool history_item_t::matches_search(const wcstring &term, enum history_search_type_t type,
                                    bool case_sensitive) const {
    // Note that 'term' has already been lowercased when constructing the
    // search object if we're doing a case insensitive search.
    wcstring contents_lower;
    if (!case_sensitive) {
        contents_lower = wcstolower(contents);
    }
    const wcstring &content_to_match = case_sensitive ? contents : contents_lower;

    switch (type) {
        case history_search_type_t::exact: {
            return term == content_to_match;
        }
        case history_search_type_t::contains: {
            return content_to_match.find(term) != wcstring::npos;
        }
        case history_search_type_t::prefix: {
            return string_prefixes_string(term, content_to_match);
        }
        case history_search_type_t::contains_glob: {
            wcstring wcpattern1 = parse_util_unescape_wildcards(term);
            if (wcpattern1.front() != ANY_STRING) wcpattern1.insert(0, 1, ANY_STRING);
            if (wcpattern1.back() != ANY_STRING) wcpattern1.push_back(ANY_STRING);
            return wildcard_match(content_to_match, wcpattern1);
        }
        case history_search_type_t::prefix_glob: {
            wcstring wcpattern2 = parse_util_unescape_wildcards(term);
            if (wcpattern2.back() != ANY_STRING) wcpattern2.push_back(ANY_STRING);
            return wildcard_match(content_to_match, wcpattern2);
        }
        case history_search_type_t::match_everything: {
            return true;
        }
    }
    DIE("unexpected history_search_type_t value");
}

struct history_impl_t {
    // Privately add an item. If pending, the item will not be returned by history searches until a
    // call to resolve_pending.
    void add(const history_item_t &item, bool pending = false, bool do_save = true);

    // Internal function.
    void clear_file_state();

    // The name of this list. Used for picking a suitable filename and for switching modes.
    const wcstring name;

    // New items. Note that these are NOT discarded on save. We need to keep these around so we can
    // distinguish between items in our history and items in the history of other shells that were
    // started after we were started.
    history_item_list_t new_items;

    // The index of the first new item that we have not yet written.
    size_t first_unwritten_new_item_index{0};

    // Whether we have a pending item. If so, the most recently added item is ignored by
    // item_at_index.
    bool has_pending_item{false};

    // Whether we should disable saving to the file for a time.
    uint32_t disable_automatic_save_counter{0};

    // Deleted item contents.
    std::unordered_set<wcstring> deleted_items{};

    // The buffer containing the history file contents.
    std::unique_ptr<history_file_contents_t> file_contents{};

    // The file ID of the history file.
    file_id_t history_file_id = kInvalidFileID;

    // The boundary timestamp distinguishes old items from new items. Items whose timestamps are <=
    // the boundary are considered "old". Items whose timestemps are > the boundary are new, and are
    // ignored by this instance (unless they came from this instance). The timestamp may be adjusted
    // by incorporate_external_changes().
    time_t boundary_timestamp{time(nullptr)};

    // How many items we add until the next vacuum. Initially a random value.
    int countdown_to_vacuum{-1};

    // Whether we've loaded old items.
    bool loaded_old{false};

    // List of old items, as offsets into out mmap data.
    std::deque<size_t> old_item_offsets{};

    // Figure out the offsets of our file contents.
    void populate_from_file_contents();

    // Loads old items if necessary.
    void load_old_if_needed();

    // Deletes duplicates in new_items.
    void compact_new_items();

    // Attempts to rewrite the existing file to a target temporary file
    // Returns false on error, true on success
    bool rewrite_to_temporary_file(int existing_fd, int dst_fd) const;

    // Saves history by rewriting the file.
    bool save_internal_via_rewrite();

    // Saves history by appending to the file.
    bool save_internal_via_appending();

    // Saves history.
    void save(bool vacuum = false);

    // Saves history unless doing so is disabled.
    void save_unless_disabled();

    explicit history_impl_t(wcstring name) : name(std::move(name)) {}
    history_impl_t(history_impl_t &&) = default;
    ~history_impl_t() = default;

    /// Returns whether this is using the default name.
    bool is_default() const;

    // Determines whether the history is empty. Unfortunately this cannot be const, since it may
    // require populating the history.
    bool is_empty();

    // Add a new history item to the end. If pending is set, the item will not be returned by
    // item_at_index until a call to resolve_pending(). Pending items are tracked with an offset
    // into the array of new items, so adding a non-pending item has the effect of resolving all
    // pending items.
    void add(const wcstring &str, history_identifier_t ident = 0, bool pending = false,
             bool save = true);

    // Remove a history item.
    void remove(const wcstring &str);

    // Add a new pending history item to the end, and then begin file detection on the items to
    // determine which arguments are paths
    void add_pending_with_file_detection(const wcstring &str, const wcstring &working_dir_slash);

    // Resolves any pending history items, so that they may be returned in history searches.
    void resolve_pending();

    // Enable / disable automatic saving. Main thread only!
    void disable_automatic_saving();
    void enable_automatic_saving();

    // Irreversibly clears history.
    void clear();

    // Populates from older location ()in config path, rather than data path).
    void populate_from_config_path();

    // Populates from a bash history file.
    void populate_from_bash(FILE *stream);

    // Incorporates the history of other shells into this history.
    void incorporate_external_changes();

    // Gets all the history into a list. This is intended for the $history environment variable.
    // This may be long!
    void get_history(wcstring_list_t &result);

    // Let indexes be a list of one-based indexes into the history, matching the interpretation of
    // $history. That is, $history[1] is the most recently executed command. Values less than one
    // are skipped. Return a mapping from index to history item text.
    std::unordered_map<long, wcstring> items_at_indexes(const std::vector<long> &idxs);

    // Sets the valid file paths for the history item with the given identifier.
    void set_valid_file_paths(const wcstring_list_t &valid_file_paths, history_identifier_t ident);

    // Return the specified history at the specified index. 0 is the index of the current
    // commandline. (So the most recent item is at index 1.)
    history_item_t item_at_index(size_t idx);

    // Return the number of history entries.
    size_t size();
};

void history_impl_t::add(const history_item_t &item, bool pending, bool do_save) {
    // We use empty items as sentinels to indicate the end of history.
    // Do not allow them to be added (#6032).
    if (item.contents.empty()) {
        return;
    }
    // Try merging with the last item.
    if (!new_items.empty() && new_items.back().merge(item)) {
        // We merged, so we don't have to add anything. Maybe this item was pending, but it just got
        // merged with an item that is not pending, so pending just becomes false.
        this->has_pending_item = false;
    } else {
        // We have to add a new item.
        new_items.push_back(item);
        this->has_pending_item = pending;
        if (do_save) save_unless_disabled();
    }
}

void history_impl_t::save_unless_disabled() {
    // Respect disable_automatic_save_counter.
    if (disable_automatic_save_counter > 0) {
        return;
    }

    // We may or may not vacuum. We try to vacuum every kVacuumFrequency items, but start the
    // countdown at a random number so that even if the user never runs more than 25 commands, we'll
    // eventually vacuum.  If countdown_to_vacuum is -1, it means we haven't yet picked a value for
    // the counter.
    const int kVacuumFrequency = 25;
    if (countdown_to_vacuum < 0) {
        // Generate a number in the range [0, kVacuumFrequency).
        std::uniform_int_distribution<unsigned> dist{0, kVacuumFrequency - 1};
        unsigned seed =
            static_cast<unsigned>(std::chrono::system_clock::now().time_since_epoch().count());
        std::minstd_rand gen{seed};
        countdown_to_vacuum = dist(gen);
    }

    // Determine if we're going to vacuum.
    bool vacuum = false;
    if (countdown_to_vacuum == 0) {
        countdown_to_vacuum = kVacuumFrequency;
        vacuum = true;
    }

    // This might be a good candidate for moving to a background thread.
    time_profiler_t profiler(vacuum ? "save vacuum"       //!OCLINT(unused var)
                                    : "save no vacuum");  //!OCLINT(side-effect)
    this->save(vacuum);

    // Update our countdown.
    assert(countdown_to_vacuum > 0);
    countdown_to_vacuum--;
}

void history_impl_t::add(const wcstring &str, history_identifier_t ident, bool pending,
                         bool do_save) {
    time_t when = time(nullptr);
    // Big hack: do not allow timestamps equal to our boundary date. This is because we include
    // items whose timestamps are equal to our boundary when reading old history, so we can catch
    // "just closed" items. But this means that we may interpret our own items, that we just wrote,
    // as old items, if we wrote them in the same second as our birthdate.
    if (when == this->boundary_timestamp) {
        when++;
    }

    this->add(history_item_t(str, when, ident), pending, do_save);
}

// Remove matching history entries from our list of new items. This only supports literal,
// case-sensitive, matches.
void history_impl_t::remove(const wcstring &str_to_remove) {
    // Add to our list of deleted items.
    deleted_items.insert(str_to_remove);

    size_t idx = new_items.size();
    while (idx--) {
        bool matched = new_items.at(idx).str() == str_to_remove;
        if (matched) {
            new_items.erase(new_items.begin() + idx);
            // If this index is before our first_unwritten_new_item_index, then subtract one from
            // that index so it stays pointing at the same item. If it is equal to or larger, then
            // we have not yet written this item, so we don't have to adjust the index.
            if (idx < first_unwritten_new_item_index) {
                first_unwritten_new_item_index--;
            }
        }
    }
    assert(first_unwritten_new_item_index <= new_items.size());
}

void history_impl_t::set_valid_file_paths(const wcstring_list_t &valid_file_paths,
                                          history_identifier_t ident) {
    // 0 identifier is used to mean "not necessary".
    if (ident == 0) {
        return;
    }

    // Look for an item with the given identifier. It is likely to be at the end of new_items.
    for (auto iter = new_items.rbegin(); iter != new_items.rend(); ++iter) {
        if (iter->identifier == ident) {  // found it
            iter->required_paths = valid_file_paths;
            break;
        }
    }
}

void history_impl_t::get_history(wcstring_list_t &result) {
    // If we have a pending item, we skip the first encountered (i.e. last) new item.
    bool next_is_pending = this->has_pending_item;
    std::unordered_set<wcstring> seen;

    // Append new items.
    for (auto iter = new_items.crbegin(); iter < new_items.crend(); ++iter) {
        // Skip a pending item if we have one.
        if (next_is_pending) {
            next_is_pending = false;
            continue;
        }

        if (seen.insert(iter->str()).second) result.push_back(iter->str());
    }

    // Append old items.
    load_old_if_needed();
    for (auto iter = old_item_offsets.crbegin(); iter != old_item_offsets.crend(); ++iter) {
        size_t offset = *iter;
        const history_item_t item = file_contents->decode_item(offset);
        if (seen.insert(item.str()).second) result.push_back(item.str());
    }
}

size_t history_impl_t::size() {
    size_t new_item_count = new_items.size();
    if (this->has_pending_item && new_item_count > 0) new_item_count -= 1;
    load_old_if_needed();
    size_t old_item_count = old_item_offsets.size();
    return new_item_count + old_item_count;
}

history_item_t history_impl_t::item_at_index(size_t idx) {
    // 0 is considered an invalid index.
    assert(idx > 0);
    idx--;

    // Determine how many "resolved" (non-pending) items we have. We can have at most one pending
    // item, and it's always the last one.
    size_t resolved_new_item_count = new_items.size();
    if (this->has_pending_item && resolved_new_item_count > 0) {
        resolved_new_item_count -= 1;
    }

    // idx == 0 corresponds to the last resolved item.
    if (idx < resolved_new_item_count) {
        return new_items.at(resolved_new_item_count - idx - 1);
    }

    // Now look in our old items.
    idx -= resolved_new_item_count;
    load_old_if_needed();
    size_t old_item_count = old_item_offsets.size();
    if (idx < old_item_count) {
        // idx == 0 corresponds to last item in old_item_offsets.
        size_t offset = old_item_offsets.at(old_item_count - idx - 1);
        return file_contents->decode_item(offset);
    }

    // Index past the valid range, so return an empty history item.
    return history_item_t{};
}

std::unordered_map<long, wcstring> history_impl_t::items_at_indexes(const std::vector<long> &idxs) {
    std::unordered_map<long, wcstring> result;
    for (long idx : idxs) {
        if (idx <= 0) {
            // Skip non-positive entries.
            continue;
        }
        // Insert an empty string to see if this is the first time the index is encountered. If so,
        // we have to go fetch the item.
        auto iter_inserted = result.emplace(idx, wcstring{});
        if (iter_inserted.second) {
            // New key.
            auto item = item_at_index(size_t(idx));
            iter_inserted.first->second = std::move(item.contents);
        }
    }
    return result;
}

void history_impl_t::populate_from_file_contents() {
    old_item_offsets.clear();
    if (file_contents) {
        size_t cursor = 0;
        while (auto offset = file_contents->offset_of_next_item(&cursor, boundary_timestamp)) {
            // Remember this item.
            old_item_offsets.push_back(*offset);
        }
    }

    FLOGF(history, "Loaded %lu old items", old_item_offsets.size());
}

void history_impl_t::load_old_if_needed() {
    if (loaded_old) return;
    loaded_old = true;

    time_profiler_t profiler("load_old");  //!OCLINT(side-effect)
    if (maybe_t<wcstring> filename = history_filename(name)) {
        autoclose_fd_t file{wopen_cloexec(*filename, O_RDONLY)};
        int fd = file.fd();
        if (fd >= 0) {
            // Take a read lock to guard against someone else appending. This is released when the
            // file is closed (below). We will read the file after releasing the lock, but that's
            // not a problem, because we never modify already written data. In short, the purpose of
            // this lock is to ensure we don't see the file size change mid-update.
            //
            // We may fail to lock (e.g. on lockless NFS - see issue #685. In that case, we proceed
            // as if it did not fail. The risk is that we may get an incomplete history item; this
            // is unlikely because we only treat an item as valid if it has a terminating newline.
            //
            // Simulate a failing lock in chaos_mode.
            if (!history_t::chaos_mode) history_file_lock(fd, LOCK_SH);
            file_contents = history_file_contents_t::create(fd);
            this->history_file_id = file_contents ? file_id_for_fd(fd) : kInvalidFileID;
            if (!history_t::chaos_mode) history_file_lock(fd, LOCK_UN);

            time_profiler_t profiler("populate_from_file_contents");  //!OCLINT(side-effect)
            this->populate_from_file_contents();
        }
    }
}

bool history_search_t::go_backwards() {
    // Backwards means increasing our index.
    const auto max_index = static_cast<size_t>(-1);

    if (current_index_ == max_index) return false;

    size_t index = current_index_;
    while (++index < max_index) {
        history_item_t item = history_->item_at_index(index);

        // We're done if it's empty or we cancelled.
        if (item.empty()) {
            return false;
        }

        // Look for an item that matches and (if deduping) that we haven't seen before.
        if (!item.matches_search(canon_term_, search_type_, !ignores_case())) {
            continue;
        }

        // Skip if deduplicating.
        if (dedup() && !deduper_.insert(item.str()).second) {
            continue;
        }

        // This is our new item.
        current_item_ = std::move(item);
        current_index_ = index;
        return true;
    }
    return false;
}

const history_item_t &history_search_t::current_item() const {
    assert(current_item_ && "No current item");
    return *current_item_;
}

const wcstring &history_search_t::current_string() const { return this->current_item().str(); }

void history_impl_t::clear_file_state() {
    // Erase everything we know about our file.
    file_contents.reset();
    loaded_old = false;
    old_item_offsets.clear();
}

void history_impl_t::compact_new_items() {
    // Keep only the most recent items with the given contents. This algorithm could be made more
    // efficient, but likely would consume more memory too.
    std::unordered_set<wcstring> seen;
    size_t idx = new_items.size();
    while (idx--) {
        const history_item_t &item = new_items[idx];
        if (!seen.insert(item.contents).second) {
            // This item was not inserted because it was already in the set, so delete the item at
            // this index.
            new_items.erase(new_items.begin() + idx);

            if (idx < first_unwritten_new_item_index) {
                // Decrement first_unwritten_new_item_index if we are deleting a previously written
                // item.
                first_unwritten_new_item_index--;
            }
        }
    }
}

// Given the fd of an existing history file, or -1 if none, write
// a new history file to temp_fd. Returns true on success, false
// on error
bool history_impl_t::rewrite_to_temporary_file(int existing_fd, int dst_fd) const {
    // We are reading FROM existing_fd and writing TO dst_fd
    // dst_fd must be valid; existing_fd does not need to be
    assert(dst_fd >= 0);

    // Make an LRU cache to save only the last N elements.
    history_lru_cache_t lru(HISTORY_SAVE_MAX);

    // Read in existing items (which may have changed out from underneath us, so don't trust our
    // old file contents).
    if (auto local_file = history_file_contents_t::create(existing_fd)) {
        size_t cursor = 0;
        while (auto offset = local_file->offset_of_next_item(&cursor, 0)) {
            // Try decoding an old item.
            history_item_t old_item = local_file->decode_item(*offset);

            if (old_item.empty() || deleted_items.count(old_item.str()) > 0) {
                continue;
            }
            // Add this old item.
            lru.add_item(std::move(old_item));
        }
    }

    // Insert any unwritten new items
    for (auto iter = new_items.cbegin() + this->first_unwritten_new_item_index;
         iter != new_items.cend(); ++iter) {
        lru.add_item(*iter);
    }

    // Stable-sort our items by timestamp
    // This is because we may have read "old" items with a later timestamp than our "new" items
    // This is the essential step that roughly orders items by history
    lru.stable_sort([](const history_item_t &item1, const history_item_t &item2) {
        return item1.timestamp() < item2.timestamp();
    });

    // Write them out.
    int err = 0;
    std::string buffer;
    buffer.reserve(HISTORY_OUTPUT_BUFFER_SIZE + 128);
    for (const auto key_item : lru) {
        append_history_item_to_buffer(key_item.second, &buffer);
        err = flush_to_fd(&buffer, dst_fd, HISTORY_OUTPUT_BUFFER_SIZE);
        if (err) break;
    }
    if (!err) {
        err = flush_to_fd(&buffer, dst_fd, 0);
    }
    if (err) {
        FLOGF(history_file, L"Error %d when writing to temporary history file", err);
    }

    return err == 0;
}

// Returns the fd of an opened temporary file, or an invalid fd on failure.
static autoclose_fd_t create_temporary_file(const wcstring &name_template, wcstring *out_path) {
    for (int attempt = 0; attempt < 10; attempt++) {
        std::string narrow_str = wcs2string(name_template);
        autoclose_fd_t out_fd{fish_mkstemp_cloexec(&narrow_str[0])};
        if (out_fd.valid()) {
            *out_path = str2wcstring(narrow_str);
            return out_fd;
        }
    }
    return autoclose_fd_t{};
}

bool history_impl_t::save_internal_via_rewrite() {
    FLOGF(history, "Saving %lu items via rewrite",
          new_items.size() - first_unwritten_new_item_index);
    bool ok = false;

    // We want to rewrite the file, while holding the lock for as briefly as possible
    // To do this, we speculatively write a file, and then lock and see if our original file changed
    // Repeat until we succeed or give up
    const maybe_t<wcstring> target_name = history_filename(name);
    const maybe_t<wcstring> tmp_name_template = history_filename(name, L".XXXXXX");
    if (!target_name.has_value() || !tmp_name_template.has_value()) {
        return false;
    }

    // Make our temporary file
    // Remember that we have to close this fd!
    wcstring tmp_name;
    autoclose_fd_t tmp_file = create_temporary_file(*tmp_name_template, &tmp_name);
    if (!tmp_file.valid()) {
        return false;
    }
    const int tmp_fd = tmp_file.fd();
    bool done = false;
    for (int i = 0; i < max_save_tries && !done; i++) {
        // Open any target file, but do not lock it right away
        autoclose_fd_t target_fd_before{
            wopen_cloexec(*target_name, O_RDONLY | O_CREAT, history_file_mode)};
        file_id_t orig_file_id = file_id_for_fd(target_fd_before.fd());  // possibly invalid
        bool wrote = this->rewrite_to_temporary_file(target_fd_before.fd(), tmp_fd);
        target_fd_before.close();
        if (!wrote) {
            // Failed to write, no good
            break;
        }

        // The crux! We rewrote the history file; see if the history file changed while we
        // were rewriting it. Make an effort to take the lock before checking, to avoid racing.
        // If the open fails, then proceed; this may be because there is no current history
        file_id_t new_file_id = kInvalidFileID;
        autoclose_fd_t target_fd_after{wopen_cloexec(*target_name, O_RDONLY)};
        if (target_fd_after.valid()) {
            // critical to take the lock before checking file IDs,
            // and hold it until after we are done replacing
            // Also critical to check the file at the path, NOT based on our fd
            // It's only OK to replace the file while holding the lock
            history_file_lock(target_fd_after.fd(), LOCK_EX);
            new_file_id = file_id_for_path(*target_name);
        }
        bool can_replace_file = (new_file_id == orig_file_id || new_file_id == kInvalidFileID);
        if (!can_replace_file) {
            // The file has changed, so we're going to re-read it
            // Truncate our tmp_fd so we can reuse it
            if (ftruncate(tmp_fd, 0) == -1 || lseek(tmp_fd, 0, SEEK_SET) == -1) {
                FLOGF(history_file, L"Error %d when truncating temporary history file", errno);
            }
        } else {
            // The file is unchanged, or the new file doesn't exist or we can't read it
            // We also attempted to take the lock, so we feel confident in replacing it

            // Ensure we maintain the ownership and permissions of the original (#2355). If the
            // stat fails, we assume (hope) our default permissions are correct. This
            // corresponds to e.g. someone running sudo -E as the very first command. If they
            // did, it would be tricky to set the permissions correctly. (bash doesn't get this
            // case right either).
            struct stat sbuf;
            if (target_fd_after.valid() && fstat(target_fd_after.fd(), &sbuf) >= 0) {
                if (fchown(tmp_fd, sbuf.st_uid, sbuf.st_gid) == -1) {
                    FLOGF(history_file, L"Error %d when changing ownership of history file", errno);
                }
                if (fchmod(tmp_fd, sbuf.st_mode) == -1) {
                    FLOGF(history_file, L"Error %d when changing mode of history file", errno);
                }
            }

            // Slide it into place
            if (wrename(tmp_name, *target_name) == -1) {
                FLOGF(history_file, L"Error %d when renaming history file", errno);
            }

            // We did it
            done = true;
        }
    }

    // Ensure we never leave the old file around
    wunlink(tmp_name);

    if (done) {
        // We've saved everything, so we have no more unsaved items.
        this->first_unwritten_new_item_index = new_items.size();

        // We deleted our deleted items.
        this->deleted_items.clear();

        // Our history has been written to the file, so clear our state so we can re-reference the
        // file.
        this->clear_file_state();
    }

    return ok;
}

// Function called to save our unwritten history file by appending to the existing history file
// Returns true on success, false on failure.
bool history_impl_t::save_internal_via_appending() {
    FLOGF(history, "Saving %lu items via appending",
          new_items.size() - first_unwritten_new_item_index);
    // No deleting allowed.
    assert(deleted_items.empty());

    bool ok = false;

    // If the file is different (someone vacuumed it) then we need to update our mmap.
    bool file_changed = false;

    // Get the path to the real history file.
    maybe_t<wcstring> maybe_history_path = history_filename(name);
    if (!maybe_history_path) {
        return true;
    }
    wcstring history_path = maybe_history_path.acquire();

    // We are going to open the file, lock it, append to it, and then close it
    // After locking it, we need to stat the file at the path; if there is a new file there, it
    // means the file was replaced and we have to try again.
    // Limit our max tries so we don't do this forever.
    autoclose_fd_t history_fd{};
    for (int i = 0; i < max_save_tries; i++) {
        autoclose_fd_t fd{wopen_cloexec(history_path, O_WRONLY | O_APPEND)};
        if (!fd.valid()) {
            // can't open, we're hosed
            break;
        }
        // Exclusive lock on the entire file. This is released when we close the file (below). This
        // may fail on (e.g.) lockless NFS. If so, proceed as if it did not fail; the risk is that
        // we may get interleaved history items, which is considered better than no history, or
        // forcing everything through the slow copy-move mode. We try to minimize this possibility
        // by writing with O_APPEND.
        //
        // Simulate a failing lock in chaos_mode
        if (!history_t::chaos_mode) history_file_lock(fd.fd(), LOCK_EX);
        const file_id_t file_id = file_id_for_fd(fd.fd());
        if (file_id_for_path(history_path) == file_id) {
            // File IDs match, so the file we opened is still at that path
            // We're going to use this fd
            if (file_id != this->history_file_id) {
                file_changed = true;
            }
            history_fd = std::move(fd);
            break;
        }
    }

    if (history_fd.valid()) {
        // We (hopefully successfully) took the exclusive lock. Append to the file.
        // Note that this is sketchy for a few reasons:
        //   - Another shell may have appended its own items with a later timestamp, so our file may
        // no longer be sorted by timestamp.
        //   - Another shell may have appended the same items, so our file may now contain
        // duplicates.
        //
        // We cannot modify any previous parts of our file, because other instances may be reading
        // those portions. We can only append.
        //
        // Originally we always rewrote the file on saving, which avoided both of these problems.
        // However, appending allows us to save history after every command, which is nice!
        //
        // Periodically we "clean up" the file by rewriting it, so that most of the time it doesn't
        // have duplicates, although we don't yet sort by timestamp (the timestamp isn't really used
        // for much anyways).

        // So far so good. Write all items at or after first_unwritten_new_item_index. Note that we
        // write even a pending item - pending items are ignored by history within the command
        // itself, but should still be written to the file.
        // TODO: consider filling the buffer ahead of time, so we can just lock, splat, and unlock?
        int err = 0;
        // Use a small buffer size for appending, we usually only have 1 item
        std::string buffer;
        while (first_unwritten_new_item_index < new_items.size()) {
            const history_item_t &item = new_items.at(first_unwritten_new_item_index);
            append_history_item_to_buffer(item, &buffer);
            err = flush_to_fd(&buffer, history_fd.fd(), HISTORY_OUTPUT_BUFFER_SIZE);
            if (err) break;
            // We wrote this item, hooray.
            first_unwritten_new_item_index++;
        }

        if (!err) {
            err = flush_to_fd(&buffer, history_fd.fd(), 0);
        }

        // Since we just modified the file, update our mmap_file_id to match its current state
        // Otherwise we'll think the file has been changed by someone else the next time we go to
        // write.
        // We don't update the mapping since we only appended to the file, and everything we
        // appended remains in our new_items
        this->history_file_id = file_id_for_fd(history_fd.fd());

        ok = (err == 0);
    }
    history_fd.close();

    // If someone has replaced the file, forget our file state.
    if (file_changed) {
        this->clear_file_state();
    }

    return ok;
}

/// Save the specified mode to file; optionally also vacuums.
void history_impl_t::save(bool vacuum) {
    // Nothing to do if there's no new items.
    if (first_unwritten_new_item_index >= new_items.size() && deleted_items.empty()) return;

    if (!history_filename(name).has_value()) {
        // We're in the "incognito" mode. Pretend we've saved the history.
        this->first_unwritten_new_item_index = new_items.size();
        this->deleted_items.clear();
        this->clear_file_state();
    }

    // Compact our new items so we don't have duplicates.
    this->compact_new_items();

    // Try saving. If we have items to delete, we have to rewrite the file. If we do not, we can
    // append to it.
    bool ok = false;
    if (!vacuum && deleted_items.empty()) {
        // Try doing a fast append.
        ok = save_internal_via_appending();
        if (!ok) {
            FLOGF(history, "Appending failed");
        }
    }
    if (!ok) {
        // We did not or could not append; rewrite the file ("vacuum" it).
        this->save_internal_via_rewrite();
    }
}

// Formats a single history record, including a trailing newline.
//
// Returns nothing. The only possible failure involves formatting the timestamp. If that happens we
// simply omit the timestamp from the output.
static void format_history_record(const history_item_t &item, const wchar_t *show_time_format,
                                  bool null_terminate, wcstring *result) {
    result->clear();
    if (show_time_format) {
        const time_t seconds = item.timestamp();
        struct tm timestamp;
        if (localtime_r(&seconds, &timestamp)) {
            const int max_tstamp_length = 100;
            wchar_t timestamp_string[max_tstamp_length + 1];
            if (std::wcsftime(timestamp_string, max_tstamp_length, show_time_format, &timestamp) !=
                0) {
                result->append(timestamp_string);
            }
        }
    }

    result->append(item.str());
    result->push_back(null_terminate ? L'\0' : L'\n');
}

void history_impl_t::disable_automatic_saving() {
    disable_automatic_save_counter++;
    assert(disable_automatic_save_counter != 0);  // overflow!
}

void history_impl_t::enable_automatic_saving() {
    assert(disable_automatic_save_counter > 0);  // underflow
    disable_automatic_save_counter--;
    save_unless_disabled();
}

void history_impl_t::clear() {
    new_items.clear();
    deleted_items.clear();
    first_unwritten_new_item_index = 0;
    old_item_offsets.clear();
    if (maybe_t<wcstring> filename = history_filename(name)) {
        wunlink(*filename);
    }
    this->clear_file_state();
}

bool history_impl_t::is_default() const { return name == DFLT_FISH_HISTORY_SESSION_ID; }

bool history_impl_t::is_empty() {
    // If we have new items, we're not empty.
    if (!new_items.empty()) return false;

    bool empty = false;
    if (loaded_old) {
        // If we've loaded old items, see if we have any offsets.
        empty = old_item_offsets.empty();
    } else {
        // If we have not loaded old items, don't actually load them (which may be expensive); just
        // stat the file and see if it exists and is nonempty.
        const maybe_t<wcstring> where = history_filename(name);
        if (!where.has_value()) {
            return true;
        }

        struct stat buf = {};
        if (wstat(*where, &buf) != 0) {
            // Access failed, assume missing.
            empty = true;
        } else {
            // We're empty if the file is empty.
            empty = (buf.st_size == 0);
        }
    }
    return empty;
}

/// Populates from older location (in config path, rather than data path) This is accomplished by
/// clearing ourselves, and copying the contents of the old history file to the new history file.
/// The new contents will automatically be re-mapped later.
void history_impl_t::populate_from_config_path() {
    maybe_t<wcstring> new_file = history_filename(name);
    if (!new_file.has_value()) {
        return;
    }

    wcstring old_file;
    if (path_get_config(old_file)) {
        old_file.append(L"/");
        old_file.append(name);
        old_file.append(L"_history");
        autoclose_fd_t src_fd{wopen_cloexec(old_file, O_RDONLY, 0)};
        if (src_fd.valid()) {
            // Clear must come after we've retrieved the new_file name, and before we open
            // destination file descriptor, since it destroys the name and the file.
            this->clear();

            autoclose_fd_t dst_fd{wopen_cloexec(*new_file, O_WRONLY | O_CREAT, history_file_mode)};
            char buf[BUFSIZ];
            ssize_t size;
            while ((size = read(src_fd.fd(), buf, BUFSIZ)) > 0) {
                ssize_t written = write(dst_fd.fd(), buf, static_cast<size_t>(size));
                if (written < 0) {
                    // This message does not have high enough priority to be shown by default.
                    FLOGF(history_file, L"Error when writing history file");
                    break;
                }
            }
        }
    }
}

/// Decide whether we ought to import a bash history line into fish. This is a very crude heuristic.
static bool should_import_bash_history_line(const wcstring &line) {
    if (line.empty()) return false;

    if (ast::ast_t::parse(line).errored()) return false;

    // In doing this test do not allow incomplete strings. Hence the "false" argument.
    parse_error_list_t errors;
    parse_util_detect_errors(line, &errors);
    if (!errors.empty()) return false;

    // The following are Very naive tests!

    // Skip comments.
    if (line[0] == '#') return false;

    // Skip lines with backticks.
    if (line.find('`') != std::string::npos) return false;

    // Skip lines with [[...]] and ((...)) since we don't handle those constructs.
    if (line.find(L"[[") != std::string::npos) return false;
    if (line.find(L"]]") != std::string::npos) return false;
    if (line.find(L"((") != std::string::npos) return false;
    if (line.find(L"))") != std::string::npos) return false;
    // Skip lines with literal tabs since we don't handle them well and we don't know what they mean.
    // It could just be whitespace or it's actually passed somewhere (like e.g. `sed`).
    if (line.find(L'\t') != std::string::npos) return false;

    // Skip lines that end with a backslash. We do not handle multiline commands from bash history.
    if (line.back() == L'\\') return false;

    return true;
}

/// Import a bash command history file. Bash's history format is very simple: just lines with #s for
/// comments. Ignore a few commands that are bash-specific. It makes no attempt to handle multiline
/// commands. We can't actually parse bash syntax and the bash history file does not unambiguously
/// encode multiline commands.
void history_impl_t::populate_from_bash(FILE *stream) {
    // Process the entire history file until EOF is observed.
    bool eof = false;
    while (!eof) {
        auto line = std::string();

        // Loop until we've read a line or EOF is observed.
        while (true) {
            char buff[128];
            if (!fgets(buff, sizeof buff, stream)) {
                eof = true;
                break;
            }

            // Deal with the newline if present.
            char *a_newline = std::strchr(buff, '\n');
            if (a_newline) *a_newline = '\0';
            line.append(buff);
            if (a_newline) break;
        }

        wcstring wide_line = str2wcstring(line);
        // Add this line if it doesn't contain anything we know we can't handle.
        if (should_import_bash_history_line(wide_line)) {
            this->add(wide_line, 0, false /* pending */, false /* do_save */);
        }
    }
    this->save_unless_disabled();
}

void history_impl_t::incorporate_external_changes() {
    // To incorporate new items, we simply update our timestamp to now, so that items from previous
    // instances get added. We then clear the file state so that we remap the file. Note that this
    // is somehwhat expensive because we will be going back over old items. An optimization would be
    // to preserve old_item_offsets so that they don't have to be recomputed. (However, then items
    // *deleted* in other instances would not show up here).
    time_t new_timestamp = time(nullptr);

    // If for some reason the clock went backwards, we don't want to start dropping items; therefore
    // we only do work if time has progressed. This also makes multiple calls cheap.
    if (new_timestamp > this->boundary_timestamp) {
        this->boundary_timestamp = new_timestamp;
        this->clear_file_state();

        // We also need to erase new_items, since we go through those first, and that means we
        // will not properly interleave them with items from other instances.
        // We'll pick them up from the file (#2312)
        this->save(false);
        this->new_items.clear();
        this->first_unwritten_new_item_index = 0;
    }
}

/// Return the prefix for the files to be used for command and read history.
wcstring history_session_id(const environment_t &vars) {
    wcstring result = DFLT_FISH_HISTORY_SESSION_ID;

    const auto var = vars.get(L"fish_history");
    if (var) {
        wcstring session_id = var->as_string();
        if (session_id.empty()) {
            result.clear();
        } else if (session_id == L"default") {
            // using the default value
        } else if (valid_var_name(session_id)) {
            result = session_id;
        } else {
            FLOGF(error,
                  _(L"History session ID '%ls' is not a valid variable name. "
                    L"Falling back to `%ls`."),
                  session_id.c_str(), result.c_str());
        }
    }

    return result;
}

path_list_t valid_paths(const path_list_t &paths, const wcstring &working_directory) {
    ASSERT_IS_BACKGROUND_THREAD();
    wcstring_list_t result;
    for (const wcstring &path : paths) {
        if (path_is_valid(path, working_directory)) {
            result.push_back(path);
        }
    }
    return result;
}

bool all_paths_are_valid(const path_list_t &paths, const wcstring &working_directory) {
    ASSERT_IS_BACKGROUND_THREAD();
    for (const wcstring &path : paths) {
        if (!path_is_valid(path, working_directory)) {
            return false;
        }
    }
    return true;
}

static bool string_could_be_path(const wcstring &potential_path) {
    // Assume that things with leading dashes aren't paths.
    return !(potential_path.empty() || potential_path.at(0) == L'-');
}

/// impl_wrapper_t is used to avoid forming owning_lock<incomplete_type> in
/// the .h file; see #7023.
struct history_t::impl_wrapper_t {
    owning_lock<history_impl_t> impl;
    explicit impl_wrapper_t(wcstring &&name) : impl(history_impl_t(std::move(name))) {}
};

/// Very simple, just mark that we have no more pending items.
void history_impl_t::resolve_pending() { this->has_pending_item = false; }

bool history_t::chaos_mode = false;
bool history_t::never_mmap = false;

history_t::history_t(wcstring name) : wrap_(make_unique<impl_wrapper_t>(std::move(name))) {}

history_t::~history_t() = default;

acquired_lock<history_impl_t> history_t::impl() { return wrap_->impl.acquire(); }

acquired_lock<const history_impl_t> history_t::impl() const { return wrap_->impl.acquire(); }

bool history_t::is_default() const { return impl()->is_default(); }

bool history_t::is_empty() { return impl()->is_empty(); }

void history_t::add(const history_item_t &item, bool pending) { impl()->add(item, pending); }

void history_t::add(const wcstring &str, history_identifier_t ident, bool pending) {
    impl()->add(str, ident, pending);
}

void history_t::remove(const wcstring &str) { impl()->remove(str); }

void history_t::add_pending_with_file_detection(const wcstring &str,
                                                const wcstring &working_dir_slash) {
    // We use empty items as sentinels to indicate the end of history.
    // Do not allow them to be added (#6032).
    if (str.empty()) {
        return;
    }

    // Find all arguments that look like they could be file paths.
    bool needs_sync_write = false;
    using namespace ast;
    auto ast = ast_t::parse(str);

    path_list_t potential_paths;
    for (const node_t &node : ast) {
        if (const argument_t *arg = node.try_as<argument_t>()) {
            wcstring potential_path = arg->source(str);
            bool unescaped = unescape_string_in_place(&potential_path, UNESCAPE_DEFAULT);
            if (unescaped && string_could_be_path(potential_path)) {
                potential_paths.push_back(potential_path);
            }
        } else if (const decorated_statement_t *stmt = node.try_as<decorated_statement_t>()) {
            // Hack hack hack - if the command is likely to trigger an exit, then don't do
            // background file detection, because we won't be able to write it to our history file
            // before we exit.
            // Also skip it for 'echo'. This is because echo doesn't take file paths, but also
            // because the history file test wants to find the commands in the history file
            // immediately after running them, so it can't tolerate the asynchronous file detection.
            if (stmt->decoration() == statement_decoration_t::exec) {
                needs_sync_write = true;
            }

            wcstring command = stmt->command.source(str);
            unescape_string_in_place(&command, UNESCAPE_DEFAULT);
            if (command == L"exit" || command == L"reboot" || command == L"restart" ||
                command == L"echo") {
                needs_sync_write = true;
            }
        }
    }

    // If we got a path, we'll perform file detection for autosuggestion hinting.
    bool wants_file_detection = !potential_paths.empty() && !needs_sync_write;
    auto imp = this->impl();

    history_identifier_t identifier = 0;
    if (wants_file_detection) {
        // Grab the next identifier.
        static relaxed_atomic_t<history_identifier_t> s_last_identifier{0};
        identifier = ++s_last_identifier;
        imp->disable_automatic_saving();

        // Add the item. Then check for which paths are valid on a background thread,
        // and unblock the item.
        // Don't hold the lock while we perform this file detection.
        imp->add(str, identifier, true /* pending */);
        iothread_perform([=]() {
            auto validated_paths = valid_paths(potential_paths, working_dir_slash);
            auto imp = this->impl();
            imp->set_valid_file_paths(validated_paths, identifier);
            imp->enable_automatic_saving();
        });
    } else {
        // Add the item.
        // If we think we're about to exit, save immediately, regardless of any disabling. This may
        // cause us to lose file hinting for some commands, but it beats losing history items.
        imp->add(str, identifier, true /* pending */);
        if (needs_sync_write) {
            imp->save();
        }
    }
}
void history_t::resolve_pending() { impl()->resolve_pending(); }

void history_t::save() { impl()->save(); }

/// Perform a search of \p hist for \p search_string. Invoke a function \p func for each match. If
/// \p func returns true, continue the search; else stop it.
static void do_1_history_search(history_t &hist, history_search_type_t search_type,
                                const wcstring &search_string, bool case_sensitive,
                                const std::function<bool(const history_item_t &item)> &func,
                                const cancel_checker_t &cancel_check) {
    history_search_t searcher = history_search_t(hist, search_string, search_type,
                                                 case_sensitive ? 0 : history_search_ignore_case);
    while (!cancel_check() && searcher.go_backwards()) {
        if (!func(searcher.current_item())) {
            break;
        }
    }
}

// Searches history.
bool history_t::search(history_search_type_t search_type, const wcstring_list_t &search_args,
                       const wchar_t *show_time_format, size_t max_items, bool case_sensitive,
                       bool null_terminate, bool reverse, const cancel_checker_t &cancel_check,
                       io_streams_t &streams) {
    wcstring_list_t collected;
    wcstring formatted_record;
    size_t remaining = max_items;

    // The function we use to act on each item.
    std::function<bool(const history_item_t &item)> func = [&](const history_item_t &item) -> bool {
        if (remaining == 0) return false;
        remaining -= 1;
        format_history_record(item, show_time_format, null_terminate, &formatted_record);
        if (reverse) {
            // We need to collect this for later.
            collected.push_back(std::move(formatted_record));
        } else {
            // We can output this immediately.
            streams.out.append(formatted_record);
        }
        return true;
    };

    if (search_args.empty()) {
        // The user had no search terms; just append everything.
        do_1_history_search(*this, history_search_type_t::match_everything, {}, false, func,
                            cancel_check);
    } else {
        for (const wcstring &search_string : search_args) {
            if (search_string.empty()) {
                streams.err.append_format(L"Searching for the empty string isn't allowed");
                return false;
            }
            do_1_history_search(*this, search_type, search_string, case_sensitive, func,
                                cancel_check);
        }
    }

    // Output any items we collected (which only happens in reverse).
    for (auto iter = collected.rbegin(); iter != collected.rend(); ++iter) {
        streams.out.append(*iter);
    }
    return true;
}

void history_t::clear() { impl()->clear(); }

void history_t::populate_from_config_path() { impl()->populate_from_config_path(); }

void history_t::populate_from_bash(FILE *f) { impl()->populate_from_bash(f); }

void history_t::incorporate_external_changes() { impl()->incorporate_external_changes(); }

void history_t::get_history(wcstring_list_t &result) { impl()->get_history(result); }

std::unordered_map<long, wcstring> history_t::items_at_indexes(const std::vector<long> &idxs) {
    return impl()->items_at_indexes(idxs);
}

history_item_t history_t::item_at_index(size_t idx) { return impl()->item_at_index(idx); }

size_t history_t::size() { return impl()->size(); }

/// The set of all histories.
static owning_lock<std::map<wcstring, std::unique_ptr<history_t>>> s_histories;

void history_save_all() {
    auto histories = s_histories.acquire();
    for (auto &p : *histories) {
        p.second->save();
    }
}

history_t &history_t::history_with_name(const wcstring &name) {
    // Return a history for the given name, creating it if necessary
    // Note that histories are currently never deleted, so we can return a reference to them without
    // using something like shared_ptr
    auto hs = s_histories.acquire();
    std::unique_ptr<history_t> &hist = (*hs)[name];
    if (!hist) {
        hist = make_unique<history_t>(name);
    }
    return *hist;
}

static relaxed_atomic_bool_t private_mode{false};

void start_private_mode(env_stack_t &vars) {
    private_mode = true;
    vars.set_one(L"fish_history", ENV_GLOBAL, L"");
    vars.set_one(L"fish_private_mode", ENV_GLOBAL, L"1");
}

bool in_private_mode() { return private_mode; }
