/** \file history.h
  Prototypes for history functions, part of the user interface.
*/

#ifndef FISH_HISTORY_H
#define FISH_HISTORY_H

#include <wchar.h>
#include "common.h"
#include "pthread.h"
#include "wutil.h"
#include <vector>
#include <utility>
#include <list>
#include <set>

/* fish supports multiple shells writing to history at once. Here is its strategy:

1. All history files are append-only. Data, once written, is never modified.
2. A history file may be re-written ("vacuumed"). This involves reading in the file and writing a new one, while performing maintenance tasks: discarding items in an LRU fashion until we reach the desired maximum count, removing duplicates, and sorting them by timestamp (eventually, not implemented yet). The new file is atomically moved into place via rename().
3. History files are mapped in via mmap(). Before the file is mapped, the file takes a fcntl read lock. The purpose of this lock is to avoid seeing a transient state where partial data has been written to the file.
4. History is appended to under a fcntl write lock.
5. The chaos_mode boolean can be set to true to do things like lower buffer sizes which can trigger race conditions. This is useful for testing.
*/

typedef std::vector<wcstring> path_list_t;

enum history_search_type_t
{
    /** The history searches for strings containing the given string */
    HISTORY_SEARCH_TYPE_CONTAINS,

    /** The history searches for strings starting with the given string */
    HISTORY_SEARCH_TYPE_PREFIX
};

class history_item_t
{
    friend class history_t;
    friend class history_lru_node_t;
    friend class history_tests_t;

private:
    explicit history_item_t(const wcstring &);
    explicit history_item_t(const wcstring &, time_t, const path_list_t &paths = path_list_t());

    /** Attempts to merge two compatible history items together */
    bool merge(const history_item_t &item);

    /** The actual contents of the entry */
    wcstring contents;

    /** Original creation time for the entry */
    time_t creation_timestamp;

    /** Paths that we require to be valid for this item to be autosuggested */
    path_list_t required_paths;

public:
    const wcstring &str() const
    {
        return contents;
    }

    bool empty() const
    {
        return contents.empty();
    }

    /* Whether our contents matches a search term. */
    bool matches_search(const wcstring &term, enum history_search_type_t type) const;

    time_t timestamp() const
    {
        return creation_timestamp;
    }

    const path_list_t &get_required_paths() const
    {
        return required_paths;
    }

    bool operator==(const history_item_t &other) const
    {
        return contents == other.contents &&
               creation_timestamp == other.creation_timestamp &&
               required_paths == other.required_paths;
    }
};

/* The type of file that we mmap'd */
enum history_file_type_t
{
    history_type_unknown,
    history_type_fish_2_0,
    history_type_fish_1_x
};

class history_t
{
    friend class history_tests_t;
private:
    /** No copying */
    history_t(const history_t&);
    history_t &operator=(const history_t&);

    /** Private creator */
    history_t(const wcstring &pname);

    /** Privately add an item */
    void add(const history_item_t &item);

    /** Destructor */
    ~history_t();

    /** Lock for thread safety */
    pthread_mutex_t lock;

    /** Internal function */
    void clear_file_state();

    /** The name of this list. Used for picking a suitable filename and for switching modes. */
    const wcstring name;

    /** New items. Note that these are NOT discarded on save. We need to keep these around so we can distinguish between items in our history and items in the history of other shells that were started after we were started. */
    std::vector<history_item_t> new_items;

    /** The index of the first new item that we have not yet written. */
    size_t first_unwritten_new_item_index;

    /** Deleted item contents. */
    std::set<wcstring> deleted_items;

    /** The mmaped region for the history file */
    const char *mmap_start;

    /** The size of the mmap'd region */
    size_t mmap_length;

    /** The type of file we mmap'd */
    history_file_type_t mmap_type;

    /** The file ID of the file we mmap'd */
    file_id_t mmap_file_id;

    /** Timestamp of when this history was created */
    const time_t birth_timestamp;

    /** How many items we add until the next vacuum. Initially a random value. */
    int countdown_to_vacuum;

    /** Figure out the offsets of our mmap data */
    void populate_from_mmap(void);

    /** List of old items, as offsets into out mmap data */
    std::vector<size_t> old_item_offsets;

    /** Whether we've loaded old items */
    bool loaded_old;

    /** Loads old if necessary */
    bool load_old_if_needed(void);

    /** Memory maps the history file if necessary */
    bool mmap_if_needed(void);

    /** Deletes duplicates in new_items. */
    void compact_new_items();

    /** Saves history by rewriting the file */
    bool save_internal_via_rewrite();

    /** Saves history by appending to the file */
    bool save_internal_via_appending();

    /** Saves history */
    void save_internal(bool vacuum);

