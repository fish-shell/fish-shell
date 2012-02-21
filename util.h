/** \file util.h
	Generic utilities library.

	All containers in this library except strinb_buffer_t are written
	so that they don't allocate any memory until the first element is
	inserted into them. That way it is known to be very cheap to
	initialize various containers at startup, supporting the fish
	notion of doing as much lazy initalization as possible.
*/

#ifndef FISH_UTIL_H
#define FISH_UTIL_H

#include <wchar.h>
#include <stdarg.h>
#include <unistd.h>

/**
   Typedef for a generic function pointer
 */
typedef void (*func_ptr_t)();

/**
   A union of all types that can be stored in an array_list_t. This is
   used to make sure that the pointer type can fit whatever we want to
   insert.
 */
typedef union 
{
	/**
	   long value
	 */
	long long_val;
	/**
	   pointer value
	 */
	void *ptr_val;
	/**
	   function pointer value
	 */
	func_ptr_t func_val;
}
	anything_t;

/**
   Data structure for an automatically resizing dynamically allocated
   priority queue. A priority queue allows quick retrieval of the
   smallest element of a set (This implementation uses O(log n) time).
   This implementation uses a heap for storing the queue.
*/
typedef struct priority_queue
{
	/** Array contining the data */
	void **arr;
	/** Number of elements*/
	int count;
	/** Length of array */
	int size;
	/** Comparison function */
	int (*compare)(void *e1, void *e2);
}
priority_queue_t;

/**
   Linked list node.
*/
typedef struct _ll_node
{
	/** Next node */
	struct _ll_node *next, /** Previous node */ *prev;
	/** Node data */
	void *data;
}
ll_node_t;

/**
   Buffer for concatenating arbitrary data.
*/
typedef struct buffer
{
	char *buff; /**<data buffer*/
	size_t length; /**< Size of buffer */
	size_t used; /**< Size of data in buffer */
}
buffer_t;


/**
   String buffer struct.  An autoallocating buffer used for
   concatenating strings. This is really just a buffer_t.
*/
typedef buffer_t string_buffer_t;

/**
   Set the out-of-memory handler callback function. If a memory
   allocation fails, this function will be called. 
*/	
void (*util_set_oom_handler( void (*h)(void *) ))(void *);

/**
   This is a possible out of memory handler that will kill the current
   process in response to any out of memory event, while also printing
   an error message describing what allocation failed.

   This is the default out of memory handler.
*/
void util_die_on_oom( void *p );

/**
   Returns the larger of two ints
*/
int maxi( int a, int b );

/**
   Returns the smaller of two ints
 */
int mini( int a, int b );

/**
   Compares two wide character strings with an (arguably) intuitive
   ordering.

   This function tries to order strings in a way which is intuitive to
   humans with regards to sorting strings containing numbers.

   Most sorting functions would sort the strings 'file1.txt'
   'file5.txt' and 'file12.txt' as:

   file1.txt
   file12.txt
   file5.txt

   This function regards any sequence of digits as a single entity
   when performing comparisons, so the output is instead:

   file1.txt
   file5.txt
   file12.txt

   Which most people would find more intuitive.

   This won't return the optimum results for numbers in bases higher
   than ten, such as hexadecimal, but at least a stable sort order
   will result.

   This function performs a two-tiered sort, where difference in case
   and in number of leading zeroes in numbers only have effect if no
   other differences between strings are found. This way, a 'file1'
   and 'File1' will not be considered identical, and hence their
   internal sort order is not arbitrary, but the names 'file1',
   'File2' and 'file3' will still be sorted in the order given above.
*/
int wcsfilecmp( const wchar_t *a, const wchar_t *b );


/*
  String buffer functions
*/

/**
   Initialize the specified string_buffer
*/
void sb_init( string_buffer_t * );

/**
   Allocate memory for storing a stringbuffer and init it
*/
string_buffer_t *sb_new();

/**
   Append a part of a string to the buffer.
*/
void sb_append_substring( string_buffer_t *, const wchar_t *, size_t );

/**
   Append a character to the buffer.
*/
void sb_append_char( string_buffer_t *, wchar_t );

/**
   Append all specified items to buffer.
 */
#define sb_append( sb,... ) sb_append_internal( sb, __VA_ARGS__, NULL )

/**
   Append a null terminated list of strings to the buffer.
   Example:

   sb_append2( my_buff, L"foo", L"bar", NULL );

   Do not forget to cast the last 0 to (void *), or you might encounter errors on 64-bit platforms!
*/
__sentinel void sb_append_internal( string_buffer_t *, ... );

/**
   Append formated string data to the buffer. This function internally
   relies on \c vswprintf, so any filter options supported by that
   function is also supported by this function.
*/
int sb_printf( string_buffer_t *buffer, const wchar_t *format, ... );

/**
   Vararg version of sb_printf.
*/
int sb_vprintf( string_buffer_t *buffer, const wchar_t *format, va_list va_orig );

/**
  Destroy the buffer and free its memory
*/
void sb_destroy( string_buffer_t * );

/**
   Completely truncate the buffer. This will not deallocate the memory
   used, it will only set the contents of the string to L"\\0".
*/
void sb_clear( string_buffer_t * );

/**
   Truncate the string to the specified number of characters. This
   will not deallocate the memory used.
*/
void sb_truncate( string_buffer_t *, int chars_left );

/**
   Return the number of characters in the string
*/
ssize_t sb_length( string_buffer_t * );


/*
  Buffer functions
*/

/**
   Initialize the specified buffer_t
*/
void b_init( buffer_t *b);

/**
   Destroy the specified buffer_t
*/

void b_destroy( buffer_t *b );

/**
   Add data of the specified length to the specified buffer_t

   \return 0 on error, non-zero otherwise
*/
int b_append( buffer_t *b, const void *d, ssize_t len );

/**
   Get the current time in microseconds since Jan 1, 1970
*/
long long get_time();

#endif
