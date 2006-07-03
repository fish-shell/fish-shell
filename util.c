/** \file util.c
	Generic utilities library.

	Contains datastructures such as hash tables, automatically growing array lists, priority queues, etc.
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
   Minimum size for hash tables
*/
#define HASH_MIN_SIZE 7

/**
   Maximum number of characters that can be inserted using a single
   call to sb_printf. This is needed since vswprintf doesn't tell us
   what went wrong. We don't know if we ran out of space or something
   else went wrong. Therefore we assume that any error is an out of
   memory-error and try again until we reach this size.
*/
#define SB_MAX_SIZE 32767

float minf( float a,
			float b )
{
	return a<b?a:b;
}


float maxf( float a,
			float b )
{
	return a>b?a:b;
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


/* Queue functions */


void q_init( dyn_queue_t *q )
{
	q->start = (void **)malloc( sizeof(void*)*1 );
	q->stop = &q->start[1];
	q->put_pos = q->get_pos = q->start;
}

void q_destroy( dyn_queue_t *q )
{
	free( q->start );
}

/*
  static q_print( dyn_queue_t *q )
  {
  int i;
  int size = (q->stop-q->start);

  printf( "Storlek: %d\n", size );
  for( i=0; i< size; i++ )
  {
  printf( " %c%c %d: %d\n",
  &q->start[i]==q->get_pos?'g':' ',
  &q->start[i]==q->put_pos?'p':' ',
  i,
  q->start[i] );
  }
  }

*/

/**
   Reallocate the queue_t
*/
static int q_realloc( dyn_queue_t *q )
{
	void **old_start = q->start;
	void **old_stop = q->stop;
	int diff;
	int new_size;
	
	new_size = 2*(q->stop-q->start);
	
	q->start=(void**)realloc( q->start, sizeof(void*)*new_size );
	if( q->start == 0 )
	{
		q->start = old_start;
		return 0;
	}
	
	diff = q->start - old_start;
	q->get_pos += diff;
	q->stop = &q->start[new_size];
	memcpy( old_stop + diff, q->start, sizeof(void*)*(q->get_pos-q->start));
	q->put_pos = old_stop + diff + (q->get_pos-q->start);
	
	return 1;
}

int q_put( dyn_queue_t *q, void *e )
{
	*q->put_pos = e;

//	fprintf( stderr, "Put element %d to queue %d\n", e, q );
	
	if( ++q->put_pos == q->stop )
		q->put_pos = q->start;
	if( q->put_pos == q->get_pos )
		return q_realloc( q );
	return 1;
}

void *q_get( dyn_queue_t *q)
{
	void *e = *q->get_pos;
	if( ++q->get_pos == q->stop )
		q->get_pos = q->start;
	return e;
}

void *q_peek( dyn_queue_t *q )
{
	return *q->get_pos;
}
 
int q_empty( dyn_queue_t *q )
{
//	fprintf( stderr, "Queue %d is %s\n", q, (q->put_pos == q->get_pos)?"empty":"non-empty" );
	return q->put_pos == q->get_pos;
}


/* Hash table functions */

void hash_init2( hash_table_t *h,
				int (*hash_func)(void *key),
				 int (*compare_func)(void *key1, void *key2),
				 size_t capacity)
{
	int i;
	size_t sz = capacity*4/3+1;
	
	h->arr = malloc( sizeof(hash_struct_t)*sz );
	h->size = sz;
	for( i=0; i< sz; i++ )
		h->arr[i].key = 0;
	h->count=0;
	h->hash_func = hash_func;
	h->compare_func = compare_func;
}

void hash_init( hash_table_t *h,
				int (*hash_func)(void *key),
				int (*compare_func)(void *key1, void *key2) )
{
	h->arr = 0;
	h->size = 0;
	h->count=0;
	h->hash_func = hash_func;
	h->compare_func = compare_func;
}


void hash_destroy( hash_table_t *h )
{
	free( h->arr );
}

/**
   Search for the specified hash key in the table
   \return index in the table, or to the first free index if the key is not in the table
*/
static int hash_search( hash_table_t *h,
						void *key )
{
	int hv;
	int pos;

	hv = h->hash_func( key );
	pos = abs(hv) % h->size;
	while(1)
	{
		if( (h->arr[pos].key == 0 ) ||
			( h->compare_func( h->arr[pos].key, key ) ) )
		{
			return pos;
		}
		pos++;
		pos %= h->size;
	}
}

/**
   Reallocate the hash array. This is quite expensive, as every single entry has to be rehashed and moved.
*/
static int hash_realloc( hash_table_t *h,
						 int sz )
{

	/* Avoid reallocating when using pathetically small tables */
	if( ( sz < h->size ) && (h->size < HASH_MIN_SIZE))
		return 1;
	sz = maxi( sz, HASH_MIN_SIZE );
	
	hash_struct_t *old_arr = h->arr;
	int old_size = h->size;
	
	int i;

	h->arr = malloc( sizeof( hash_struct_t) * sz );
	if( h->arr == 0 )
	{
		h->arr = old_arr;
		return 0;
	}

	memset( h->arr,
			0,
			sizeof( hash_struct_t) * sz );
	h->size = sz;

	for( i=0; i<old_size; i++ )
	{
		if( old_arr[i].key != 0 )
		{
			int pos = hash_search( h, old_arr[i].key );
			h->arr[pos].key = old_arr[i].key;
			h->arr[pos].data = old_arr[i].data;
		}
	}
	free( old_arr );

	return 1;
}


int hash_put( hash_table_t *h,
			  const void *key,
			  const void *data )
{
	int pos;
	
	if( (float)(h->count+1)/h->size > 0.75f )
	{
		if( !hash_realloc( h, (h->size+1) * 2 -1 ) )
		{
			return 0;
		}
	}

	pos = hash_search( h, (void *)key );

	if( h->arr[pos].key == 0 )
	{
		h->count++;
	}

	h->arr[pos].key = (void *)key;
	h->arr[pos].data = (void *)data;
	return 1;
}

void *hash_get( hash_table_t *h,
				const void *key )
{
	if( !h->count )
		return 0;
	
	int pos = hash_search( h, (void *)key );	
	if( h->arr[pos].key == 0 )
		return 0;
	else
		return h->arr[pos].data;
}

void *hash_get_key( hash_table_t *h,
					const void *key )
{	
	if( !h->count )
		return 0;
	
	int pos = hash_search( h, (void *)key );
	if( h->arr[pos].key == 0 )
		return 0;
	else
		return h->arr[pos].key;
}

int hash_get_count( hash_table_t *h)
{
	return h->count;
}

void hash_remove( hash_table_t *h,
				  const void *key,
				  void **old_key,
				  void **old_val )
{
	if( !h->count )
	{

		if( old_key != 0 )
			*old_key = 0;
		if( old_val != 0 )
			*old_val = 0;
		return;
	}

	int pos = hash_search( h, (void *)key );
	int next_pos;

	if( h->arr[pos].key == 0 )
	{

		if( old_key != 0 )
			*old_key = 0;
		if( old_val != 0 )
			*old_val = 0;
		return;
	}

	h->count--;

	if( old_key != 0 )
		*old_key = h->arr[pos].key;
	if( old_val != 0 )
		*old_val = h->arr[pos].data;

	h->arr[pos].key = 0;

	next_pos = pos+1;
	next_pos %= h->size;

	while( h->arr[next_pos].key != 0 )
	{

		int hv = h->hash_func( h->arr[next_pos].key );
		int ideal_pos = abs( hv ) % h->size;
		int dist_old = (next_pos - ideal_pos + h->size)%h->size;
		int dist_new = (pos - ideal_pos + h->size)%h->size;
		if ( dist_new < dist_old )
		{
			h->arr[pos].key = h->arr[next_pos].key;
			h->arr[pos].data = h->arr[next_pos].data;
			h->arr[next_pos].key = 0;
			pos = next_pos;
		}
		next_pos++;

		next_pos %= h->size;

	}

	if( (float)(h->count+1)/h->size < 0.2f && h->count < 63 )
	{
		hash_realloc( h, (h->size+1) / 2 -1 );
	}

	return;
}

int hash_contains( hash_table_t *h,
				   const void *key )
{
	if( !h->count )
		return 0;
	
	int pos = hash_search( h, (void *)key );
	return h->arr[pos].key != 0;
}

/**
   Push hash value into array_list_t
*/
static void hash_put_data( void *key,
						   void *data,
						   void *al )
{
	al_push( (array_list_t *)al,
			 data );
}


void hash_get_data( hash_table_t *h,
					array_list_t *arr )
{
	hash_foreach2( h, &hash_put_data, arr );
}

/**
   Push hash key into array_list_t
*/
static void hash_put_key( void *key, void *data, void *al )
{
	al_push( (array_list_t *)al, key );
}


void hash_get_keys( hash_table_t *h,
					array_list_t *arr )
{
	hash_foreach2( h, &hash_put_key, arr );
}

void hash_foreach( hash_table_t *h,
				   void (*func)( void *, void *) )
{
	int i;
	for( i=0; i<h->size; i++ )
	{
		if( h->arr[i].key != 0 )
		{
			func( h->arr[i].key, h->arr[i].data );
		}
	}
}

void hash_foreach2( hash_table_t *h,
					void (*func)( void *, void *, void * ),
					void *aux )
{
	int i;
	for( i=0; i<h->size; i++ )
	{
		if( h->arr[i].key != 0 )
		{
			func( h->arr[i].key, h->arr[i].data, aux );
		}
	}
}


int hash_str_cmp( void *a, void *b )
{
	return strcmp((char *)a,(char *)b) == 0;
}

/**
   Helper function for hash_wcs_func
*/
static uint rotl5( uint in )
{
	return (in<<5|in>>27);
}


int hash_str_func( void *data )
{
	int res = 0x67452301u;
	const char *str = data;	

	while( *str )
		res = (18499*rotl5(res)) ^ *str++;
	
	return res;
}

int hash_wcs_func( void *data )
{
	int res = 0x67452301u;
	const wchar_t *str = data;	

	while( *str )
		res = (18499*rotl5(res)) ^ *str++;
	
	return res;
}


int hash_wcs_cmp( void *a, void *b )
{
	return wcscmp((wchar_t *)a,(wchar_t *)b) == 0;
}

int hash_ptr_func( void *data )
{
	return (int)(long) data;
}

/**
   Hash comparison function suitable for direct pointer comparison
*/
int hash_ptr_cmp( void *a,
                  void *b )
{
	return a == b;
}

void pq_init( priority_queue_t *q,
			  int (*compare)(void *e1, void *e2) )
{
	q->arr=0;
	q->size=0;
	q->count=0;
	q->compare = compare;
}

/**
   Check that the priority queue is in a valid state
*/
/*
  static void pq_check( priority_queue_t *q, int i )
  {
  int l,r;
  if( q->count <= i )
  return;

  l=i*2+1;
  r=i*2+2;


  if( (q->count > l) && (q->compare(q->arr[i], q->arr[l]) < 0) )
  {
  printf( "ERROR: Place %d less than %d\n", i, l );
  }
  if( (q->count > r) && (q->compare(q->arr[i], q->arr[r]) < 0) )
  {
  printf( "ERROR: Place %d less than %d\n", i, r );
  }
  pq_check( q, l );
  pq_check( q, r );
  }
*/

int pq_put( priority_queue_t *q,
			void *e )
{
	int i;

	if( q->size == q->count )
	{
		void **old_arr = q->arr;
		int old_size = q->size;
		q->size = maxi( 4, 2*q->size );
		q->arr = (void **)realloc( q->arr, sizeof(void*)*q->size );
		if( q->arr == 0 )
		{
			q->arr = old_arr;
			q->size = old_size;
			return 0;
		}
	}

	i = q->count;
	while( (i>0) && (q->compare( q->arr[(i-1)/2], e )<0 ) )
	{
		q->arr[i] = q->arr[(i-1)/2];
		i = (i-1)/2;
	}
	q->arr[i]=e;

	q->count++;

	return 1;

}

/**
   Make a valid head
*/
static void pq_heapify( priority_queue_t *q, int i )
{
	int l, r, largest;
	l = 2*(i)+1;
	r = 2*(i)+2;
	if( (l < q->count) && (q->compare(q->arr[l],q->arr[i])>0) )
	{
		largest = l;
	}
	else
	{
		largest = i;
	}
	if( (r < q->count) && (q->compare( q->arr[r],q->arr[largest])>0) )
	{
		largest = r;
	}

	if( largest != i )
	{
		void *tmp = q->arr[largest];
		q->arr[largest]=q->arr[i];
		q->arr[i]=tmp;
		pq_heapify( q, largest );
	}
}

void *pq_get( priority_queue_t *q )
{
	void *result = q->arr[0];
	q->arr[0] = q->arr[--q->count];
	pq_heapify( q, 0 );

/*	pq_check(q, 0 );	*/
/*	pq_print( q ); */

	return result;
}

void *pq_peek( priority_queue_t *q )
{
	return q->arr[0];
}

int pq_empty( priority_queue_t *q )
{
	return q->count == 0;
}

int pq_get_count( priority_queue_t *q )
{
	return q->count;
}

void pq_destroy(  priority_queue_t *q )
{
	free( q->arr );
}


array_list_t *al_new()
{
	array_list_t *res = malloc( sizeof( array_list_t ) );
	if( !res )
		DIE_MEM();
	al_init( res );
	return res;
}


void al_init( array_list_t *l )
{
	memset( l, 0, sizeof( array_list_t ) );
}

void al_destroy( array_list_t *l )
{
	free( l->arr );
}

int al_push( array_list_t *l, const void *o )
{
	if( l->pos >= l->size )
	{
		int new_size = l->pos == 0 ? MIN_SIZE : 2 * l->pos;
		void *tmp = realloc( l->arr, sizeof( void *)*new_size );
		if( tmp == 0 )
			return 0;
		l->arr = tmp;
		l->size = new_size;		
	}
	l->arr[l->pos++] = (void *)o;
	return 1;
}

int al_push_all( array_list_t *a, array_list_t *b )
{
	int k;
	for( k=0; k<al_get_count( b ); k++ )
	{
		if( !al_push( a, al_get( b, k ) ) )
			return 0;
	}
	return 1;
}


int al_set( array_list_t *l, int pos, const void *o )
{
	int old_pos;
	
	if( pos < 0 )
		return 0;
	if( pos < l->pos )
	{
		l->arr[pos] = (void *)o;
		return 1;
	}
	old_pos=l->pos;
	
	l->pos = pos;
	if( al_push( l, o ) )
	{
/*		fwprintf( stderr, L"Clearing from index %d to index %d\n", 
				  old_pos, pos );
*/	
		memset( &l->arr[old_pos], 
				0,
				sizeof(void *) * (pos - old_pos) );
		return 1;		
	}
	return 0;	
}

void *al_get( array_list_t *l, int pos )
{
	if( pos < 0 )
		return 0;
	if( pos >= l->pos )
		return 0;
	return l->arr[pos];
}

void al_truncate( array_list_t *l, int new_sz )
{
	l->pos = new_sz;
}

void *al_pop( array_list_t *l )
{
	void *e = l->arr[--l->pos];
	if( (l->pos*3 < l->size) && (l->size < MIN_SIZE) )
	{
		void ** old_arr = l->arr;
		int old_size = l->size;
		l->size = l->size/2;
		l->arr = realloc( l->arr, sizeof(void*)*l->size );
		if( l->arr == 0 )
		{
			l->arr = old_arr;
			l->size = old_size;
		}
	}
	return e;
}

void *al_peek( array_list_t *l )
{

	return l->pos>0?l->arr[l->pos-1]:0;
}

int al_empty( array_list_t *l )
{
	return l->pos == 0;
}

int al_get_count( array_list_t *l )

{
	return l->pos;
}

void al_foreach( array_list_t *l, void (*func)( void * ))
{
	int i;
	for( i=0; i<l->pos; i++ )
		func( l->arr[i] );
}

void al_foreach2( array_list_t *l, void (*func)( void *, void *), void *aux)
{
	int i;
	for( i=0; i<l->pos; i++ )
		func( l->arr[i], aux );
}

int wcsfilecmp( const wchar_t *a, const wchar_t *b )
{
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
		long al = wcstol( a, &aend, 10 );
		long bl = wcstol( b, &bend, 10 );
		int diff = al - bl;
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
	switch( abs(res) )
	{
		case 2:
			return res;
		default:
			if( secondary_diff )
				return secondary_diff>0?1:-1;
	}
	return 0;

}

void sb_init( string_buffer_t * b)
{
	wchar_t c=0;

	if( !b )
	{
		return;
	}

	memset( b, 0, sizeof(string_buffer_t) );	
	b_append( b, &c, sizeof( wchar_t));
	b->used -= sizeof(wchar_t);
}

string_buffer_t *sb_new()
{
	string_buffer_t *res = malloc( sizeof( string_buffer_t ) );
	if( !res )
		DIE_MEM();
	sb_init( res );
	return res;
}


void sb_append( string_buffer_t *b, const wchar_t * s)
{
//	fwprintf( stderr, L"Append string \'%ls\'\n", s );

	if( !s )
	{
		return;
	}
	
	if( !b )
	{
		return;
	}

	b_append( b, s, sizeof(wchar_t)*(wcslen(s)+1) );
	b->used -= sizeof(wchar_t);
}

void sb_append_substring( string_buffer_t *b, const wchar_t *s, size_t l )
{
    wchar_t tmp=0;

	if( !s )
	{
		return;
	}
	
	if( !b )
	{
		return;
	}

    b_append( b, s, sizeof(wchar_t)*l );
    b_append( b, &tmp, sizeof(wchar_t) );
    b->used -= sizeof(wchar_t);
}


void sb_append_char( string_buffer_t *b, wchar_t c )
{
    wchar_t tmp=0;

	if( !b )
	{
		return;
	}

	b_append( b, &c, sizeof(wchar_t) );
    b_append( b, &tmp, sizeof(wchar_t) );
    b->used -= sizeof(wchar_t);
}

void sb_append2( string_buffer_t *b, ... )
{
	va_list va;
	wchar_t *arg;

	if( !b )
	{
		return;
	}

	va_start( va, b );
	while( (arg=va_arg(va, wchar_t *) )!= 0 )
	{
		sb_append( b, arg );
	}
	va_end( va );
}

int sb_printf( string_buffer_t *buffer, const wchar_t *format, ... )
{
	va_list va;
	int res;
	
	if( !buffer )
	{
		return -1;
	}

	va_start( va, format );
	res = sb_vprintf( buffer, format, va );	
	va_end( va );
	
	return res;	
}

int sb_vprintf( string_buffer_t *buffer, const wchar_t *format, va_list va_orig )
{
	int res;
	
	if( !buffer )
	{
		return -1;
	}

	if( !buffer->length )
	{
		buffer->length = MIN_SIZE;
		buffer->buff = malloc( MIN_SIZE );
		if( !buffer->buff )
			DIE_MEM();
	}
	

	while( 1 )
	{
		va_list va;
		va_copy( va, va_orig );
		
		res = vswprintf( (wchar_t *)((char *)buffer->buff+buffer->used), 
						 (buffer->length-buffer->used)/sizeof(wchar_t), 
						 format,
						 va );
		

		va_end( va );		
		if( res >= 0 )
		{
			buffer->used+= res*sizeof(wchar_t);
			break;
		}

		/*
		  As far as I know, there is no way to check if a
		  vswprintf-call failed because of a badly formated string
		  option or because the supplied destination string was to
		  small. In GLIBC, errno seems to be set to EINVAL either way. 

		  Because of this, sb_printf will on failiure try to
		  increase the buffer size until the free space is larger than
		  SB_MAX_SIZE, at which point it will conclude that the error
		  was probably due to a badly formated string option, and
		  return an error. 
		*/
	
		if( buffer->length - buffer->used > SB_MAX_SIZE )
			break;

		buffer->buff = realloc( buffer->buff, 2*buffer->length );
		if( !buffer->buff )
			DIE_MEM();
		buffer->length *= 2;				
	}
	return res;	
}




void sb_destroy( string_buffer_t * b )
{
	if( !b )
	{
		return;
	}

	free( b->buff );
}

void sb_clear( string_buffer_t * b )
{
	wchar_t c=0;
	b->used=0;
	b_append( b, &c, sizeof( wchar_t));
	b->used -= sizeof(wchar_t);
}


void b_init( buffer_t *b)
{
	memset( b,0,sizeof(buffer_t));
}



void b_destroy( buffer_t *b )
{
	free( b->buff );
}


void b_append( buffer_t *b, const void *d, ssize_t len )
{
	if( len<=0 )
		return;

	if( !b )
	{
		debug( 2, L"Copy to null buffer" );
		return;
	}

	if( !d )
	{
		debug( 2, L"Copy from null pointer" );
		return;
	}

	if( len < 0 )
	{
		debug( 2, L"Negative number of characters to be copied" );
		return;
	}


	if( b->length <= (b->used + len) )
	{
		size_t l = maxi( b->length*2,
						 maxi( b->used+len+MIN_SIZE,MIN_SIZE));

		void *d = realloc( b->buff, l );
		if( !d )
		{
			DIE_MEM();
			
		}
		b->buff=d;
		b->length = l;
	}
	memcpy( ((char*)b->buff)+b->used,
			d,
			len );

//	fwprintf( stderr, L"Copy %s, new value %s\n", d, b->buff );
	b->used+=len;
}

long long get_time()
{
	struct timeval time_struct;
	gettimeofday( &time_struct, 0 );
	return 1000000ll*time_struct.tv_sec+time_struct.tv_usec;
}