    /* Do a private, read-only map of the entirety of a history file with the given name. Returns true if successful. Returns the mapped memory region by reference. */
    bool map_file(const wcstring &name, const char **out_map_start, size_t *out_map_len, file_id_t *file_id);

    /** Whether we're in maximum chaos mode, useful for testing */
    bool chaos_mode;

    /* Versioned decoding */
    static history_item_t decode_item_fish_2_0(const char *base, size_t len);
    static history_item_t decode_item_fish_1_x(const char *base, size_t len);
    static history_item_t decode_item(const char *base, size_t len, history_file_type_t type);

public:
    /** Returns history with the given name, creating it if necessary */
    static history_t & history_with_name(const wcstring &name);

    /** Determines whether the history is empty. Unfortunately this cannot be const, since it may require populating the history. */
    bool is_empty(void);

    /** Add a new history item to the end */
    void add(const wcstring &str, const path_list_t &valid_paths = path_list_t());

    /** Remove a history item */
    void remove(const wcstring &str);

    /** Add a new history item to the end */
    void add_with_file_detection(const wcstring &str);

    /** Saves history */
    void save();

    /** Performs a full (non-incremental) save */
    void save_and_vacuum();

    /** Irreversibly clears history */
    void clear();

    /** Populates from a bash history file */
    void populate_from_bash(FILE *f);

    /* Gets all the history into a string with ARRAY_SEP_STR. This is intended for the $history environment variable. This may be long! */
    void get_string_representation(wcstring &str, const wcstring &separator);

    /** Return the specified history at the specified index. 0 is the index of the current commandline. (So the most recent item is at index 1.) */
    history_item_t item_at_index(size_t idx);
};

class history_search_t
{

    /** The history in which we are searching */
    history_t * history;

    /** Our type */
    enum history_search_type_t search_type;

    /** Our list of previous matches as index, value. The end is the current match. */
    typedef std::pair<size_t, history_item_t> prev_match_t;
    std::vector<prev_match_t> prev_matches;

    /** Returns yes if a given term is in prev_matches. */
    bool match_already_made(const wcstring &match) const;

    /** The search term */
    wcstring term;

    /** Additional strings to skip (sorted) */
    wcstring_list_t external_skips;

    bool should_skip_match(const wcstring &str) const;

public:

    /** Gets the search term */
    const wcstring &get_term() const
    {
        return term;
    }

    /** Sets additional string matches to skip */
    void skip_matches(const wcstring_list_t &skips);

    /** Finds the next search term (forwards in time). Returns true if one was found. */
    bool go_forwards(void);

    /** Finds the previous search result (backwards in time). Returns true if one was found. */
    bool go_backwards(void);

    /** Goes to the end (forwards) */
    void go_to_end(void);

    /** Returns if we are at the end. We start out at the end. */
    bool is_at_end(void) const;

    /** Goes to the beginning (backwards) */
    void go_to_beginning(void);

    /** Returns the current search result item. asserts if there is no current item. */
    history_item_t current_item(void) const;

    /** Returns the current search result item contents. asserts if there is no current item. */
    wcstring current_string(void) const;


    /** Constructor */
    history_search_t(history_t &hist, const wcstring &str, enum history_search_type_t type = HISTORY_SEARCH_TYPE_CONTAINS) :
        history(&hist),
        search_type(type),
        term(str)
    {}

    /* Default constructor */
    history_search_t() :
        history(),
        search_type(HISTORY_SEARCH_TYPE_CONTAINS),
        term()
    {}

};



/**
   Init history library. The history file won't actually be loaded
   until the first time a history search is performed.
*/
void history_init();

/**
   Saves the new history to disc.
*/
void history_destroy();

/**
   Perform sanity checks
*/
void history_sanity_check();

/* A helper class for threaded detection of paths */
struct file_detection_context_t
{

    /* Constructor */
    file_detection_context_t(history_t *hist, const wcstring &cmd);

    /* Determine which of potential_paths are valid, and put them in valid_paths */
    int perform_file_detection();

    /* The history associated with this context */
    history_t *history;

    /* The command */
    wcstring command;

    /* When the command was issued */
    time_t when;

    /* The working directory at the time the command was issued */
    wcstring working_directory;

    /* Paths to test */
    path_list_t potential_paths;

    /* Paths that were found to be valid */
    path_list_t valid_paths;

    /* Performs file detection. Returns 1 if every path in potential_paths is valid, 0 otherwise. If test_all is true, tests every path; otherwise stops as soon as it reaches an invalid path. */
    int perform_file_detection(bool test_all);

    /* Determine whether the given paths are all valid */
    bool paths_are_valid(const path_list_t &paths);
};

#endif
