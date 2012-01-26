/** \file function.c

    Prototypes for functions for storing and retrieving function
	information. These functions also take care of autoloading
	functions in the $fish_function_path. Actual function evaluation
	is taken care of by the parser and to some degree the builtin
	handling library.
*/

#include "config.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <wchar.h>
#include <unistd.h>
#include <termios.h>
#include <signal.h>
#include <pthread.h>
#include <errno.h>
#include <map>
#include <set>

#include "wutil.h"
#include "fallback.h"
#include "util.h"

#include "function.h"
#include "proc.h"
#include "parser.h"
#include "common.h"
#include "intern.h"
#include "event.h"
#include "reader.h"
#include "parse_util.h"
#include "parser_keywords.h"
#include "env.h"
#include "expand.h"
#include "halloc.h"
#include "halloc_util.h"
#include "builtin_scripts.h"

/**
   Table containing all functions
*/
typedef std::map<wcstring, shared_ptr<function_info_t> > function_map_t;
static function_map_t loaded_functions;

/* Lock for functions */
static pthread_mutex_t functions_lock;

/* Autoloader for functions */
class function_autoload_t : public autoload_t {
public:
    function_autoload_t();
    virtual void command_removed(const wcstring &cmd);
};

static function_autoload_t function_autoloader;

/** Constructor */
function_autoload_t::function_autoload_t() : autoload_t(L"fish_function_path",
                                                        internal_function_scripts,
                                                        sizeof internal_function_scripts / sizeof *internal_function_scripts)
{
}

/** Removes a function from our internal table, returning true if it was found and false if not */
static bool function_remove_ignore_autoload(const wcstring &name);

/** Callback when an autoloaded function is removed */
void function_autoload_t::command_removed(const wcstring &cmd) {
    function_remove_ignore_autoload(cmd);
}

/* Helper macro for vomiting */
#define VOMIT_ON_FAILURE(a) do { if (0 != (a)) { int err = errno; fprintf(stderr, "%s failed on line %d in file %s: %d (%s)\n", #a, __LINE__, __FILE__, err, strerror(err)); abort(); }} while (0)

/**
   Kludgy flag set by the load function in order to tell function_add
   that the function being defined is autoloaded. There should be a
   better way to do this...
*/
static int is_autoload = 0;

/**
   Make sure that if the specified function is a dynamically loaded
   function, it has been fully loaded.
*/
static int load( const wchar_t *name )
{
    ASSERT_IS_MAIN_THREAD();
    scoped_lock lock(functions_lock);
	int was_autoload = is_autoload;
	int res;
    function_map_t::iterator iter = loaded_functions.find(name);
	if( iter !=  loaded_functions.end() && !iter->second->is_autoload ) {
		return 0;
    }
	
	is_autoload = 1;	
	res = function_autoloader.load( name, true );
	is_autoload = was_autoload;
	return res;
}

/**
   Insert a list of all dynamically loaded functions into the
   specified list.
*/
static void autoload_names( std::set<wcstring> &names, int get_hidden )
{
	size_t i;
	
	const env_var_t path_var_wstr =  env_get_string( L"fish_function_path" );
    if (path_var_wstr.missing())
        return;
	const wchar_t *path_var = path_var_wstr.c_str(); 
	
    wcstring_list_t path_list;

	tokenize_variable_array2( path_var, path_list );
	for( i=0; i<path_list.size(); i++ )
	{
        const wcstring &ndir_str = path_list.at(i);
		const wchar_t *ndir = (wchar_t *)ndir_str.c_str();
		DIR *dir = wopendir( ndir );
		if( !dir )
			continue;
		
		wcstring name;
		while (wreaddir(dir, name))
		{
			const wchar_t *fn = name.c_str();
			const wchar_t *suffix;
			if( !get_hidden && fn[0] == L'_' )
				continue;
			
            suffix = wcsrchr( fn, L'.' );
			if( suffix && (wcscmp( suffix, L".fish" ) == 0 ) )
			{
                wcstring name(fn, suffix - fn);
                names.insert(name);
			}
		}				
		closedir(dir);
	}
}

void function_init()
{
    pthread_mutexattr_t a;
    VOMIT_ON_FAILURE(pthread_mutexattr_init(&a));
    VOMIT_ON_FAILURE(pthread_mutexattr_settype(&a,PTHREAD_MUTEX_RECURSIVE));
    VOMIT_ON_FAILURE(pthread_mutex_init(&functions_lock, &a));
    VOMIT_ON_FAILURE(pthread_mutexattr_destroy(&a));
}


