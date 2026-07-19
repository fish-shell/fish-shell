//! Helper functions for working with wcstring.

use fish_fallback::{fish_wcwidth, lowercase, lowercase_rev, wcscasecmp, wcscasecmp_fuzzy};
use fish_widestring::{ELLIPSIS_CHAR, prelude::*};
use std::sync::atomic::{AtomicUsize, Ordering};

/// Return the number of newlines in a string.
pub fn count_newlines(s: &wstr) -> usize {
    // This is a performance-sensitive function.
    // The native filter().count() produces sub-optimal codegen because of overflow checks,
    // which we currently enable in release mode. Implement it more efficiently.
    let mut count: usize = 0;
    for c in s.as_char_slice() {
        if *c == '\n' {
            count = count.wrapping_add(1);
        }
    }
    count
}

#[derive(Eq, PartialEq)]
pub enum IsPrefix {
    Prefix,
    Equal,
}
pub fn is_prefix(
    mut lhs: impl Iterator<Item = char>,
    mut rhs: impl Iterator<Item = char>,
) -> Option<IsPrefix> {
    use IsPrefix::*;
    loop {
        match (lhs.next(), rhs.next()) {
            (None, None) => return Some(Equal),
            (None, Some(_)) => return Some(Prefix),
            (Some(_), None) => return None,
            (Some(lhs), Some(rhs)) => {
                if lhs != rhs {
                    return None;
                }
            }
        }
    }
}

/// Test if a string prefixes another without regard to case. Returns true if a is a prefix of b.
pub fn string_prefixes_string_case_insensitive(proposed_prefix: &wstr, value: &wstr) -> bool {
    let proposed_prefix = lowercase(proposed_prefix.chars());
    let value = lowercase(value.chars());
    is_prefix(proposed_prefix, value).is_some()
}

pub fn string_prefixes_string_maybe_case_insensitive(
    icase: bool,
    proposed_prefix: &wstr,
    value: &wstr,
) -> bool {
    (if icase {
        string_prefixes_string_case_insensitive
    } else {
        string_prefixes_string
    })(proposed_prefix, value)
}

/// Remove the optional executable extension if there is one
/// Always returns None on non-Cygwin platforms
pub fn strip_executable_suffix(path: &wstr) -> Option<&wstr> {
    const DOT_EXE: &wstr = L!(".exe");
    (cfg!(cygwin) && { string_suffixes_string_case_insensitive(DOT_EXE, path) })
        .then(|| &path[..path.len() - DOT_EXE.len()])
}

/// Test if a string is a suffix of another.
pub fn string_suffixes_string_case_insensitive(proposed_suffix: &wstr, value: &wstr) -> bool {
    let proposed_suffix = lowercase_rev(proposed_suffix.chars());
    let value = lowercase_rev(value.chars());
    is_prefix(proposed_suffix, value).is_some()
}

/// Test if a string prefixes another. Returns true if a is a prefix of b.
pub fn string_prefixes_string(proposed_prefix: &wstr, value: &wstr) -> bool {
    value.as_slice().starts_with(proposed_prefix.as_slice())
}

/// Test if a string is a suffix of another.
pub fn string_suffixes_string(proposed_suffix: &wstr, value: &wstr) -> bool {
    value.as_slice().ends_with(proposed_suffix.as_slice())
}

/// Test if a string matches a subsequence of another.
/// Note subsequence is not substring: "foo" is a subsequence of "follow" for example.
pub fn subsequence_in_string(needle: &wstr, haystack: &wstr) -> bool {
    // Impossible if needle is larger than haystack.
    if needle.len() > haystack.len() {
        return false;
    }

    if needle.is_empty() {
        // Empty strings are considered to be subsequences of everything.
        return true;
    }

    let mut needle_it = needle.chars().peekable();
    for c in haystack.chars() {
        needle_it.next_if_eq(&c);

        if needle_it.peek().is_none() {
            return true;
        }
    }
    // We succeeded if we exhausted our sequence.
    needle_it.peek().is_none()
}

