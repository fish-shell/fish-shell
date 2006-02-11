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

#define HALLOC_BLOCK_SIZE 256
#define HALLOC_SCRAP_SIZE 16

static int child_count=0;
static int child_size=0;
static int alloc_count =0;
static int alloc_spill = 0;
static pid_t pid=0;
static int parent_count=0;


typedef struct halloc
{
	array_list_t children;
	void *scratch;
	size_t scratch_free;
	long long data[0];
}
	halloc_t;

static halloc_t *halloc_from_data( void *data )
{
	return (halloc_t *)(data - sizeof( halloc_t ) );
}

static void late_free( void *data)
{
}

static void woot()
{
	if( getpid() == pid )
	{
		debug( 1, L"%d parents, %d children with average child size of %.2f bytes caused %d allocs, average spill of %.2f bytes", 
			   parent_count, child_count, (double)child_size/child_count,
			   parent_count+alloc_count, (double)alloc_spill/(parent_count+alloc_count) );				
	}
}

void *halloc( void *context, size_t size )
{	
	halloc_t *me, *parent;
	if( context )
	{
		void *res;
		
			
		if( !child_count )
		{
			pid = getpid();
			atexit( woot );
		}
		
		child_count++;
		child_size += size;
		
		parent = halloc_from_data( context );
		if( size <= parent->scratch_free )
		{
			res = parent->scratch;
			parent->scratch_free -= size;
			parent->scratch += size;
		}
		else
		{
			alloc_count++;
			
			if( parent->scratch_free < HALLOC_SCRAP_SIZE )
			{
				alloc_spill += parent->scratch_free;
				res = calloc( 1, size + HALLOC_BLOCK_SIZE );
				parent->scratch = res + size;
				parent->scratch_free = HALLOC_BLOCK_SIZE;
			}
			else
			{
				res = calloc( 1, size );
			}
			al_push( &parent->children, &late_free );
			al_push( &parent->children, res );
			
		}
		return res;
	
	}
	else
	{
		me = (halloc_t *)calloc( 1, sizeof(halloc_t) + size + HALLOC_BLOCK_SIZE );
		
		if( !me )
			return 0;
		parent_count++;
		
		me->scratch = ((void *)me) + sizeof(halloc_t) + size;
		me->scratch_free = HALLOC_BLOCK_SIZE;
		
		al_init( &me->children );
		return &me->data;
	}
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

	alloc_spill += me->scratch_free;

	for( i=0; i<al_get_count(&me->children); i+=2 )
	{
		void (*func)(void *) = (void (*)(void *))al_get( &me->children, i );
		void * data = (void *)al_get( &me->children, i+1 );
		if( func != &late_free )
			func( data );
	}
	for( i=0; i<al_get_count(&me->children); i+=2 )
	{
		void (*func)(void *) = (void (*)(void *))al_get( &me->children, i );
		void * data = (void *)al_get( &me->children, i+1 );
		if( func == &late_free )
			free( data );
	}
	al_destroy( &me->children );
	free(me);
}
