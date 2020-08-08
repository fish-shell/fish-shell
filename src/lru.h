// Least-recently-used cache implementation.
#ifndef FISH_LRU_H
#define FISH_LRU_H

#include <cwchar>
#include <unordered_map>

#include "common.h"

// Least-recently-used cache class.
//
// This a map from wcstring to Contents, that will evict entries when the count exceeds the maximum.
// It uses CRTP to inform clients when entries are evicted. This uses the classic LRU cache
// structure: a dictionary mapping keys to nodes, where the nodes also form a linked list. Our
// linked list is circular and has a sentinel node (the "mouth" - picture a snake swallowing its
// tail). This simplifies the logic: no pointer is ever NULL! It also works well with C++'s iterator
// since the sentinel node is a natural value for end(). Our nodes also have the unusual property of
// having a "back pointer": they store an iterator to the entry in the map containing the node. This
// allows us, given a node, to immediately locate the node and its key in the dictionary. This
// allows us to avoid duplicating the key in the node.
template <class Derived, class Contents>
class lru_cache_t {
    struct lru_node_t;
    struct lru_link_t {
        // Our doubly linked list
        // The base class is used for the mouth only
        lru_link_t *prev = nullptr;
        lru_link_t *next = nullptr;
    };

    // The node type in our LRU cache
    struct lru_node_t : public lru_link_t {
        // No copying
        lru_node_t(const lru_node_t &) = delete;
        lru_node_t &operator=(const lru_node_t &) = delete;
        lru_node_t(lru_node_t &&) = default;

        // Our key in the map. This is owned by the map itself.
        const wcstring *key = nullptr;

        // The value from the client
        Contents value;

        explicit lru_node_t(Contents &&v) : value(std::move(v)) {}
    };

    // Max node count. This may be (transiently) exceeded by add_node_without_eviction, which is
    // used from background threads.
    const size_t max_node_count;

    // All of our nodes.
    // Note that our linked list contains pointers to these nodes in the map.
    // We are dependent on the iterator-noninvalidation guarantees of std::unordered_map.
    std::unordered_map<wcstring, lru_node_t> node_map;

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
        // We should never evict the mouth.
        assert(node != &mouth && node != nullptr && node->key != nullptr);

        auto iter = this->node_map.find(*node->key);
        assert(iter != this->node_map.end());

        // Remove it from the linked list.
        node->prev->next = node->next;
        node->next->prev = node->prev;

        // Pull out our key and value
        // Note we copy the key in case the map needs it to erase the node
        wcstring key = *node->key;
        Contents value(std::move(node->value));

        // Remove us from the map. This deallocates node!
        node_map.erase(iter);

