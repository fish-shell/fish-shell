/** \file history.h
  Prototypes for history functions, part of the user interface.
*/

#ifndef FISH_HISTORY_H
#define FISH_HISTORY_H

#include <wchar.h>
#include "common.h"
#include "pthread.h"
#include <vector>
#include <tr1/memory>
using std::tr1::shared_ptr;

class history_item_t {
    friend class history_t;

    private:
    history_item_t(const wcstring &);
    history_item_t(const wcstring &, time_t);

	/** The actual contents of the entry */
	wcstring contents;
    	
	/** Original creation time for the entry */
	time_t creation_timestamp;
    
    public:
    const wcstring &str() const { return contents; }
    bool empty() const { return contents.empty(); }
    bool matches_search(const wcstring &val) const { return contents.find(val) != wcstring::npos; }
    time_t timestamp() const { return creation_timestamp; }
    
    bool write_to_file(FILE *f) const;
};

class history_t {
private:
    /** No copying */
    history_t(const history_t&);
    history_t &operator=(const history_t&);

    /** Private creator */
    history_t(const wcstring &pname);
    
    /** Destructor */
    ~history_t();

    /** Lock for thread safety */
    pthread_mutex_t lock;

	/** The name of this list. Used for picking a suitable filename and for switching modes. */
	const wcstring name;
	
	/** New items. */
	std::vector<history_item_t> new_items;
    
	/** The mmaped region for the history file */
	const char *mmap_start;

	/** The size of the mmaped region */
	size_t mmap_length;

	/**
	   Timestamp of last save
	*/
	time_t save_timestamp;
    
    static history_item_t decode_item(const char *ptr, size_t len);
    
    void populate_from_mmap(void);
    
    /** List of old items, as offsets into out mmap data */
    std::vector<size_t> old_item_offsets;
    
    /** Whether we've loaded old items */
    bool loaded_old;
    
    /** Loads old if necessary */
    void load_old_if_needed(void);
    
    /** Saves history */
    void save_internal();
    
public:
    /** Returns history with the given name, creating it if necessary */
    static history_t & history_with_name(const wcstring &name);
    
    /** Add a new history item to the end */
    void add(const wcstring &str);
    
    /** Saves history */
    void save();
    
    /** Return the specified history at the specified index. 0 is the index of the current commandline. */
    history_item_t item_at_index(size_t idx);
};

class history_search_t {

    /** The history in which we are searching */
    history_t * history;

    /** The search term */
    wcstring term;
    
    /** Current index into the history */
    size_t idx;
        
public:

    /** Finds the next search term (forwards in time). Returns true if one was found. */
    bool go_forwards(void);
    
    /** Finds the previous search result (backwards in time). Returns true if one was found. */
    bool go_backwards(void);
    
    /** Goes to the end (forwards) */
    void go_to_end(void);
    
    /** Goes to the beginning (backwards) */
    void go_to_beginning(void);
    
    /** Returns the current search result. asserts if there is no current item. */
    history_item_t current_item(void) const;

    /** Constructor */
    history_search_t(history_t &hist, const wcstring &str) :
        history(&hist),
        term(str),
        idx()
    {}
    
    /* Default constructor */
    history_search_t() :
        history(),
        term(),
        idx()
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

#endif
