/** \file halloc.h 

    A hierarchical memory allocation system. Works just like talloc
	used in Samba, except that an arbitrary block allocated with
	malloc() can be registered to be freed by halloc_free.

*/

/**
   Allocate new memory using specified parent memory context. If \c
   context is null, a new root context is created. Context _must_ be
   either 0 or the result of a previous call to halloc.

   If \c context is null, the resulting block must be freed with a
   call to halloc_free().

   If \c context is not null, the resulting memory block must never be
   explicitly freed, it will be automatically freed whenever the
   parent context is freed.
*/
void *halloc( void *context, size_t size );

/**
   Free memory context and all children contexts. Only root contexts
   may be freed explicitly.
*/
void halloc_free( void *context );

/**
   Free the memory pointed to by \c data when the memory pointed to by
   \c context is free:d. Note that this will _not_ turn the specified
   memory area into a valid halloc context. Only memory areas created
   using a call to halloc() can be used as a context.
*/
void *halloc_register( void *context, void *data );