/// Case-insensitive string search, modeled after std::string::find().
/// \param fuzzy indicates this is being used for fuzzy matching and case insensitivity is
/// expanded to include symbolic characters (#3584).
/// Return the offset of the first case-insensitive matching instance of `needle` within
/// `haystack`, or `string::npos()` if no results were found.
pub fn ifind(haystack: &wstr, needle: &wstr, fuzzy: bool /* = false */) -> Option<usize> {
    if needle.is_empty() {
        return Some(0);
    }
    haystack
        .as_char_slice()
        .windows(needle.len())
        .position(|window| {
            // In fuzzy matching treat treat `-` and `_` as equal (#3584).
            fn fuzzy_canonicalize(c: char) -> char {
                if c == '_' { '-' } else { c }
            }

            wcscasecmp_fuzzy(
                wstr::from_char_slice(window),
                needle,
                if fuzzy {
                    fuzzy_canonicalize
                } else {
                    std::convert::identity
                },
            )
            .is_eq()
        })
}

/// The maximum edit distance tab completion will correct, i.e. the largest distance a candidate may
/// be from a prefix of the typed token and still match (see [`StringFuzzyMatch::try_create`]).
/// Configured via `fish_typo_completion_distance`; 0 (the default) disables typo correction
pub static FISH_TYPO_COMPLETION_DISTANCE: AtomicUsize = AtomicUsize::new(0);

// The ways one string can contain another.
//
// Note that the order of entries below affects the sort order of completions.
#[derive(Copy, Clone, Debug, Eq, PartialEq)]
pub enum ContainType {
    /// Exact match
    Exact,
    /// Prefix match: `foo` matches `foobar`
    Prefix,
    /// Substring match: `ooba` matches `foobar`
    Substr,
    /// Subsequence match: `fbr` matches `foobar`
    Subseq,
    /// Typo match: `foovar` matches `foobar` (if within the user specified distance).
    /// This is the lowest-priority match type and is only used when nothing else matches.
    EditDistance,
}

// The case-folding required for the match.
//
// Note that the order of entries below affects the sort order of completions.
#[derive(Copy, Clone, Debug, Eq, PartialEq, PartialOrd, Ord)]
pub enum CaseSensitivity {
    /// Exact match: `foobar` only matches `foobar`
    Sensitive,
    /// Case insensitive match if lowercase input: `foobar` matches `FooBar`.
    Smart,
    /// Case insensitive: `FooBaR` matches `foobAr`
    Insensitive,
}

/// A lightweight value-type describing how closely a string fuzzy-matches another string.
#[derive(Copy, Clone, Debug, Eq, PartialEq)]
pub struct StringFuzzyMatch {
    pub from_separator: bool,
    pub typ: ContainType,
    pub case_fold: CaseSensitivity,
}

impl StringFuzzyMatch {
    pub fn new(typ: ContainType, case_fold: CaseSensitivity) -> Self {
        Self {
            from_separator: false,
            typ,
            case_fold,
        }
    }
    // Helper to return an exact match.
    #[inline(always)]
    pub fn exact_match() -> Self {
        Self::new(ContainType::Exact, CaseSensitivity::Sensitive)
    }
    /// Return whether this is a samecase exact match.
    #[inline(always)]
    pub fn is_samecase_exact(&self) -> bool {
        self.typ == ContainType::Exact && self.case_fold == CaseSensitivity::Sensitive
    }
    /// Return if we are exact or prefix match.
    #[inline(always)]
    pub fn is_exact_or_prefix(&self) -> bool {
        matches!(self.typ, ContainType::Exact | ContainType::Prefix)
    }
    // Return if our match requires a full replacement, i.e. is not a strict extension of our
    // existing string. This is false only if our case matches and our type is prefix or exact.
    #[inline(always)]
    pub fn requires_full_replacement(&self) -> bool {
        if self.case_fold != CaseSensitivity::Sensitive {
            return true;
        }
        matches!(
            self.typ,
            ContainType::Substr | ContainType::Subseq | ContainType::EditDistance
        )
    }

