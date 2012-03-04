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

/**
   Get the current time in microseconds since Jan 1, 1970
*/
long long get_time();

#endif
