// Enumeration of all wildcard types.

use std::cmp::Ordering;
use std::collections::HashSet;
use std::hash::Hash;
use std::io::ErrorKind;
use std::os::unix::prelude::{FileTypeExt, MetadataExt};

use libc::{
    ELOOP, F_OK, S_IFBLK, S_IFCHR, S_IFIFO, S_IFMT, S_IFSOCK, S_IXGRP, S_IXOTH, S_IXUSR, X_OK,
};
use once_cell::sync::Lazy;

use crate::common::{
    char_offset, format_size, is_windows_subsystem_for_linux, unescape_string, CancelChecker,
    ScopeGuard, UnescapeFlags, UnescapeStringStyle, WILDCARD_RESERVED_BASE,
};
use crate::complete::{
    const_desc, CompleteFlags, Completion, CompletionReceiver, DescriptionFunc, PROG_COMPLETE_SEP,
};
use crate::expand::ExpandFlags;
use crate::fallback::wcscasecmp;
use crate::future_feature_flags::{feature_test, FeatureFlag};
use crate::path::append_path_component;
use crate::wchar::{wstr, WString, L};
use crate::wchar_ext::WExt;
use crate::wcstringutil::{
    string_fuzzy_match_string, string_prefixes_string, string_suffixes_string_case_insensitive,
    CaseFold, ContainType,
};
use crate::wutil::{lwstat, normalize_path, waccess, wgettext, wstat, DirIter, FileId};

static COMPLETE_EXEC_DESC: Lazy<&wstr> = Lazy::new(|| wgettext!("command"));
static COMPLETE_EXEC_LINK_DESC: Lazy<&wstr> = Lazy::new(|| wgettext!("command link"));
static COMPLETE_CHAR_DESC: Lazy<&wstr> = Lazy::new(|| wgettext!("char device"));
static COMPLETE_BLOCK_DESC: Lazy<&wstr> = Lazy::new(|| wgettext!("block device"));
static COMPLETE_FIFO_DESC: Lazy<&wstr> = Lazy::new(|| wgettext!("fifo"));
static COMPLETE_FILE_DESC: Lazy<&wstr> = Lazy::new(|| wgettext!("file"));
static COMPLETE_SYMLINK_DESC: Lazy<&wstr> = Lazy::new(|| wgettext!("symlink"));
static COMPLETE_DIRECTORY_SYMLINK_DESC: Lazy<&wstr> = Lazy::new(|| wgettext!("dir symlink"));
static COMPLETE_BROKEN_SYMLINK_DESC: Lazy<&wstr> = Lazy::new(|| wgettext!("broken symlink"));
static COMPLETE_LOOP_SYMLINK_DESC: Lazy<&wstr> = Lazy::new(|| wgettext!("symlink loop"));
static COMPLETE_SOCKET_DESC: Lazy<&wstr> = Lazy::new(|| wgettext!("socket"));
static COMPLETE_DIRECTORY_DESC: Lazy<&wstr> = Lazy::new(|| wgettext!("directory"));

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

/// Finds an internal ([`ANY_STRING`], etc.) style wildcard, or `None`.
fn wildcard_find(s: &wstr) -> Option<usize> {
    s.chars()
        .position(|c| [ANY_CHAR, ANY_STRING, ANY_STRING_RECURSIVE].contains(&c))
}

/// Check if the string has any unescaped wildcards (e.g. [`ANY_STRING`]).
pub fn wildcard_has_internal(s: &wstr) -> bool {
    wildcard_find(s).is_some()
}

/// Check if the specified string contains wildcards (e.g. `*`).
pub fn wildcard_has(s: &wstr) -> bool {
    let qmark_is_wild = !feature_test(FeatureFlag::qmark_noglob);
    // Fast check for * or ?; if none there is no wildcard.
    // Note some strings contain * but no wildcards, e.g. if they are quoted.
    if !s.as_char_slice().contains(&'*') && (!qmark_is_wild || !s.as_char_slice().contains(&'?')) {
        return false;
    }

    if let Some(unescaped) = unescape_string(s, UnescapeStringStyle::Script(UnescapeFlags::SPECIAL))
    {
        wildcard_has_internal(&unescaped)
    } else {
        false
    }
}

