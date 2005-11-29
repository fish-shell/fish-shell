/** \file function.h 

    Prototypes for functions for storing and retrieving function
	information. Actual function evaluation is taken care of by the
	parser and to some degree the builtin handling library.
*/

#ifndef FISH_FUNCTION_H
#define FISH_FUNCTION_H

#include <wchar.h>

#include "util.h"

/**
   Initialize function data   
*/
void function_init();
/**
   Destroy function data
*/
void function_destroy();

/**
   Add an function. The parameters values are copied and should be freed by the caller.
*/
void function_add( const wchar_t *name,
				   const wchar_t *val,
				   const wchar_t *desc,
				   array_list_t *events,
				   int is_binding );

/**
   Remove the function with the specified name.
*/
void function_remove( const wchar_t *name );

/**
   Returns true if the function with the name name uses internal variables, false otherwise.
*/
int function_use_vars( const wchar_t *name );

/**
   Returns the definition of the function with the name \c name.
*/
const wchar_t *function_get_definition( const wchar_t *name );

/**
   Returns the description of the function with the name \c name.
*/
const wchar_t *function_get_desc( const wchar_t *name );

/**
   Sets the description of the function with the name \c name.
*/
void function_set_desc( const wchar_t *name, const wchar_t *desc );

/**
   Returns true if the function witrh the name name exists.
*/
int function_exists( const wchar_t *name );

/**
   Insert all function names into l. These are not copies of the strings and should not be freed after use.
   
   \param list the list to add the names to
   \param get_hidden whether to include hidden functions, i.e. ones starting with an underscore
*/
void function_get_names( array_list_t *list, 
						 int get_hidden );

#endif
