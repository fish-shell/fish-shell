/** \file path.h

	Directory utilities. This library contains functions for locating
	configuration directories, for testing if a command with a given
	name can be found in the PATH, and various other path-related
	issues.
*/

#ifndef FISH_PATH_H
#define FISH_PATH_H

/**
   Returns the user configuration directory for fish. If the directory
   or one of it's parents doesn't exist, they are first created.

   \param context the halloc context to use for memory allocations
   \return 0 if the no configuration directory can be located or created, the directory path otherwise.
*/
wchar_t *path_get_config( void *context);

/**
   Finds the full path of an executable in a newly allocated string.
   
   \param cmd The name of the executable.
   \param context the halloc context to use for memory allocations
   \return 0 if the command can not be found, the path of the command otherwise.
*/
wchar_t *path_get_path( void *context, const wchar_t *cmd );

/**
   Returns the full path of the specified directory. If the \c in is a
   full path to an existing directory, a copy of the string is
   returned. If \c in is a directory relative to one of the
   directories i the CDPATH, the full path is returned. If no
   directory can be found, 0 is returned.

   \param in The name of the directory.
   \param context the halloc context to use for memory allocations
   \return 0 if the command can not be found, the path of the command otherwise.
*/
wchar_t *path_get_cdpath( void *context, wchar_t *in );

#endif
