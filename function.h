/** \file function.h 

    Prototypes for functions for storing and retrieving function
	information. These functions also take care of autoloading
	functions in the $fish_function_path. Actual function evaluation
	is taken care of by the parser and to some degree the builtin
	handling library.
*/

#ifndef FISH_FUNCTION_H
#define FISH_FUNCTION_H

#include <wchar.h>

#include "util.h"
#include "common.h"
#include "event.h"

#include <tr1/memory>
using std::tr1::shared_ptr;

class parser_t;
class env_vars;

/**
   Structure describing a function. This is used by the parser to
   store data on a function while parsing it. It is not used
   internally to store functions, the function_internal_data_t
   structure is used for that purpose. Parhaps these two should be
   merged.
  */
struct function_data_t
{
	/**
	   Name of function
	 */
	wchar_t *name;
	/**
	   Description of function
	 */
	wchar_t *description;
	/**
	   Function definition
	 */
	wchar_t *definition;
	/**
	   List of all event handlers for this function
	 */
	std::vector<event_t> events;
	/**
	   List of all named arguments for this function
	 */
	wcstring_list_t named_arguments;
	/**
	   Set to non-zero if invoking this function shadows the variables
	   of the underlying function.
	 */
	int shadows;
};

class function_info_t {
public:
    /** Function definition */
    wcstring definition;
    
    /** Function description */
    wcstring description;
    
	/**
	   File where this function was defined (intern'd string)
	*/
    const wchar_t *definition_file;
    
	/**
	   Line where definition started
	*/
	int definition_offset;
    
	/**
	   List of all named arguments for this function
	 */
    wcstring_list_t named_arguments;
    
	/**
	   Flag for specifying that this function was automatically loaded
	*/
    bool is_autoload;
    
	/**
	   Set to non-zero if invoking this function shadows the variables
	   of the underlying function.
	 */
	bool shadows;
};


/**
   Initialize function data   
*/
void function_init();

/**
   Add a function. The parameters values are copied and should be
   freed by the caller.
*/
void function_add( function_data_t *data, const parser_t &parser );

/**
   Remove the function with the specified name.
*/
void function_remove( const wcstring &name );

/**
    Gets a function by name.
*/
shared_ptr<function_info_t> function_get(const wcstring &name);

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
   Returns true if the function with the name name exists.
*/
int function_exists( const wchar_t *name );

/**
   Returns true if the function with the name name exists, without triggering autoload.
*/
int function_exists_no_autoload( const wchar_t *name, const env_vars &vars );

/**
   Returns all function names.
   
   \param get_hidden whether to include hidden functions, i.e. ones starting with an underscore
*/
wcstring_list_t function_get_names( int get_hidden );

/**
   Returns tha absolute path of the file where the specified function
   was defined. Returns 0 if the file was defined on the commandline.

   This function does not autoload functions, it will only work on
   functions that have already been defined.
*/
const wchar_t *function_get_definition_file( const wchar_t *name );

/**
   Returns the linenumber where the definition of the specified
   function started.

   This function does not autoload functions, it will only work on
   functions that have already been defined.
*/
int function_get_definition_offset( const wchar_t *name );

/**
   Returns a list of all named arguments of the specified function.
*/
wcstring_list_t function_get_named_arguments( const wchar_t *name );

/**
   Creates a new function using the same definition as the specified function.
   Returns non-zero if copy is successful.
*/
int function_copy( const wchar_t *name, const wchar_t *new_name );


/**
   Returns whether this function shadows variables of the underlying function
*/
int function_get_shadows( const wchar_t *name );

#endif
