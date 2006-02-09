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
		al_push( &parent->children, &halloc_free );
		al_push( &parent->children, &me->data );
	}
	
	return &me->data;
}

void halloc_register_function( void *context, void (*func)(void *), void *data )
{
	halloc_t *me;
	if( !context )
		return;
	
	me = halloc_from_data( context );
	al_push( &me->children, func );
	al_push( &me->children, data );
}

void halloc_free( void *context )
{
	halloc_t *me;
	int i;
	
	if( !context )
		return;
	
	me = halloc_from_data( context );
	for( i=0; i<al_get_count(&me->children); i+=2 )
	{
		void (*func)(void *) = (void (*)(void *))al_get( &me->children, i );
		void * data = (void *)al_get( &me->children, i+1 );
		func( data );
	}
	al_destroy( &me->children );
	free(me);
}
