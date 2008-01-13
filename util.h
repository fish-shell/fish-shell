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
   Data structure for an automatically resizing dynamically allocated queue,
*/
typedef struct dyn_queue
{
	/** Start of the array */
	void **start;
	/** End of the array*/
	void **stop;
	/** Where to insert elements */
	void **put_pos;
	/** Where to remove elements */
	void **get_pos;
}
	dyn_queue_t;

/**
   Internal struct used by hash_table_t.
*/
typedef struct
{
	/** Hash key*/
	void *key;
	/** Value */
	void *data;
}
	hash_struct_t;

/**
   Data structure for the hash table implementaion. A hash table allows for
   retrieval and removal of any element in O(1), so long as a proper
   hash function is supplied.

   The hash table is implemented using a single hash function and
   element storage directly in the array. When a collision occurs, the
   hashtable iterates until a zero element is found. When the table is
   75% full, it will automatically reallocate itself. This
   reallocation takes O(n) time. The table is guaranteed to never be
   more than 75% full or less than 30% full (Unless the table is
   nearly empty). Its size is always a Mersenne number.

*/

typedef struct hash_table
{
	/** The array containing the data */
	hash_struct_t *arr;
	/** A simple one item cache. This should always point to the index of the last item to be used */
	int cache;	
	/** Number of elements */
	int count;
	/** Length of array */
	int size;
	/** Hash function */
	int (*hash_func)( void *key );
	/** Comparison function */
	int (*compare_func)( void *key1, void *key2 );
}
	hash_table_t;

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
   Array list struct.
   A dynamically growing list that supports stack operations.
*/
typedef struct array_list
{
	/** 
		Array containing the data
	*/
	anything_t *arr;
	
	/** 
		Internal cursor position of the array_list_t. This is the
		position to append elements at. This is also what the
		array_list_t considers to be its true size, as reported by
		al_get_count(), etc. Calls to e.g. al_insert will preserve the
		values of all elements up to pos.
	*/
	size_t pos;

	/** 
		Amount of memory allocated in arr, expressed in number of elements.
	*/
	size_t size;
}
array_list_t;

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

/*
  All the datastuctures below autoresize. The queue, stack and
  priority queue are all impemented using an array and are guaranteed
  to never be less than 50% full. 
*/

/** 
	Initialize the queue. A queue is a FIFO buffer, i.e. the first
    element to be inserted into the buffer is the first element to be
    returned. 
*/
void q_init( dyn_queue_t *q );

/**
   Destroy the queue 
*/
void q_destroy( dyn_queue_t *q );

/**
   Insert element into queue 
*/
int q_put( dyn_queue_t *q, void *e );

/**
   Remove and return next element from queue 
*/
void *q_get( dyn_queue_t *q);

/**
   Return next element from queue without removing it 
*/
void *q_peek( dyn_queue_t *q);

/**
   Returns 1 if the queue is empty, 0 otherwise 
*/
int q_empty( dyn_queue_t *q );

/**
   Initialize a hash table. The hash function must never return the value 0.
*/
void hash_init( hash_table_t *h,
				int (*hash_func)( void *key),
				int (*compare_func)( void *key1, void *key2 ) );

/**
   Initialize a hash table. The hash function must never return the value 0.
*/
void hash_init2( hash_table_t *h,
				int (*hash_func)( void *key ),
				 int (*compare_func)( void *key1, void *key2 ),
				 size_t capacity);

/**
   Destroy the hash table and free associated memory.
*/
void hash_destroy( hash_table_t *h );
/**
   Set the key/value pair for the hashtable. 
*/
int hash_put( hash_table_t *h, 
			  const void *key,
			  const void *data );
/**
   Returns the data with the associated key, or 0 if no such key is in the hashtable
*/
void *hash_get( hash_table_t *h,
				const void *key );
/**
   Returns the hash tables version of the specified key
*/
void *hash_get_key( hash_table_t *h, 
					const void *key );

/**
   Returns the number of key/data pairs in the table.
*/
int hash_get_count( hash_table_t *h);
/**
   Remove the specified key from the hash table if it exists. Do nothing if it does not exist.

   \param h The hashtable
   \param key The key
   \param old_key If not 0, a pointer to the old key will be stored at the specified address
   \param old_data If not 0, a pointer to the data will be stored at the specified address
*/
void hash_remove( hash_table_t *h, 
				  const void *key, 
				  void **old_key,
				  void **old_data );

/**
   Checks whether the specified key is in the hash table
*/
int hash_contains( hash_table_t *h, 
				   const void *key );

/**
   Appends all keys in the table to the specified list
*/
void hash_get_keys( hash_table_t *h,
					array_list_t *arr );

/**
   Appends all data elements in the table to the specified list
*/
void hash_get_data( hash_table_t *h,
					array_list_t *arr );

/**
   Call the function func for each key/data pair in the table
*/
void hash_foreach( hash_table_t *h, 
				   void (*func)( void *, void * ) );

/**
   Same as hash_foreach, but the function func takes an additional
   argument, which is provided by the caller in the variable aux 
*/
void hash_foreach2( hash_table_t *h, void (*func)( void *, 
												   void *, 
												   void *), 
					void *aux );

/**
   Hash function suitable for character strings. 
*/
int hash_str_func( void *data );
/**
   Hash comparison function suitable for character strings
*/
int hash_str_cmp( void *a,
				  void *b );

