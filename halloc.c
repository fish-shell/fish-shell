/** \file halloc.c

    A hierarchical memory allocation system. Works just like talloc
	used in Samba, except that an arbitrary block allocated with
	malloc() can be registered to be freed by halloc_free.

*/

#include "config.h"


#include <stdlib.h>
#include <unistd.h>

#include "fallback.h"
#include "util.h"

#include "common.h"
#include "halloc.h"

/**
   Extra size to allocate whenever doing a halloc, in order to fill uyp smaller halloc calls
*/
#define HALLOC_BLOCK_SIZE 128

/**
   Maximum size of trailing halloc space to refuse to discard. This is
   set to be larger on 64-bit platforms, since we don't want to get
   'stuck' with an unusably small slice of memory, and what is
   unusably small often depends on pointer size.
*/
#define HALLOC_SCRAP_SIZE (4*sizeof(void *))

#ifdef HALLOC_DEBUG
/**
   Debug statistic parameter
*/
static int child_count=0;
/**
   Debug statistic parameter
*/
static int child_size=0;
/**
   Debug statistic parameter
*/
static int alloc_count =0;
/**
   Debug statistic parameter
*/
static int alloc_spill = 0;
/**
   Debug statistic parameter
*/
static pid_t pid=0;
/**
   Debug statistic parameter
*/
static int parent_count=0;
#endif

/**
   The main datastructure for a main halloc context
*/
typedef struct halloc
{
	/**
	   List of all addresses and functions to call on them
	*/
	array_list_t children;
	/**
	   Memory scratch area used to fullfil smaller memory allocations
	*/
	char *scratch;
	/**
	   Amount of free space in the scratch area
	*/
	ssize_t scratch_free;
}
	halloc_t;

/**
   Allign the specified pointer
 */
static char *align_ptr( char *in )
{
	unsigned long step = maxi(sizeof(double),sizeof(void *));
	unsigned long inc = step-1;
	unsigned long long_in = (long)in;
	unsigned long long_out = ((long_in+inc)/step)*step;
	return (char *)long_out;
}

/**
   Allign specifies size_t
 */
static size_t align_sz( size_t in )
{
	size_t step = maxi(sizeof(double),sizeof(void *));
	size_t inc = step-1;
	return ((in+inc)/step)*step;
}

/**
   Get the offset of the halloc structure before a data block
*/
static halloc_t *halloc_from_data( void *data )
{
	return (halloc_t *)(((char *)data) - align_sz(sizeof( halloc_t ) ));
}

/**
   A function that does nothing
*/
static void late_free( void *data)
{
}

#ifdef HALLOC_DEBUG
/**
   Debug function, called at exit when in debug mode. Prints usage
   statistics, like number of allocations and number of internal calls
   to malloc.
*/
static void halloc_report()
{
	if( getpid() == pid )
	{
		debug( 1, L"%d parents, %d children with average child size of %.2f bytes caused %d allocs, average spill of %.2f bytes", 
			   parent_count, child_count, (double)child_size/child_count,
			   parent_count+alloc_count, (double)alloc_spill/(parent_count+alloc_count) );				
	}
}
#endif


void *halloc( void *context, size_t size )
{	
	halloc_t *me, *parent;
	if( context )
	{
		char *res;
		char *aligned;
		
#ifdef HALLOC_DEBUG
			
		if( !child_count )
		{
			pid = getpid();
			atexit( &halloc_report );
		}
		 
		child_count++;
		child_size += size;
#endif	
		parent = halloc_from_data( context );

		/*
		  Align memory address 
		*/
		aligned = align_ptr( parent->scratch );

		parent->scratch_free -= (aligned-parent->scratch);

		if( parent->scratch_free < 0 )
			parent->scratch_free=0;
		
		parent->scratch = aligned;

		if( size <= parent->scratch_free )
		{
			res = parent->scratch;
			parent->scratch_free -= size;
			parent->scratch = ((char *)parent->scratch)+size;
		}
		else
		{

#ifdef HALLOC_DEBUG
			alloc_count++;
#endif	
		
			if( parent->scratch_free < HALLOC_SCRAP_SIZE )
			{
#ifdef HALLOC_DEBUG
				alloc_spill += parent->scratch_free;
#endif
				res = calloc( 1, size + HALLOC_BLOCK_SIZE );
				if( !res )
					DIE_MEM();
				parent->scratch = (char *)res + size;
				parent->scratch_free = HALLOC_BLOCK_SIZE;
			}
			else
			{
				res = calloc( 1, size );
				if( !res )
					DIE_MEM();
			}
			al_push_func( &parent->children, &late_free );
			al_push( &parent->children, res );
			
		}
		return res;
	
	}
	else
	{
		me = (halloc_t *)calloc( 1, align_sz(sizeof(halloc_t)) + align_sz(size) + HALLOC_BLOCK_SIZE );
		
		if( !me )
			DIE_MEM();
#ifdef HALLOC_DEBUG
		parent_count++;
#endif		
		me->scratch = ((char *)me) + align_sz(sizeof(halloc_t)) + align_sz(size);
		me->scratch_free = HALLOC_BLOCK_SIZE;
		
		al_init( &me->children );
		return ((char *)me) + align_sz(sizeof(halloc_t));
	}
}

void halloc_register_function( void *context, void (*func)(void *), void *data )
{
	halloc_t *me;
	if( !context )
		return;
	
	me = halloc_from_data( context );
	al_push_func( &me->children, func );
	al_push( &me->children, data );
}

void halloc_free( void *context )
{
	halloc_t *me;
	int i;
	
	if( !context )
		return;

	
	me = halloc_from_data( context );

#ifdef HALLOC_DEBUG
	alloc_spill += me->scratch_free;
#endif
	for( i=0; i<al_get_count(&me->children); i+=2 )
	{
		void (*func)(void *) = (void (*)(void *))al_get( &me->children, i );
		void * data = (void *)al_get( &me->children, i+1 );
		if( func != &late_free )
			func( data );
	}
	for( i=0; i<al_get_count(&me->children); i+=2 )
	{
		void (*func)(void *) = (void (*)(void *))al_get_func( &me->children, i );
		void * data = (void *)al_get( &me->children, i+1 );
		if( func == &late_free )
			free( data );
	}
	al_destroy( &me->children );
	free(me);
}
