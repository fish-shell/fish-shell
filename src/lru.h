// Least-recently-used cache implementation.
#ifndef FISH_LRU_H
#define FISH_LRU_H

#include <assert.h>
#include <wchar.h>
#include <list>
#include <map>
#include <set>

#include "common.h"

/// A predicate to compare dereferenced pointers.
struct dereference_less_t {
    template <typename ptr_t>
    bool operator()(ptr_t p1, ptr_t p2) const {
        return *p1 < *p2;
    }
};

class lru_node_t {
    template <class T>
    friend class lru_cache_t;

    /// Our linked list pointer.
    lru_node_t *prev, *next;

   public:
    /// The key used to look up in the cache.
    const wcstring key;

    /// Constructor.
    explicit lru_node_t(const wcstring &pkey) : prev(NULL), next(NULL), key(pkey) {}

    /// Virtual destructor that does nothing for classes that inherit lru_node_t.
    virtual ~lru_node_t() {}

    /// operator< for std::set
    bool operator<(const lru_node_t &other) const { return key < other.key; }
};

template <class node_type_t>
class lru_cache_t {
   private:
    /// Max node count. This may be (transiently) exceeded by add_node_without_eviction, which is
    /// used from background threads.
    const size_t max_node_count;

    /// Count of nodes.
    size_t node_count;

    /// The set of nodes.
    typedef std::set<lru_node_t *, dereference_less_t> node_set_t;
    node_set_t node_set;

    void promote_node(node_type_t *node) {
        // We should never promote the mouth.
        assert(node != &mouth);

        // First unhook us.
        node->prev->next = node->next;
        node->next->prev = node->prev;

        // Put us after the mouth.
        node->next = mouth.next;
        node->next->prev = node;
        node->prev = &mouth;
        mouth.next = node;
    }

    void evict_node(node_type_t *condemned_node) {
        // We should never evict the mouth.
        assert(condemned_node != NULL && condemned_node != &mouth);

        // Remove it from the linked list.
        condemned_node->prev->next = condemned_node->next;
        condemned_node->next->prev = condemned_node->prev;

        // Remove us from the set.
        node_set.erase(condemned_node);
        node_count--;

        // Tell ourselves.
        this->node_was_evicted(condemned_node);
    }

    void evict_last_node(void) { evict_node((node_type_t *)mouth.prev); }

    static lru_node_t *get_previous(lru_node_t *node) { return node->prev; }

   protected:
    /// Head of the linked list.
    lru_node_t mouth;

    /// Overridable callback for when a node is evicted.
    virtual void node_was_evicted(node_type_t *node) {}

   public:
    /// Constructor
    explicit lru_cache_t(size_t max_size = 1024)
        : max_node_count(max_size), node_count(0), mouth(wcstring()) {
        // Hook up the mouth to itself: a one node circularly linked list!
        mouth.prev = mouth.next = &mouth;
    }

    /// Note that we do not evict nodes in our destructor (even though they typically need to be
    /// deleted by their creator).
    virtual ~lru_cache_t() {}

    /// Returns the node for a given key, or NULL.
    node_type_t *get_node(const wcstring &key) {
        node_type_t *result = NULL;

        // Construct a fake node as our key.
        lru_node_t node_key(key);

        // Look for it in the set.
        node_set_t::iterator iter = node_set.find(&node_key);

        // If we found a node, promote and return it.
        if (iter != node_set.end()) {
            result = static_cast<node_type_t *>(*iter);
            promote_node(result);
        }
        return result;
    }

    /// Evicts the node for a given key, returning true if a node was evicted.
    bool evict_node(const wcstring &key) {
        // Construct a fake node as our key.
        lru_node_t node_key(key);

        // Look for it in the set.
        node_set_t::iterator iter = node_set.find(&node_key);
        if (iter == node_set.end()) return false;

        // Evict the given node.
        evict_node(static_cast<node_type_t *>(*iter));
        return true;
    }

    /// Adds a node under the given key. Returns true if the node was added, false if the node was
    /// not because a node with that key is already in the set.
    bool add_node(node_type_t *node) {
        // Add our node without eviction.
        if (!this->add_node_without_eviction(node)) return false;

        while (node_count > max_node_count) evict_last_node();  // evict
        return true;
    }

    /// Adds a node under the given key without triggering eviction. Returns true if the node was
    /// added, false if the node was not because a node with that key is already in the set.
    bool add_node_without_eviction(node_type_t *node) {
        assert(node != NULL && node != &mouth);

        // Try inserting; return false if it was already in the set.
        if (!node_set.insert(node).second) return false;

        // Add the node after the mouth.
        node->next = mouth.next;
        node->next->prev = node;
        node->prev = &mouth;
        mouth.next = node;

        // Update the count. This may push us over the maximum node count.
        node_count++;
        return true;
    }

    /// Counts nodes.
    size_t size(void) { return node_count; }

    /// Evicts all nodes.
    void evict_all_nodes(void) {
        while (node_count > 0) {
            evict_last_node();
        }
    }

    /// Iterator for walking nodes, from least recently used to most.
    class iterator {
        lru_node_t *node;

       public:
        explicit iterator(lru_node_t *val) : node(val) {}
        void operator++() { node = lru_cache_t::get_previous(node); }
        void operator++(int x) { node = lru_cache_t::get_previous(node); }
        bool operator==(const iterator &other) { return node == other.node; }
        bool operator!=(const iterator &other) { return !(*this == other); }
        node_type_t *operator*() { return static_cast<node_type_t *>(node); }
    };

    iterator begin() { return iterator(mouth.prev); }
    iterator end() { return iterator(&mouth); }
};

#endif
