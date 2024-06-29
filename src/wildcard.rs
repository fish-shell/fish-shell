// Enumeration of all wildcard types.

use libc::X_OK;
use std::cmp::Ordering;
use std::collections::HashSet;
use std::fs;

use crate::common::{
    char_offset, is_windows_subsystem_for_linux, unescape_string, UnescapeFlags,
    UnescapeStringStyle, WILDCARD_RESERVED_BASE, WSL,
};
use crate::complete::{CompleteFlags, Completion, CompletionReceiver, PROG_COMPLETE_SEP};
use crate::expand::ExpandFlags;
use crate::fallback::wcscasecmp;
use crate::future_feature_flags::feature_test;
use crate::future_feature_flags::FeatureFlag;
use crate::wchar::prelude::*;
use crate::wcstringutil::{
    string_fuzzy_match_string, string_suffixes_string_case_insensitive, CaseFold,
};
use crate::wutil::dir_iter::DirEntryType;
use crate::wutil::{dir_iter::DirEntry, lwstat, waccess};
use once_cell::sync::Lazy;

static COMPLETE_EXEC_DESC: Lazy<&wstr> = Lazy::new(|| wgettext!("command"));
static COMPLETE_EXEC_LINK_DESC: Lazy<&wstr> = Lazy::new(|| wgettext!("command link"));
static COMPLETE_FILE_DESC: Lazy<&wstr> = Lazy::new(|| wgettext!("file"));
static COMPLETE_SYMLINK_DESC: Lazy<&wstr> = Lazy::new(|| wgettext!("symlink"));
static COMPLETE_DIRECTORY_SYMLINK_DESC: Lazy<&wstr> = Lazy::new(|| wgettext!("dir symlink"));
static COMPLETE_DIRECTORY_DESC: Lazy<&wstr> = Lazy::new(|| wgettext!("directory"));

/// Character representing any character except '/' (slash).
pub const ANY_CHAR: char = char_offset(WILDCARD_RESERVED_BASE, 0);
/// Character representing any character string not containing '/' (slash).
pub const ANY_STRING: char = char_offset(WILDCARD_RESERVED_BASE, 1);
/// Character representing any character string.
pub const ANY_STRING_RECURSIVE: char = char_offset(WILDCARD_RESERVED_BASE, 2);
/// This is a special pseudo-char that is not used other than to mark the
/// end of the special characters so we can sanity check the enum range.
#[allow(dead_code)]
pub const ANY_SENTINEL: char = char_offset(WILDCARD_RESERVED_BASE, 3);

#[derive(PartialEq)]
pub enum WildcardResult {
    /// The wildcard did not match.
    NoMatch,
    /// The wildcard did match.
    Match,
    /// Expansion was cancelled (e.g. control-C).
    Cancel,
    /// Expansion produced too many results.
    Overflow,
}

// This does something horrible refactored from an even more horrible function.
fn resolve_description(
    full_completion: &wstr,
    completion: &mut &wstr,
    expand_flags: ExpandFlags,
    description_func: Option<&dyn Fn(&wstr) -> WString>,
) -> WString {
    if let Some(complete_sep_loc) = completion.find_char(PROG_COMPLETE_SEP) {
        // This completion has an embedded description, do not use the generic description.
        let description = completion[complete_sep_loc + 1..].to_owned();
        *completion = &completion[..complete_sep_loc];
        return description;
    }

    if let Some(f) = description_func {
        if expand_flags.contains(ExpandFlags::GEN_DESCRIPTIONS) {
            return f(full_completion);
        }
    }

    WString::new()
}

// A transient parameter pack needed by wildcard_complete.
struct WcCompletePack<'orig, 'f> {
    pub orig: &'orig wstr,
    pub desc_func: Option<&'f dyn Fn(&wstr) -> WString>,
    pub expand_flags: ExpandFlags,
}

