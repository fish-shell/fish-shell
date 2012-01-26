/** \file autoload.cpp
	
The classes responsible for autoloading functions and completions.
*/

#include "config.h"
#include "autoload.h"
#include "wutil.h"
#include "common.h"
#include "signal.h"
#include "env.h"
#include "builtin_scripts.h"
#include "exec.h"
#include <assert.h>

static const size_t kLRULimit = 16;

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
    
    /* Add the node after the mouth */
    node->next = mouth.next;
    node->prev = &mouth;
    mouth.next = node;
    
    /* Update the count */
    node_count++;
    
    /* Evict */
    while (node_count > kLRULimit)
        evict_last_node();
    
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



autoload_t::autoload_t(const wcstring &env_var_name_var, const builtin_script_t * const scripts, size_t script_count) :
                       env_var_name(env_var_name_var),
                       builtin_scripts(scripts),
                       builtin_script_count(script_count)
{
}

void autoload_t::node_was_evicted(autoload_function_t *node) {
    // Tell ourselves that the command was removed, unless it was a placeholder
    if (! node->is_placeholder)
        this->command_removed(node->key);
    delete node;
}

void autoload_t::reset( )
{
    this->evict_all_nodes();
}

int autoload_t::unload( const wcstring &cmd )
{
    return this->evict_node(cmd);
}

int autoload_t::load( const wcstring &cmd, bool reload )
{
	int res;
	int c, c2;
	
	CHECK_BLOCK( 0 );

	const env_var_t path_var = env_get_string( env_var_name.c_str() );	
	
	/*
	  Do we know where to look?
	*/
	if( path_var.empty() )
	{
		return 0;
	}
    
    /*
      Check if the lookup path has changed. If so, drop all loaded
      files.
    */
    if( path_var != this->path )
    {
        this->path = path_var;
        this->reset();
    }

    /**
       Warn and fail on infinite recursion
    */
    if (this->is_loading(cmd))
    {
        debug( 0, 
               _( L"Could not autoload item '%ls', it is already being autoloaded. " 
                  L"This is a circular dependency in the autoloading scripts, please remove it."), 
               cmd.c_str() );
        return 1;
    }



    std::vector<wcstring> path_list;
	tokenize_variable_array2( path_var, path_list );
	
	c = path_list.size();
	
    is_loading_set.insert(cmd);

	/*
	  Do the actual work in the internal helper function
	*/
	res = this->load_internal( cmd, reload, path_list );
    
    int erased = is_loading_set.erase(cmd);
    assert(erased);
	
	c2 = path_list.size();

	/**
	   Make sure we didn't 'drop' something
	*/
	
	assert( c == c2 );
	
	return res;
}

static bool script_name_precedes_script_name(const builtin_script_t &script1, const builtin_script_t &script2)
{
    return wcscmp(script1.name, script2.name) < 0;
}

/**
   This internal helper function does all the real work. By using two
   functions, the internal function can return on various places in
   the code, and the caller can take care of various cleanup work.
*/

int autoload_t::load_internal( const wcstring &cmd,
                               int reload,
                               const wcstring_list_t &path_list )
{

	size_t i;
	int reloaded = 0;

    /* Get the function */
    autoload_function_t * func = this->get_function_with_name(cmd);

    /* Return if already loaded and we are skipping reloading */
	if( !reload && func )
		return 0;

    /* Nothing to do if we just checked it */
    if (func && time(NULL) - func->access.last_checked <= 1)
        return 0;
    
    /* The source of the script will end up here */
    wcstring script_source;
    bool has_script_source = false;
    
    /*
     Look for built-in scripts via a binary search
    */
    const builtin_script_t *matching_builtin_script = NULL;
    if (builtin_script_count > 0)
    {
        const builtin_script_t test_script = {cmd.c_str(), NULL};
        const builtin_script_t *array_end = builtin_scripts + builtin_script_count;
        const builtin_script_t *found = std::lower_bound(builtin_scripts, array_end, test_script, script_name_precedes_script_name);
        if (found != array_end && ! wcscmp(found->name, test_script.name))
        {
            /* We found it */
            matching_builtin_script = found;
        }
    }
    if (matching_builtin_script) {
        has_script_source = true;
        script_source = str2wcstring(matching_builtin_script->def);
    }
    
    if (! has_script_source)
    {
        /*
          Iterate over path searching for suitable completion files
        */
        for( i=0; i<path_list.size(); i++ )
        {
            wcstring next = path_list.at(i);
            wcstring path = next + L"/" + cmd + L".fish";

            const file_access_attempt_t access = access_file(path, R_OK);
            if (access.accessible) {
                if (! func || access.mod_time != func->access.mod_time) {
                    wcstring esc = escape_string(path, 1);
                    script_source = L". " + esc;
                    has_script_source = true;
                    
                    if( !func )
                        func = new autoload_function_t(cmd);
                    func->access = access;
                    
                    // Remove this command because we are going to reload it
                    command_removed(cmd);
                                        
                    reloaded = 1;
                }
                else if( func )
                {
                    /*
                      If we are rechecking an autoload file, and it hasn't
                      changed, update the 'last check' timestamp.
                    */
                    func->access = access;
                }
                
                break;
            }
        }

        /*
          If no file was found we insert a placeholder function. Later we only
          research if the current time is at least five seconds later.
          This way, the files won't be searched over and over again.
        */
        if( !func )
        {
            func = new autoload_function_t(cmd);
            func->access.last_checked = time(NULL);
            func->is_placeholder = true;
        }
    }
    
    /* If we have a script, either built-in or a file source, then run it */
    if (has_script_source)
    {
        if( exec_subshell( script_source.c_str(), 0 ) == -1 )
        {
            /*
              Do nothing on failiure
            */
        }

    }

	return reloaded;	
}