    /// Try creating a fuzzy match for `string` against `match_against`.
    /// `string` is something like "foo" and `match_against` is like "FooBar".
    /// If `anchor_start` is set, then only exact and prefix matches are permitted.
    pub fn try_create(
        string: &wstr,
        match_against: &wstr,
        anchor_start: bool,
    ) -> Option<StringFuzzyMatch> {
        // Helper to lazily compute if case insensitive matches should use icase or smartcase.
        // Use icase if the input contains any uppercase characters, smartcase otherwise.
        #[inline(always)]
        fn get_case_fold(s: &wstr) -> CaseSensitivity {
            if s.chars().any(|c| c.is_uppercase()) {
                CaseSensitivity::Insensitive
            } else {
                CaseSensitivity::Smart
            }
        }

        // A string cannot fuzzy match against a shorter string.
        if string.len() > match_against.len() {
            return None;
        }

        // exact samecase
        if string == match_against {
            return Some(StringFuzzyMatch::new(
                ContainType::Exact,
                CaseSensitivity::Sensitive,
            ));
        }

        // prefix samecase
        if match_against.starts_with(string) {
            return Some(StringFuzzyMatch::new(
                ContainType::Prefix,
                CaseSensitivity::Sensitive,
            ));
        }

        // exact icase
        if wcscasecmp(string, match_against).is_eq() {
            return Some(StringFuzzyMatch::new(
                ContainType::Exact,
                get_case_fold(string),
            ));
        }

        // prefix icase
        if string_prefixes_string_case_insensitive(string, match_against) {
            return Some(StringFuzzyMatch::new(
                ContainType::Prefix,
                get_case_fold(string),
            ));
        }

        // If anchor_start is set, this is as far as we go.
        if anchor_start {
            return None;
        }

        // substr samecase
        if match_against
            .as_char_slice()
            .windows(string.len())
            .any(|window| wstr::from_char_slice(window) == string)
        {
            return Some(StringFuzzyMatch::new(
                ContainType::Substr,
                CaseSensitivity::Sensitive,
            ));
        }

        // substr icase
        if ifind(match_against, string, true /* fuzzy */).is_some() {
            return Some(StringFuzzyMatch::new(
                ContainType::Substr,
                get_case_fold(string),
            ));
        }

        // subseq samecase
        if subsequence_in_string(string, match_against) {
            return Some(StringFuzzyMatch::new(
                ContainType::Subseq,
                CaseSensitivity::Sensitive,
            ));
        }

        // We do not currently test subseq icase.

        // Typo Check
        let max_dist = max_typo_distance(string.len());
        if max_dist > 0 {
            let needle: Vec<char> = string.chars().collect();
            let haystack: Vec<char> = match_against.chars().collect();
            // Prefer a same-case correction; fall back to a case-insensitive one.
            if bounded_prefix_edit_distance(&needle, &haystack, max_dist, |a, b| a == b).is_some() {
                return Some(StringFuzzyMatch::new(
                    ContainType::EditDistance,
                    CaseSensitivity::Sensitive,
                ));
            }
            if bounded_prefix_edit_distance(&needle, &haystack, max_dist, chars_equal_ci).is_some()
            {
                return Some(StringFuzzyMatch::new(
                    ContainType::EditDistance,
                    get_case_fold(string),
                ));
            }
        }

        None
    }

    pub fn rank(&self) -> u32 {
        // Combine our type and our case fold into a single number, such that better matches are
        // smaller. Treat 'exact' types the same as 'prefix' types; this is because we do not
        // prefer exact matches to prefix matches when presenting completions to the user.
        // Treat smartcase the same as samecase; see #3978.
        let effective_type = if self.typ == ContainType::Exact {
            ContainType::Prefix
        } else {
            self.typ
        };
        let effective_case = if self.case_fold == CaseSensitivity::Smart {
            CaseSensitivity::Sensitive
        } else {
            self.case_fold
        };

        // Separator dominates type dominates fold.
        ((self.from_separator as u32) << 5)
            + ((effective_type as u32) << 2)
            + (effective_case as u32)
    }
}

