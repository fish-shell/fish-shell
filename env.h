/** \file env.h
	Prototypes for functions for setting and getting environment variables.
*/

#ifndef FISH_ENV_H
#define FISH_ENV_H

#include <wchar.h>

#include "util.h"

/**
   Flag for local (to the current block) variable
*/
#define ENV_LOCAL 1

/**
   Flag for exported (to commands) variable
*/
#define ENV_EXPORT 2

/**
   Flag for unexported variable
*/
#define ENV_UNEXPORT 16

/**
   Flag for global variable
*/
#define ENV_GLOBAL 4

/**
   Flag for variable update request from the user. All variable
   changes that are made directly by the user, such as those from the
   'set' builtin must have this flag set.
*/
#define ENV_USER 8

/**
   Flag for universal variable
*/
#define ENV_UNIVERSAL 32

/**
   Error code for trying to alter read-only variable
*/
enum{
	ENV_PERM = 1,
	ENV_INVALID
}
;


/**
   Initialize environment variable data
*/
void env_init();

/**
   Destroy environment variable data
*/
void env_destroy();


/**
   Set the value of the environment variable whose name matches key to val. 

   Memory policy: All keys and values are copied, the parameters can and should be freed by the caller afterwards

   \param key The key
   \param val The value
   \param mode The type of the variable. Can be any combination of ENV_GLOBAL, ENV_LOCAL, ENV_EXPORT and ENV_USER. If mode is zero, the current variable space is searched and the current mode is used. If no current variable with the same name is found, ENV_LOCAL is assumed.

   \returns 0 on suicess or an error code on failiure.

   The current error codes are:

   * ENV_PERM, can only be returned when setting as a user, e.g. ENV_USER is set. This means that the user tried to change a read-only variable.
   * ENV_INVALID, the variable name or mode was invalid
*/

int env_set( const wchar_t *key, 
			 const wchar_t *val,
			 int mode );


/**
  Return the value of the variable with the specified name.  Returns 0
  if the key does not exist.  The returned string should not be
  modified or freed. The returned string is only guaranteed to be
  valid until the next call to env_get(), env_set(), env_push() or
  env_pop() takes place.
*/
wchar_t *env_get( const wchar_t *key );

/**
   Returns 1 if the specified key exists. This can't be reliably done
   using env_get, since env_get returns null for 0-element arrays

   \param key The name of the variable to remove
   \param mode the scope to search in. All scopes are searched if unset
*/
int env_exist( const wchar_t *key, int mode );

/**
   Remove environemnt variable
   
   \param key The name of the variable to remove
   \param mode should be ENV_USER if this is a remove request from the user, 0 otherwise. If this is a user request, read-only variables can not be removed. The mode may also specify the scope of the variable that should be erased.

   \return zero if the variable existed, and non-zero if the variable did not exist
*/
int env_remove( const wchar_t *key, int mode );

/**
  Push the variable stack. Used for implementing local variables for functions and for-loops.
*/
void env_push( int new_scope );

/**
  Pop the variable stack. Used for implementing local variables for functions and for-loops.
*/
void env_pop();

/**
   Returns an array containing all exported variables in a format suitable for execv.
*/
char **env_export_arr( int recalc );

/**
  Insert all variable names into l. These are not copies of the strings and should not be freed after use.
*/
void env_get_names( array_list_t *l, int flags );

/**
   Update the PWD variable
   directory
*/
int env_set_pwd();


#endif
