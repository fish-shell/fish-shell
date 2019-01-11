// Directory utilities. This library contains functions for locating configuration directories, for
// testing if a command with a given name can be found in the PATH, and various other path-related
// issues.
#ifndef FISH_PATH_H
#define FISH_PATH_H

#include <stddef.h>

#include "common.h"
#include "env.h"

/// Return value for path_cdpath_get when locatied a rotten symlink.
#define EROTTEN 1

/// Returns the user configuration directory for fish. If the directory or one of its parents
/// doesn't exist, they are first created.
///
/// \param path The directory as an out param
/// \return whether the directory was returned successfully
bool path_get_config(wcstring &path);

/// Returns the user data directory for fish. If the directory or one of its parents doesn't exist,
/// they are first created.
///
/// Volatile files presumed to be local to the machine, such as the fish_history and all the
/// generated_completions, will be stored in this directory.
///
/// \param path The directory as an out param
/// \return whether the directory was returned successfully
bool path_get_data(wcstring &path);

/// Finds the full path of an executable.
///
/// Args:
/// cmd - The name of the executable.
/// output_or_NULL - If non-NULL, store the full path.
/// vars - The environment variables to use
///
/// Returns:
/// false if the command can not be found else true. The result
/// should be freed with free().
bool path_get_path(const wcstring &cmd, wcstring *output_or_NULL, const environment_t &vars);

/// Return all the paths that match the given command.
wcstring_list_t path_get_paths(const wcstring &cmd, const environment_t &vars);

/// Returns the full path of the specified directory, using the CDPATH variable as a list of base
/// directories for relative paths.
///
/// If no valid path is found, false is returned and errno is set to ENOTDIR if at least one such
/// path was found, but it did not point to a directory, EROTTEN if a arotten symbolic link was
/// found, or ENOENT if no file of the specified name was found. If both a rotten symlink and a file
/// are found, it is undefined which error status will be returned.
///
/// \param dir The name of the directory.
/// \param out_or_NULL If non-NULL, return the path to the resolved directory
/// \param wd The working directory. The working directory should have a slash appended at the end.
/// \param vars The environment variables to use (for the CDPATH variable)
/// \return the command, or none() if it could not be found.
maybe_t<wcstring> path_get_cdpath(const wcstring &dir, const wcstring &wd,
                                  const environment_t &vars);

/// Returns the path resolved as an implicit cd command, or none() if none. This requires it to
/// start with one of the allowed prefixes (., .., ~) and resolve to a directory.
maybe_t<wcstring> path_as_implicit_cd(const wcstring &path, const wcstring &wd,
                                      const environment_t &vars);

/// Remove double slashes and trailing slashes from a path, e.g. transform foo//bar/ into foo/bar.
/// The string is modified in-place.
void path_make_canonical(wcstring &path);

/// Check if two paths are equivalent, which means to ignore runs of multiple slashes (or trailing
/// slashes).
bool paths_are_equivalent(const wcstring &p1, const wcstring &p2);

bool path_is_valid(const wcstring &path, const wcstring &working_directory);

/// Returns whether the two paths refer to the same file.
bool paths_are_same_file(const wcstring &path1, const wcstring &path2);

/// If the given path looks like it's relative to the working directory, then prepend that working
/// directory. This operates on unescaped paths only (so a ~ means a literal ~).
wcstring path_apply_working_directory(const wcstring &path, const wcstring &working_directory);

#endif