// Weirdly specific and non-reusable helper function that makes its one call site much clearer.
fn has_prefix_match(comps: &CompletionReceiver, first: usize) -> bool {
    comps[first..]
        .iter()
        .any(|c| c.r#match.is_exact_or_prefix() && c.r#match.case_fold == CaseFold::samecase)
}

/// Matches the string against the wildcard, and if the wildcard is a possible completion of the
/// string, the remainder of the string is inserted into the out vector.
///
/// We ignore ANY_STRING_RECURSIVE here. The consequence is that you cannot tab complete **
/// wildcards. This is historic behavior.
/// is_first_call is default false.
fn wildcard_complete_internal(
    s: &wstr,
    wc: &wstr,
    params: &WcCompletePack,
    flags: CompleteFlags,
    // it is easier to recurse with this over taking it by value
    mut out: Option<&mut CompletionReceiver>,
    is_first_call: bool,
) -> WildcardResult {
    // Maybe early out for hidden files. We require that the wildcard match these exactly (i.e. a
    // dot); ANY_STRING not allowed.
    if is_first_call
        && !params
            .expand_flags
            .contains(ExpandFlags::ALLOW_NONLITERAL_LEADING_DOT)
        && s.char_at(0) == '.'
        && wc.char_at(0) != '.'
    {
        return WildcardResult::NoMatch;
    }

    // Locate the next wildcard character position, e.g. ANY_CHAR or ANY_STRING.
    let next_wc_char_pos = wc
        .chars()
        .position(|c| matches!(c, ANY_CHAR | ANY_STRING | ANY_STRING_RECURSIVE));

    // Maybe we have no more wildcards at all. This includes the empty string.
    if next_wc_char_pos.is_none() {
        // Try matching
        let Some(m) = string_fuzzy_match_string(wc, s, false) else {
            return WildcardResult::NoMatch;
        };

        // If we're not allowing fuzzy match, then we require a prefix match.
        let needs_prefix_match = !params.expand_flags.contains(ExpandFlags::FUZZY_MATCH);
        if needs_prefix_match && !m.is_exact_or_prefix() {
            return WildcardResult::NoMatch;
        }

        // The match was successful. If the string is not requested we're done.
        let Some(out) = out else {
            return WildcardResult::Match;
        };

        // Wildcard complete.
        let full_replacement =
            m.requires_full_replacement() || flags.contains(CompleteFlags::REPLACES_TOKEN);

        // If we are not replacing the token, be careful to only store the part of the string after
        // the wildcard.
        assert!(!full_replacement || wc.len() <= s.len());
        let mut out_completion = match full_replacement {
            true => params.orig,
            false => s.slice_from(wc.len()),
        };
        let out_desc = resolve_description(
            params.orig,
            &mut out_completion,
            params.expand_flags,
            params.desc_func,
        );

        // Note: out_completion may be empty if the completion really is empty, e.g. tab-completing
        // 'foo' when a file 'foo' exists.
        let local_flags = if full_replacement {
            flags | CompleteFlags::REPLACES_TOKEN
        } else {
            flags
        };
        if !out.add(Completion::new(
            out_completion.to_owned(),
            out_desc,
            m,
            local_flags,
        )) {
            return WildcardResult::Overflow;
        }
        return WildcardResult::Match;
    } else if let Some(next_wc_char_pos @ 1..) = next_wc_char_pos {
        // The literal portion of a wildcard cannot be longer than the string itself,
        // e.g. `abc*` can never match a string that is only two characters long.
        if next_wc_char_pos >= s.len() {
            return WildcardResult::NoMatch;
        }

        let (s_pre, s_suf) = s.split_at(next_wc_char_pos);
        let (wc_pre, wc_suf) = wc.split_at(next_wc_char_pos);

        // Here we have a non-wildcard prefix. Note that we don't do fuzzy matching for stuff before
        // a wildcard, so just do case comparison and then recurse.
        if s_pre == wc_pre {
            // Normal match.
            return wildcard_complete_internal(s_suf, wc_suf, params, flags, out, false);
        }

        if wcscasecmp(s_pre, wc_pre) == Ordering::Equal {
            // Case insensitive match.
            return wildcard_complete_internal(
                s.slice_from(next_wc_char_pos),
                wc.slice_from(next_wc_char_pos),
                params,
                flags | CompleteFlags::REPLACES_TOKEN,
                out,
                false,
            );
        }

        return WildcardResult::NoMatch;
    }

    // Our first character is a wildcard.
    assert_eq!(next_wc_char_pos, Some(0));
    match wc.char_at(0) {
        ANY_CHAR => {
            if s.is_empty() {
                return WildcardResult::NoMatch;
            }
            return wildcard_complete_internal(
                s.slice_from(1),
                wc.slice_from(1),
                params,
                flags,
                out,
                false,
            );
        }
        ANY_STRING => {
            // Hackish. If this is the last character of the wildcard, then just complete with
            // the empty string. This fixes cases like "f*<tab>" -> "f*o".
            if wc.len() == 1 {
                return wildcard_complete_internal(L!(""), L!(""), params, flags, out, false);
            }

            // Try all submatches. Issue #929: if the recursive call gives us a prefix match,
            // just stop. This is sloppy - what we really want to do is say, once we've seen a
            // match of a particular type, ignore all matches of that type further down the
            // string, such that the wildcard produces the "minimal match.".
            let mut has_match = false;
            for i in 0..s.len() {
                let before_count = out.as_ref().map(|o| o.len()).unwrap_or_default();
                let submatch_res = wildcard_complete_internal(
                    s.slice_from(i),
                    wc.slice_from(1),
                    params,
                    flags,
                    out.as_deref_mut(),
                    false,
                );

                match submatch_res {
                    WildcardResult::NoMatch => continue,
                    WildcardResult::Match => {
                        has_match = true;
                        // If out is NULL, we don't care about the actual matches. If out is not
                        // NULL but we have a prefix match, stop there.
                        let Some(out) = out.as_mut() else {
                            return WildcardResult::Match;
                        };

                        if has_prefix_match(out, before_count) {
                            return WildcardResult::Match;
                        }
                        continue;
                    }
                    // Note early return
                    WildcardResult::Cancel | WildcardResult::Overflow => return submatch_res,
                }
            }

            return match has_match {
                true => WildcardResult::Match,
                false => WildcardResult::NoMatch,
            };
        }
        // We don't even try with this one.
        ANY_STRING_RECURSIVE => WildcardResult::NoMatch,
        _ => unreachable!(),
    }
}

pub fn wildcard_complete(
    s: &wstr,
    wc: &wstr,
    desc_func: Option<&dyn Fn(&wstr) -> WString>,
    out: Option<&mut CompletionReceiver>,
    expand_flags: ExpandFlags,
    flags: CompleteFlags,
) -> WildcardResult {
    let params = WcCompletePack {
        orig: s,
        desc_func,
        expand_flags,
    };

    return wildcard_complete_internal(s, wc, &params, flags, out, true);
}

/// Obtain a description string for the file specified by the filename.
///
/// It assumes the file exists and won't run stat() to confirm.
/// It assumes the file exists and won't run stat() to confirm.
/// The returned value is a string constant and should not be free'd.
///
/// \param filename The file for which to find a description string
/// \param is_dir Whether the file is a directory or not (might be behind a link)
/// \param is_link Whether it's a link (that might point to a directory)
/// \param definitely_executable Whether we know that it is executable, or don't know
fn file_get_desc(
    filename: &wstr,
    is_dir: bool,
    is_link: bool,
    definitely_executable: bool,
) -> &'static wstr {
    let is_executable =
        |filename: &wstr| -> bool { definitely_executable || waccess(filename, X_OK) == 0 };

    return if is_link {
        if is_dir {
            *COMPLETE_DIRECTORY_SYMLINK_DESC
        } else if is_executable(filename) {
            *COMPLETE_EXEC_LINK_DESC
        } else {
            *COMPLETE_SYMLINK_DESC
        }
    } else if is_dir {
        *COMPLETE_DIRECTORY_DESC
    } else if is_executable(filename) {
        *COMPLETE_EXEC_DESC
    } else {
        *COMPLETE_FILE_DESC
    };
}

