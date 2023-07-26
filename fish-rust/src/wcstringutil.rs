//! Helper functions for working with wcstring.

use crate::common::{get_ellipsis_char, get_ellipsis_str};
use crate::compat::MB_CUR_MAX;
use crate::expand::INTERNAL_SEPARATOR;
use crate::fallback::{fish_wcwidth, wcscasecmp};
use crate::flog::FLOG;
use crate::wchar::{decode_byte_from_char, prelude::*};
use crate::wutil::encoding::{wcrtomb, zero_mbstate, AT_LEAST_MB_LEN_MAX};

/// Test if a string prefixes another without regard to case. Returns true if a is a prefix of b.
pub fn string_prefixes_string_case_insensitive(proposed_prefix: &wstr, value: &wstr) -> bool {
    let prefix_size = proposed_prefix.len();
    prefix_size <= value.len() && wcscasecmp(&value[..prefix_size], proposed_prefix).is_eq()
}

/// Test if a string is a suffix of another.
pub fn string_suffixes_string_case_insensitive(proposed_suffix: &wstr, value: &wstr) -> bool {
    let suffix_size = proposed_suffix.len();
    suffix_size <= value.len()
        && wcscasecmp(&value[value.len() - suffix_size..], proposed_suffix).is_eq()
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

    let mut ni = needle.chars();
    let mut nc = ni.next();
    for hc in haystack.chars() {
        if nc == Some(hc) {
            nc = ni.next();
        }
    }
    // We succeeded if we exhausted our sequence.
    nc.is_none()
}

/// Case-insensitive string search, modeled after std::string::find().
/// \param fuzzy indicates this is being used for fuzzy matching and case insensitivity is
/// expanded to include symbolic characters (#3584).
/// \return the offset of the first case-insensitive matching instance of `needle` within
/// `haystack`, or `string::npos()` if no results were found.
pub fn ifind(haystack: &wstr, needle: &wstr, fuzzy: bool) -> Option<usize> {
    haystack
        .as_char_slice()
        .windows(needle.len())
        .position(|window| {
            for (l, r) in window.iter().zip(needle.chars()) {
                // In fuzzy matching treat treat `-` and `_` as equal (#3584).
                if fuzzy && ['-', '_'].contains(l) && ['-', '_'].contains(&r) {
                    continue;
                }
                // TODO Decide what to do for different lengths.
                let l = l.to_lowercase();
                let r = r.to_lowercase();
                for (l, r) in l.zip(r) {
                    if l != r {
                        return false;
                    }
                }
            }
            true
        })
}

// The ways one string can contain another.
#[derive(Copy, Clone, Debug, Eq, PartialEq)]
pub enum ContainType {
    /// exact match: foobar matches foo
    exact,
    /// prefix match: foo matches foobar
    prefix,
    /// substring match: ooba matches foobar
    substr,
    /// subsequence match: fbr matches foobar
    subseq,
}

// The case-folding required for the match.
#[derive(Copy, Clone, Debug, Eq, PartialEq)]
pub enum CaseFold {
    /// exact match: foobar matches foobar
    samecase,
    /// case insensitive match with lowercase input. foobar matches FoBar.
    smartcase,
    /// case insensitive: FoBaR matches foobAr
    icase,
}

/// A lightweight value-type describing how closely a string fuzzy-matches another string.
#[derive(Debug, Eq, PartialEq, Clone)]
pub struct StringFuzzyMatch {
    pub typ: ContainType,
    pub case_fold: CaseFold,
}

impl StringFuzzyMatch {
    pub fn new(typ: ContainType, case_fold: CaseFold) -> Self {
        Self { typ, case_fold }
    }
    // Helper to return an exact match.
    pub fn exact_match() -> Self {
        Self::new(ContainType::exact, CaseFold::samecase)
    }
    /// \return whether this is a samecase exact match.
    pub fn is_samecase_exact(&self) -> bool {
        self.typ == ContainType::exact && self.case_fold == CaseFold::samecase
    }
    /// \return if we are exact or prefix match.
    pub fn is_exact_or_prefix(&self) -> bool {
        matches!(self.typ, ContainType::exact | ContainType::prefix)
    }
    // \return if our match requires a full replacement, i.e. is not a strict extension of our
    // existing string. This is false only if our case matches, and our type is prefix or exact.
    pub fn requires_full_replacement(&self) -> bool {
        if self.case_fold != CaseFold::samecase {
            return true;
        }
        matches!(self.typ, ContainType::substr | ContainType::subseq)
    }

