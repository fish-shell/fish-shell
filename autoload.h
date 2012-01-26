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

/** A struct responsible for recording an attempt to access a file. */
struct file_access_attempt_t {
    time_t mod_time; /** The modification time of the file */
    time_t last_checked; /** When we last checked the file */
    bool accessible; /** Whether we believe we could access this file */
    bool stale; /** Whether the access attempt is stale */
    int error; /** If we could not access the file, the error code */
};

/** A predicate to compare dereferenced pointers */
struct dereference_less_t {
    template <typename ptr_t>
    bool operator()(ptr_t p1, ptr_t p2) const { return *p1 < *p2; }
};

file_access_attempt_t access_file(const wcstring &path, int mode);

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