void function_add( function_data_t *data, const parser_t &parser )
{
	int i;
	
	CHECK( data->name, );
	CHECK( data->definition, );
	scoped_lock lock(functions_lock);
	function_remove( data->name );
	    
    shared_ptr<function_info_t> &info = loaded_functions[data->name];
    if (! info) {
        info.reset(new function_info_t);
    }
    
	info->definition_offset = parse_util_lineno( parser.get_buffer(), parser.current_block->tok_pos )-1;
	info->definition = data->definition;

	if( data->named_arguments )
	{
		for( i=0; i<al_get_count( data->named_arguments ); i++ )
		{
            info->named_arguments.push_back((wchar_t *)al_get( data->named_arguments, i ));
		}
	}
	
	
	if (data->description)
        info->description = data->description;
	info->definition_file = intern(reader_current_filename());
	info->is_autoload = is_autoload;
	info->shadows = data->shadows;
	
	
	for( i=0; i<al_get_count( data->events ); i++ )
	{
		event_add_handler( (event_t *)al_get( data->events, i ) );
	}
}

static int function_exists_internal( const wchar_t *cmd, bool autoload )
{
	int res;
	CHECK( cmd, 0 );
	
	if( parser_keywords_is_reserved(cmd) )
		return 0;
	
    scoped_lock lock(functions_lock);
	if ( autoload ) load( cmd );
    res = loaded_functions.find(cmd) != loaded_functions.end();
    return res;
}

int function_exists( const wchar_t *cmd )
{
    return function_exists_internal( cmd, true );
}

int function_exists_no_autoload( const wchar_t *cmd )
{
    return function_exists_internal( cmd, false );    
}

static bool function_remove_ignore_autoload(const wcstring &name)
{
    scoped_lock lock(functions_lock);
    bool erased = (loaded_functions.erase(name) > 0);
	
	if (erased) {
        event_t ev;
        ev.type=EVENT_ANY;
        ev.function_name=name.c_str();	
        event_remove( &ev );
    }
    return erased;

}

void function_remove( const wcstring &name )
{
    if (function_remove_ignore_autoload(name))
        function_autoloader.unload( name );
}

shared_ptr<function_info_t> function_get(const wcstring &name)
{
    scoped_lock lock(functions_lock);
    function_map_t::iterator iter = loaded_functions.find(name);
    if (iter == loaded_functions.end()) {
        return shared_ptr<function_info_t>();
    } else {
        return iter->second;
    }
}
	
const wchar_t *function_get_definition( const wchar_t *name )
{
    shared_ptr<function_info_t> func = function_get(name);
    return func ? func->definition.c_str() : NULL;
}

wcstring_list_t function_get_named_arguments( const wchar_t *name )
{
    shared_ptr<function_info_t> func = function_get(name);
    return func ? func->named_arguments : wcstring_list_t();
}

int function_get_shadows( const wchar_t *name )
{
    shared_ptr<function_info_t> func = function_get(name);
    return func ? func->shadows : false;
}

	
const wchar_t *function_get_desc( const wchar_t *name )
{
    /* Empty length string goes to NULL */
    shared_ptr<function_info_t> func = function_get(name);
    if (func && func->description.size()) {
        return _(func->description.c_str());
    } else {
        return NULL;
    }
}

void function_set_desc( const wchar_t *name, const wchar_t *desc )
{
	CHECK( name, );
	CHECK( desc, );
	
	load( name );
    shared_ptr<function_info_t> func = function_get(name);
    if (func) func->description = desc;
}

int function_copy( const wchar_t *name, const wchar_t *new_name )
{
    int result = 0;
    scoped_lock lock(functions_lock);
    function_map_t::const_iterator iter = loaded_functions.find(name);
    if (iter != loaded_functions.end()) {
        shared_ptr<function_info_t> &new_info = loaded_functions[new_name];
        new_info.reset(new function_info_t(*iter->second));
        
		// This new instance of the function shouldn't be tied to the def 
		// file of the original. 
		new_info->definition_file = 0;
		new_info->is_autoload = 0;
        
        result = 1;
    }
	return result;
}

wcstring_list_t function_get_names( int get_hidden )
{
    std::set<wcstring> names;
    scoped_lock lock(functions_lock);
	autoload_names( names, get_hidden );
	
    function_map_t::const_iterator iter;
    for (iter = loaded_functions.begin(); iter != loaded_functions.end(); iter++) {
        const wcstring &name = iter->first;
        
        /* Maybe skip hidden */
        if (! get_hidden) {
            if (name.size() == 0 || name.at(0) == L'_') continue;
        }
        
        names.insert(name);
    }
    
    return wcstring_list_t(names.begin(), names.end());
}

const wchar_t *function_get_definition_file( const wchar_t *name )
{
    shared_ptr<function_info_t> func = function_get(name);
    return func ? func->definition_file : NULL;
}


int function_get_definition_offset( const wchar_t *name )
{
    shared_ptr<function_info_t> func = function_get(name);
    return func ? func->definition_offset : -1;
}

