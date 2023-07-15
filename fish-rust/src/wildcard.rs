// Enumeration of all wildcard types.

use cxx::CxxWString;

use crate::common::{
    char_offset, unescape_string, UnescapeFlags, UnescapeStringStyle, WILDCARD_RESERVED_BASE,
};
use crate::future_feature_flags::feature_test;
use crate::future_feature_flags::FeatureFlag;
use crate::wchar::prelude::*;
use crate::wchar_ffi::WCharFromFFI;

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
enum WildcardResult {
    /// The wildcard did not match.
    NoMatch,
    /// The wildcard did match.
    Match,
    /// Expansion was cancelled (e.g. control-C).
    Cancel,
    /// Expansion produced too many results.
    Overflow,
}

// pub fn wildcard_expand_string(wc: &wstr, working_directory: &wstr, flags: ExpandFlags, cancel_checker: impl CancelChecker, output: *mut completion_receiver_t) -> WildcardResult {
//     todo!()
// }

/// Test whether the given wildcard matches the string. Does not perform any I/O.
///
/// \param str The string to test
/// \param wc The wildcard to test against
/// \param leading_dots_fail_to_match if set, strings with leading dots are assumed to be hidden
/// files and are not matched (default was false)
///
/// \return true if the wildcard matched
#[must_use]
pub fn wildcard_match(
    name: impl AsRef<wstr>,
    pattern: impl AsRef<wstr>,
    leading_dots_fail_to_match: bool,
) -> bool {
    let name = name.as_ref();
    let pattern = pattern.as_ref();
    // Hackish fix for issue #270. Prevent wildcards from matching . or .., but we must still allow
    // literal matches.
    if leading_dots_fail_to_match && (name == L!(".") || name == L!("..")) {
        // The string is '.' or '..' so the only possible match is an exact match.
        return name == pattern;
    }

    // Near Linear implementation as proposed here https://research.swtch.com/glob.
    let mut px = 0;
    let mut nx = 0;
    let mut next_px = 0;
    let mut next_nx = 0;

    while px < pattern.len() || nx < name.len() {
        if px < pattern.len() {
            match pattern.char_at(px) {
                ANY_STRING | ANY_STRING_RECURSIVE => {
                    // Ignore hidden file
                    if leading_dots_fail_to_match && nx == 0 && name.char_at(0) == '.' {
                        return false;
                    }

                    // Common case of * at the end. In that case we can early out since we know it will
                    // match.
                    if px == pattern.len() - 1 {
                        return true;
                    }

                    // Try to match at nx.
                    // If that doesn't work out, restart at nx+1 next.
                    next_px = px;
                    next_nx = nx + 1;
                    px += 1;
                    continue;
                }
                ANY_CHAR => {
                    if nx < name.len() {
                        if nx == 0 && name.char_at(nx) == '.' {
                            return false;
                        }

                        px += 1;
                        nx += 1;
                        continue;
                    }
                }
                c => {
                    // ordinary char
                    if nx < name.len() && name.char_at(nx) == c {
                        px += 1;
                        nx += 1;
                        continue;
                    }
                }
            }
        }

        // Mismatch. Maybe restart.
        if 0 < next_nx && next_nx <= name.len() {
            px = next_px;
            nx = next_nx;
            continue;
        }
        return false;
    }
    // Matched all of pattern to all of name. Success.
    true
}

// Check if the string has any unescaped wildcards (e.g. ANY_STRING).
#[inline]
#[must_use]
fn wildcard_has_internal(s: impl AsRef<wstr>) -> bool {
    s.as_ref()
        .chars()
        .any(|c| matches!(c, ANY_STRING | ANY_STRING_RECURSIVE | ANY_CHAR))
}

/// Check if the specified string contains wildcards (e.g. *).
#[must_use]
fn wildcard_has(s: impl AsRef<wstr>) -> bool {
    let s = s.as_ref();
    let qmark_is_wild = !feature_test(FeatureFlag::qmark_noglob);
    // Fast check for * or ?; if none there is no wildcard.
    // Note some strings contain * but no wildcards, e.g. if they are quoted.
    if !s.contains('*') && (!qmark_is_wild || !s.contains('?')) {
        return false;
    }
    let unescaped =
        unescape_string(s, UnescapeStringStyle::Script(UnescapeFlags::SPECIAL)).unwrap_or_default();
    return wildcard_has_internal(unescaped);
}

/// Test wildcard completion.
// pub fn wildcard_complete(str: &wstr, wc: &wstr, desc_func: impl Fn(&wstr) -> WString, out: *mut completion_receiver_t, expand_flags: ExpandFlags, flags: CompleteFlags) -> bool {
//     todo!()
// }

#[cfg(test)]
mod tests {
    use super::*;
    use crate::future_feature_flags::scoped_test;

    #[test]
    fn test_wildcards() {
        assert!(!wildcard_has(L!("")));
        assert!(wildcard_has(L!("*")));
        assert!(!wildcard_has(L!("\\*")));

        let wc = L!("foo*bar");
        assert!(wildcard_has(wc) && !wildcard_has_internal(wc));
        let wc = unescape_string(wc, UnescapeStringStyle::Script(UnescapeFlags::SPECIAL)).unwrap();
        assert!(!wildcard_has(&wc) && wildcard_has_internal(&wc));

        scoped_test(FeatureFlag::qmark_noglob, false, || {
            assert!(wildcard_has(L!("?")));
            assert!(!wildcard_has(L!("\\?")));
        });

        scoped_test(FeatureFlag::qmark_noglob, true, || {
            assert!(!wildcard_has(L!("?")));
            assert!(!wildcard_has(L!("\\?")));
        });
    }
}

#[cxx::bridge]
mod ffi {
    extern "C++" {
        include!("wutil.h");
    }
    extern "Rust" {
        #[cxx_name = "wildcard_match_ffi"]
        fn wildcard_match_ffi(
            str: &CxxWString,
            wc: &CxxWString,
            leading_dots_fail_to_match: bool,
        ) -> bool;

        #[cxx_name = "wildcard_has"]
        fn wildcard_has_ffi(s: &CxxWString) -> bool;

        #[cxx_name = "wildcard_has_internal"]
        fn wildcard_has_internal_ffi(s: &CxxWString) -> bool;
    }
}

fn wildcard_match_ffi(str: &CxxWString, wc: &CxxWString, leading_dots_fail_to_match: bool) -> bool {
    wildcard_match(str.from_ffi(), wc.from_ffi(), leading_dots_fail_to_match)
}

fn wildcard_has_ffi(s: &CxxWString) -> bool {
    wildcard_has(s.from_ffi())
}

fn wildcard_has_internal_ffi(s: &CxxWString) -> bool {
    wildcard_has_internal(s.from_ffi())
}