/// Cover over string_fuzzy_match_t::try_create().
pub fn string_fuzzy_match_string(
    string: &wstr,
    match_against: &wstr,
    anchor_start: bool, /* = false */
) -> Option<StringFuzzyMatch> {
    StringFuzzyMatch::try_create(string, match_against, anchor_start)
}

/// Compare two characters ignoring case, for typo matching.
fn chars_equal_ci(a: char, b: char) -> bool {
    a == b || a.to_lowercase().eq(b.to_lowercase())
}

/// Demand minimum size to prevent overly aggressive matching
fn max_typo_distance(needle_len: usize) -> usize {
    if needle_len <= 2 {
        return 0;
    }
    FISH_TYPO_COMPLETION_DISTANCE.load(Ordering::Relaxed)
}

/// Damerau-Levenshtein distance from `needle` to the closest-matching prefix of `haystack`
/// (e.g. `Deks` is 1 edit from the prefix `Desk` of `Desktop`), comparing characters with
/// `char_eq`. `None` if it exceeds `max_dist`.
fn bounded_prefix_edit_distance(
    needle: &[char],
    haystack: &[char],
    max_dist: usize,
    char_eq: impl Fn(char, char) -> bool,
) -> Option<usize> {
    let n = needle.len();
    let m = haystack.len();

    // D[i][j] is the distance between needle[..i] and haystack[..j]; the answer is min_j D[n][j].
    // We keep only the last two rows: `prev_prev` is D[i-2], `prev` is D[i-1], `cur` is D[i].
    let mut prev_prev: Vec<usize> = vec![0; m + 1];
    let mut prev: Vec<usize> = (0..=m).collect(); // D[0][j] = j
    let mut cur: Vec<usize> = vec![0; m + 1];

    for i in 1..=n {
        cur[0] = i; // D[i][0] = i: delete every character of the needle
        let mut row_min = cur[0];
        for j in 1..=m {
            let cost = usize::from(!char_eq(needle[i - 1], haystack[j - 1]));
            let mut v = (prev[j] + 1) // deletion from needle
                .min(cur[j - 1] + 1) // insertion into needle
                .min(prev[j - 1] + cost); // substitution
            // Adjacent transposition (Damerau).
            if i > 1
                && j > 1
                && char_eq(needle[i - 1], haystack[j - 2])
                && char_eq(needle[i - 2], haystack[j - 1])
            {
                v = v.min(prev_prev[j - 2] + 1);
            }
            cur[j] = v;
            row_min = row_min.min(v);
        }
        // The row minimum is non-decreasing as `i` grows, so once it exceeds the budget no prefix
        // of haystack can be reached within `max_dist`.
        if row_min > max_dist {
            return None;
        }
        std::mem::swap(&mut prev_prev, &mut prev);
        std::mem::swap(&mut prev, &mut cur);
    }

    // `prev` now holds D[n][*]; take the best prefix.
    let best = prev.iter().copied().min().unwrap_or(usize::MAX);
    (best <= max_dist).then_some(best)
}

