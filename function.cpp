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


/**
   Struct describing a function
*/
typedef struct
{
	/** Function definition */
	wchar_t *definition;
	/** Function description */
	wchar_t *description;	
	/**
	   File where this function was defined
	*/
	const wchar_t *definition_file;
	/**
	   Line where definition started
	*/
	int definition_offset;	
	
	/**
	   List of all named arguments for this function
	 */
	array_list_t *named_arguments;
	
	
	/**
	   Flag for specifying that this function was automatically loaded
	*/
	int is_autoload;

	/**
	   Set to non-zero if invoking this function shadows the variables
	   of the underlying function.
	 */
	int shadows;
}
	function_internal_data_t;

/**
   Table containing all functions
*/
static hash_table_t functions;

/* Lock for functions */
static pthread_mutex_t functions_lock;

/* Helper macro for vomiting */
#define VOMIT_ON_FAILURE(a) do { if (0 != (a)) { int err = errno; fprintf(stderr, "%s failed on line %d in file %s: %d (%s)\n", #a, __LINE__, __FILE__, err, strerror(err)); abort(); }} while (0)

static int kLockDepth = 0;
static char kLockFunction[1024];

/**
   Lock and unlock the functions hash
*/
static void lock_functions(const char *func) {
    VOMIT_ON_FAILURE(pthread_mutex_lock(&functions_lock));
    if (! kLockDepth++) {
        strcat(kLockFunction, func);
    }
}

static void unlock_functions(void) {
    if (! --kLockDepth) {
        memset(kLockFunction, 0, sizeof kLockFunction);
    }
    VOMIT_ON_FAILURE(pthread_mutex_unlock(&functions_lock));
}

#define LOCK_FUNCTIONS() lock_functions(__FUNCTION__)
#define UNLOCK_FUNCTIONS() unlock_functions()


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
	int was_autoload = is_autoload;
	int res;
	function_internal_data_t *data;
    LOCK_FUNCTIONS();
	data = (function_internal_data_t *)hash_get( &functions, name );
	if( data && !data->is_autoload ) {
        UNLOCK_FUNCTIONS();
		return 0;
    }
    UNLOCK_FUNCTIONS();
	
	is_autoload = 1;	
	res = parse_util_load( name,
						   L"fish_function_path",
						   &function_remove,
						   1 );
	is_autoload = was_autoload;
	return res;
}

/**
   Insert a list of all dynamically loaded functions into the
   specified list.
*/
static void autoload_names( array_list_t *out, int get_hidden )
{
	int i;
	
	array_list_t path_list;
	const wchar_t *path_var = env_get( L"fish_function_path" );
	
	if( ! path_var )
		return;
	
	al_init( &path_list );

	tokenize_variable_array( path_var, &path_list );
	for( i=0; i<al_get_count( &path_list ); i++ )
	{
		wchar_t *ndir = (wchar_t *)al_get( &path_list, i );
		DIR *dir = wopendir( ndir );
		if( !dir )
			continue;
		
		wcstring name;
		while (wreaddir(dir, name))
		{
			const wchar_t *fn = name.c_str();
			wchar_t *suffix;
			if( !get_hidden && fn[0] == L'_' )
				continue;
			
			suffix = wcsrchr( fn, L'.' );
			if( suffix && (wcscmp( suffix, L".fish" ) == 0 ) )
			{
				const wchar_t *dup;
				*suffix = 0;
				dup = intern( fn );
				if( !dup )
					DIE_MEM();
				al_push( out, dup );
			}
		}				
		closedir(dir);
	}
	al_foreach( &path_list, &free );
	al_destroy( &path_list );
}

void function_init()
{
	hash_init( &functions,
			   &hash_wcs_func,
			   &hash_wcs_cmp );
    
    pthread_mutexattr_t a;
    VOMIT_ON_FAILURE(pthread_mutexattr_init(&a));
    VOMIT_ON_FAILURE(pthread_mutexattr_settype(&a,PTHREAD_MUTEX_RECURSIVE));
    VOMIT_ON_FAILURE(pthread_mutex_init(&functions_lock, &a));
    VOMIT_ON_FAILURE(pthread_mutexattr_destroy(&a));
}

/**
   Clear specified value, but not key
 */
