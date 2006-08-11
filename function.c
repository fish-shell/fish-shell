/** \file function.c
  Functions for storing and retrieving function information.
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
#include "env.h"
#include "expand.h"


/**
   Struct describing a function
*/
typedef struct
{
	/** Function definition */
	wchar_t *cmd;
	/** Function description */
	wchar_t *desc;	
	/**
	   File where this function was defined
	*/
	const wchar_t *definition_file;
	/**
	   Line where definition started
	*/
	int definition_offset;	
	/**
	   Flag for specifying functions which are actually key bindings
	*/
	int is_binding;
	
	/**
	   Flag for specifying that this function was automatically loaded
	*/
	int is_autoload;
}
	function_data_t;

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
	function_data_t *data;
	data = (function_data_t *)hash_get( &function, name );
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


/**
   Free all contents of an entry to the function hash table
*/
static void clear_function_entry( void *key, 
								  void *data )
{
	function_data_t *d = (function_data_t *)data;
	free( (void *)d->cmd );
	free( (void *)d->desc );
	free( (void *)d );
}

void function_init()
{
	hash_init( &function,
			   &hash_wcs_func,
			   &hash_wcs_cmp );
}

void function_destroy()
{
	hash_foreach( &function, &clear_function_entry );
	hash_destroy( &function );
}


void function_add( const wchar_t *name, 
				   const wchar_t *val,
				   const wchar_t *desc,
				   array_list_t *events,
				   int is_binding )
{
	int i;
	wchar_t *cmd_end;
	function_data_t *d;
	
	CHECK( name, );
	CHECK( val, );
	
	function_remove( name );
	
	d = malloc( sizeof( function_data_t ) );
	d->definition_offset = parse_util_lineno( parser_get_buffer(), current_block->tok_pos )-1;
	d->cmd = wcsdup( val );
	
	cmd_end = d->cmd + wcslen(d->cmd)-1;
	while( (cmd_end>d->cmd) && wcschr( L"\n\r\t ", *cmd_end ) )
	{
		*cmd_end-- = 0;
	}
	
	d->desc = desc?wcsdup( desc ):0;
	d->is_binding = is_binding;
	d->definition_file = intern(reader_current_filename());
	d->is_autoload = is_autoload;
		
	hash_put( &function, intern(name), d );
	
	for( i=0; i<al_get_count( events ); i++ )
	{
		event_add_handler( (event_t *)al_get( events, i ) );
	}
	
}

int function_exists( const wchar_t *cmd )
{
	
	CHECK( cmd, 0 );
	
	if( parser_is_reserved(cmd) )
		return 0;
	
	load( cmd );
	return (hash_get(&function, cmd) != 0 );
}

void function_remove( const wchar_t *name )
{
	void *key;
	void *dv;
	function_data_t *d;
	event_t ev;
	
	CHECK( name, );

	hash_remove( &function,
				 name,
				 &key,
				 &dv );

	d=(function_data_t *)dv;
	
	if( !key )
		return;

	ev.type=EVENT_ANY;
	ev.function_name=name;	
	event_remove( &ev );

	clear_function_entry( key, d );

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
	
const wchar_t *function_get_definition( const wchar_t *argv )
{
	function_data_t *data;
	
	CHECK( argv, 0 );
	
	load( argv );
	data = (function_data_t *)hash_get( &function, argv );
	if( data == 0 )
		return 0;
	return data->cmd;
}
	
const wchar_t *function_get_desc( const wchar_t *argv )
{
	function_data_t *data;
	
	CHECK( argv, 0 );
		
	load( argv );
	data = (function_data_t *)hash_get( &function, argv );
	if( data == 0 )
		return 0;
	
	return _(data->desc);
}

void function_set_desc( const wchar_t *name, const wchar_t *desc )
{
	function_data_t *data;
	
	CHECK( name, );
	CHECK( desc, );
	
	load( name );
	data = (function_data_t *)hash_get( &function, name );
	if( data == 0 )
		return;

	data->desc =wcsdup(desc);
}

/**
   Search arraylist of strings for specified string
*/
static int al_contains_str( array_list_t *list, const wchar_t * str )
{
	int i;
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
	function_data_t *f = (function_data_t *)val;
	
	if( name[0] != L'_' && !f->is_binding && !al_contains_str( (array_list_t *)aux, name ) )
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

const wchar_t *function_get_definition_file( const wchar_t *argv )
{
	function_data_t *data;

	CHECK( argv, 0 );
		
	load( argv );
	data = (function_data_t *)hash_get( &function, argv );
	if( data == 0 )
		return 0;
	
	return data->definition_file;
}


int function_get_definition_offset( const wchar_t *argv )
{
	function_data_t *data;

	CHECK( argv, -1 );
		
	load( argv );
	data = (function_data_t *)hash_get( &function, argv );
	if( data == 0 )
		return -1;
	
	return data->definition_offset;
}

