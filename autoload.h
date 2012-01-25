/** \file autoload.h

    The classes responsible for autoloading functions and completions.
*/

#ifndef FISH_PARSE_UTIL_H
#define FISH_PARSE_UTIL_H

#include <wchar.h>
#include <map>
#include <set>
#include <list>
#include "common.h"

extern const time_t kFishDefaultStalenessInterval;

/** A class responsible for recording an attempt to access a file. */
class file_access_attempt_t {
public:
    time_t mod_time; /** The modification time of the file */
    time_t last_checked; /** When we last checked the file */
    bool accessible; /** Whether we believe we could access this file */
    bool stale; /** Whether the access attempt is stale */
    int error; /** If we could not access the file, the error code */
};

class file_access_node_t;

/** A predicate to compare dereferenced pointers */
struct dereference_less_t {
    template <typename ptr_t>
    bool operator()(ptr_t p1, ptr_t p2) const { return *p1 < *p2; }
};


/** A class responsible for tracking accesses to files, including auto-expiration. */
class access_tracker_t {
    private:
        
        file_access_node_t * while_locked_find_node(const wcstring &str) const;
        void vacuum_one_node(void);
        void promote_node(file_access_node_t *);
        
        file_access_attempt_t attempt_access(const wcstring& path) const;
        
        unsigned int node_count;
        typedef std::set<file_access_node_t *, dereference_less_t> access_set_t;
        access_set_t access_set;
        file_access_node_t *mouth;
        
        /* Lock for thread safety */
        pthread_mutex_t lock;
        
        /** How long until a file access attempt is considered stale. */
        const time_t stale_interval;
        
        /** Mode for waccess calls */
        const int mode;
    
    public:
        
        /** Constructor, that takes a staleness interval */
        access_tracker_t(time_t stale, int the_mode);
        
        /** Destructor */
        ~access_tracker_t();
    
        /** Attempt to access the given file, if the last cached access is stale. Caches and returns the access attempt. */
        file_access_attempt_t access_file(const wcstring &path);
        
        /** Returns whether there is a cached access (even if stale), without touching the disk; if the result is true, return by reference that access attempt. */
        bool access_file_only_cached(const wcstring &path, file_access_attempt_t &attempt);
};

#endif