static void clear_entry( void *key, void *value )
{
	halloc_free( value );
}

void function_destroy()
{
    LOCK_FUNCTIONS();
	hash_foreach( &functions, &clear_entry );
	hash_destroy( &functions );
    UNLOCK_FUNCTIONS();
}


void function_add( function_data_t *data )
{
	int i;
	wchar_t *cmd_end;
	function_internal_data_t *d;
	
	CHECK( data->name, );
	CHECK( data->definition, );
	LOCK_FUNCTIONS();
	function_remove( data->name );
	
	d = (function_internal_data_t *)halloc( 0, sizeof( function_internal_data_t ) );
	d->definition_offset = parse_util_lineno( parser_get_buffer(), current_block->tok_pos )-1;
	d->definition = halloc_wcsdup( d, data->definition );

	if( data->named_arguments )
	{
		d->named_arguments = al_halloc( d );

		for( i=0; i<al_get_count( data->named_arguments ); i++ )
		{
			al_push( d->named_arguments, halloc_wcsdup( d, (wchar_t *)al_get( data->named_arguments, i ) ) );
		}
	}
	
	cmd_end = d->definition + wcslen(d->definition)-1;
	
	d->description = data->description?halloc_wcsdup( d, data->description ):0;
	d->definition_file = intern(reader_current_filename());
	d->is_autoload = is_autoload;
	d->shadows = data->shadows;
	
	hash_put( &functions, intern(data->name), d );
	
	for( i=0; i<al_get_count( data->events ); i++ )
	{
		event_add_handler( (event_t *)al_get( data->events, i ) );
	}
	UNLOCK_FUNCTIONS();
}

static int function_exists_internal( const wchar_t *cmd, int autoload )
{
	int res;
	CHECK( cmd, 0 );
	
	if( parser_keywords_is_reserved(cmd) )
		return 0;
	
    LOCK_FUNCTIONS();
	if ( autoload ) load( cmd );
    res = (hash_get(&functions, cmd) != 0 );
	UNLOCK_FUNCTIONS();
    return res;
}

int function_exists( const wchar_t *cmd )
{
    return function_exists_internal( cmd, 1 );
}

int function_exists_no_autoload( const wchar_t *cmd )
{
    return function_exists_internal( cmd, 0 );    
}

void function_remove( const wchar_t *name )
{
	void *key;
	void *dv;
	function_internal_data_t *d;
	event_t ev;
	
	CHECK( name, );

    LOCK_FUNCTIONS();
	hash_remove( &functions,
				 name,
				 &key,
				 &dv );

	d=(function_internal_data_t *)dv;
	
	if( !key ) {
        UNLOCK_FUNCTIONS();
		return;
    }

	ev.type=EVENT_ANY;
	ev.function_name=name;	
	event_remove( &ev );

	halloc_free( d );

	/*
	  Notify the autoloader that the specified function is erased, but
	  only if this call to fish_remove is not made by the autoloader
	  itself.
	*/
	if( !is_autoload )
	{
		parse_util_unload( name, L"fish_function_path", 0 );
	}
    UNLOCK_FUNCTIONS();
}
	
const wchar_t *function_get_definition( const wchar_t *name )
{
	function_internal_data_t *data;
	
	CHECK( name, 0 );
	LOCK_FUNCTIONS();
	load( name );
	data = (function_internal_data_t *)hash_get( &functions, name );
    UNLOCK_FUNCTIONS();
	if( data == 0 )
		return 0;
	return data->definition;
}

array_list_t *function_get_named_arguments( const wchar_t *name )
{
	function_internal_data_t *data;
	
	CHECK( name, 0 );
    LOCK_FUNCTIONS();
	load( name );
	data = (function_internal_data_t *)hash_get( &functions, name );
    UNLOCK_FUNCTIONS();
	if( data == 0 )
		return 0;
	return data->named_arguments;
}

int function_get_shadows( const wchar_t *name )
{
	function_internal_data_t *data;
	
	CHECK( name, 0 );
	LOCK_FUNCTIONS();
	load( name );
	data = (function_internal_data_t *)hash_get( &functions, name );
    UNLOCK_FUNCTIONS();
	if( data == 0 )
		return 0;
	return data->shadows;
}

	
const wchar_t *function_get_desc( const wchar_t *name )
{
	function_internal_data_t *data;
	
	CHECK( name, 0 );
		
	load( name );
    LOCK_FUNCTIONS();
	data = (function_internal_data_t *)hash_get( &functions, name );
    UNLOCK_FUNCTIONS();
	if( data == 0 )
		return 0;
	
	return _(data->description);
}

