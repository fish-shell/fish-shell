/** \file halloc.c
	A hierarchical memory allocation system
*/

#include "config.h"

#include <stdlib.h>
#include <unistd.h>

#include "util.h"
#include "common.h"
#include "halloc.h"

typedef struct halloc
{
	array_list_t children;
	long long data[0];
}
	halloc_t;

static halloc_t *halloc_from_data( void *data )
{
	return (halloc_t *)(data - sizeof( halloc_t ) );
}


void *halloc( void *context, size_t size )
{
	halloc_t *res = (halloc_t *)calloc( 1, sizeof(halloc_t) + size );
	if( !res )
		return 0;
	
	al_init( &res->children );
	
	if( context )
	{
		halloc_t *parent = halloc_from_data( context );
		al_push( &parent->children, &res->data );
	}
	return &res->data;
}

void halloc_free( void *context )
{
	halloc_t *me;
	if( !context )
		return;
	
	me = halloc_from_data( context );
	al_foreach( &me->children, (void (*)(const void *))&halloc_free );
	al_destroy( &me->children );
	free(me);
}

wchar_t *halloc_wcsdup( void *context, const wchar_t *in )
{
	size_t len=wcslen(in);
	wchar_t *out = halloc( context, sizeof( wchar_t)*(len+1));
	if( out == 0 )
	{
		die_mem();
	}

	memcpy( out, in, sizeof( wchar_t)*(len+1));
	return out;
}

wchar_t *halloc_wcsndup( void *context, const wchar_t *in, int c )
{
	wchar_t *res = halloc( context, sizeof(wchar_t)*(c+1) );
	if( res == 0 )
	{
		die_mem();
	}
	wcsncpy( res, in, c );
	res[c] = L'\0';	
	return res;	
}
