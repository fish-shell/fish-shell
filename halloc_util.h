/**
   \file halloc_util.h

   Various halloc-related utility functions.
*/

#ifndef FISH_HALLOC_UTIL_H
#define FISH_HALLOC_UTIL_H

/**
   This pointer is a valid halloc context that will be freed right
   before program shutdown. It may be used to allocate memory that
   should be freed when the program shuts down.
*/
extern void *global_context;

/**
   Create the global_context halloc object
*/
void halloc_util_init();

/**
   Free the global_context halloc object
*/
void halloc_util_destroy();

/**
   Allocate a array_list_t that will be automatically disposed of when
   the specified context is free'd
*/
array_list_t *al_halloc( void *context );

/**
   Allocate a string_buffer_t that will be automatically disposed of
   when the specified context is free'd
*/
string_buffer_t *sb_halloc( void *context );

/**
   Register the specified function to run when the specified context
   is free'd. This function is related to halloc_register_function,
   but the specified function dowes not take an argument.
*/
void halloc_register_function_void( void *context, void (*func)() );

/**
   Free the memory pointed to by \c data when the memory pointed to by
   \c context is free:d. Note that this will _not_ turn the specified
   memory area into a valid halloc context. Only memory areas created
   using a call to halloc( 0, size ) can be used as a context.
*/
void *halloc_register( void *context, void *data );

/**
   Make a copy of the specified string using memory allocated using
   halloc and the specified context
*/
wchar_t *halloc_wcsdup( void *context, const wchar_t *str );

/**
   Make a copy of the specified substring using memory allocated using
   halloc and the specified context
*/
wchar_t *halloc_wcsndup( void * context, const wchar_t *in, int c );

#endif
