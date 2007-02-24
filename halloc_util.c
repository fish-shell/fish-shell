/** \file halloc_util.c

    A hierarchical memory allocation system. Works just like talloc
	used in Samba, except that an arbitrary block allocated with
	malloc() can be registered to be freed by halloc_free.

*/

#include "config.h"


#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#include "fallback.h"
#include "util.h"

#include "common.h"
#include "halloc.h"

void *global_context=0;

void halloc_util_init()
{
	global_context = halloc( 0, 0 );
}

void halloc_util_destroy()
{
	halloc_free( global_context );
}

array_list_t *al_halloc( void *context )
{
	array_list_t *res = halloc( context, sizeof( array_list_t ) );
	if( !res )
		DIE_MEM();
	al_init( res );
	halloc_register_function( context?context:res, (void (*)(void *)) &al_destroy, res );
	return res;
}

string_buffer_t *sb_halloc( void *context )
{
	string_buffer_t *res = halloc( context, sizeof( string_buffer_t ) );
	if( !res )
		DIE_MEM();
	sb_init( res );
	halloc_register_function( context?context:res, (void (*)(void *)) &sb_destroy, res );
	return res;
}

/**
   A function that takes a single parameter, which is a function pointer, and calls it.
*/
static void halloc_passthrough( void *f )
{
	void (*func)() = (void (*)() )f;
	func();
}

void halloc_register_function_void( void *context, void (*func)() )
{
	halloc_register_function( context, &halloc_passthrough, (void *)func );
}

void *halloc_register( void *context, void *data )
{
	if( !data )
		return 0;
	
	halloc_register_function( context, &free, data );
	return data;
}

wchar_t *halloc_wcsdup( void *context, const wchar_t *in )
{
	size_t len=wcslen(in);
	wchar_t *out = halloc( context, sizeof( wchar_t)*(len+1));
	
	if( out == 0 )
	{
		DIE_MEM();
	}
	memcpy( out, in, sizeof( wchar_t)*(len+1));
	return out;
}

wchar_t *halloc_wcsndup( void * context, const wchar_t *in, int c )
{
	wchar_t *res = halloc( context, sizeof(wchar_t)*(c+1) );
	if( res == 0 )
	{
		DIE_MEM();
	}
	wcslcpy( res, in, c+1 );
	res[c] = L'\0';	
	return res;	
}
