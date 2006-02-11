/**
   \file halloc_util.h

   Various halloc-related utility functions.
*/

#ifndef FISH_HALLOC_UTIL_H
#define FISH_HALLOC_UTIL_H

extern void *global_context;

void halloc_util_init();

void halloc_util_destroy();


array_list_t *al_halloc( void *context );

string_buffer_t *sb_halloc( void *context );

void halloc_register_function_void( void *context, void (*func)() );
/**
   Free the memory pointed to by \c data when the memory pointed to by
   \c context is free:d. Note that this will _not_ turn the specified
   memory area into a valid halloc context. Only memory areas created
   using a call to halloc() can be used as a context.
*/
void *halloc_register( void *context, void *data );

wchar_t *halloc_wcsdup( void *context, wchar_t *str );
wchar_t *halloc_wcsndup( void * context, const wchar_t *in, int c );



#endif