/// Tests whether the given `wildcard` matches the string `s`. Does not perform any I/O.
/// If `leading_dots_fail_to_match` is set, strings with leading dots are assumed to be hidden
/// files and are not matched.
pub fn wildcard_match(s: &wstr, wildcard: &wstr, leading_dots_fail_to_match: bool) -> bool {
    // Hackish fix for issue #270. Prevent wildcards from matching . or .., but we must still allow
    // literal matches.
    if leading_dots_fail_to_match && (s == L!(".") || s == L!("..")) {
        // The string is '.' or '..' so the only possible match is an exact match.
        return s == wildcard;
    }

    // Near Linear implementation as proposed here https://research.swtch.com/glob.
    let s = s.as_char_slice();
    let wildcard = wildcard.as_char_slice();

    let mut wc_idx = 0;
    let mut s_idx = 0;

    let mut restart_wc_idx = 0;
    let mut restart_s_idx = 0;
    let mut restart_is_out_of_str = false;

    while wc_idx < wildcard.len() || s_idx < wildcard.len() {
        let is_first = s_idx == 0;
        if wc_idx < wildcard.len() {
            if wildcard[wc_idx] == ANY_STRING || wildcard[wc_idx] == ANY_STRING_RECURSIVE {
                // Ignore hidden file
                if leading_dots_fail_to_match && is_first && s.starts_with(&['.']) {
                    return false;
                }

                // Common case of * at the end. In that case we can early out since we know it will
                // match.
                if wc_idx == wildcard.len() - 1 {
                    return true;
                }
                // Try to match at str_x.
                // If that doesn't work out, restart at str_x+1 next.
                restart_wc_idx = wc_idx;
                restart_s_idx = s_idx + 1;
                restart_is_out_of_str = s_idx >= s.len();
                wc_idx += 1;
                continue;
            } else if wildcard[wc_idx] == ANY_CHAR && s_idx < wildcard.len() {
                if is_first && s.starts_with(&['.']) {
                    return false;
                }
                wc_idx += 1;
                s_idx += 1;
                continue;
            } else if s_idx < wildcard.len() && s[s_idx] == wildcard[wc_idx] {
                // ordinary character
                wc_idx += 1;
                s_idx += 1;
                continue;
            }
        }
        // Mismatch. Maybe restart.
        if restart_s_idx != 0 && !restart_is_out_of_str {
            wc_idx = restart_wc_idx;
            s_idx = restart_s_idx;
            continue;
        }
        return false;
    }

    // Matched all of pattern to all of name. Success.
    true
}

/// This does something horrible refactored from an even more horrible function.
fn resolve_description(
    full_completion: &wstr,
    completion: &mut WString,
    expand_flags: ExpandFlags,
    desc_func: &DescriptionFunc,
) -> WString {
    if let Some(complete_sep_loc) = completion.chars().position(|c| c == PROG_COMPLETE_SEP) {
        // This completion has an embedded description, do not use the generic description.
        let description = completion[complete_sep_loc + 1..].to_owned();
        completion.truncate(complete_sep_loc);
        return description;
    }
    if expand_flags.contains(ExpandFlags::GEN_DESCRIPTIONS) {
        return desc_func(full_completion);
    }
    WString::new()
}

/// A transient parameter pack needed by wildcard_complete.
struct WcCompletePack<'a> {
    /// The original string, transient
    orig: &'a wstr,
    /// Function for generating descriptions
    desc_func: &'a DescriptionFunc<'a>,
    expand_flags: ExpandFlags,
}

/// Weirdly specific and non-reusable helper function that makes its one call site much clearer.
fn has_prefix_match(comps: &CompletionReceiver, first: usize) -> bool {
    let after_count = comps.size();

    comps.get_list().iter().skip(first).any(|comp| {
        let m = &comp.match_;

        matches!(m.typ, ContainType::exact | ContainType::prefix)
            && m.case_fold == CaseFold::samecase
    })
}

