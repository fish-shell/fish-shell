/** \file halloc.h 

    A hierarchical memory allocation system. Works mostly like talloc
	used in Samba, except that an arbitrary block allocated with
	malloc() can be registered to be freed by halloc_free.

*/

#ifndef FISH_HALLOC_H
#define FISH_HALLOC_H

/**
   Allocate new memory using specified parent memory context. Context
   _must_ be either 0 or the result of a previous call to halloc. The
   resulting memory is set to zero.

   If \c context is null, the resulting block is a root block, and
   must be freed with a call to halloc_free().

   If \c context is not null, context must be a halloc root block. the
   resulting memory block is a child context, and must never be
   explicitly freed, it will be automatically freed whenever the
   parent context is freed. Child blocks can also never be used as the
   context in calls to halloc_register_function, halloc_free, etc.
*/
void *halloc( void *context, size_t size );

/**
   Make the specified function run whenever context is free'd, using data as argument.

   \c context a halloc root block
*/
void halloc_register_function( void *context, void (*func)(void *), void *data );

/**
   Free memory context and all children contexts. Only root contexts
   may be freed explicitly.

   All functions registered with halloc_register_function are run in
   the order they where added. Afterwards, all memory allocated using
   halloc itself is free'd.

   \c context a halloc root block
*/
void halloc_free( void *context );


#endif

