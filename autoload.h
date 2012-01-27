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
    
    /** Max node count */
    const size_t max_node_count;
    
    /** Count of nodes */
    size_t node_count;
    
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
    lru_cache_impl_t(size_t max_size = 1024 );
    
    /** Returns the node for a given key, or NULL */
    lru_node_t *get_node(const wcstring &key);
    
    /** Evicts the node for a given key, returning true if a node was evicted. */
    bool evict_node(const wcstring &key);
    
    /** Adds a node under the given key. Returns true if the node was added, false if the node was not because a node with that key is already in the set. */
    bool add_node(lru_node_t *node);
    
    /** Counts nodes */
    size_t size(void) { return node_count; }
    
    /** Evicts all nodes */
    void evict_all_nodes(void);
};

/** Template cover to avoid casting */
template<class node_type_t>
class lru_cache_t : public lru_cache_impl_t {
public:
    node_type_t *get_node(const wcstring &key) { return static_cast<node_type_t *>(lru_cache_impl_t::get_node(key)); }
    bool add_node(node_type_t *node) { return lru_cache_impl_t::add_node(node); }
    lru_cache_t(size_t max_size = 1024 ) : lru_cache_impl_t(max_size) { }
};

struct autoload_function_t : public lru_node_t
{   
    autoload_function_t(const wcstring &key) : lru_node_t(key), is_loaded(false), is_placeholder(false) { bzero(&access, sizeof access); }
    file_access_attempt_t access; /** The last access attempt */
    bool is_loaded; /** Whether we have actually loaded this function */
    bool is_placeholder; /** Whether we are a placeholder that stands in for "no such function". If this is true, then is_loaded must be false. */
};


struct builtin_script_t;

/**
  A class that represents a path from which we can autoload, and the autoloaded contents.
 */
class autoload_t : private lru_cache_t<autoload_function_t> {
private:

    /** The environment variable name */
    const wcstring env_var_name;
    
    /** Builtin script array */
    const struct builtin_script_t *const builtin_scripts;
    
    /** Builtin script count */
    const size_t builtin_script_count;

    /** The path from which we most recently autoloaded */
    wcstring path;

	/**
	   A table containing all the files that are currently being
	   loaded. This is here to help prevent recursion.
	*/
    std::set<wcstring> is_loading_set;

    bool is_loading(const wcstring &name) const {
        return is_loading_set.find(name) != is_loading_set.end();
    }
    
    void remove_all_functions(void) {
        this->evict_all_nodes();
    }
        
    bool locate_file_and_maybe_load_it( const wcstring &cmd, bool really_load, bool reload, const wcstring_list_t &path_list );
    
    virtual void node_was_evicted(autoload_function_t *node);


    protected:    
    /** Overridable callback for when a command is removed */
    virtual void command_removed(const wcstring &cmd) { }
    
    public:
    
    /** Create an autoload_t for the given environment variable name */
    autoload_t(const wcstring &env_var_name_var, const builtin_script_t *scripts, size_t script_count );
    
    /**
       Autoload the specified file, if it exists in the specified path. Do
       not load it multiple times unless it's timestamp changes or
       parse_util_unload is called.

       Autoloading one file may unload another. 

       \param cmd the filename to search for. The suffix '.fish' is always added to this name
       \param on_unload a callback function to run if a suitable file is found, which has not already been run. unload will also be called for old files which are unloaded.
       \param reload wheter to recheck file timestamps on already loaded files
    */
    int load( const wcstring &cmd, bool reload );

    /**
       Tell the autoloader that the specified file, in the specified path,
       is no longer loaded.

       \param cmd the filename to search for. The suffix '.fish' is always added to this name
       \param on_unload a callback function which will be called before (re)loading a file, may be used to unload the previous file.
       \return non-zero if the file was removed, zero if the file had not yet been loaded
    */
    int unload( const wcstring &cmd );
    
    /**
       Unloads all files.
    */
    void unload_all( );
    
    /** Check whether the given command could be loaded, but do not load it. */
    bool can_load( const wcstring &cmd );

};

#endif