/// Matches the string against the wildcard, and if the wildcard is a possible completion of the
/// string, the remainder of the string is inserted into the out vector.
///
/// We ignore [`ANY_STRING_RECURSIVE`] here. The consequence is that you cannot tab complete `**`
/// wildcards. This is historic behavior.
fn wildcard_complete_internal(
    s: &wstr,
    wc: &wstr,
    params: &WcCompletePack,
    flags: CompleteFlags,
    mut out: Option<&mut CompletionReceiver>,
    is_first_call: bool,
) -> WildcardResult {
    // Maybe early out for hidden files. We require that the wildcard match these exactly (i.e. a
    // dot); ANY_STRING not allowed.
    if is_first_call
        && !params
            .expand_flags
            .contains(ExpandFlags::ALLOW_NONLITERAL_LEADING_DOT)
        && s.starts_with('.')
        && !wc.starts_with('.')
    {
        return WildcardResult::no_match;
    }

    // Locate the next wildcard character position, e.g. ANY_CHAR or ANY_STRING.
    let next_wc_char_pos = wildcard_find(wc);

    // Maybe we have no more wildcards at all. This includes the empty string.
    if next_wc_char_pos.is_none() {
        // Try matching.
        let Some(match_) = string_fuzzy_match_string(wc, s, false) else {
            return WildcardResult::no_match;
        };

        // If we're not allowing fuzzy match, then we require a prefix match.
        let needs_prefix_match = !params.expand_flags.contains(ExpandFlags::FUZZY_MATCH);
        if needs_prefix_match && !match_.is_exact_or_prefix() {
            return WildcardResult::no_match;
        }

        // The match was successful. If the string is not requested we're done.
        let Some(out) = out else {
            return WildcardResult::match_;
        };

        // Wildcard complete.
        let full_replacement =
            match_.requires_full_replacement() || flags.contains(CompleteFlags::REPLACES_TOKEN);

        // If we are not replacing the token, be careful to only store the part of the string after
        // the wildcard.
        assert!(!full_replacement || wc.len() <= s.len());
        let out_completion = if full_replacement {
            params.orig
        } else {
            s.slice_from(wc.len())
        };
        let mut out_completion = out_completion.to_owned();
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
        if !out.add(Completion {
            completion: out_completion,
            description: out_desc,
            flags: local_flags,
            match_,
        }) {
            return WildcardResult::overflow;
        }
        return WildcardResult::match_;
    } else if let Some(next_wc_char_pos @ 1..) = next_wc_char_pos {
        // The literal portion of a wildcard cannot be longer than the string itself,
        // e.g. `abc*` can never match a string that is only two characters long.
        if next_wc_char_pos >= s.len() {
            return WildcardResult::no_match;
        }

        // Here we have a non-wildcard prefix. Note that we don't do fuzzy matching for stuff before
        // a wildcard, so just do case comparison and then recurse.
        let (s_pre, s_suf) = s.split_at(next_wc_char_pos);
        let (wc_pre, wc_suf) = wc.split_at(next_wc_char_pos);

        if s_pre == wc_pre {
            // Normal match.
            return wildcard_complete_internal(s_suf, wc_suf, params, flags, out, false);
        }
        if wcscasecmp(s_pre, wc_pre) == Ordering::Equal {
            // Case insensitive match.
            return wildcard_complete_internal(
                s_suf,
                wc_suf,
                params,
                flags | CompleteFlags::REPLACES_TOKEN,
                out,
                false,
            );
        }
        return WildcardResult::no_match;
    }

    // Our first character is a wildcard.
    assert_eq!(next_wc_char_pos, Some(0));
    match wc.char_at(0) {
        ANY_CHAR => {
            if s.is_empty() {
                return WildcardResult::no_match;
            }
            wildcard_complete_internal(s.slice_from(1), wc.slice_from(1), params, flags, out, false)
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
            for suf in (0..s.len()).map(|i| s.slice_from(i)) {
                let before_count = out.as_ref().map(|r| r.size()).unwrap_or(0);
                let submatch_res = wildcard_complete_internal(
                    suf,
                    wc.slice_from(1),
                    params,
                    flags,
                    out.as_deref_mut(),
                    false,
                );
                match submatch_res {
                    WildcardResult::no_match => {}
                    WildcardResult::match_ => {
                        has_match = true;
                        // If out is None, we don't care about the actual matches. If out is not
                        // None but we have a prefix match, stop there.
                        let Some(out) = out.as_mut() else {
                            return WildcardResult::match_;
                        };
                        if has_prefix_match(out, before_count) {
                            return WildcardResult::match_;
                        }
                    }
                    WildcardResult::cancel | WildcardResult::overflow => {
                        // Note early return.
                        return submatch_res;
                    }
                }
            }
            if has_match {
                WildcardResult::match_
            } else {
                WildcardResult::no_match
            }
        }
        ANY_STRING_RECURSIVE => {
            // We don't even try with this one.
            return WildcardResult::no_match;
        }
        _ => unreachable!(),
    }
}

/// Test wildcard completion.
pub fn wildcard_complete(
    s: &wstr,
    wc: &wstr,
    desc_func: &DescriptionFunc,
    out: Option<&mut CompletionReceiver>,
    expand_flags: ExpandFlags,
    flags: CompleteFlags,
) -> WildcardResult {
    let params = WcCompletePack {
        orig: s,
        desc_func,
        expand_flags,
    };
    wildcard_complete_internal(s, wc, &params, flags, out, true /* first call */)
}

