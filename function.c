/** \file function.c
  Functions for storing and retrieving function information.
*/
#include <stdlib.h>
#include <stdio.h>
#include <wchar.h>
#include <unistd.h>
#include <termios.h>
#include <signal.h>

#include "config.h"
#include "wutil.h"
#include "util.h"
#include "function.h"
#include "proc.h"
#include "parser.h"
#include "common.h"
#include "intern.h"
#include "event.h"
#include "reader.h"


/**
   Struct describing a function
*/
typedef struct
{
	/** Function definition */
	wchar_t *cmd;
	/** Function description */
	wchar_t *desc;	
	const wchar_t *definition_file;
	int definition_offset;	
	int is_binding;
}
	function_data_t;

/**
   Table containing all functions
*/
static hash_table_t function;

/**
   Free all contents of an entry to the function hash table
*/
static void clear_function_entry( const void *key, 
							   const void *data )
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

static int count_lineno( const wchar_t *str, int len )
{
	int res = 0;
	int i;
	for( i=0; i<len; i++ )
	{
		if( str[i] == L'\n' )
			res++;
	}
	return res;
}


void function_add( const wchar_t *name, 
				   const wchar_t *val,
				   const wchar_t *desc,
				   array_list_t *events,
				   int is_binding )
{
	int i;
	wchar_t *cmd_end;
	

	if( function_exists( name ) )
		function_remove( name );
	

	
	function_data_t *d = malloc( sizeof( function_data_t ) );
	d->definition_offset = count_lineno( parser_get_buffer(), current_block->tok_pos );
	d->cmd = wcsdup( val );
	cmd_end = d->cmd + wcslen(d->cmd)-1;
	while( (cmd_end>d->cmd) && wcschr( L"\n\r\t ", *cmd_end ) )
	{
		*cmd_end-- = 0;
	}
	
	d->desc = desc?wcsdup( desc ):0;
	d->is_binding = is_binding;
	d->definition_file = reader_current_filename()?intern(reader_current_filename()):0;
	hash_put( &function, intern(name), d );
	
	for( i=0; i<al_get_count( events ); i++ )
	{
		event_add_handler( (event_t *)al_get( events, i ) );
	}
	
}

int function_exists( const wchar_t *cmd )
{
	return (hash_get(&function, cmd) != 0 );
}

void function_remove( const wchar_t *name )
{
	void *key;
	function_data_t *d;

	event_t ev;
	ev.type=EVENT_ANY;
	ev.function_name=name;	
	event_remove( &ev );

	hash_remove( &function,
				 name,
				 (const void **) &key,
				 (const void **)&d );

	if( !d )
		return;

	clear_function_entry( key, d );
}
	
const wchar_t *function_get_definition( const wchar_t *argv )
{
	function_data_t *data = 
		(function_data_t *)hash_get( &function, argv );
	if( data == 0 )
		return 0;
	return data->cmd;
}
	
const wchar_t *function_get_desc( const wchar_t *argv )
{
	function_data_t *data = 
		(function_data_t *)hash_get( &function, argv );
	if( data == 0 )
		return 0;
	
	return data->desc;
}

void function_set_desc( const wchar_t *name, const wchar_t *desc )
{
	function_data_t *data = 
		(function_data_t *)hash_get( &function, name );
	if( data == 0 )
		return;

	data->desc =wcsdup(desc);
}

/**
   Helper function for removing hidden functions 
*/
static void get_names_internal( const void *key,
								const void *val,
								void *aux )
{
	wchar_t *name = (wchar_t *)key;
	function_data_t *f = (function_data_t *)val;
	
	if( name[0] != L'_' && !f->is_binding)
		al_push( (array_list_t *)aux, name );
}

void function_get_names( array_list_t *list, int get_hidden )
{
	if( get_hidden )
		hash_get_keys( &function, list );
	else
		hash_foreach2( &function, &get_names_internal, list );
	
}

const wchar_t *function_get_definition_file( const wchar_t *argv )
{
	function_data_t *data = 
		(function_data_t *)hash_get( &function, argv );
	if( data == 0 )
		return 0;
	
	return data->definition_file;
}


int function_get_definition_offset( const wchar_t *argv )
{
	function_data_t *data = 
		(function_data_t *)hash_get( &function, argv );
	if( data == 0 )
		return -1;
	
	return data->definition_offset;
}



