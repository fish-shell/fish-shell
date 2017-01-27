// Least-recently-used cache implementation.
#ifndef FISH_LRU_H
#define FISH_LRU_H

#include <assert.h>
#include <wchar.h>
#include <map>

#include "common.h"

// Least-recently-used cache class
// This a map from wcstring to CONTENTS, that will evict entries when the count exceeds the maximum.
// It uses CRTP to inform clients when entries are evicted. This uses the classic LRU cache structure:
// a dictionary mapping keys to nodes, where the nodes also form a linked list. Our linked list is
// circular and has a sentinel node (the "mouth" - picture a snake swallowing its tail). This simplifies
// the logic: no pointer is ever NULL! It also works well with C++'s iterator since the sentinel node
// is a natural value for end(). Our nodes also have the unusual property of having a "back pointer":
// they store an iterator to the entry in the map containing the node. This allows us, given a node, to
// immediately locate the node and its key in the dictionary. This allows us to avoid duplicating the key
// in the node.
template <class DERIVED, class CONTENTS>
class lru_cache_t {

    struct lru_node_t;
    typedef typename std::map<wcstring, lru_node_t>::iterator node_iter_t;

    struct lru_link_t {
        // Our doubly linked list
        // The base class is used for the mouth only
        lru_link_t *prev = NULL;
        lru_link_t *next = NULL;
    };

    // The node type in our LRU cache
    struct lru_node_t : public lru_link_t {
        // No copying
        lru_node_t(const lru_node_t &) = delete;
        lru_node_t &operator=(const lru_node_t &) = delete;
        lru_node_t(lru_node_t &&) = default;

        // Our location in the map!
        node_iter_t iter;

        // The value from the client
        CONTENTS value;

        explicit lru_node_t(CONTENTS v) :
            value(std::move(v))
        {}
    };


    // Max node count. This may be (transiently) exceeded by add_node_without_eviction, which is
    // used from background threads.
    const size_t max_node_count;

    // All of our nodes
    // Note that our linked list contains pointers to these nodes in the map
    // We are dependent on the iterator-noninvalidation guarantees of std::map
    std::map<wcstring, lru_node_t> node_map;

    // Head of the linked list
    // The list is circular!
    // If "empty" the mouth just points at itself.
    lru_link_t mouth;

    // Take a node and move it to the front of the list
    void promote_node(lru_node_t *node) {
        assert(node != &mouth);

        // First unhook us
        node->prev->next = node->next;
        node->next->prev = node->prev;

        // Put us after the mouth
        node->next = mouth.next;
        node->next->prev = node;
        node->prev = &mouth;
        mouth.next = node;
    }

    // Remove the node
    void evict_node(lru_node_t *node) {
        assert(node != &mouth);

        // We should never evict the mouth.
        assert(node != NULL && node->iter != this->node_map.end());

        // Remove it from the linked list.
        node->prev->next = node->next;
        node->next->prev = node->prev;

        // Pull out our key and value
        wcstring key = std::move(node->iter->first);
        CONTENTS value(std::move(node->value));

        // Remove us from the map. This deallocates node!
        node_map.erase(node->iter);
        node = NULL;

        // Tell ourselves what we did
        DERIVED *dthis = static_cast<DERIVED *>(this);
        dthis->entry_was_evicted(std::move(key), std::move(value));
    }

    // Evicts the last node
    void evict_last_node() {
        assert(mouth.prev != &mouth);
        evict_node(static_cast<lru_node_t *>(mouth.prev));
    }

    // CRTP callback for when a node is evicted.
    // Clients can implement this
    void entry_was_evicted(wcstring key, CONTENTS value) {
        USE(key);
        USE(value);
    }

   public:
    // Constructor
    // Note our linked list is always circular!
    explicit lru_cache_t(size_t max_size = 1024) : max_node_count(max_size) {
        mouth.next = mouth.prev = &mouth;
    }

    // Returns the value for a given key, or NULL.
    // This counts as a "use" and so promotes the node
    CONTENTS *get(const wcstring &key) {
        auto where = this->node_map.find(key);
        if (where == this->node_map.end()) {
            // not found
            return NULL;
        }
        promote_node(&where->second);
        return &where->second.value;
    }

    // Evicts the node for a given key, returning true if a node was evicted.
    bool evict_node(const wcstring &key) {
        auto where = this->node_map.find(key);
        if (where == this->node_map.end()) return false;
        evict_node(&where->second);
        return true;
    }

    // Adds a node under the given key. Returns true if the node was added, false if the node was
    // not because a node with that key is already in the set.
    bool insert(wcstring key, CONTENTS value) {
        if (! this->insert_no_eviction(std::move(key), std::move(value))) {
            return false;
        }

        while (this->node_map.size() > max_node_count) {
            evict_last_node();
        }
        return true;
    }

    // Adds a node under the given key without triggering eviction. Returns true if the node was
    // added, false if the node was not because a node with that key is already in the set.
    bool insert_no_eviction(wcstring key, CONTENTS value) {
        // Try inserting; return false if it was already in the set.
        auto iter_inserted = this->node_map.emplace(std::move(key), lru_node_t(std::move(value)));
        if (! iter_inserted.second) {
            // already present
            return false;
        }

        // Tell the node where it is in the map
        node_iter_t iter = iter_inserted.first;
        lru_node_t *node = &iter->second;
        node->iter = iter;

        node->next = mouth.next;
        node->next->prev = node;
        node->prev = &mouth;
        mouth.next = node;
        return true;
    }

    // Number of entries
    size_t size() { return this->node_map.size(); }

    void evict_all_nodes(void) {
        while (this->size() > 0) {
            evict_last_node();
        }
    }

    // Iterator for walking nodes, from least recently used to most.
    class iterator {
        lru_link_t *node;
       public:
        typedef std::pair<const wcstring &, const CONTENTS &> value_type;

        explicit iterator(lru_link_t *val) : node(val) {}
        void operator++() { node = node->prev; }
        bool operator==(const iterator &other) { return node == other.node; }
        bool operator!=(const iterator &other) { return !(*this == other); }
        value_type operator*() const {
            const lru_node_t *dnode = static_cast<const lru_node_t *>(node);
            const wcstring &key = dnode->iter->first;
            return {key, dnode->value};
        }
    };

    iterator begin() { return iterator(mouth.prev); }
    iterator end() { return iterator(&mouth); }
};

#endif
