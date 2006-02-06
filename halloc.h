/** \file halloc.h
	A hierarchical memory allocation system
*/

/**
   Allocate new memory using specified parent memory context. If \c
   context is null, a new root context is created.
*/
void *halloc( void *context, size_t size );

/**
   Free memory context and all children. Only root contexts may be
   freed explicitly.
*/
void halloc_free( void *context );

wchar_t *halloc_wcsdup( void *context, const wchar_t *str );
wchar_t *halloc_wcsndup( void *context, const wchar_t *in, int c );
