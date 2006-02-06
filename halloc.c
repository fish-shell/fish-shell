/** \file halloc.c

    A hierarchical memory allocation system. Works just like talloc
	used in Samba, except that an arbitrary block allocated with
	malloc() can be registered to be freed by halloc_free.

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
	array_list_t hchildren;
	long long data[0];
}
	halloc_t;

static halloc_t *halloc_from_data( void *data )
{
	return (halloc_t *)(data - sizeof( halloc_t ) );
}


void *halloc( void *context, size_t size )
{
	
	halloc_t *me, *parent;
	
	me = (halloc_t *)calloc( 1, sizeof(halloc_t) + size );

	if( !me )
		return 0;

	al_init( &me->children );

	if( context )
	{		
		parent = halloc_from_data( context );
		al_push( &parent->hchildren, &me->data );
	}
		
	return &me->data;
}

void *halloc_register( void *context, void *data )
{
	halloc_t *me;
	if( !context )
		return data;
	
	me = halloc_from_data( context );
	al_push( &me->children, data );
	return data;
}


void halloc_free( void *context )
{
	halloc_t *me;
	if( !context )
		return;

	me = halloc_from_data( context );
	al_foreach( &me->hchildren, (void (*)(const void *))&halloc_free );
	al_foreach( &me->children, (void (*)(const void *))&free );
	al_destroy( &me->children );
	al_destroy( &me->hchildren );
	free(me);
}

