// Enumeration of all wildcard types.

use crate::common::{char_offset, CancelChecker, WILDCARD_RESERVED_BASE};
use crate::complete::CompletionReceiver;
use crate::expand::ExpandFlags;
use crate::wchar::wstr;

/// Character representing any character except '/' (slash).
pub const ANY_CHAR: char = char_offset(WILDCARD_RESERVED_BASE, 0);
/// Character representing any character string not containing '/' (slash).
pub const ANY_STRING: char = char_offset(WILDCARD_RESERVED_BASE, 1);
/// Character representing any character string.
pub const ANY_STRING_RECURSIVE: char = char_offset(WILDCARD_RESERVED_BASE, 2);
/// This is a special pseudo-char that is not used other than to mark the
/// end of the the special characters so we can sanity check the enum range.
pub const ANY_SENTINEL: char = char_offset(WILDCARD_RESERVED_BASE, 3);

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
#[derive(Eq, PartialEq)]
pub enum WildcardResult {
    /// The wildcard did not match.
    no_match,
    /// The wildcard did match.
    match_,
    /// Expansion was cancelled (e.g. control-C).
    cancel,
    /// Expansion produced too many results.
    overflow,
}

pub fn wildcard_expand_string(
    wc: &wstr,
    working_directory: &wstr,
    flags: ExpandFlags,
    cancel_checker: &CancelChecker,
    output: &mut CompletionReceiver,
) -> WildcardResult {
    todo!()
}

pub fn wildcard_has_internal(p: &wstr) -> bool {
    todo!()
}

pub fn wildcard_match(a: &wstr, b: &wstr) -> bool {
    todo!()
}
