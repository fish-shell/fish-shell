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
#include <wchar.h>
#include <unistd.h>
#include <termios.h>
#include <signal.h>

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
static hash_table_t function;

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
	data = (function_internal_data_t *)hash_get( &function, name );
	if( data && !data->is_autoload )
		return 0;
	
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
		
		struct wdirent *next;
		while( (next=wreaddir(dir))!=0 )
		{
			wchar_t *fn = next->d_name;
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
	hash_init( &function,
			   &hash_wcs_func,
			   &hash_wcs_cmp );
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
	hash_foreach( &function, &clear_entry );
	hash_destroy( &function );
}


void function_add( function_data_t *data )
{
	int i;
	wchar_t *cmd_end;
	function_internal_data_t *d;
	
	CHECK( data->name, );
	CHECK( data->definition, );
	
	function_remove( data->name );
	
	d = halloc( 0, sizeof( function_internal_data_t ) );
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
	
	hash_put( &function, intern(data->name), d );
	
	for( i=0; i<al_get_count( data->events ); i++ )
	{
		event_add_handler( (event_t *)al_get( data->events, i ) );
	}
	
}

int function_exists( const wchar_t *cmd )
{
	
	CHECK( cmd, 0 );
	
	if( parser_keywords_is_reserved(cmd) )
		return 0;
	
	load( cmd );
	return (hash_get(&function, cmd) != 0 );
}

void function_remove( const wchar_t *name )
{
	void *key;
	void *dv;
	function_internal_data_t *d;
	event_t ev;
	
	CHECK( name, );

	hash_remove( &function,
				 name,
				 &key,
				 &dv );

	d=(function_internal_data_t *)dv;
	
	if( !key )
		return;

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
}
	
const wchar_t *function_get_definition( const wchar_t *name )
{
	function_internal_data_t *data;
	
	CHECK( name, 0 );
	
	load( name );
	data = (function_internal_data_t *)hash_get( &function, name );
	if( data == 0 )
		return 0;
	return data->definition;
}

array_list_t *function_get_named_arguments( const wchar_t *name )
{
	function_internal_data_t *data;
	
	CHECK( name, 0 );
	
	load( name );
	data = (function_internal_data_t *)hash_get( &function, name );
	if( data == 0 )
		return 0;
	return data->named_arguments;
}

int function_get_shadows( const wchar_t *name )
{
	function_internal_data_t *data;
	
	CHECK( name, 0 );
	
	load( name );
	data = (function_internal_data_t *)hash_get( &function, name );
	if( data == 0 )
		return 0;
	return data->shadows;
}

	
const wchar_t *function_get_desc( const wchar_t *name )
{
	function_internal_data_t *data;
	
	CHECK( name, 0 );
		
	load( name );
	data = (function_internal_data_t *)hash_get( &function, name );
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
	data = (function_internal_data_t *)hash_get( &function, name );
	if( data == 0 )
		return;

	data->description = halloc_wcsdup( data, desc );
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
		if( wcscmp( al_get( list, i ), str) == 0 )
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
		
	autoload_names( list, get_hidden );
	
	if( get_hidden )
	{
		hash_foreach2( &function, &get_names_internal_all, list );
	}
	else
	{
		hash_foreach2( &function, &get_names_internal, list );
	}
	
}

const wchar_t *function_get_definition_file( const wchar_t *name )
{
	function_internal_data_t *data;

	CHECK( name, 0 );
		
	data = (function_internal_data_t *)hash_get( &function, name );
	if( data == 0 )
		return 0;
	
	return data->definition_file;
}


int function_get_definition_offset( const wchar_t *name )
{
	function_internal_data_t *data;

	CHECK( name, -1 );
		
	data = (function_internal_data_t *)hash_get( &function, name );
	if( data == 0 )
		return -1;
	
	return data->definition_offset;
}