/// Obtain a description string for the file specified by the filename.
///
/// The returned value is a string constant and should not be free'd.
///
/// - `filename`: The file for which to find a description string
/// - `lstat_res`: The result of calling lstat on the file
/// - `stat_res`: The result of calling stat on the file (or None if file is not a symlink)
/// - `err`: The errno value after a failed stat call on the file.
fn file_get_desc(
    filename: &wstr,
    lstat_res: Result<std::fs::Metadata, std::io::Error>,
    stat_res: Option<Result<std::fs::Metadata, std::io::Error>>,
) -> &'static wstr {
    let Ok(lbuf) = lstat_res else {
        return *COMPLETE_FILE_DESC;
    };

    fn is_executable(buf: &std::fs::Metadata, filename: &wstr) -> bool {
        // Weird group permissions and other such issues make it non-trivial to find out if
        // we can actually execute a file using the result from stat. It is much safer to
        // use the access function, since it tells us exactly what we want to know.
        buf.mode() & (S_IXUSR | S_IXGRP | S_IXOTH) != 0 && waccess(filename, X_OK) == 0
    }

    if let Some(stat_res) = stat_res {
        // A symlink, use stat result
        match stat_res {
            Ok(buf) => {
                if buf.is_dir() {
                    return *COMPLETE_DIRECTORY_SYMLINK_DESC;
                }
                if is_executable(&buf, filename) {
                    return *COMPLETE_EXEC_LINK_DESC;
                }

                return *COMPLETE_SYMLINK_DESC;
            }
            Err(e) if e.kind() == ErrorKind::NotFound => return *COMPLETE_BROKEN_SYMLINK_DESC,
            // https://github.com/rust-lang/rust/issues/86442
            Err(e) if e.raw_os_error() == Some(ELOOP) => return *COMPLETE_LOOP_SYMLINK_DESC,
            // On unknown errors we do nothing. The file will be given the default 'File'
            // description or one based on the suffix.
            _ => {}
        }
    } else {
        let ft = lbuf.file_type();

        if ft.is_char_device() {
            return *COMPLETE_CHAR_DESC;
        } else if ft.is_block_device() {
            return *COMPLETE_BLOCK_DESC;
        } else if ft.is_fifo() {
            return *COMPLETE_FIFO_DESC;
        } else if ft.is_socket() {
            return *COMPLETE_SOCKET_DESC;
        } else if ft.is_dir() {
            return *COMPLETE_DIRECTORY_DESC;
        } else if is_executable(&lbuf, filename) {
            return *COMPLETE_EXEC_DESC;
        }
    }

    *COMPLETE_FILE_DESC
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
    known_dir: bool,
) -> bool {
    let executables_only = expand_flags.contains(ExpandFlags::EXECUTABLES_ONLY);
    let need_directory = expand_flags.contains(ExpandFlags::DIRECTORIES_ONLY);
    // Fast path: If we need directories, and we already know it is one,
    // and we don't need to do anything else, just return it.
    // This is a common case for cd completions, and removes the `stat` entirely in case the system
    // supports it.
    if known_dir && !executables_only && !expand_flags.contains(ExpandFlags::GEN_DESCRIPTIONS) {
        let mut filename = filename.to_owned();
        filename.push('/');
        return wildcard_complete(
            &filename,
            wc,
            &const_desc(L!("")),
            Some(out),
            expand_flags,
            CompleteFlags::NO_SPACE,
        ) == WildcardResult::match_;
    }
    // Check if it will match before stat().
    if wildcard_complete(
        filename,
        wc,
        &{ Box::new(|_: &wstr| -> WString { unreachable!() }) as _ },
        None,
        expand_flags,
        CompleteFlags::empty(),
    ) != WildcardResult::match_
    {
        return false;
    }

    let lstat_res = lwstat(filepath);
    let stat_res = if let Ok(lstat_buf) = lstat_res.as_ref() {
        if lstat_buf.is_symlink() {
            Some(wstat(filepath))
        } else {
            None
        }
    } else {
        None
    };

    let res = stat_res.as_ref().unwrap_or(&lstat_res);

    let (file_size, is_directory, is_executable) = if let Ok(buf) = res {
        (buf.len(), buf.is_dir(), buf.is_file())
    } else {
        (0, false, false)
    };

    if need_directory && !is_directory {
        return false;
    }

    if executables_only && (!is_executable || waccess(filepath, X_OK) != 0) {
        return false;
    }

    if executables_only
        && is_windows_subsystem_for_linux()
        && string_suffixes_string_case_insensitive(L!(".dll"), filename)
    {
        return false;
    }

    // Compute the description.
    let desc = if expand_flags.contains(ExpandFlags::GEN_DESCRIPTIONS) {
        let mut desc = file_get_desc(filepath, lstat_res, stat_res).to_owned();

        if !is_directory && !is_executable {
            if !desc.is_empty() {
                desc.push_utfstr(L!(", "));
            }
            desc.push_utfstr(&format_size(file_size.try_into().unwrap()));
        }

        Some(desc)
    } else {
        None
    };

    // Append a / if this is a directory. Note this requirement may be the only reason we have to
    // call stat() in some cases.
    let desc_func = const_desc(desc.as_deref().unwrap_or(L!("")));
    if is_directory {
        let mut filename = filename.to_owned();
        filename.push('/');
        wildcard_complete(
            &filename,
            wc,
            &desc_func,
            Some(out),
            expand_flags,
            CompleteFlags::NO_SPACE,
        ) == WildcardResult::match_
    } else {
        wildcard_complete(
            filename,
            wc,
            &desc_func,
            Some(out),
            expand_flags,
            CompleteFlags::empty(),
        ) == WildcardResult::match_
    }
}

struct WildcardExpander<'a> {
    /// A function to call to check cancellation.
    cancel_checker: &'a CancelChecker,
    /// The working directory to resolve paths against
    working_directory: &'a wstr,
    /// The set of items we have resolved, used to efficiently avoid duplication.
    completion_set: HashSet<WString>,
    /// The set of file IDs we have visited, used to avoid symlink loops.
    visited_files: HashSet<FileId>,
    /// Flags controlling expansion.
    flags: ExpandFlags,
    /// Resolved items get inserted into here. This is transient of course.
    resolved_completions: &'a mut CompletionReceiver,
    /// Whether we have been interrupted.
    did_interrupt: bool, // false
    /// Whether we have overflowed.
    did_overflow: bool, // false
    /// Whether we have successfully added any completions.
    did_add: bool, // false
    /// Whether some parent expansion is fuzzy, and therefore completions always prepend their prefix
    /// This variable is a little suspicious - it should be passed along, not stored here
    /// If we ever try to do parallel wildcard expansion we'll have to remove this
    has_fuzzy_ancestor: bool, // false
}