void function_set_desc( const wchar_t *name, const wchar_t *desc )
{
	function_internal_data_t *data;
	
	CHECK( name, );
	CHECK( desc, );
	
	load( name );
    LOCK_FUNCTIONS();
	data = (function_internal_data_t *)hash_get( &functions, name );
    UNLOCK_FUNCTIONS();
	if( data == 0 )
		return;

	data->description = halloc_wcsdup( data, desc );
}

int function_copy( const wchar_t *name, const wchar_t *new_name )
{
	int i;
	function_internal_data_t *d, *orig_d;
	
	CHECK( name, 0 );
	CHECK( new_name, 0 );

	orig_d = (function_internal_data_t *)hash_get(&functions, name);
	if( !orig_d )
		return 0;

	d = (function_internal_data_t *)halloc(0, sizeof( function_internal_data_t ) );
	d->definition_offset = orig_d->definition_offset;
	d->definition = halloc_wcsdup( d, orig_d->definition );
	if( orig_d->named_arguments )
	{
		d->named_arguments = al_halloc( d );
		for( i=0; i<al_get_count( orig_d->named_arguments ); i++ )
		{
			al_push( d->named_arguments, halloc_wcsdup( d, (wchar_t *)al_get( orig_d->named_arguments, i ) ) );
		}
		d->description = orig_d->description?halloc_wcsdup(d, orig_d->description):0;
		d->shadows = orig_d->shadows;

		// This new instance of the function shouldn't be tied to the def 
		// file of the original. 
		d->definition_file = 0;
		d->is_autoload = 0;
	}

	hash_put( &functions, intern(new_name), d );

	return 1;
}

/**
   Search arraylist of strings for specified string
*/
static int al_contains_str( array_list_t *list, const wchar_t * str )
{
	int i;

	CHECK( list, 0 );
	CHECK( str, 0 );
	
	for( i=0; i<al_get_count( list ); i++ )
	{
		if( wcscmp( (const wchar_t *)al_get( list, i ), str) == 0 )
		{
			return 1;
		}
	}
	return 0;
}
	
/**
   Helper function for removing hidden functions 
*/
static void get_names_internal( void *key,
								void *val,
								void *aux )
{
	wchar_t *name = (wchar_t *)key;
	if( name[0] != L'_' && !al_contains_str( (array_list_t *)aux, name ) )
	{
		al_push( (array_list_t *)aux, name );
	}
}

/**
   Helper function for removing hidden functions 
*/
static void get_names_internal_all( void *key,
									void *val,
									void *aux )
{
	wchar_t *name = (wchar_t *)key;
	
	if( !al_contains_str( (array_list_t *)aux, name ) )
	{
		al_push( (array_list_t *)aux, name );
	}
}

void function_get_names( array_list_t *list, int get_hidden )
{
	CHECK( list, );
    LOCK_FUNCTIONS();
	autoload_names( list, get_hidden );
	
	if( get_hidden )
	{
		hash_foreach2( &functions, &get_names_internal_all, list );
	}
	else
	{
		hash_foreach2( &functions, &get_names_internal, list );
	}
    UNLOCK_FUNCTIONS();
	
}

const wchar_t *function_get_definition_file( const wchar_t *name )
{
	function_internal_data_t *data;

	CHECK( name, 0 );
    LOCK_FUNCTIONS();
	data = (function_internal_data_t *)hash_get( &functions, name );
    UNLOCK_FUNCTIONS();
	if( data == 0 )
		return 0;
	
	return data->definition_file;
}


int function_get_definition_offset( const wchar_t *name )
{
	function_internal_data_t *data;

	CHECK( name, -1 );
    LOCK_FUNCTIONS();
	data = (function_internal_data_t *)hash_get( &functions, name );
    UNLOCK_FUNCTIONS();
	if( data == 0 )
		return -1;
	
	return data->definition_offset;
}