/// Test if the given file is an executable (if executables_only) or directory (if
/// directories_only). If it matches, call wildcard_complete() with some description that we make
/// up. Note that the filename came from a readdir() call, so we know it exists.
fn wildcard_test_flags_then_complete(
    filepath: &wstr,
    filename: &wstr,
    wc: &wstr,
    expand_flags: ExpandFlags,
    out: &mut CompletionReceiver,
    entry: &DirEntry,
) -> bool {
    let executables_only = expand_flags.contains(ExpandFlags::EXECUTABLES_ONLY);
    let need_directory = expand_flags.contains(ExpandFlags::DIRECTORIES_ONLY);
    // Fast path: If we need directories, and we already know it is one,
    // and we don't need to do anything else, just return it.
    // This is a common case for cd completions, and removes the `stat` entirely in case the system
    // supports it.
    if entry.is_dir() && !executables_only && !expand_flags.contains(ExpandFlags::GEN_DESCRIPTIONS)
    {
        return wildcard_complete(
            &(filename.to_owned() + L!("/")),
            wc,
            Some(&|_| L!("").to_owned()),
            Some(out),
            expand_flags,
            CompleteFlags::NO_SPACE,
        ) == WildcardResult::Match;
    }
    // Check if it will match before stat().
    if wildcard_complete(
        filename,
        wc,
        None,
        None,
        expand_flags,
        CompleteFlags::default(),
    ) != WildcardResult::Match
    {
        return false;
    }

    if need_directory && !entry.is_dir() {
        return false;
    }

    if executables_only
        && is_windows_subsystem_for_linux(WSL::Any)
        && string_suffixes_string_case_insensitive(L!(".dll"), filename)
    {
        return false;
    }

    // regular file *excludes* broken links - we have no use for them as commands.
    let is_regular_file = entry
        .check_type()
        .map(|x| x == DirEntryType::reg)
        .unwrap_or(false);
    if executables_only && (!is_regular_file || waccess(filepath, X_OK) != 0) {
        return false;
    }

    // Compute the description.
    // This is effectively only for command completions,
    // because we disable descriptions for regular file completions.
    let desc = if expand_flags.contains(ExpandFlags::GEN_DESCRIPTIONS) {
        let is_link: bool = match entry.is_possible_link() {
            Some(n) => n,
            None => {
                // We do not know it's a link from the d_type,
                // so we will have to do an lstat().
                let lstat: Option<fs::Metadata> = lwstat(filepath).ok();
                if let Some(md) = &lstat {
                    md.is_symlink()
                } else {
                    // This file is no longer be usable, skip it.
                    return false;
                }
            }
        };
        // If we have executables_only, we already checked waccess above,
        // so we tell file_get_desc that this file is definitely executable so it can skip the check.
        Some(file_get_desc(filename, entry.is_dir(), is_link, executables_only).to_owned())
    } else {
        None
    };

    // Append a / if this is a directory. Note this requirement may be the only reason we have to
    // call stat() in some cases.
    let desc_func = |_: &wstr| match desc.as_ref() {
        Some(d) => d.to_owned(),
        None => WString::new(),
    };
    let desc_func: Option<&dyn Fn(&wstr) -> WString> = Some(&desc_func);
    if entry.is_dir() {
        return wildcard_complete(
            &(filename.to_owned() + L!("/")),
            wc,
            desc_func,
            Some(out),
            expand_flags,
            CompleteFlags::NO_SPACE,
        ) == WildcardResult::Match;
    }

    wildcard_complete(
        filename,
        wc,
        desc_func,
        Some(out),
        expand_flags,
        CompleteFlags::empty(),
    ) == WildcardResult::Match
}