    /// Try creating a fuzzy match for \p string against \p match_against.
    /// \p string is something like "foo" and \p match_against is like "FooBar".
    /// If \p anchor_start is set, then only exact and prefix matches are permitted.
    pub fn try_create(
        string: &wstr,
        match_against: &wstr,
        anchor_start: bool,
    ) -> Option<StringFuzzyMatch> {
        // Helper to lazily compute if case insensitive matches should use icase or smartcase.
        // Use icase if the input contains any uppercase characters, smartcase otherwise.
        let get_case_fold = || {
            for c in string.chars() {
                if c.to_lowercase().next().unwrap() != c {
                    return CaseFold::icase;
                }
            }
            CaseFold::smartcase
        };

        // A string cannot fuzzy match against a shorter string.
        if string.len() > match_against.len() {
            return None;
        }

        // exact samecase
        if string == match_against {
            return Some(StringFuzzyMatch::new(
                ContainType::exact,
                CaseFold::samecase,
            ));
        }

        // prefix samecase
        if match_against.starts_with(string) {
            return Some(StringFuzzyMatch::new(
                ContainType::prefix,
                CaseFold::samecase,
            ));
        }

        // exact icase
        if wcscasecmp(string, match_against).is_eq() {
            return Some(StringFuzzyMatch::new(ContainType::exact, get_case_fold()));
        }

        // prefix icase
        if string_prefixes_string_case_insensitive(string, match_against) {
            return Some(StringFuzzyMatch::new(ContainType::prefix, get_case_fold()));
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
                ContainType::substr,
                CaseFold::samecase,
            ));
        }

        // substr icase
        if ifind(match_against, string, true /* fuzzy */).is_some() {
            return Some(StringFuzzyMatch::new(ContainType::substr, get_case_fold()));
        }

        // subseq samecase
        if subsequence_in_string(string, match_against) {
            return Some(StringFuzzyMatch::new(
                ContainType::subseq,
                CaseFold::samecase,
            ));
        }

        // We do not currently test subseq icase.
        None
    }

    pub fn rank(&self) -> u32 {
        // Combine our type and our case fold into a single number, such that better matches are
        // smaller. Treat 'exact' types the same as 'prefix' types; this is because we do not
        // prefer exact matches to prefix matches when presenting completions to the user.
        // Treat smartcase the same as samecase; see #3978.
        let effective_type = if self.typ == ContainType::exact {
            ContainType::prefix
        } else {
            self.typ
        };
        let effective_case = if self.case_fold == CaseFold::smartcase {
            CaseFold::samecase
        } else {
            self.case_fold
        };

        // Type dominates fold.
        effective_type as u32 * 8 + effective_case as u32
    }
}

/// Cover over string_fuzzy_match_t::try_create().
pub fn string_fuzzy_match_string(
    string: &wstr,
    match_against: &wstr,
    anchor_start: bool,
) -> Option<StringFuzzyMatch> {
    StringFuzzyMatch::try_create(string, match_against, anchor_start)
}

/// Implementation of wcs2string that accepts a callback.
/// This invokes \p func with (const char*, size_t) pairs.
/// If \p func returns false, it stops; otherwise it continues.
/// \return false if the callback returned false, otherwise true.
pub fn wcs2string_callback(input: &wstr, mut func: impl FnMut(&[u8]) -> bool) -> bool {
    let mut state = zero_mbstate();
    let mut converted = [0_u8; AT_LEAST_MB_LEN_MAX];

    for c in input.chars() {
        // TODO: this doesn't seem sound.
        if c == INTERNAL_SEPARATOR {
            // do nothing
        } else if let Some(byte) = decode_byte_from_char(c) {
            converted[0] = byte;
            if !func(&converted[..1]) {
                return false;
            }
        } else if MB_CUR_MAX() == 1 {
            // single-byte locale (C/POSIX/ISO-8859)
            // If `c` contains a wide character we emit a question-mark.
            converted[0] = u8::try_from(u32::from(c)).unwrap_or(b'?');
            if !func(&converted[..1]) {
                return false;
            }
        } else {
            converted = [0; AT_LEAST_MB_LEN_MAX];
            let len = unsafe {
                wcrtomb(
                    std::ptr::addr_of_mut!(converted[0]).cast(),
                    c as libc::wchar_t,
                    &mut state,
                )
            };
            if len == 0_usize.wrapping_sub(1) {
                wcs2string_bad_char(c);
                state = zero_mbstate();
            } else if !func(&converted[..len]) {
                return false;
            }
        }
    }
    true
}

fn wcs2string_bad_char(c: char) {
    FLOG!(
        char_encoding,
        L!("Wide character U+%4X has no narrow representation"),
        c
    );
}

/// Split a string by a separator character.
pub fn split_string(val: &wstr, sep: char) -> Vec<WString> {
    val.as_char_slice()
        .split(|c| *c == sep)
        .map(WString::from_chars)
        .collect()
}

/// Split a string by runs of any of the separator characters provided in \p seps.
/// Note the delimiters are the characters in \p seps, not \p seps itself.
/// \p seps may contain the NUL character.
/// Do not output more than \p max_results results. If we are to output exactly that much,
/// the last output is the the remainder of the input, including leading delimiters,
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
        pos += match val[pos..].iter().position(|c| !seps.contains(*c)) {
            Some(p) => p,
            None => break,
        };

        // Find next sep.
        let next_sep = val[pos..]
            .iter()
            .position(|c| seps.contains(*c))
            .map(|p| pos + p)
            .unwrap_or(end);
        out.push(wstr::from_char_slice(&val[pos..next_sep]));
        // Note we skip exactly one sep here. This is because on the last iteration we retain all
        // but the first leading separators. This is historical.
        pos = next_sep + 1;
    }
    if pos < end && max_results > 0 {
        assert!(out.len() + 1 == max_results, "Should have split the max");
        out.push(wstr::from_char_slice(&val[pos..]));
    }
    assert!(out.len() <= max_results, "Got too many results");
    out
}

