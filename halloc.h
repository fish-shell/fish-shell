/** \file halloc.h 

    A hierarchical memory allocation system. Works just like talloc
	used in Samba, except that an arbitrary block allocated with
	malloc() can be registered to be freed by halloc_free.

*/

#ifndef FISH_HALLOC_H
#define FISH_HALLOC_H

/**
   Allocate new memory using specified parent memory context. Context
   _must_ be either 0 or the result of a previous call to halloc.

   If \c context is null, the resulting block is a root context, and
   must be freed with a call to halloc_free().

   If \c context is not null, the resulting memory block is a child
   context, and must never be explicitly freed, it will be
   automatically freed whenever the parent context is freed.
*/
void *halloc( void *context, size_t size );

/**
   Make the specified function run whenever context is free'd, using data as argument.
*/
void halloc_register_function( void *context, void (*func)(void *), void *data );

/**
   Free memory context and all children contexts. Only root contexts
   may be freed explicitly.
*/
void halloc_free( void *context );


#endif