use expander::WildCardExpander;
mod expander {
    use libc::F_OK;

    use crate::{
        common::scoped_push,
        path::append_path_component,
        wutil::{dir_iter::DirIter, normalize_path, FileId},
    };

    use super::*;

    pub struct WildCardExpander<'e> {
        /// A function to call to check cancellation.
        cancel_checker: &'e mut dyn FnMut() -> bool,
        /// The working directory to resolve paths against
        working_directory: &'e wstr,
        /// The set of items we have resolved, used to efficiently avoid duplication.
        completion_set: HashSet<WString>,
        /// The set of file IDs we have visited, used to avoid symlink loops.
        visited_files: HashSet<FileId>,
        /// Flags controlling expansion.
        flags: ExpandFlags,
        /// Resolved items get inserted into here. This is transient of course.
        resolved_completions: &'e mut CompletionReceiver,
        /// Whether we have been interrupted.
        did_interrupt: bool,
        /// Whether we have overflowed.
        did_overflow: bool,
        /// Whether we have successfully added any completions.
        did_add: bool,
        /// Whether some parent expansion is fuzzy, and therefore completions always prepend their prefix
        /// This variable is a little suspicious - it should be passed along, not stored here
        /// If we ever try to do parallel wildcard expansion we'll have to remove this
        has_fuzzy_ancestor: bool,
    }

    impl<'e> WildCardExpander<'e> {
        pub fn new(
            working_directory: &'e wstr,
            flags: ExpandFlags,
            cancel_checker: &'e mut dyn FnMut() -> bool,
            resolved_completions: &'e mut CompletionReceiver,
        ) -> Self {
            Self {
                cancel_checker,
                working_directory,
                completion_set: resolved_completions
                    .iter()
                    .map(|c| c.completion.to_owned())
                    .collect(),
                visited_files: HashSet::new(),
                flags,
                resolved_completions,
                did_add: false,
                did_interrupt: false,
                did_overflow: false,
                has_fuzzy_ancestor: false,
            }
        }

        /// The real implementation of wildcard expansion is in this function. Other functions are just
        /// wrappers around this one.
        ///
        /// This function traverses the relevant directory tree looking for matches, and recurses when
        /// needed to handle wildcards spanning multiple components and recursive wildcards.
        ///
        /// Args:
        /// base_dir: the "working directory" against which the wildcard is to be resolved
        /// wc: the wildcard string itself, e.g. foo*bar/baz (where * is actually ANY_CHAR)
        /// effective_prefix: the string that should be prepended for completions that replace their token.
        ///    This is usually the same thing as the original wildcard, but for fuzzy matching, we
        ///    expand intermediate segments. effective_prefix is always either empty, or ends with a slash
        pub fn expand(&mut self, base_dir: &wstr, wc: &wstr, effective_prefix: &wstr) {
            if self.interrupted_or_overflowed() {
                return;
            }

            // Get the current segment and compute interesting properties about it.
            let (wc_segment, wc_remainder) = if let Some(next_slash) = wc.find_char('/') {
                let (seg, rem) = wc.split_at(next_slash);
                let rem_without_slash = rem.slice_from(1);
                (seg, Some(rem_without_slash))
            } else {
                (wc, None)
            };
            let is_last_segment = wc_remainder.is_none();

            let segment_has_wildcards = wildcard_has_internal(wc_segment);

            if wc_segment.is_empty() {
                assert!(!segment_has_wildcards);
                if is_last_segment {
                    self.expand_trailing_slash(base_dir, effective_prefix);
                } else {
                    let mut prefix = effective_prefix.to_owned();
                    prefix.push('/');
                    self.expand(base_dir, wc_remainder.unwrap(), &prefix);
                }
            } else if !segment_has_wildcards && !is_last_segment {
                // Literal intermediate match. Note that we may not be able to actually read the directory
                // (issue #2099).
                let wc_remainder = wc_remainder.unwrap(); // TODO: if-let-chains

                // Absolute path of the intermediate directory
                let intermediate_dirpath: WString = base_dir.to_owned() + wc_segment + L!("/");

                // This just trumps everything
                let before = self.resolved_completions.len();
                let prefix: WString = effective_prefix.to_owned() + wc_segment + L!("/");
                self.expand(&intermediate_dirpath, wc_remainder, &prefix);

                // Maybe try a fuzzy match (#94) if nothing was found with the literal match. Respect
                // EXPAND_NO_DIRECTORY_ABBREVIATIONS (issue #2413).
                // Don't do fuzzy matches if the literal segment was valid (#3211)
                let allow_fuzzy = self.flags.contains(ExpandFlags::FUZZY_MATCH)
                    && !self.flags.contains(ExpandFlags::NO_FUZZY_DIRECTORIES);

                if allow_fuzzy
                    && self.resolved_completions.len() == before
                    && waccess(&intermediate_dirpath, F_OK) != 0
                {
                    assert!(self.flags.contains(ExpandFlags::FOR_COMPLETIONS));
                    if let Ok(mut base_dir_iter) = self.open_dir(base_dir, false) {
                        self.expand_literal_intermediate_segment_with_fuzz(
                            base_dir,
                            &mut base_dir_iter,
                            wc_segment,
                            wc_remainder,
                            effective_prefix,
                        );
                    }
                }
            } else {
                assert!(!wc_segment.is_empty() && (segment_has_wildcards || is_last_segment));

                if !is_last_segment && matches!(wc_segment.as_char_slice(), [ANY_STRING_RECURSIVE])
                {
                    // Hack for #7222. This is an intermediate wc segment that is exactly **. The
                    // tail matches in subdirectories as normal, but also the current directory.
                    // That is, '**/bar' may match 'bar' and 'foo/bar'.
                    // Implement this by matching the wildcard tail only, in this directory.
                    // Note if the segment is not exactly ANY_STRING_RECURSIVE then the segment may only
                    // match subdirectories.
                    self.expand(base_dir, wc_remainder.unwrap(), effective_prefix);
                    if self.interrupted_or_overflowed() {
                        return;
                    }
                }

                // return "." and ".." entries if we're doing completions
                let Ok(mut dir) = self.open_dir(
                    base_dir, /* return . and .. */
                    self.flags.contains(ExpandFlags::FOR_COMPLETIONS),
                ) else {
                    return;
                };

                if let Some(wc_remainder) = wc_remainder {
                    // Not the last segment, nonempty wildcard.
                    self.expand_intermediate_segment(
                        base_dir,
                        &mut dir,
                        wc_segment,
                        wc_remainder,
                        &(effective_prefix.to_owned() + wc_segment + L!("/")),
                    );
                } else {
                    // Last wildcard segment, nonempty wildcard.
                    self.expand_last_segment(base_dir, &mut dir, wc_segment, effective_prefix);
                }

                let Some(asr_idx) = wc_segment.find_char(ANY_STRING_RECURSIVE) else {
                    return;
                };

                // Apply the recursive **.
                // Construct a "head + any" wildcard for matching stuff in this directory, and an
                // "any + tail" wildcard for matching stuff in subdirectories. Note that the
                // ANY_STRING_RECURSIVE character is present in both the head and the tail.
                let head_any = wc_segment.slice_to(asr_idx + 1);
                let any_tail = wc.slice_from(asr_idx);
                assert!(head_any.chars().next_back().unwrap() == ANY_STRING_RECURSIVE);
                assert!(any_tail.chars().next().unwrap() == ANY_STRING_RECURSIVE);

                dir.rewind();
                self.expand_intermediate_segment(
                    base_dir,
                    &mut dir,
                    head_any,
                    any_tail,
                    effective_prefix,
                );
            }
        }

        pub fn status_code(&self) -> WildcardResult {
            if self.did_interrupt {
                return WildcardResult::Cancel;
            } else if self.did_overflow {
                return WildcardResult::Overflow;
            } else if self.did_add {
                WildcardResult::Match
            } else {
                WildcardResult::NoMatch
            }
        }
    }

    impl<'e> WildCardExpander<'e> {
        /// We are a trailing slash - expand at the end.
        fn expand_trailing_slash(&mut self, base_dir: &wstr, prefix: &wstr) {
            if self.interrupted_or_overflowed() {
                return;
            }

            if !self.flags.contains(ExpandFlags::FOR_COMPLETIONS) {
                // Trailing slash and not accepting incomplete, e.g. `echo /xyz/`. Insert this file after checking it exists.
                if waccess(base_dir, F_OK) == 0 {
                    self.add_expansion_result(base_dir.to_owned());
                }
                return;
            }
            // Trailing slashes and accepting incomplete, e.g. `echo /xyz/<tab>`. Everything is added.
            let Ok(mut dir) = self.open_dir(base_dir, false) else {
                return;
            };

            // wreaddir_resolving without the out argument is just wreaddir.
            // So we can use the information in case we need it.
            let need_dir = self.flags.contains(ExpandFlags::DIRECTORIES_ONLY);

            while let Some(Ok(entry)) = dir.next() {
                if self.interrupted_or_overflowed() {
                    break;
                }

                // Note that is_dir() may cause a stat() call.
                let known_dir = need_dir && entry.is_dir();
                if need_dir && !known_dir {
                    continue;
                };
                if !entry.name.is_empty() && !entry.name.starts_with('.') {
                    self.try_add_completion_result(
                        &(base_dir.to_owned() + entry.name.as_utfstr()),
                        &entry.name,
                        L!(""),
                        prefix,
                        entry,
                    );
                }
            }
        }

        /// Given a directory base_dir, which is opened as base_dir_iter, expand an intermediate segment
        /// of the wildcard. Treat ANY_STRING_RECURSIVE as ANY_STRING. wc_segment is the wildcard
        /// segment for this directory, wc_remainder is the wildcard for subdirectories,
        /// prefix is the prefix for completions.
        fn expand_intermediate_segment(
            &mut self,
            base_dir: &wstr,
            base_dir_iter: &mut DirIter,
            wc_segment: &wstr,
            wc_remainder: &wstr,
            prefix: &wstr,
        ) {
            let is_final = wc_remainder.is_empty() && !wc_segment.contains(ANY_STRING_RECURSIVE);
            while !self.interrupted_or_overflowed() {
                let Some(Ok(entry)) = base_dir_iter.next() else {
                    break;
                };
                // Note that it's critical we ignore leading dots here, else we may descend into . and ..
                if !wildcard_match(&entry.name, wc_segment, true) {
                    // Doesn't match the wildcard for this segment, skip it.
                    continue;
                }
                if !entry.is_dir() {
                    continue;
                }

                // Fast path: If this entry can't be a link (we know via d_type),
                // we don't need to protect against symlink loops.
                // This is *not* deduplication, we just don't want a loop.
                //
                // We only do this when we are the last `*/` component,
                // because we're a bit inconsistent on when we will enter loops.
                if is_final && !entry.is_possible_link().unwrap_or(true) {
                    let full_path: WString = base_dir.to_owned() + entry.name.as_utfstr() + L!("/");
                    let prefix: WString = prefix.to_owned() + wc_segment + L!("/");

                    self.expand(&full_path, wc_remainder, &prefix);
                    continue;
                }

                let Some(statbuf) = entry.stat() else {
                    continue;
                };

                let file_id = FileId::from_stat(&statbuf);
                if !self.visited_files.insert(file_id.clone()) {
                    // Symlink loop! This directory was already visited, so skip it.
                    continue;
                }

                let full_path: WString = base_dir.to_owned() + entry.name.as_utfstr() + L!("/");
                let prefix: WString = prefix.to_owned() + wc_segment + L!("/");

                self.expand(&full_path, wc_remainder, &prefix);

                // Now remove the visited file. This is for #2414: only directories "beneath" us should be
                // considered visited.
                self.visited_files.remove(&file_id);
            }
        }

        /// Given a directory base_dir, which is opened as base_dir_fp, expand an intermediate literal
        /// segment. Use a fuzzy matching algorithm.
        fn expand_literal_intermediate_segment_with_fuzz(
            &mut self,
            base_dir: &wstr,
            base_dir_iter: &mut DirIter,
            wc_segment: &wstr,
            wc_remainder: &wstr,
            prefix: &wstr,
        ) {
            // Mark that we are fuzzy for the duration of this function
            let mut zelf = scoped_push(self, |e| &mut e.has_fuzzy_ancestor, true);
            while !zelf.interrupted_or_overflowed() {
                let Some(Ok(entry)) = base_dir_iter.next() else {
                    break;
                };

                // Don't bother with . and ..
                if entry.name == "." || entry.name == ".." {
                    continue;
                }

                let Some(m) = string_fuzzy_match_string(wc_segment, &entry.name, false) else {
                    continue;
                };
                // The first port had !n.is_samecase_exact
                if m.is_samecase_exact() {
                    continue;
                }

                // Note is_dir() may trigger a stat call.
                if !entry.is_dir() {
                    continue;
                }

                // Determine the effective prefix for our children.
                // Normally this would be the wildcard segment, but here we know our segment doesn't have
                // wildcards ("literal") and we are doing fuzzy expansion, which means we replace the
                // segment with files found through fuzzy matching.
                let child_prefix: WString = prefix.to_owned() + entry.name.as_utfstr() + L!("/");
                let new_full_path: WString = base_dir.to_owned() + entry.name.as_utfstr() + L!("/");

                // Ok, this directory matches. Recurse to it. Then mark each resulting completion as fuzzy.
                let before = zelf.resolved_completions.len();
                zelf.expand(&new_full_path, wc_remainder, &child_prefix);
                let after = zelf.resolved_completions.len();

                assert!(before <= after);
                for c in zelf.resolved_completions[before..after].iter_mut() {
                    // Mark the completion as replacing.
                    if !c.replaces_token() {
                        c.flags |= CompleteFlags::REPLACES_TOKEN;
                        c.prepend_token_prefix(&child_prefix);
                    }

                    // And every match must be made at least as fuzzy as ours.
                    // TODO: justify this, tests do not exercise it yet.
                    if m.rank() > c.r#match.rank() {
                        // Our match is fuzzier.
                        c.r#match = m;
                    }
                }
            }
        }

        /// Given a directory base_dir, which is opened as base_dir_iter, expand the last segment of the
        /// wildcard. Treat ANY_STRING_RECURSIVE as ANY_STRING. wc is the wildcard segment to use for
        /// matching, wc_remainder is the wildcard for subdirectories, prefix is the prefix for
        /// completions.
        fn expand_last_segment(
            &mut self,
            base_dir: &wstr,
            base_dir_iter: &mut DirIter,
            wc: &wstr,
            prefix: &wstr,
        ) {
            let need_dir = self.flags.contains(ExpandFlags::DIRECTORIES_ONLY);

            while !self.interrupted_or_overflowed() {
                let Some(Ok(entry)) = base_dir_iter.next() else {
                    break;
                };

                if need_dir && !entry.is_dir() {
                    continue;
                }

                if self.flags.contains(ExpandFlags::FOR_COMPLETIONS) {
                    self.try_add_completion_result(
                        &(base_dir.to_owned() + entry.name.as_utfstr()),
                        &entry.name,
                        wc,
                        prefix,
                        entry,
                    );
                } else {
                    // Normal wildcard expansion, not for completions.
                    if wildcard_match(
                        &entry.name,
                        wc,
                        true, /* skip files with leading dots */
                    ) {
                        self.add_expansion_result(base_dir.to_owned() + entry.name.as_utfstr());
                    }
                }
            }
        }

        /// Indicate whether we should cancel wildcard expansion. This latches 'interrupt'.
        fn interrupted_or_overflowed(&mut self) -> bool {
            self.did_interrupt |= (self.cancel_checker)();
            self.did_interrupt || self.did_overflow
        }

        fn add_expansion_result(&mut self, result: WString) {
            // This function is only for the non-completions case.
            assert!(!self.flags.contains(ExpandFlags::FOR_COMPLETIONS));
            #[allow(clippy::collapsible_if)]
            if self.completion_set.insert(result.clone()) {
                if !self.resolved_completions.add(result) {
                    self.did_overflow = true;
                }
            }
        }

        // Given a start point as an absolute path, for any directory that has exactly one non-hidden
        // entity in it which is itself a directory, return that. The result is a relative path. For
        // example, if start_point is '/usr' we may return 'local/bin/'.
        //
        // The result does not have a leading slash, but does have a trailing slash if non-empty.
        fn descend_unique_hierarchy(&mut self, start_point: &mut WString) -> WString {
            assert!(!start_point.is_empty() && start_point.starts_with('/'));

            let mut unique_hierarchy = WString::new();
            let abs_unique_hierarchy = start_point;

            // Ensure we don't fall into a symlink loop.
            // Ideally we would compare both devices and inodes, but devices require a stat call, so we
            // use inodes exclusively.
            let mut visited_inodes: HashSet<libc::ino_t> = HashSet::new();

            loop {
                let mut unique_entry = WString::new();
                let Ok(mut dir) = DirIter::new(abs_unique_hierarchy) else {
                    break;
                };

                while let Some(Ok(entry)) = dir.next() {
                    if entry.name.is_empty() || entry.name.starts_with('.') {
                        // either hidden, or . and .. entries -- skip them
                        continue;
                    }
                    if !visited_inodes.insert(entry.inode) {
                        // Either we've visited this inode already or there's multiple files;
                        // either way stop.
                        break;
                    } else if entry.is_dir() && unique_entry.is_empty() {
                        // first candidate
                        unique_entry = entry.name.to_owned();
                    } else {
                        // We either have two or more candidates, or the child is not a directory. We're
                        // done.
                        unique_entry.clear();
                        break;
                    }
                }

                // We stop if we got two or more entries; also stop if we got zero or were interrupted
                if unique_entry.is_empty() || self.interrupted_or_overflowed() {
                    break;
                }

                append_path_component(&mut unique_hierarchy, &unique_entry);
                unique_hierarchy.push('/');

                append_path_component(abs_unique_hierarchy, &unique_entry);
                abs_unique_hierarchy.push('/');
            }

            return unique_hierarchy;
        }

        fn try_add_completion_result(
            &mut self,
            filepath: &wstr,
            filename: &wstr,
            wildcard: &wstr,
            prefix: &wstr,
            entry: &DirEntry,
        ) {
            // This function is only for the completions case.
            assert!(self.flags.contains(ExpandFlags::FOR_COMPLETIONS));
            let mut abs_path = self.working_directory.to_owned();
            append_path_component(&mut abs_path, filepath);

            // We must normalize the path to allow 'cd ..' to operate on logical paths.
            if self.flags.contains(ExpandFlags::SPECIAL_FOR_CD) {
                abs_path = normalize_path(&abs_path, true);
            }

            let before = self.resolved_completions.len();

            if wildcard_test_flags_then_complete(
                &abs_path,
                filename,
                wildcard,
                self.flags,
                self.resolved_completions,
                entry,
            ) {
                // Hack. We added this completion result based on the last component of the wildcard.
                // Prepend our prefix to each wildcard that replaces its token.
                // Note that prepend_token_prefix is a no-op unless COMPLETE_REPLACES_TOKEN is set
                let after = self.resolved_completions.len();
                for c in self.resolved_completions[before..after].iter_mut() {
                    if self.has_fuzzy_ancestor && !(c.flags.contains(CompleteFlags::REPLACES_TOKEN))
                    {
                        c.flags |= CompleteFlags::REPLACES_TOKEN;
                        c.prepend_token_prefix(wildcard);
                    }
                    c.prepend_token_prefix(prefix);
                }

                // Implement special_for_cd_autosuggestion by descending the deepest unique
                // hierarchy we can, and then appending any components to each new result.
                // Only descend deepest unique for cd autosuggest and not for cd tab completion
                // (issue #4402).
                if self
                    .flags
                    .contains(ExpandFlags::SPECIAL_FOR_CD_AUTOSUGGESTION)
                {
                    let unique_hierarchy = self.descend_unique_hierarchy(&mut abs_path);
                    if !unique_hierarchy.is_empty() {
                        for c in self.resolved_completions[before..after].iter_mut() {
                            c.completion.push_utfstr(&unique_hierarchy);
                        }
                    }
                }

                self.did_add = true;
            }
        }

        // Helper to resolve using our prefix.
        /// dotdot default is false
        fn open_dir(&self, base_dir: &wstr, dotdot: bool) -> std::io::Result<DirIter> {
            let mut path = self.working_directory.to_owned();
            append_path_component(&mut path, base_dir);
            if self.flags.contains(ExpandFlags::SPECIAL_FOR_CD) {
                // cd operates on logical paths.
                // for example, cd ../<tab> should complete "without resolving symlinks".
                path = normalize_path(&path, true);
            }

            return match dotdot {
                true => DirIter::new_with_dots(&path),
                false => DirIter::new(&path),
            };
        }
    }
}

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
pub fn wildcard_expand_string<'closure>(
    wc: &wstr,
    working_directory: &wstr,
    flags: ExpandFlags,
    mut cancel_checker: impl FnMut() -> bool + 'closure,
    output: &mut CompletionReceiver,
) -> WildcardResult {
    // Fuzzy matching only if we're doing completions.
    assert!(
        flags.contains(ExpandFlags::FOR_COMPLETIONS) || !flags.contains(ExpandFlags::FUZZY_MATCH)
    );

    // ExpandFlags::SPECIAL_FOR_CD requires expand_flag::DIRECTORIES_ONLY and
    // ExpandFlags::FOR_COMPLETIONS and !expand_flag::GEN_DESCRIPTIONS.
    assert!(
        !(flags.contains(ExpandFlags::SPECIAL_FOR_CD))
            || ((flags.contains(ExpandFlags::DIRECTORIES_ONLY))
                && (flags.contains(ExpandFlags::FOR_COMPLETIONS))
                && (!flags.contains(ExpandFlags::GEN_DESCRIPTIONS)))
    );

    // Hackish fix for issue #1631. We are about to call c_str(), which will produce a string
    // truncated at any embedded nulls. We could fix this by passing around the size, etc. However
    // embedded nulls are never allowed in a filename, so we just check for them and return 0 (no
    // matches) if there is an embedded null.
    if wc.contains('\0') {
        return WildcardResult::NoMatch;
    }

    // We do not support tab-completing recursive (**) wildcards. This is historic behavior.
    // Do not descend any directories if there is a ** wildcard.
    if flags.contains(ExpandFlags::FOR_COMPLETIONS) && wc.contains(ANY_STRING_RECURSIVE) {
        return WildcardResult::NoMatch;
    }

    // Compute the prefix and base dir. The prefix is what we prepend for filesystem operations
    // (i.e. the working directory), the base_dir is the part of the wildcard consumed thus far,
    // which we also have to append. The difference is that the base_dir is returned as part of the
    // expansion, and the prefix is not.
    //
    // Check for a leading slash. If we find one, we have an absolute path: the prefix is empty, the
    // base dir is /, and the wildcard is the remainder. If we don't find one, the prefix is the
    // working directory, the base dir is empty.
    let (prefix, base_dir, effective_wc) = if wc.starts_with(L!("/")) {
        (L!(""), L!("/"), wc.slice_from(1))
    } else {
        (working_directory, L!(""), wc)
    };

    let mut expander = WildCardExpander::new(prefix, flags, &mut cancel_checker, output);
    expander.expand(base_dir, effective_wc, base_dir);
    return expander.status_code();
}

/// Test whether the given wildcard matches the string. Does not perform any I/O.
///
/// \param str The string to test
/// \param wc The wildcard to test against
/// \param leading_dots_fail_to_match if set, strings with leading dots are assumed to be hidden
/// files and are not matched (default was false)
///
/// Return true if the wildcard matched
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
    if leading_dots_fail_to_match && (name == "." || name == "..") {
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
pub fn wildcard_has_internal(s: impl AsRef<wstr>) -> bool {
    s.as_ref()
        .chars()
        .any(|c| matches!(c, ANY_STRING | ANY_STRING_RECURSIVE | ANY_CHAR))
}

/// Check if the specified string contains wildcards (e.g. *).
#[must_use]
pub fn wildcard_has(s: impl AsRef<wstr>) -> bool {
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