/// Split a string by runs of any of the separator characters provided in `seps`.
/// Note the delimiters are the characters in `seps`, not `seps` itself.
/// `seps` may contain the NUL character.
/// Do not output more than `max_results` results. If we are to output exactly that much,
/// the last output is the remainder of the input, including leading delimiters,
/// except for the first. This is historical behavior.
/// Example: split_string_tok(" a  b   c ", " ", 3) -> {"a", "b", "  c  "}
pub fn split_string_tok<'val>(
    val: &'val wstr,
    seps: &wstr,
    max_results: Option<usize>,
) -> Vec<&'val wstr> {
    let mut out = vec![];
    let val = val.as_char_slice();
    let end = val.len();
    let mut pos = 0;
    let max_results = max_results.unwrap_or(usize::MAX);
    while pos < end && out.len() + 1 < max_results {
        // Skip leading seps.
        match val[pos..].iter().position(|c| !seps.contains(*c)) {
            Some(p) => pos += p,
            None => {
                pos = end;
                break;
            }
        }

        // Find next sep.
        let next_sep = val[pos..]
            .iter()
            .position(|c| seps.contains(*c))
            .map_or(end, |p| pos + p);
        out.push(wstr::from_char_slice(&val[pos..next_sep]));
        // Note we skip exactly one sep here. This is because on the last iteration we retain all
        // but the first leading separators. This is historical.
        pos = next_sep + 1;
    }
    if pos < end && max_results > 0 {
        assert_eq!(out.len() + 1, max_results, "Should have split the max");
        out.push(wstr::from_char_slice(&val[pos..]));
    }
    assert!(out.len() <= max_results, "Got too many results");
    out
}

/// Joins strings with a separator.
/// This supports both <code>&[&[wstr]]</code> and <code>&[&[WString]]</code>
pub fn join_strings<S: AsRef<wstr>>(strs: &[S], sep: char) -> WString {
    if strs.is_empty() {
        return WString::new();
    }
    let capacity = strs.iter().fold(0, |acc, s| acc + s.as_ref().len()) + strs.len() - 1;
    let mut result = WString::with_capacity(capacity);
    for (i, s) in strs.iter().enumerate() {
        if i > 0 {
            result.push(sep);
        }
        result.push_utfstr(&s);
    }
    result
}

pub fn bool_from_string(x: &wstr) -> bool {
    matches!(x.chars().next(), Some('Y' | 'T' | 'y' | 't' | '1'))
}

/// Given iterators into a string (forward or reverse), splits the haystack iterators
/// about the needle sequence, up to max times. Inserts splits into the output array.
/// If the iterators are forward, this does the normal thing.
/// If the iterators are backward, this returns reversed strings, in reversed order!
/// If the needle is empty, split on individual elements (characters).
/// Max output entries will be max + 1 (after max splits)
pub fn split_about<'haystack>(
    haystack: &'haystack wstr,
    needle: &wstr,
    max: usize,     /*=usize::MAX*/
    no_empty: bool, /*=false*/
) -> Vec<&'haystack wstr> {
    let mut output = vec![];
    let mut remaining = max;
    let mut haystack = haystack.as_char_slice();
    while remaining > 0 && !haystack.is_empty() {
        let split_point = if needle.is_empty() {
            // empty needle, we split on individual elements
            1
        } else {
            match haystack
                .windows(needle.len())
                .position(|window| window == needle.as_char_slice())
            {
                Some(pos) => pos,
                None => break, // not found
            }
        };

        if haystack.len() == split_point {
            break;
        }

        if !no_empty || split_point != 0 {
            output.push(wstr::from_char_slice(&haystack[..split_point]));
        }
        remaining -= 1;
        // Need to skip over the needle for the next search note that the needle may be empty.
        haystack = &haystack[split_point + needle.len()..];
    }
    // Trailing component, possibly empty.
    if !no_empty || !haystack.is_empty() {
        output.push(wstr::from_char_slice(haystack));
    }
    output
}

// TODO: This should work on render width rather than the number of codepoints.
pub fn truncate(input: &wstr, max_len: usize) -> WString {
    if input.len() <= max_len {
        return input.to_owned();
    }
    let mut output = input[..max_len - 1].to_owned();
    output.push(ELLIPSIS_CHAR);
    output
}

fn trim_indices(input: &wstr, any_of: Option<&wstr>) -> std::ops::Range<usize> {
    let any_of = any_of.unwrap_or(L!("\t\x0B \r\n"));
    let result = input;
    let Some(suffix) = result.chars().rposition(|c| !any_of.contains(c)) else {
        return 0..0;
    };
    let prefix = result
        .chars()
        .position(|c| !any_of.contains(c))
        .expect("Should have one non-trimmed character");
    prefix..(suffix + 1)
}

