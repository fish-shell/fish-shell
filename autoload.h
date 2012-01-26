/** \file autoload.h

    The classes responsible for autoloading functions and completions.
*/

#ifndef FISH_AUTOLOAD_H
#define FISH_AUTOLOAD_H

#include <wchar.h>
#include <map>
#include <set>
#include <list>
#include "common.h"

extern const time_t kFishDefaultStalenessInterval;

/** A struct responsible for recording an attempt to access a file. */
struct file_access_attempt_t {
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

file_access_attempt_t access_file(const wcstring &path, int mode);

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

class lru_node_t {
    friend class lru_cache_impl_t;
    /** Our linked list pointer */
    lru_node_t *prev, *next;
    
public:
    /** The key used to look up in the cache */
    const wcstring key;
    
    /** Constructor */
    lru_node_t(const wcstring &keyVar) : prev(NULL), next(NULL), key(keyVar) { }
    bool operator<(const lru_node_t &other) const { return key < other.key; }
};

class lru_cache_impl_t {
private:
    void promote_node(lru_node_t *);
    void evict_node(lru_node_t *node);
    void evict_last_node(void);
    
    /** Count of nodes */
    unsigned int node_count;
    
    /** The set of nodes */
    typedef std::set<lru_node_t *, dereference_less_t> node_set_t;
    node_set_t node_set;
    
    /** Head of the linked list */
    lru_node_t mouth;
    
protected:
    /** Overridable callback for when a node is evicted */
    virtual void node_was_evicted(lru_node_t *node);
    
public:
    /** Constructor */
    lru_cache_impl_t();
    
    /** Returns the node for a given key, or NULL */
    lru_node_t *get_node(const wcstring &key);
    
    /** Evicts the node for a given key, returning true if a node was evicted. */
    bool evict_node(const wcstring &key);
    
    /** Adds a node under the given key. Returns true if the node was added, false if the node was not because a node with that key is already in the set. */
    bool add_node(lru_node_t *node);
    
    /** Evicts all nodes */
    void evict_all_nodes(void);
};

/** Template cover to avoid casting */
template<class node_type_t>
class lru_cache_t : public lru_cache_impl_t {
public:
    node_type_t *get_node(const wcstring &key) { return static_cast<node_type_t>(lru_cache_impl_t::get_node(key)); }
    bool add_node(node_type_t *node) { return lru_cache_impl_t::add_node(node); }
protected:
    virtual void node_was_evicted(lru_node_t *node) { this->node_was_evicted(static_cast<node_type_t *>(node)); }
    virtual void node_was_evicted(node_type_t *node) { }
};


#endif
