/** \file path.h

	Directory utilities. This library contains functions for locating
	configuration directories, for testing if a command with a given
	name can be found in the PATH, and various other path-related
	issues.
*/

#ifndef FISH_PATH_H
#define FISH_PATH_H

/**
   Return value for path_cdpath_get when locatied a rotten symlink
 */
#define EROTTEN 1

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
   Returns the full path of the specified directory, using the CDPATH
   variable as a list of base directories for relative paths. The
   returned string is allocated using halloc and the specified
   context.
   
   If no valid path is found, null is returned and errno is set to
   ENOTDIR if at least one such path was found, but it did not point
   to a directory, EROTTEN if a arotten symbolic link was found, or
   ENOENT if no file of the specified name was found. If both a rotten
   symlink and a file are found, it is undefined which error status
   will be returned.
   
   \param in The name of the directory.
   \param context the halloc context to use for memory allocations
   \return 0 if the command can not be found, the path of the command otherwise.
*/
wchar_t *path_get_cdpath( void *context, wchar_t *in );

/**
   Remove doulbe slashes and trailing slashes from a path,
   e.g. transform foo//bar/ into foo/bar.
   
   The returned string is allocated using the specified halloc
   context.
 */
wchar_t *path_make_canonical( void *context, const wchar_t *path );


#endif