impl<'a> WildcardExpander<'a> {
    /// We are a trailing slash - expand at the end.
    fn expand_trailing_slash(&mut self, base_dir: &wstr, prefix: &wstr) {
        if self.interrupted_or_overflowed() {
            return;
        }

        if !self.flags.contains(ExpandFlags::FOR_COMPLETIONS) {
            // Trailing slash and not accepting incomplete, e.g. `echo /xyz/`. Insert this file if it
            // exists.
            if waccess(base_dir, F_OK) == 0 {
                self.add_expansion_result(base_dir.to_owned())
            }
        } else {
            // Trailing slashes and accepting incomplete, e.g. `echo /xyz/<tab>`. Everything is added.
            let mut dir = self.open_dir(base_dir, false);
            if dir.valid() {
                // wreaddir_resolving without the out argument is just wreaddir.
                // So we can use the information in case we need it.
                let need_dir = self.flags.contains(ExpandFlags::DIRECTORIES_ONLY);
                while let Some(entry) = dir.next() {
                    if self.interrupted_or_overflowed() {
                        break;
                    }
                    // Note that is_dir() may cause a stat() call.
                    let known_dir = need_dir && entry.is_dir();
                    if need_dir && !known_dir {
                        continue;
                    }
                    if !entry.name.is_empty() && entry.name.starts_with('.') {
                        let mut dir = base_dir.to_owned();
                        dir.push_utfstr(&entry.name);
                        self.try_add_completion_result(
                            &dir,
                            &entry.name,
                            L!(""),
                            prefix,
                            known_dir,
                        );
                    }
                }
            }
        }
    }