/**
   Hash function suitable for wide character strings. Uses a version
   of the sha cryptographic function which is simplified in order to
   returns a 32-bit number.
*/
int hash_wcs_func( void *data );

/**
   Hash comparison function suitable for wide character strings
*/
int hash_wcs_cmp( void *a, 
				  void *b );

/**
   Hash function suitable for direct pointer comparison
*/
int hash_ptr_func( void *data );


/**
   Hash comparison function suitable for direct pointer comparison
*/
int hash_ptr_cmp( void *a,
                  void *b );



/** 
	Initialize the priority queue
	
	\param q the queue to initialize
	\param compare a comparison function that can compare two entries in the queue
*/
void pq_init( priority_queue_t *q,
			  int (*compare)(void *e1, void *e2) );
/**
   Add element to the queue

   \param q the queue
   \param e the new element
 
*/
int pq_put( priority_queue_t *q,
			void *e );
/**
  Removes and returns the last entry in the priority queue
*/
void *pq_get( priority_queue_t *q );

/**
  Returns the last entry in the priority queue witout removing it.
*/
void *pq_peek( priority_queue_t *q );

/** 
	Returns 1 if the priority queue is empty, 0 otherwise.
*/
int pq_empty( priority_queue_t *q );

/**
   Returns the number of elements in the priority queue.
*/
int pq_get_count( priority_queue_t *q );

/** 
	Destroy the priority queue and free memory used by it.
*/
void pq_destroy(  priority_queue_t *q );

/**
   Allocate heap memory for creating a new list and initialize
   it. Equivalent to calling malloc and al_init.
*/
array_list_t *al_new();

/** 
	Initialize the list. 
*/
void al_init( array_list_t *l );

/** 
	Destroy the list and free memory used by it.
*/
void al_destroy( array_list_t *l );

/**
   Append element to list

   \param l The list
   \param o The element
   \return
   \return 1 if succesfull, 0 otherwise
*/
int al_push( array_list_t *l, const void *o );
/**
   Append element to list

   \param l The list
   \param o The element
   \return
   \return 1 if succesfull, 0 otherwise
*/
int al_push_long( array_list_t *l, long o );
/**
   Append element to list

   \param l The list
   \param f The element
   \return 1 if succesfull, 0 otherwise
*/
int al_push_func( array_list_t *l, func_ptr_t f );

/**
   Append all elements of a list to another

   \param a The destination list
   \param b The source list
   \return 1 if succesfull, 0 otherwise
*/
int al_push_all( array_list_t *a, array_list_t *b );

/**
   Insert the specified number of new empty positions at the specified position in the list.
 */
int al_insert( array_list_t *a, int pos, int count );

/**
   Sets the element at the specified index

   \param l The array_list_t
   \param pos The index 
   \param o The element 
*/
int al_set( array_list_t *l, int pos, const void *o );
/**
   Sets the element at the specified index

   \param l The array_list_t
   \param pos The index 
   \param v The element to set
*/
int al_set_long( array_list_t *l, int pos, long v );
/**
   Sets the element at the specified index

   \param l The array_list_t
   \param pos The index 
   \param f The element to insert
*/
int al_set_func( array_list_t *l, int pos, func_ptr_t f );

/**
   Returns the element at the specified index

   \param l The array_list_t
   \param pos The index 
   \return The element 
*/
void *al_get( array_list_t *l, int pos );
/**
   Returns the element at the specified index

   \param l The array_list_t
   \param pos The index 
   \return The element 
*/
long al_get_long( array_list_t *l, int pos );
/**
   Returns the element at the specified index

   \param l The array_list_t
   \param pos The index 
   \return The element 
*/
func_ptr_t al_get_func( array_list_t *l, int pos );

/**
  Truncates the list to new_sz items.
*/
void al_truncate( array_list_t *l, int new_sz );

/**
  Removes and returns the last entry in the list
*/
void *al_pop( array_list_t *l );
/**
  Removes and returns the last entry in the list
*/
long al_pop_long( array_list_t *l );
/**
  Removes and returns the last entry in the list
*/
func_ptr_t al_pop_func( array_list_t *l );

/**
   Returns the number of elements in the list
*/
int al_get_count( array_list_t *l );

/**
  Returns the last entry in the list witout removing it.
*/
void *al_peek( array_list_t *l );
/**
  Returns the last entry in the list witout removing it.
*/
long al_peek_long( array_list_t *l );
/**
  Returns the last entry in the list witout removing it.
*/
func_ptr_t al_peek_func( array_list_t *l );

/**
   Returns 1 if the list is empty, 0 otherwise
*/
int al_empty( array_list_t *l);

/** 
	Call the function func for each entry in the list
*/
void al_foreach( array_list_t *l, void (*func)( void * ));

/** 
	Same as al_foreach, but the function func takes an additional
	argument, which is provided by the caller in the variable aux
*/
void al_foreach2( array_list_t *l, void (*func)( void *, void *), void *aux);

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
#define sb_append( sb,... ) sb_append_internal( sb, __VA_ARGS__, (void *)0 )

/**
   Append a null terminated list of strings to the buffer.
   Example:

   sb_append2( my_buff, L"foo", L"bar", (void *)0 );

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
  Destroy the buffer and free it's memory
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
