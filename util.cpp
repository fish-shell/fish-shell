/** \file util.c
	Generic utilities library.

	Contains datastructures such as automatically growing array lists, priority queues, etc.
*/

#include "config.h"


#include <stdio.h>
#include <stdlib.h>
#include <wchar.h>
#include <math.h>
#include <sys/time.h>
#include <stdarg.h>
#include <string.h>
#include <ctype.h>
#include <wctype.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <errno.h>
#include <assert.h>

#include "fallback.h"
#include "util.h"

#include "common.h"
#include "wutil.h"

/**
   Minimum allocated size for data structures. Used to avoid excessive
   memory allocations for lists, hash tables, etc, which are nearly
   empty.
*/
#define MIN_SIZE 32

/**
   Maximum number of characters that can be inserted using a single
   call to sb_printf. This is needed since vswprintf doesn't tell us
   what went wrong. We don't know if we ran out of space or something
   else went wrong. We assume that any error is an out of memory-error
   and try again until we reach this size.  After this size has been
   reached, it is instead assumed that something was wrong with the
   format string.
*/
#define SB_MAX_SIZE (128*1024*1024)

/**
   Handle oom condition. Default action is to print a stack trace and
   exit, but an alternative action can be specified.
 */
#define oom_handler( p )						\
	{											\
		if( oom_handler_internal == util_die_on_oom )	\
		{												\
			DIE_MEM();									\
		}												\
		oom_handler_internal( p );						\
	}													\
		


void util_die_on_oom( void * p);

void (*oom_handler_internal)(void *) = &util_die_on_oom;

void (*util_set_oom_handler( void (*h)(void *) ))(void *)
{
	void (*old)(void *) = oom_handler_internal;
	
	if( h )
		oom_handler_internal = h;
	else
		oom_handler_internal = &util_die_on_oom;

	return old;
}

void util_die_on_oom( void *p )
{
}

int mini( int a,
		  int b )
{
	return a<b?a:b;
}


int maxi( int a,
		  int b )
{
	return a>b?a:b;
}

int wcsfilecmp( const wchar_t *a, const wchar_t *b )
{
	CHECK( a, 0 );
	CHECK( b, 0 );
	
	if( *a==0 )
	{
		if( *b==0)
			return 0;
		return -1;
	}
	if( *b==0 )
	{
		return 1;
	}

	int secondary_diff=0;
	if( iswdigit( *a ) && iswdigit( *b ) )
	{
		wchar_t *aend, *bend;
		long al;
		long bl;
		int diff;

		errno = 0;		
		al = wcstol( a, &aend, 10 );
		bl = wcstol( b, &bend, 10 );

		if( errno )
		{
			/*
			  Huuuuuuuuge numbers - fall back to regular string comparison
			*/
			return wcscmp( a, b );
		}
		
		diff = al - bl;
		if( diff )
			return diff>0?2:-2;

		secondary_diff = (aend-a) - (bend-b);

		a=aend-1;
		b=bend-1;
	}
	else
	{
		int diff = towlower(*a) - towlower(*b);
		if( diff != 0 )
			return (diff>0)?2:-2;

		secondary_diff = *a-*b;
	}

	int res = wcsfilecmp( a+1, b+1 );

	if( abs(res) < 2 )
	{
		/*
		  No primary difference in rest of string.
		  Use secondary difference on this element if found.
		*/
		if( secondary_diff )
		{
			return secondary_diff>0?1:-1;
		}
	}
	
	return res;	
}

void b_init( buffer_t *b)
{
	CHECK( b, );
	memset( b,0,sizeof(buffer_t));
}



void b_destroy( buffer_t *b )
{
	CHECK( b, );
	free( b->buff );
}


int b_append( buffer_t *b, const void *d, ssize_t len )
{
	if( len<=0 )
		return 0;

	CHECK( b, -1 );	

	if( !b )
	{
		return 0;
	}

	if( !d )
	{
		return 0;
	}

	if( b->length <= (b->used + len) )
	{
		size_t l = maxi( b->length*2,
						 b->used+len+MIN_SIZE );
		
		void *d = realloc( b->buff, l );
		if( !d )
		{
			oom_handler( b );
			return -1;			
		}
		b->buff=(char *)d;
		b->length = l;
	}
	memcpy( ((char*)b->buff)+b->used,
			d,
			len );

//	fwprintf( stderr, L"Copy %s, new value %s\n", d, b->buff );
	b->used+=len;

	return 1;
}

long long get_time()
{
	struct timeval time_struct;
	gettimeofday( &time_struct, 0 );
	return 1000000ll*time_struct.tv_sec+time_struct.tv_usec;
}

