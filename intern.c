/** \file intern.c

    Library for pooling common strings

*/
#include "config.h"

#include <stdlib.h>
#include <stdio.h>
#include <wchar.h>
#include <unistd.h>

#include "util.h"
#include "common.h"
#include "intern.h"

hash_table_t *intern_table=0;
hash_table_t *intern_static_table=0;

/**
   Load static strings that are universally common. Currently only loads the empty string.
*/
static void intern_load_common_static()
{
	intern_static( L"" );
}

const wchar_t *intern( const wchar_t *in )
{
	const wchar_t *res=0;
	
	if( !in )
		return 0;
	
	intern_load_common_static();
	

	if( !intern_table )
	{
		intern_table = malloc( sizeof( hash_table_t ) );
		if( !intern_table )
		{
			die_mem();
		}
		hash_init( intern_table, &hash_wcs_func, &hash_wcs_cmp );
	}
	
	if( intern_static_table )
	{
		res = hash_get( intern_static_table, in );
	}
	
	if( !res )
	{
		res = hash_get( intern_table, in );
		
		if( !res )
		{
			res = wcsdup( in );
			if( !res )
			{
				die_mem();
			}
			
			hash_put( intern_table, res, res );
		}
	}
	
	return res;
}

const wchar_t *intern_static( const wchar_t *in )
{
	const wchar_t *res=0;
	
	if( !in )
		return 0;
	
	if( !intern_static_table )
	{
		intern_static_table = malloc( sizeof( hash_table_t ) );
		if( !intern_static_table )
		{
			die_mem();			
		}
		hash_init( intern_static_table, &hash_wcs_func, &hash_wcs_cmp );
	}
	
	res = hash_get( intern_static_table, in );
	
	if( !res )
	{
		res = in;
		hash_put( intern_static_table, res, res );
	}
	
	return res;
}

/**
   Free the specified key/value pair. Should only be called by intern_free_all at shutdown
*/
static void clear_value( const void *key, const void *data )
{
	debug( 3,  L"interned string: '%ls'", data );	
	free( (void *)data );
}

void intern_free_all()
{
	if( intern_table )
	{
		hash_foreach( intern_table, &clear_value );		
		hash_destroy( intern_table );		
		free( intern_table );
		intern_table=0;		
	}

	if( intern_static_table )
	{
		hash_destroy( intern_static_table );		
		free( intern_static_table );
		intern_static_table=0;		
	}
	
}
