// My own globbing implementation. Needed to implement this instead of using libs globbing to
// support tab-expansion of globbed paramaters.
#ifndef FISH_WILDCARD_H
#define FISH_WILDCARD_H

#include <stdbool.h>
#include <vector>

#include "common.h"
#include "complete.h"
#include "expand.h"

// Enumeration of all wildcard types.
enum {
    /// Character representing any character except '/' (slash).
    ANY_CHAR = WILDCARD_RESERVED_BASE,
    /// Character representing any character string not containing '/' (slash).
    ANY_STRING,
    /// Character representing any character string.
    ANY_STRING_RECURSIVE,
    /// This is a special psuedo-char that is not used other than to mark the
    /// end of the the special characters so we can sanity check the enum range.
    ANY_SENTINAL
};

/// Expand the wildcard by matching against the filesystem.
///
/// New strings are allocated using malloc and should be freed by the caller.
///
/// wildcard_expand works by dividing the wildcard into segments at each directory boundary. Each
/// segment is processed separatly. All except the last segment are handled by matching the wildcard
/// segment against all subdirectories of matching directories, and recursively calling
/// wildcard_expand for matches. On the last segment, matching is made to any file, and all matches
/// are inserted to the list.
///
/// If wildcard_expand encounters any errors (such as insufficient priviliges) during matching, no
/// error messages will be printed and wildcard_expand will continue the matching process.
///
/// \param wc The wildcard string
/// \param working_directory The working directory
/// \param flags flags for the search. Can be any combination of EXPAND_FOR_COMPLETIONS and
/// EXECUTABLES_ONLY
/// \param out The list in which to put the output
///
/// \return 1 if matches where found, 0 otherwise. Return -1 on abort (I.e. ^C was pressed).
int wildcard_expand_string(const wcstring &wc, const wcstring &working_directory,
                           expand_flags_t flags, std::vector<completion_t> *out);

/// Test whether the given wildcard matches the string. Does not perform any I/O.
///
/// \param str The string to test
/// \param wc The wildcard to test against
/// \param leading_dots_fail_to_match if set, strings with leading dots are assumed to be hidden
/// files and are not matched
///
/// \return true if the wildcard matched
bool wildcard_match(const wcstring &str, const wcstring &wc,
                    bool leading_dots_fail_to_match = false);

/// Check if the specified string contains wildcards.
bool wildcard_has(const wcstring &, bool internal);
bool wildcard_has(const wchar_t *, bool internal);

/// Test wildcard completion.
bool wildcard_complete(const wcstring &str, const wchar_t *wc, const wchar_t *desc,
                       wcstring (*desc_func)(const wcstring &), std::vector<completion_t> *out,
                       expand_flags_t expand_flags, complete_flags_t flags);

#endif