// Remove leading and trailing characters in `any_of` from the string.
// By default, trim whitespace.
pub fn trim<'a>(input: &'a wstr, any_of: Option<&wstr>) -> &'a wstr {
    let range = trim_indices(input, any_of);
    &input[range]
}

// Remove leading and trailing characters in `any_of` from the string.
// By default, trim whitespace.
// This trims in-place.
pub fn trim_in_place(input: &mut WString, any_of: Option<&wstr>) {
    let range = trim_indices(input, any_of);
    input.truncate(range.end);
    input.drain(0..range.start);
}

/// Return the number of escaping backslashes before a character.
/// `idx` may be "one past the end."
pub fn count_preceding_backslashes(text: &wstr, idx: usize) -> usize {
    assert!(idx <= text.len(), "Out of bounds");
    text.chars()
        .take(idx)
        .rev()
        .take_while(|&c| c == '\\')
        .count()
}

/// Support for iterating over a newline-separated string.
pub struct LineIterator<'a> {
    // The string we're iterating.
    coll: &'a [u8],

    // The current location in the iteration.
    current: usize,
}

impl<'a> LineIterator<'a> {
    pub fn new(coll: &'a [u8]) -> Self {
        Self { coll, current: 0 }
    }
}

impl<'a> Iterator for LineIterator<'a> {
    type Item = &'a [u8];

    fn next(&mut self) -> Option<Self::Item> {
        if self.current == self.coll.len() {
            return None;
        }
        let newline_or_end = self.coll[self.current..]
            .iter()
            .position(|b| *b == b'\n')
            .map_or(self.coll.len(), |pos| self.current + pos);
        let result = &self.coll[self.current..newline_or_end];
        self.current = newline_or_end;

        // Skip the newline.
        if self.current != self.coll.len() {
            self.current += 1;
        }
        Some(result)
    }
}

/// Like fish_wcwidth, but returns 0 for characters with no real width instead of none.
pub fn fish_wcwidth_visible(c: char) -> isize {
    if c == '\x08' {
        return -1;
    }
    fish_wcwidth(c).unwrap_or_default().try_into().unwrap()
}

#[cfg(test)]
mod tests {
    use super::{
        CaseSensitivity, ContainType, FISH_TYPO_COMPLETION_DISTANCE, LineIterator, count_newlines,
        ifind, join_strings, split_string_tok, string_fuzzy_match_string,
        string_prefixes_string_case_insensitive, string_suffixes_string_case_insensitive, trim,
        trim_in_place,
    };
    use fish_widestring::prelude::*;

    #[test]
    fn test_string_prefixes_string_case_insensitive() {
        macro_rules! validate {
            ($prefix:literal, $s:literal, $expected:expr) => {
                assert_eq!(
                    string_prefixes_string_case_insensitive(L!($prefix), L!($s)),
                    $expected
                );
            };
        }
        validate!("i", "i_", true);
        validate!("İ", "i\u{307}_", true);
        validate!("i\u{307}", "İ", true); // prefix is longer
        validate!("i", "İ", true);
        validate!("gs", "gs_", true);
        validate!("gs_", "gs", false);
        assert_eq!("İn".to_lowercase().as_str(), "i\u{307}n");
        validate!("echo in", "echo İnstall", false);
    }

    #[test]
    fn test_string_suffixes_string_case_insensitive() {
        macro_rules! validate {
            ($suffix:literal, $s:literal, $expected:expr) => {
                assert_eq!(
                    string_suffixes_string_case_insensitive(L!($suffix), L!($s)),
                    $expected
                );
            };
        }
        validate!("i", "_i", true);
        validate!("i\u{307}", "İ", true);
        validate!("İ", "i\u{307}", true); // suffix is longer
        validate!("İ", "_İ", true);
        validate!("i", "_İ", false);
        validate!("gs", "_gs", true);
        validate!("_gs ", "gs", false);
    }

