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

/** A node in our LRU map */
class file_access_node_t {
public:
    file_access_node_t *prev, *next;
    const wcstring path;
    file_access_attempt_t attempt;
    
    file_access_node_t(const wcstring &pathVar) : path(pathVar) { }
    
    bool operator<(const file_access_node_t &other) const { return path < other.path; }
};

/* By default, things are stale after 60 seconds */
const time_t kFishDefaultStalenessInterval = 60;

access_tracker_t::access_tracker_t(time_t stale, int the_mode) :
        node_count(0),
        mouth(NULL),
        stale_interval(stale),
        mode(the_mode)
{
    VOMIT_ON_FAILURE(pthread_mutex_init(&lock, NULL));
}

access_tracker_t::~access_tracker_t() {
    VOMIT_ON_FAILURE(pthread_mutex_destroy(&lock));
}

void access_tracker_t::vacuum_one_node(void) {
    /* Removes the least recently used access */
    assert(mouth && mouth->prev != mouth);
    
    /* Remove us from the linked list */
    file_access_node_t *condemned_node = mouth->prev;
    condemned_node->prev->next = condemned_node->next;
    condemned_node->next->prev = condemned_node->prev;
    
    /* Remove us from the set */
    access_set.erase(condemned_node);
    
    /* Deleted */
    node_count--;
    delete condemned_node;
}

void access_tracker_t::promote_node(file_access_node_t *node) {
    /* Promotes a node to the mouth, unless we're already there... */
    if (node == mouth) return;
    
    /* First unhook us */
    node->prev->next = node->next;
    node->next->prev = node->prev;
    
    /* Now become the mouth */
    node->next = mouth;
    node->prev = mouth->prev;
    mouth = node;
}

/* Return the node referenced by the given string, or NULL */
file_access_node_t *access_tracker_t::while_locked_find_node(const wcstring &path) const {
    file_access_node_t key(path);
    access_set_t::iterator iter = access_set.find(&key);
    return iter != access_set.end() ? *iter : NULL;
}

file_access_attempt_t access_tracker_t::attempt_access(const wcstring& path) const {
    return ::access_file(path, this->mode);
}

bool access_tracker_t::access_file_only_cached(const wcstring &path, file_access_attempt_t &attempt) {
    bool result = false;
    
    /* Lock our lock */
    scoped_lock locker(lock);
    
    /* Search for the node */
    file_access_node_t *node = while_locked_find_node(path);
    if (node) {
        promote_node(node);
        attempt = node->attempt;
        attempt.stale = (time(NULL) - node->attempt.last_checked > this->stale_interval);
        result = true;
    }    
    return result;
}

file_access_attempt_t access_tracker_t::access_file(const wcstring &path) {
    file_access_attempt_t result;
    
    /* Try just using our cache */
    if (access_file_only_cached(path, result) && ! result.stale) {
        return result;
    }
    
    /* Really access the file. Note we are not yet locked, and don't want to be, since this may be slow. */
    result = attempt_access(path);
    
    /* Take the lock so we can insert. */
    scoped_lock locker(lock);
    
    /* Maybe we had it cached and it was stale, or maybe someone else may have put it in the cache while we were unlocked */
    file_access_node_t *node = while_locked_find_node(path);
    
    if (node != NULL) {
        /* Someone else put it in. Promote and overwrite it. */
        node->attempt = result;
        promote_node(node);
    } else {
        /* We did not find this node. Add it. */
        file_access_node_t *node = new file_access_node_t(path);
        node->attempt = result;
        
        /* Insert into the set */
        access_set.insert(node);
        
        /* Insert it into the linked list */
        if (mouth == NULL) {
            /* One element circularly linked list! */
            mouth = node->next = node->prev = node;
        } else {
            /* Normal circularly linked list operation */
            node->next = mouth;
            node->prev = mouth->prev;
            mouth = node;
        }
        
        /* We have one more node now */
        ++node_count;
        
        /* Clean up if we're over our limit */
        while (node_count > kLRULimit)
            vacuum_one_node();
    }
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
