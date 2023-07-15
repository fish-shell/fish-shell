// My own globbing implementation. Needed to implement this instead of using libs globbing to
// support tab-expansion of globbed parameters.
#ifndef FISH_WILDCARD_H
#define FISH_WILDCARD_H

#include <stddef.h>

#include "common.h"
#include "complete.h"
#include "expand.h"

/// Description for generic executable.
#define COMPLETE_EXEC_DESC _(L"command")
/// Description for link to executable.
#define COMPLETE_EXEC_LINK_DESC _(L"command link")
/// Description for character device.
#define COMPLETE_CHAR_DESC _(L"char device")
/// Description for block device.
#define COMPLETE_BLOCK_DESC _(L"block device")
/// Description for fifo buffer.
#define COMPLETE_FIFO_DESC _(L"fifo")
/// Description for fifo buffer.
#define COMPLETE_FILE_DESC _(L"file")
/// Description for symlink.
#define COMPLETE_SYMLINK_DESC _(L"symlink")
/// Description for symlink.
#define COMPLETE_DIRECTORY_SYMLINK_DESC _(L"dir symlink")
/// Description for Rotten symlink.
#define COMPLETE_BROKEN_SYMLINK_DESC _(L"broken symlink")
/// Description for symlink loop.
#define COMPLETE_LOOP_SYMLINK_DESC _(L"symlink loop")
/// Description for socket files.
#define COMPLETE_SOCKET_DESC _(L"socket")
/// Description for directories.
#define COMPLETE_DIRECTORY_DESC _(L"directory")

// Enumeration of all wildcard types.
enum {
    /// Character representing any character except '/' (slash).
    ANY_CHAR = WILDCARD_RESERVED_BASE,
    /// Character representing any character string not containing '/' (slash).
    ANY_STRING,
    /// Character representing any character string.
    ANY_STRING_RECURSIVE,
    /// This is a special pseudo-char that is not used other than to mark the
    /// end of the the special characters so we can sanity check the enum range.
    ANY_SENTINEL
};

/// Expand the wildcard by matching against the filesystem.
///
/// wildcard_expand works by dividing the wildcard into segments at each directory boundary. Each
/// segment is processed separately. All except the last segment are handled by matching the
/// wildcard segment against all subdirectories of matching directories, and recursively calling
/// wildcard_expand for matches. On the last segment, matching is made to any file, and all matches
/// are inserted to the list.
///
/// If wildcard_expand encounters any errors (such as insufficient privileges) during matching, no
/// error messages will be printed and wildcard_expand will continue the matching process.
///
/// \param wc The wildcard string
/// \param working_directory The working directory
/// \param flags flags for the search. Can be any combination of for_completions and
/// executables_only
/// \param output The list in which to put the output
///
enum class wildcard_result_t {
    no_match,  /// The wildcard did not match.
    match,     /// The wildcard did match.
    cancel,    /// Expansion was cancelled (e.g. control-C).
    overflow,  /// Expansion produced too many results.
};
wildcard_result_t wildcard_expand_string(const wcstring &wc, const wcstring &working_directory,
                                         expand_flags_t flags,
                                         const cancel_checker_t &cancel_checker,
                                         completion_receiver_t *output);

#if INCLUDE_RUST_HEADERS

#include "wildcard.rs.h"

#else
/// Test whether the given wildcard matches the string. Does not perform any I/O.
///
/// \param str The string to test
/// \param wc The wildcard to test against
/// \param leading_dots_fail_to_match if set, strings with leading dots are assumed to be hidden
/// files and are not matched
///
/// \return true if the wildcard matched
bool wildcard_match_ffi(const wcstring &str, const wcstring &wc, bool leading_dots_fail_to_match);

// Check if the string has any unescaped wildcards (e.g. ANY_STRING).
bool wildcard_has_internal(const wcstring &s);

/// Check if the specified string contains wildcards (e.g. *).
bool wildcard_has(const wcstring &s);

#endif

inline bool wildcard_match(const wcstring &str, const wcstring &wc,
                    bool leading_dots_fail_to_match = false) {
                        return wildcard_match_ffi(str, wc, leading_dots_fail_to_match);
                    }

inline bool wildcard_has(const wchar_t *s, size_t len) {
    return wildcard_has(wcstring(s, len));
};

/// Test wildcard completion.
wildcard_result_t wildcard_complete(const wcstring &str, const wchar_t *wc,
                                    const description_func_t &desc_func, completion_receiver_t *out,
                                    expand_flags_t expand_flags, complete_flags_t flags);

#endif