    #[test]
    fn test_ifind() {
        macro_rules! validate {
            ($haystack:expr, $needle:expr, $expected:expr) => {
                assert_eq!(ifind(L!($haystack), L!($needle), false), $expected);
            };
        }
        validate!("alpha", "alpha", Some(0));
        validate!("alphab", "alpha", Some(0));
        validate!("alpha", "balpha", None);
        validate!("balpha", "alpha", Some(1));
        validate!("alphab", "balpha", None);
        validate!("balpha", "lPh", Some(2));
        validate!("balpha", "Plh", None);
        validate!("echo Ö", "ö", Some(5));
    }

    #[test]
    fn test_ifind_fuzzy() {
        macro_rules! validate {
            ($haystack:expr, $needle:expr, $expected:expr) => {
                assert_eq!(ifind(L!($haystack), L!($needle), true), $expected);
            };
        }
        validate!("alpha", "alpha", Some(0));
        validate!("alphab", "alpha", Some(0));
        validate!("alpha-b", "alpha_b", Some(0));
        validate!("alpha-_", "alpha_-", Some(0));
        validate!("alpha-b", "alpha b", None);
    }

    #[test]
    fn test_fuzzy_match() {
        // Check that a string fuzzy match has the expected type and case folding.
        macro_rules! validate {
            ($needle:expr, $haystack:expr, $contain_type:expr, $case_fold:expr) => {
                let m = string_fuzzy_match_string(L!($needle), L!($haystack), false).unwrap();
                assert_eq!(m.typ, $contain_type);
                assert_eq!(m.case_fold, $case_fold);
            };
            ($needle:expr, $haystack:expr, None) => {
                assert_eq!(
                    string_fuzzy_match_string(L!($needle), L!($haystack), false),
                    None,
                );
            };
        }
        validate!("", "", ContainType::Exact, CaseSensitivity::Sensitive);
        validate!(
            "alpha",
            "alpha",
            ContainType::Exact,
            CaseSensitivity::Sensitive
        );
        validate!(
            "alp",
            "alpha",
            ContainType::Prefix,
            CaseSensitivity::Sensitive
        );
        validate!("alpha", "AlPhA", ContainType::Exact, CaseSensitivity::Smart);
        validate!(
            "alpha",
            "AlPhA!",
            ContainType::Prefix,
            CaseSensitivity::Smart
        );
        validate!(
            "ALPHA",
            "alpha!",
            ContainType::Prefix,
            CaseSensitivity::Insensitive
        );
        validate!(
            "ALPHA!",
            "alPhA!",
            ContainType::Exact,
            CaseSensitivity::Insensitive
        );
        validate!(
            "alPh",
            "ALPHA!",
            ContainType::Prefix,
            CaseSensitivity::Insensitive
        );
        validate!(
            "LPH",
            "ALPHA!",
            ContainType::Substr,
            CaseSensitivity::Sensitive
        );
        validate!("lph", "AlPhA!", ContainType::Substr, CaseSensitivity::Smart);
        validate!(
            "lPh",
            "ALPHA!",
            ContainType::Substr,
            CaseSensitivity::Insensitive
        );
        validate!(
            "AA",
            "ALPHA!",
            ContainType::Subseq,
            CaseSensitivity::Sensitive
        );
        // no subseq icase
        validate!("lh", "ALPHA!", None);
        validate!("BB", "ALPHA!", None);
    }