/// Joins strings with a separator.
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
    if x.is_empty() {
        return false;
    }
    matches!(x.chars().next().unwrap(), 'Y' | 'T' | 'y' | 't' | '1')
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
    max: usize,
    no_empty: bool,
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

#[derive(Eq, PartialEq)]
pub enum EllipsisType {
    None,
    // Prefer niceness over minimalness
    Prettiest,
    // Make every character count ($ instead of ...)
    Shortest,
}

pub fn truncate(input: &wstr, max_len: usize, etype: Option<EllipsisType>) -> WString {
    let etype = etype.unwrap_or(EllipsisType::Prettiest);
    if input.len() <= max_len {
        return input.to_owned();
    }

    if etype == EllipsisType::None {
        return input[..max_len].to_owned();
    }
    if etype == EllipsisType::Prettiest {
        let ellipsis_str = get_ellipsis_str();
        let mut output = input[..max_len - ellipsis_str.len()].to_owned();
        output += ellipsis_str;
        return output;
    }
    let mut output = input[..max_len - 1].to_owned();
    output.push(get_ellipsis_char());
    output
}

pub fn trim(input: WString, any_of: Option<&wstr>) -> WString {
    let any_of = any_of.unwrap_or(L!("\t\x0B \r\n"));
    let mut result = input;
    let Some(suffix) = result.chars().rposition(|c| !any_of.contains(c)) else {
        return WString::new();
    };
    result.truncate(suffix + 1);

    let prefix = result
        .chars()
        .position(|c| !any_of.contains(c))
        .expect("Should have one non-trimmed character");
    result.split_off(prefix)
}

/// \return the number of escaping backslashes before a character.
/// \p idx may be "one past the end."
pub fn count_preceding_backslashes(text: &wstr, idx: usize) -> usize {
    assert!(idx <= text.len(), "Out of bounds");
    let mut backslashes = 0;
    while backslashes < idx && text.char_at(idx - backslashes - 1) == '\\' {
        backslashes += 1;
    }
    backslashes
}

/// Support for iterating over a newline-separated string.
pub struct LineIterator<'a> {
    // The string we're iterating.
    coll: &'a str,

    // The current location in the iteration.
    current: usize,
}

impl<'a> LineIterator<'a> {
    pub fn new(coll: &'a str) -> Self {
        Self { coll, current: 0 }
    }
}

impl<'a> Iterator for LineIterator<'a> {
    type Item = &'a str;

    fn next(&mut self) -> Option<Self::Item> {
        if self.current == self.coll.len() {
            return None;
        }
        let newline_or_end = self.coll[self.current..]
            .bytes()
            .position(|b| b == b'\n')
            .map(|pos| self.current + pos)
            .unwrap_or(self.coll.len());
        let result = &self.coll[self.current..newline_or_end];
        self.current = newline_or_end;

        // Skip the newline.
        if self.current != self.coll.len() {
            self.current += 1;
        }
        Some(result)
    }
}

/// Like fish_wcwidth, but returns 0 for characters with no real width instead of -1.
pub fn fish_wcwidth_visible(c: char) -> i32 {
    if c == '\x08' {
        return -1;
    }
    fish_wcwidth(c).max(0)
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
    validate!("", "", ContainType::exact, CaseFold::samecase);
    validate!("alpha", "alpha", ContainType::exact, CaseFold::samecase);
    validate!("alp", "alpha", ContainType::prefix, CaseFold::samecase);
    validate!("alpha", "AlPhA", ContainType::exact, CaseFold::smartcase);
    validate!("alpha", "AlPhA!", ContainType::prefix, CaseFold::smartcase);
    validate!("ALPHA", "alpha!", ContainType::prefix, CaseFold::icase);
    validate!("ALPHA!", "alPhA!", ContainType::exact, CaseFold::icase);
    validate!("alPh", "ALPHA!", ContainType::prefix, CaseFold::icase);
    validate!("LPH", "ALPHA!", ContainType::substr, CaseFold::samecase);
    validate!("lph", "AlPhA!", ContainType::substr, CaseFold::smartcase);
    validate!("lPh", "ALPHA!", ContainType::substr, CaseFold::icase);
    validate!("AA", "ALPHA!", ContainType::subseq, CaseFold::samecase);
    // no subseq icase
    validate!("lh", "ALPHA!", None);
    validate!("BB", "ALPHA!", None);
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
    let text = "Alpha\nBeta\nGamma\n\nDelta\n";
    let mut lines = vec![];
    let iter = LineIterator::new(text);
    for line in iter {
        lines.push(line);
    }
    assert_eq!(lines, vec!["Alpha", "Beta", "Gamma", "", "Delta"]);
}