        // Tell ourselves what we did
        Derived *dthis = static_cast<Derived *>(this);
        dthis->entry_was_evicted(std::move(key), std::move(value));
    }

    // Evicts the last node
    void evict_last_node() {
        assert(mouth.prev != &mouth);
        evict_node(static_cast<lru_node_t *>(mouth.prev));
    }

    // CRTP callback for when a node is evicted.
    // Clients can implement this
    void entry_was_evicted(wcstring key, Contents value) {
        UNUSED(key);
        UNUSED(value);
    }

    // Implementation of merge step for mergesort.
    // Given two singly linked lists left and right, and a binary func F implementing less-than,
    // return the list in sorted order.
    template <typename F>
    static lru_link_t *merge(lru_link_t *left, size_t left_len, lru_link_t *right, size_t right_len,
                             const F &func) {
        assert(left_len > 0 && right_len > 0);

        auto popleft = [&]() {
            lru_link_t *ret = left;
            left = left->next;
            left_len--;
            return ret;
        };

        auto popright = [&]() {
            lru_link_t *ret = right;
            right = right->next;
            right_len--;
            return ret;
        };

        lru_link_t *head;
        lru_link_t **cursor = &head;
        while (left_len && right_len) {
            bool goleft = !func(static_cast<lru_node_t *>(left)->value,
                                static_cast<lru_node_t *>(right)->value);
            *cursor = goleft ? popleft() : popright();
            cursor = &(*cursor)->next;
        }
        while (left_len || right_len) {
            *cursor = left_len ? popleft() : popright();
            cursor = &(*cursor)->next;
        }
        return head;
    }

    // mergesort the given list of the given length.
    // This only sets the next pointers, not the prev ones.
    template <typename F>
    static lru_link_t *mergesort(lru_link_t *node, size_t length, const F &func) {
        if (length <= 1) {
            return node;
        }
        // divide us into two lists, left and right
        const size_t left_len = length / 2;
        const size_t right_len = length - left_len;
        lru_link_t *left = node;

        lru_link_t *right = node;
        for (size_t i = 0; i < left_len; i++) {
            right = right->next;
        }

        // Recursive sorting
        left = mergesort(left, left_len, func);
        right = mergesort(right, right_len, func);

        // Merge them
        return merge(left, left_len, right, right_len, func);
    }

   public:
    // Constructor. Note our linked list is always circular.
    explicit lru_cache_t(size_t max_size = 1024) : max_node_count(max_size) {
        mouth.next = mouth.prev = &mouth;
    }

    // Returns the value for a given key, or NULL.
    // This counts as a "use" and so promotes the node
    Contents *get(const wcstring &key) {
        auto where = this->node_map.find(key);
        if (where == this->node_map.end()) {
            // not found
            return nullptr;
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
    bool insert(wcstring key, Contents value) {
        if (!this->insert_no_eviction(std::move(key), std::move(value))) {
            return false;
        }

        while (this->node_map.size() > max_node_count) {
            evict_last_node();
        }
        return true;
    }

    // Adds a node under the given key without triggering eviction. Returns true if the node was
    // added, false if the node was not because a node with that key is already in the set.
    bool insert_no_eviction(wcstring &&key, Contents &&value) {
        // Try inserting; return false if it was already in the set.
        auto iter_inserted = this->node_map.emplace(std::move(key), lru_node_t(std::move(value)));
        if (!iter_inserted.second) {
            // already present - so promote it
            promote_node(&iter_inserted.first->second);
            return false;
        }

        // Tell the node where it is in the map
        auto iter = iter_inserted.first;
        lru_node_t *node = &iter->second;
        node->key = &iter->first;

        node->next = mouth.next;
        node->next->prev = node;
        node->prev = &mouth;
        mouth.next = node;
        return true;
    }

    // Number of entries
    size_t size() const { return this->node_map.size(); }

    // Given a binary function F implementing less-than on the contents, place the nodes in sorted
    // order.
    template <typename F>
    void stable_sort(const F &func) {
        // Perform the sort. This sets forward pointers only
        size_t length = this->size();
        if (length <= 1) {
            return;
        }

        lru_link_t *sorted = mergesort(this->mouth.next, length, func);
        mouth.next = sorted;
        // Go through and set back back pointers.
        lru_link_t *cursor = sorted;
        lru_link_t *prev = &mouth;
        for (size_t i = 0; i < length; i++) {
            cursor->prev = prev;
            prev = cursor;
            cursor = cursor->next;
        }
        // prev is now last element in list
        // make the list circular
        prev->next = &mouth;
        mouth.prev = prev;
    }

    void evict_all_nodes(void) {
        while (this->size() > 0) {
            evict_last_node();
        }
    }

    // Iterator for walking nodes, from least recently used to most.
    class iterator {
        const lru_link_t *node;

       public:
        typedef std::pair<const wcstring &, const Contents &> value_type;

        explicit iterator(const lru_link_t *val) : node(val) {}
        void operator++() { node = node->prev; }
        bool operator==(const iterator &other) const { return node == other.node; }
        bool operator!=(const iterator &other) const { return !(*this == other); }
        value_type operator*() const {
            const lru_node_t *dnode = static_cast<const lru_node_t *>(node);
            return {*dnode->key, dnode->value};
        }
    };

    iterator begin() const { return iterator(mouth.prev); }
    iterator end() const { return iterator(&mouth); }

    void check_sanity() const {
        // Check linked list sanity
        size_t expected_count = this->size();
        const lru_link_t *prev = &mouth;
        const lru_link_t *cursor = mouth.next;

        size_t max = 1024 * 1024 * 64;
        size_t count = 0;
        while (cursor != &mouth) {
            if (cursor->prev != prev) {
                DIE("node busted previous link");
            }
            prev = cursor;
            cursor = cursor->next;
            if (count++ > max) {
                DIE("LRU cache unable to re-reach the mouth - not circularly linked?");
            }
        }
        if (mouth.prev != prev) {
            DIE("mouth.prev does not connect to last node");
        }
        if (count != expected_count) {
            DIE("linked list count mismatch from map count");
        }

        // Count iterators
        size_t iter_dist = 0;
        for (auto p : *this) {
            UNUSED(p);
            iter_dist++;
        }
        if (iter_dist != count) {
            DIE("linked list iterator mismatch from map count");
        }
    }
};

#endif