    #[test]
    fn test_fuzzy_match_typo() {
        // Typo correction is opt-in, so configure a distance for this test.
        FISH_TYPO_COMPLETION_DISTANCE.store(1, super::Ordering::Relaxed);

        // Check that a typo match has the expected type and case folding.
        macro_rules! validate {
            ($needle:expr, $haystack:expr, $contain_type:expr, $case_fold:expr) => {
                let m = string_fuzzy_match_string(L!($needle), L!($haystack), false).unwrap();
                assert_eq!(m.typ, $contain_type);
                assert_eq!(m.case_fold, $case_fold);
            };
            ($needle:expr, $haystack:expr, None) => {
                assert_eq!(
                    string_fuzzy_match_string(L!($needle), L!($haystack), false),
                    None,
                );
            };
        }
        // transposition
        validate!(
            "Deks",
            "Desktop",
            ContainType::EditDistance,
            CaseSensitivity::Sensitive
        );
        // substitution
        validate!(
            "downloods",
            "downloads",
            ContainType::EditDistance,
            CaseSensitivity::Sensitive
        );
        // insertion
        validate!(
            "picturres",
            "pictures-backup",
            ContainType::EditDistance,
            CaseSensitivity::Sensitive
        );
        // only matches case-insensitively
        validate!(
            "dektop",
            "Desktop",
            ContainType::EditDistance,
            CaseSensitivity::Smart
        );
        // subseq priority test
        validate!(
            "est",
            "Desktop",
            ContainType::Subseq,
            CaseSensitivity::Sensitive
        );
        // Check for overly aggressive matching
        validate!("xyz", "Desktop", None);
        // too short for sensible correcting
        validate!("ab", "az", None);
        // anchored matching never produces a typo correction
        assert_eq!(
            string_fuzzy_match_string(L!("Deks"), L!("Desktop"), true),
            None
        );
    }

    #[test]
    fn test_split_string_tok() {
        macro_rules! validate {
            ($val:expr, $seps:expr, $max_len:expr, $expected:expr) => {
                assert_eq!(split_string_tok(L!($val), L!($seps), $max_len), $expected,);
            };
        }
        validate!(" hello \t   world", " \t\n", None, vec!["hello", "world"]);
        validate!(" stuff ", " ", Some(0), vec![] as Vec<&wstr>);
        validate!(" stuff ", " ", Some(1), vec![" stuff "]);
        validate!(
            " hello \t   world  andstuff ",
            " \t\n",
            Some(3),
            vec!["hello", "world", " andstuff "]
        );
        // NUL chars are OK.
        validate!("hello \x00  world", " \0", None, vec!["hello", "world"]);
    }

    #[test]
    fn test_join_strings() {
        let empty: &[&wstr] = &[];
        assert_eq!(join_strings(empty, '/'), "");
        assert_eq!(join_strings(&[] as &[&wstr], '/'), "");
        assert_eq!(join_strings(&[L!("foo")], '/'), "foo");
        assert_eq!(
            join_strings(&[L!("foo"), L!("bar"), L!("baz")], '/'),
            "foo/bar/baz"
        );
    }

    #[test]
    fn test_line_iterator() {
        let text = b"Alpha\nBeta\nGamma\n\nDelta\n";
        let mut lines = vec![];
        let iter = LineIterator::new(text);
        for line in iter {
            lines.push(line);
        }
        assert_eq!(
            lines,
            vec![
                &b"Alpha"[..],
                &b"Beta"[..],
                &b"Gamma"[..],
                &b""[..],
                &b"Delta"[..]
            ]
        );
    }

    #[test]
    fn test_count_newlines() {
        assert_eq!(count_newlines(L!("")), 0);
        assert_eq!(count_newlines(L!("foo")), 0);
        assert_eq!(count_newlines(L!("foo\nbar")), 1);
        assert_eq!(count_newlines(L!("foo\nbar\nbaz")), 2);
        assert_eq!(count_newlines(L!("\n")), 1);
        assert_eq!(count_newlines(L!("\n\n")), 2);
    }

    #[test]
    fn test_trim() {
        fn test_trim(input: &wstr, any_of: Option<&wstr>, expect: &wstr) {
            assert_eq!(trim(input, any_of), expect);

            let mut s = input.to_owned();
            trim_in_place(&mut s, any_of);
            assert_eq!(s, expect);
        }
        test_trim(L!("foo"), None, L!("foo"));
        test_trim(L!("fooff"), Some(L!("f")), L!("oo"));
        test_trim(L!("  foo  "), None, L!("foo"));
        test_trim(L!(""), None, L!(""));
        test_trim(L!("  \n\n\n"), None, L!(""));
    }
}
