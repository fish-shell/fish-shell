/** \file path.h

  Directory utilities. This library contains functions for locating
  configuration directories, for testing if a command with a given
  name can be found in the PATH, and various other path-related
  issues.
*/
#ifndef FISH_PATH_H
#define FISH_PATH_H

#include <stddef.h>
#include <stdbool.h>

#include "common.h"
#include "env.h"

/**
   Return value for path_cdpath_get when locatied a rotten symlink
 */
#define EROTTEN 1

/**
   Returns the user configuration directory for fish. If the directory
   or one of its parents doesn't exist, they are first created.

   \param path The directory as an out param
   \return whether the directory was returned successfully
*/
bool path_get_config(wcstring &path);

/**
   Returns the user data directory for fish. If the directory
   or one of its parents doesn't exist, they are first created.

   Volatile files presumed to be local to the machine,
   such as the fish_history and all the generated_completions,
   will be stored in this directory.

   \param path The directory as an out param
   \return whether the directory was returned successfully
*/
bool path_get_data(wcstring &path);

/**
   Finds the full path of an executable. Returns YES if successful.

   \param cmd The name of the executable.
   \param output_or_NULL If non-NULL, store the full path.
   \param vars The environment variables snapshot to use
   \return 0 if the command can not be found, the path of the command otherwise. The result should be freed with free().
*/
bool path_get_path(const wcstring &cmd,
                   wcstring *output_or_NULL,
                   const env_vars_snapshot_t &vars = env_vars_snapshot_t::current());

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

   \param dir The name of the directory.
   \param out_or_NULL If non-NULL, return the path to the resolved directory
   \param wd The working directory, or NULL to use the default. The working directory should have a slash appended at the end.
   \param vars The environment variable snapshot to use (for the CDPATH variable)
   \return 0 if the command can not be found, the path of the command otherwise. The path should be free'd with free().
*/
bool path_get_cdpath(const wcstring &dir,
                     wcstring *out_or_NULL,
                     const wchar_t *wd = NULL,
                     const env_vars_snapshot_t &vars = env_vars_snapshot_t::current());

/** Returns whether the path can be used for an implicit cd command; if so, also returns the path by reference (if desired). This requires it to start with one of the allowed prefixes (., .., ~) and resolve to a directory. */
bool path_can_be_implicit_cd(const wcstring &path,
                             wcstring *out_path = NULL,
                             const wchar_t *wd = NULL,
                             const env_vars_snapshot_t &vars = env_vars_snapshot_t::current());

/**
   Remove double slashes and trailing slashes from a path,
   e.g. transform foo//bar/ into foo/bar. The string is modified in-place.
 */
void path_make_canonical(wcstring &path);

/** Check if two paths are equivalent, which means to ignore runs of multiple slashes (or trailing slashes) */
bool paths_are_equivalent(const wcstring &p1, const wcstring &p2);

bool path_is_valid(const wcstring &path, const wcstring &working_directory);

/** Returns whether the two paths refer to the same file */
bool paths_are_same_file(const wcstring &path1, const wcstring &path2);

/* If the given path looks like it's relative to the working directory, then prepend that working directory. This operates on unescaped paths only (so a ~ means a literal ~) */
wcstring path_apply_working_directory(const wcstring &path, const wcstring &working_directory);

#endif
