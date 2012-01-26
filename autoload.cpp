/** \file autoload.cpp
	
The classes responsible for autoloading functions and completions.
*/

#include "config.h"
#include "autoload.h"
#include "wutil.h"
#include <assert.h>

const size_t kLRULimit = 256;

file_access_attempt_t access_file(const wcstring &path, int mode) {
    file_access_attempt_t result = {0};
    struct stat statbuf;
    if (wstat(path.c_str(), &statbuf)) {
        result.error = errno;
    } else {
        result.mod_time = statbuf.st_mtime;
        if (waccess(path.c_str(), mode)) {
            result.error = errno;
        } else {
            result.accessible = true;
        }
    }
    
    // Note that we record the last checked time after the call, on the assumption that in a slow filesystem, the lag comes before the kernel check, not after.
    result.stale = false;
    result.last_checked = time(NULL);
    return result;
}

lru_cache_impl_t::lru_cache_impl_t() : node_count(0), mouth(L"") {
    /* Hook up the mouth to itself: a one node circularly linked list */
    mouth.prev = mouth.next = &mouth;
}

void lru_cache_impl_t::node_was_evicted(lru_node_t *node) { }

void lru_cache_impl_t::evict_node(lru_node_t *condemned_node) {
    /* We should never evict the mouth */
    assert(condemned_node != NULL && condemned_node != &mouth);

    /* Remove it from the linked list */
    condemned_node->prev->next = condemned_node->next;
    condemned_node->next->prev = condemned_node->prev;

    /* Remove us from the set */
    node_set.erase(condemned_node);
    node_count--;

    /* Tell ourselves */
    this->node_was_evicted(condemned_node);
}

void lru_cache_impl_t::evict_last_node(void) {
    /* Simple */
    evict_node(mouth.prev);
}

bool lru_cache_impl_t::evict_node(const wcstring &key) {
    /* Construct a fake node as our key */
    lru_node_t node_key(key);
    
    /* Look for it in the set */
    node_set_t::iterator iter = node_set.find(&node_key);
    if (iter == node_set.end())
        return false;
    
    /* Evict the given node */
    evict_node(*iter);
    return true;
}

void lru_cache_impl_t::promote_node(lru_node_t *node) {
    /* We should never promote the mouth */
    assert(node != &mouth);
    
    /* First unhook us */
    node->prev->next = node->next;
    node->next->prev = node->prev;
    
    /* Put us after the mouth */
    node->next = mouth.next;
    node->prev = &mouth;
    mouth.next = node;
}

bool lru_cache_impl_t::add_node(lru_node_t *node) {
    assert(node != NULL && node != &mouth);
    
    /* Try inserting; return false if it was already in the set */
    if (! node_set.insert(node).second)
        return false;
    
    /* Update the count */
    node_count++;
    
    /* Add the node after the mouth */
    node->next = mouth.next;
    node->prev = &mouth;
    mouth.next = node;
    
    /* Success */
    return true;
}

lru_node_t *lru_cache_impl_t::get_node(const wcstring &key) {
    lru_node_t *result = NULL;
    
    /* Construct a fake node as our key */
    lru_node_t node_key(key);
    
    /* Look for it in the set */
    node_set_t::iterator iter = node_set.find(&node_key);
    
    /* If we found a node, promote and return it */
    if (iter != node_set.end()) {
        result = *iter;
        promote_node(result);
    }
    return result;
}

void lru_cache_impl_t::evict_all_nodes() {
    while (node_count > 0) {
        evict_last_node();
    }
}