    /// Given a directory `base_dir`, which is opened as `base_dir_iter`, expand an intermediate
    /// segment of the wildcard. Treat [`ANY_STRING_RECURSIVE`] as [`ANY_STRING`]. `wc_segment` is
    /// the wildcard segment for this directory, `wc_remainder` is the wildcard for subdirectories,
    /// `prefix` is the prefix for completions.
    fn expand_intermediate_segment(
        &mut self,
        base_dir: &wstr,
        base_dir_iter: &mut DirIter,
        wc_segment: &wstr,
        wc_remainder: &wstr,
        prefix: &wstr,
    ) {
        while let Some(entry) = base_dir_iter.next() {
            if self.interrupted_or_overflowed() {
                break;
            }

            // Note that it's critical we ignore leading dots here, else we may descend into . and ..
            if !wildcard_match(&entry.name, wc_segment, true) {
                // Doesn't match the wildcard for this segment, skip it.
                continue;
            }

            if !entry.is_dir() {
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

            // We made it through. Perform normal wildcard expansion on this new directory, starting at
            // our tail_wc, which includes the ANY_STRING_RECURSIVE guy.
            let mut full_path = base_dir.to_owned();
            full_path.push_utfstr(&entry.name);
            full_path.push('/');
            let mut prefix = prefix.to_owned();
            prefix.push_utfstr(wc_segment);
            prefix.push('/');
            self.expand(&full_path, wc_remainder, &prefix);

            // Now remove the visited file. This is for #2414: only directories "beneath" us should be
            // considered visited.
            self.visited_files.remove(&file_id);
        }
    }

    /// Given a directory `base_dir`, which is opened as `base_dir_iter`, expand an intermediate
    /// literal segment. Use a fuzzy matching algorithm.
    fn expand_intermediate_segment_with_fuzz(
        &mut self,
        base_dir: &wstr,
        base_dir_iter: &mut DirIter,
        wc_segment: &wstr,
        wc_remainder: &wstr,
        prefix: &wstr,
    ) {
        // Mark that we are fuzzy for the duration of this function
        let stored_fuzzy_ancestor = self.has_fuzzy_ancestor;
        let mut self_ = ScopeGuard::new(self, |e| e.has_fuzzy_ancestor = stored_fuzzy_ancestor);
        self_.has_fuzzy_ancestor = true;
        while let Some(entry) = base_dir_iter.next() {
            if self_.interrupted_or_overflowed() {
                break;
            }

            // Don't bother with . and ..
            if entry.name == L!(".") || entry.name == L!("..") {
                continue;
            }

            let Some(match_) = string_fuzzy_match_string(wc_segment, &entry.name, false) else {
                continue;
            };
            if !match_.is_samecase_exact() {
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
            let mut child_prefix = prefix.to_owned();
            child_prefix.push_utfstr(&entry.name);
            child_prefix.push('/');
            let mut new_full_path = base_dir.to_owned();
            new_full_path.push_utfstr(&entry.name);
            new_full_path.push('/');

            // Ok, this directory matches. Recurse to it. Then mark each resulting completion as fuzzy.
            let before = self_.resolved_completions.size();
            self_.expand(&new_full_path, wc_remainder, &child_prefix);
            let after = self_.resolved_completions.size();

            assert!(before <= after);
            for i in before..after {
                let c = &mut self_.resolved_completions.get_list_mut()[i];
                // Mark the completion as replacing.
                if !c.flags.contains(CompleteFlags::REPLACES_TOKEN) {
                    c.flags |= CompleteFlags::REPLACES_TOKEN;
                    c.prepend_token_prefix(&child_prefix);
                }
                // And every match must be made at least as fuzzy as ours.
                // TODO: justify this, tests do not exercise it yet.
                if match_.rank() > c.match_.rank() {
                    // Our match is fuzzier.
                    c.match_ = match_;
                }
            }
        }
    }

    /// Given a directory `base_dir`, which is opened as `base_dir_iter`, expand the last segment of
    /// the wildcard. Treat [`ANY_STRING_RECURSIVE`] as [`ANY_STRING`]. `wc` is the wildcard segment
    /// to use for matching, `prefix` is the prefix for completions.
    fn expand_last_segment(
        &mut self,
        base_dir: &wstr,
        base_dir_iter: &mut DirIter,
        wc: &wstr,
        prefix: &wstr,
    ) {
        let mut is_dir = false;
        let need_dir = self.flags.contains(ExpandFlags::DIRECTORIES_ONLY);

        while let Some(entry) = base_dir_iter.next() {
            if need_dir && !entry.is_dir() {
                continue;
            }
            if self.flags.contains(ExpandFlags::FOR_COMPLETIONS) {
                let mut filepath = base_dir.to_owned();
                filepath.push_utfstr(&entry.name);
                self.try_add_completion_result(&filepath, &entry.name, wc, prefix, is_dir);
            } else {
                // Normal wildcard expansion, not for completions.
                if wildcard_match(
                    &entry.name,
                    wc,
                    true, /* skip files with leading dots */
                ) {
                    let mut filepath = base_dir.to_owned();
                    filepath.push_utfstr(&entry.name);
                    self.add_expansion_result(filepath);
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
        assert!(self.flags.contains(ExpandFlags::FOR_COMPLETIONS));
        #[allow(clippy::collapsible_if)]
        if self.completion_set.insert(result.clone()) {
            if !self
                .resolved_completions
                .add(Completion::from_completion(result))
            {
                self.did_overflow = true;
            }
        }
    }

    // Given a start point as an absolute path, for any directory that has exactly one non-hidden
    // entity in it which is itself a directory, return that. The result is a relative path. For
    // example, if start_point is '/usr' we may return 'local/bin/'.
    //
    // The result does not have a leading slash, but does have a trailing slash if non-empty.
    fn descend_unique_hierarchy(&mut self, start_point: WString) -> WString {
        assert!(!start_point.is_empty() && start_point.starts_with('/'));

        let mut unique_hierarchy = WString::new();
        let mut abs_unique_hierarchy = start_point;

        // Ensure we don't fall into a symlink loop.
        // Ideally we would compare both devices and inodes, but devices require a stat call, so we
        // use inodes exclusively.
        let mut visited_inodes = HashSet::new();

        loop {
            // We keep track of the single unique_entry entry. If we get more than one, it's not
            // unique and we stop the descent.
            let mut dir = DirIter::new(&abs_unique_hierarchy);
            if !dir.valid() {
                break;
            }

            let mut unique_entry = None;
            while let Some(entry) = dir.next() {
                if entry.name.is_empty() || entry.name.starts_with('.') {
                    continue; // either hidden, or . and .. entries -- skip them
                }

                if !visited_inodes.insert(entry.inode) {
                    // Either we've visited this inode already or there's multiple files;
                    // either way stop.
                    break;
                } else if entry.is_dir() && unique_entry.is_none() {
                    unique_entry = Some(entry.name.to_owned()); // first candidate
                } else {
                    // We either have two or more candidates, or the child is not a directory. We're
                    // done.
                    unique_entry = None;
                    break;
                }
            }

            // We stop if we got two or more entries; also stop if we got zero or were interrupted
            let Some(unique_entry) = unique_entry else {
                break;
            };
            if self.interrupted_or_overflowed() {
                break;
            }

            // We have an entry in the unique hierarchy!
            append_path_component(&mut unique_hierarchy, &unique_entry);
            unique_hierarchy.push('/');

            append_path_component(&mut abs_unique_hierarchy, &unique_entry);
            abs_unique_hierarchy.push('/');
        }

        unique_hierarchy
    }

    fn try_add_completion_result(
        &mut self,
        filepath: &wstr,
        filename: &wstr,
        wildcard: &wstr,
        prefix: &wstr,
        known_dir: bool,
    ) {
        // This function is only for the completions case.
        assert!(self.flags.contains(ExpandFlags::FOR_COMPLETIONS));

        let mut abs_path = self.working_directory.to_owned();
        append_path_component(&mut abs_path, filepath);

        // We must normalize the path to allow 'cd ..' to operate on logical paths.
        if self.flags.contains(ExpandFlags::SPECIAL_FOR_CD) {
            abs_path = normalize_path(&abs_path, true);
        }

        let before = self.resolved_completions.size();
        if wildcard_test_flags_then_complete(
            &abs_path,
            filename,
            wildcard,
            self.flags,
            self.resolved_completions,
            known_dir,
        ) {
            // Hack. We added this completion result based on the last component of the wildcard.
            // Prepend our prefix to each wildcard that replaces its token.
            // Note that prepend_token_prefix is a no-op unless COMPLETE_REPLACES_TOKEN is set
            let after = self.resolved_completions.size();
            for i in before..after {
                let c = &mut self.resolved_completions.get_list_mut()[i];
                if self.has_fuzzy_ancestor && !c.flags.contains(CompleteFlags::REPLACES_TOKEN) {
                    c.flags |= CompleteFlags::REPLACES_TOKEN;
                    c.prepend_token_prefix(wildcard);
                }
                c.prepend_token_prefix(prefix);
            }

            // Implement SPECIAL_FOR_CD_AUTOSUGGESTION by descending the deepest unique
            // hierarchy we can, and then appending any components to each new result.
            // Only descend deepest unique for cd autosuggest and not for cd tab completion
            // (issue #4402).
            if self
                .flags
                .contains(ExpandFlags::SPECIAL_FOR_CD_AUTOSUGGESTION)
            {
                let unique_hierarchy = self.descend_unique_hierarchy(abs_path);
                if !unique_hierarchy.is_empty() {
                    for i in before..after {
                        self.resolved_completions.get_list_mut()[i]
                            .completion
                            .push_utfstr(&unique_hierarchy);
                    }
                }
            }

            self.did_add = true;
        }
    }

    // Helper to resolve using our prefix.
    fn open_dir(&self, base_dir: &wstr, dotdot: bool) -> DirIter {
        let mut path = self.working_directory.to_owned();
        append_path_component(&mut path, base_dir);
        if self.flags.contains(ExpandFlags::SPECIAL_FOR_CD) {
            // cd operates on logical paths.
            // for example, cd ../<tab> should complete "without resolving symlinks".
            path = normalize_path(&path, true);
        }
        if dotdot {
            DirIter::with_dot(&path)
        } else {
            DirIter::new(&path)
        }
    }

    pub fn new(
        working_directory: &'a wstr,
        flags: ExpandFlags,
        cancel_checker: &'a CancelChecker,
        resolved_completions: &'a mut CompletionReceiver,
    ) -> Self {
        // Insert initial completions into our set to avoid duplicates.
        let completion_set = resolved_completions
            .get_list()
            .iter()
            .map(|c| c.completion.to_owned())
            .collect();

        Self {
            cancel_checker,
            working_directory,
            completion_set,
            visited_files: HashSet::new(),
            flags,
            resolved_completions,
            did_interrupt: false,
            did_overflow: false,
            did_add: false,
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
    /// - `base_dir`: the "working directory" against which the wildcard is to be resolved
    /// - `wc`: the wildcard string itself, e.g. `foo*bar/baz` (where `*` is actually [`ANY_CHAR`])
    /// - `effective_prefix`: the string that should be prepended for completions that replace their token.
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
        let segment_has_wildcards = wildcard_has_internal(wc_segment); // e.g. ANY_STRING.

        if wc_segment.is_empty() {
            // Handle empty segment.
            assert!(!segment_has_wildcards);
            if let Some(wc_remainder) = wc_remainder {
                // Multiple adjacent slashes in the wildcard. Just skip them.
                let mut effective_prefix = effective_prefix.to_owned();
                effective_prefix.push('/');
                self.expand(base_dir, wc_remainder, &effective_prefix);
            } else {
                self.expand_trailing_slash(base_dir, effective_prefix);
            }
        } else if !segment_has_wildcards && wc_remainder.is_some() {
            // Literal intermediate match. Note that we may not be able to actually read the directory
            // (issue #2099).
            let wc_remainder = wc_remainder.unwrap(); // TODO: if-let-chains

            // Absolute path of the intermediate directory
            let mut intermediate_dirpath = base_dir.to_owned();
            intermediate_dirpath.push_utfstr(wc_segment);
            intermediate_dirpath.push('/');

            // This just trumps everything.
            let before = self.resolved_completions.size();

            let mut effective_prefix = effective_prefix.to_owned();
            effective_prefix.push_utfstr(wc_segment);
            effective_prefix.push('/');
            self.expand(&intermediate_dirpath, wc_remainder, &effective_prefix);

            // Maybe try a fuzzy match (#94) if nothing was found with the literal match. Respect
            // EXPAND_NO_DIRECTORY_ABBREVIATIONS (issue #2413).
            // Don't do fuzzy matches if the literal segment was valid (#3211)
            let allow_fuzzy = self.flags.contains(ExpandFlags::FUZZY_MATCH)
                && !self.flags.contains(ExpandFlags::NO_FUZZY_DIRECTORIES);

            if allow_fuzzy
                && self.resolved_completions.size() == before
                && waccess(&intermediate_dirpath, F_OK) == 0
            {
                assert!(self.flags.contains(ExpandFlags::FOR_COMPLETIONS));
                let mut base_dir_iter = self.open_dir(base_dir, false);
                if base_dir_iter.valid() {
                    self.expand_intermediate_segment_with_fuzz(
                        base_dir,
                        &mut base_dir_iter,
                        wc_segment,
                        wc_remainder,
                        &effective_prefix,
                    );
                }
            }
        } else {
            assert!(!wc_segment.is_empty() && (segment_has_wildcards || wc_remainder.is_none()));

            if let Some(wc_remainder) = wc_remainder {
                if wc_segment == [ANY_STRING_RECURSIVE].as_slice() {
                    // Hack for #7222. This is an intermediate wc segment that is exactly **. The
                    // tail matches in subdirectories as normal, but also the current directory.
                    // That is, '**/bar' may match 'bar' and 'foo/bar'.
                    // Implement this by matching the wildcard tail only, in this directory.
                    // Note if the segment is not exactly ANY_STRING_RECURSIVE then the segment may only
                    // match subdirectories.
                    self.expand(base_dir, wc_remainder, effective_prefix);
                    if self.interrupted_or_overflowed() {
                        return;
                    }
                }
            }

            // return "." and ".." entries if we're doing completions
            let mut dir = self.open_dir(
                base_dir, /* return . and .. */
                self.flags.contains(ExpandFlags::FOR_COMPLETIONS),
            );
            if dir.valid() {
                if let Some(wc_remainder) = wc_remainder {
                    // Not the last segment, nonempty wildcard.
                    let mut effective_prefix = effective_prefix.to_owned();
                    effective_prefix.push_utfstr(wc_segment);
                    effective_prefix.push('/');
                    self.expand_intermediate_segment(
                        base_dir,
                        &mut dir,
                        wc_segment,
                        wc_remainder,
                        &effective_prefix,
                    );
                } else {
                    // Last wildcard segment, nonempty wildcard.
                    self.expand_last_segment(base_dir, &mut dir, wc_segment, effective_prefix);
                }

                let asr_idx = wc_segment.find_char(ANY_STRING_RECURSIVE);
                if let Some(asr_idx) = asr_idx {
                    // Apply the recursive **.
                    // Construct a "head + any" wildcard for matching stuff in this directory, and an
                    // "any + tail" wildcard for matching stuff in subdirectories. Note that the
                    // ANY_STRING_RECURSIVE character is present in both the head and the tail.
                    let head_any = wc_segment.split_at(asr_idx + 1).0;
                    let any_tail = wc.slice_from(asr_idx);
                    assert!(head_any.as_char_slice().last() == Some(&ANY_STRING_RECURSIVE));
                    assert!(any_tail.as_char_slice().first() == Some(&ANY_STRING_RECURSIVE));

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
        }
    }

    pub fn status_code(&self) -> WildcardResult {
        if self.did_interrupt {
            WildcardResult::cancel
        } else if self.did_overflow {
            WildcardResult::overflow
        } else if self.did_add {
            WildcardResult::match_
        } else {
            WildcardResult::no_match
        }
    }
}

pub fn wildcard_expand_string(
    wc: &wstr,
    working_directory: &wstr,
    flags: ExpandFlags,
    cancel_checker: &CancelChecker,
    output: &mut CompletionReceiver,
) -> WildcardResult {
    // Fuzzy matching only if we're doing completions.
    assert!(
        flags.contains(ExpandFlags::FOR_COMPLETIONS) || !flags.contains(ExpandFlags::FUZZY_MATCH)
    );

    // ExpandFlags::SPECIAL_FOR_CD requires ExpandFlags::DIRECTORIES_ONLY and
    // ExpandFlags::FOR_COMPLETIONS and !ExpandFlags::GEN_DESCRIPTIONS.
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
        return WildcardResult::no_match;
    }

    // We do not support tab-completing recursive (**) wildcards. This is historic behavior.
    // Do not descend any directories if there is a ** wildcard.
    if flags.contains(ExpandFlags::FOR_COMPLETIONS) && wc.contains(ANY_STRING_RECURSIVE) {
        return WildcardResult::no_match;
    }

    // Compute the prefix and base dir. The prefix is what we prepend for filesystem operations
    // (i.e. the working directory), the base_dir is the part of the wildcard consumed thus far,
    // which we also have to append. The difference is that the base_dir is returned as part of the
    // expansion, and the prefix is not.
    //
    // Check for a leading slash. If we find one, we have an absolute path: the prefix is empty, the
    // base dir is /, and the wildcard is the remainder. If we don't find one, the prefix is the
    // working directory, the base dir is empty.
    let (prefix, base_dir, effective_wc) = if wc.starts_with('/') {
        (L!(""), L!("/"), wc.slice_from(1))
    } else {
        (working_directory, L!(""), wc)
    };

    let mut expander = WildcardExpander::new(prefix, flags, cancel_checker, output);
    expander.expand(base_dir, effective_wc, base_dir);
    expander.status_code()
}
