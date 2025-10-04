use std::{iter, slice};

use crate::{
    common::subslice_position,
    wchar::{wstr, WString},
    L,
};
use widestring::utfstr::CharsUtf32;

/// Helpers to convert things to widestring.
/// This is like std::string::ToString.
pub trait ToWString {
    fn to_wstring(&self) -> WString;
}

#[inline]
fn to_wstring_impl(mut val: u64, neg: bool) -> WString {
    // 20 digits max in u64: 18446744073709551616.
    let mut digits = [0; 24];
    let mut ndigits = 0;
    while val > 0 {
        digits[ndigits] = (val % 10) as u8;
        val /= 10;
        ndigits += 1;
    }
    if ndigits == 0 {
        digits[0] = 0;
        ndigits = 1;
    }
    let mut result = WString::with_capacity(ndigits + neg as usize);
    if neg {
        result.push('-');
    }
    for i in (0..ndigits).rev() {
        result.push((digits[i] + b'0') as char);
    }
    result
}

/// Implement to_wstring() for signed types.
macro_rules! impl_to_wstring_signed {
    ($($t:ty), *) => {
        $(
        impl ToWString for $t {
            fn to_wstring(&self) -> WString {
                let val = *self as i64;
                to_wstring_impl(val.unsigned_abs(), val < 0)
            }
        }
    )*
    };
}
impl_to_wstring_signed!(i8, i16, i32, i64, isize);

/// Implement to_wstring() for unsigned types.
macro_rules! impl_to_wstring_unsigned {
    ($($t:ty), *) => {
        $(
        impl ToWString for $t {
            fn to_wstring(&self) -> WString {
                to_wstring_impl(*self as u64, false)
            }
        }
    )*
    };
}

impl_to_wstring_unsigned!(u8, u16, u32, u64, u128, usize);

#[test]
fn test_to_wstring() {
    assert_eq!(0_u64.to_wstring(), "0");
    assert_eq!(1_u64.to_wstring(), "1");
    assert_eq!(0_i64.to_wstring(), "0");
    assert_eq!(1_i64.to_wstring(), "1");
    assert_eq!((-1_i64).to_wstring(), "-1");
    assert_eq!((-5_i64).to_wstring(), "-5");
    let mut val: i64 = 1;
    loop {
        assert_eq!(val.to_wstring(), val.to_string());
        let Some(next) = val.checked_mul(-3) else {
            break;
        };
        val = next;
    }
    assert_eq!(u64::MAX.to_wstring(), "18446744073709551615");
    assert_eq!(i64::MIN.to_wstring(), "-9223372036854775808");
    assert_eq!(i64::MAX.to_wstring(), "9223372036854775807");
}

/// A trait for a thing that can produce a double-ended, cloneable
/// iterator of chars.
/// Common implementations include char, &str, &wstr, &WString.
pub trait IntoCharIter {
    type Iter: DoubleEndedIterator<Item = char> + Clone;
    fn chars(self) -> Self::Iter;
}

impl IntoCharIter for char {
    type Iter = std::iter::Once<char>;
    fn chars(self) -> Self::Iter {
        std::iter::once(self)
    }
}

impl<'a> IntoCharIter for &'a str {
    type Iter = std::str::Chars<'a>;
    fn chars(self) -> Self::Iter {
        str::chars(self)
    }
}

impl<'a> IntoCharIter for &'a [char] {
    type Iter = iter::Copied<slice::Iter<'a, char>>;

    fn chars(self) -> Self::Iter {
        self.iter().copied()
    }
}

impl<'a> IntoCharIter for &'a wstr {
    type Iter = CharsUtf32<'a>;
    fn chars(self) -> Self::Iter {
        wstr::chars(self)
    }
}

impl<'a> IntoCharIter for &'a WString {
    type Iter = CharsUtf32<'a>;
    fn chars(self) -> Self::Iter {
        wstr::chars(self)
    }
}

// Also support `str.chars()` itself.
impl<'a> IntoCharIter for std::str::Chars<'a> {
    type Iter = Self;
    fn chars(self) -> Self::Iter {
        self
    }
}

// Also support `wstr.chars()` itself.
impl<'a> IntoCharIter for CharsUtf32<'a> {
    type Iter = Self;
    fn chars(self) -> Self::Iter {
        self
    }
}

/// Return true if `prefix` is a prefix of `contents`.
fn iter_prefixes_iter<Prefix, Contents>(prefix: Prefix, mut contents: Contents) -> bool
where
    Prefix: Iterator,
    Contents: Iterator,
    Prefix::Item: PartialEq<Contents::Item>,
{
    for c1 in prefix {
        match contents.next() {
            Some(c2) if c1 == c2 => {}
            _ => return false,
        }
    }
    true
}

/// Iterator type for splitting a wide string on a char.
pub struct WStrCharSplitIter<'a> {
    split: char,
    chars: Option<&'a [char]>,
}

impl<'a> Iterator for WStrCharSplitIter<'a> {
    type Item = &'a wstr;

    fn next(&mut self) -> Option<Self::Item> {
        let chars = self.chars?;
        if let Some(idx) = chars.iter().position(|c| *c == self.split) {
            let (prefix, rest) = chars.split_at(idx);
            self.chars = Some(&rest[1..]);
            return Some(wstr::from_char_slice(prefix));
        } else {
            self.chars = None;
            return Some(wstr::from_char_slice(chars));
        }
    }
}

/// Convenience functions for WString.
pub trait WExt {
    /// Access the chars of a WString or wstr.
    fn as_char_slice(&self) -> &[char];

    /// Return a char slice from a *char index*.
    /// This is different from Rust string slicing, which takes a byte index.
    fn slice_from(&self, start: usize) -> &wstr {
        let chars = self.as_char_slice();
        wstr::from_char_slice(&chars[start..])
    }

    /// Return a char slice up to a *char index*.
    /// This is different from Rust string slicing, which takes a byte index.
    fn slice_to(&self, end: usize) -> &wstr {
        let chars = self.as_char_slice();
        wstr::from_char_slice(&chars[..end])
    }

    /// Return the number of chars.
    /// This is different from Rust string len, which returns the number of bytes.
    fn char_count(&self) -> usize {
        self.as_char_slice().len()
    }

    /// Return the char at an index.
    /// If the index is equal to the length, return '\0'.
    /// If the index exceeds the length, then panic.
    fn char_at(&self, index: usize) -> char {
        let chars = self.as_char_slice();
        if index == chars.len() {
            '\0'
        } else {
            chars[index]
        }
    }

    /// Return the char at an index.
    /// If the index is equal to the length, return '\0'.
    /// If the index exceeds the length, return None.
    fn try_char_at(&self, index: usize) -> Option<char> {
        let chars = self.as_char_slice();
        match index {
            _ if index == chars.len() => Some('\0'),
            _ if index > chars.len() => None,
            _ => Some(chars[index]),
        }
    }

    /// Return an iterator over substrings, split by a given char.
    /// The split char is not included in the substrings.
    fn split(&self, c: char) -> WStrCharSplitIter<'_> {
        WStrCharSplitIter {
            split: c,
            chars: Some(self.as_char_slice()),
        }
    }

    fn split_once(&self, pos: usize) -> (&wstr, &wstr) {
        (self.slice_to(pos), self.slice_from(pos))
    }

    /// Returns the index of the first match against the provided substring or `None`.
    fn find(&self, search: impl AsRef<[char]>) -> Option<usize> {
        subslice_position(self.as_char_slice(), search.as_ref())
    }

    /// Replaces all matches of a pattern with another string.
    fn replace(&self, from: impl AsRef<[char]>, to: &wstr) -> WString {
        let from = from.as_ref();
        let mut s = self.as_char_slice().to_vec();
        let mut offset = 0;
        while let Some(relpos) = subslice_position(&s[offset..], from) {
            offset += relpos;
            s.splice(offset..(offset + from.len()), to.chars());
            offset += to.len();
        }
        WString::from_chars(s)
    }

    /// Return the index of the first occurrence of the given char, or None.
    fn find_char(&self, c: char) -> Option<usize> {
        self.as_char_slice().iter().position(|&x| x == c)
    }

    fn contains(&self, c: char) -> bool {
        self.as_char_slice().contains(&c)
    }

    /// Return whether we start with a given Prefix.
    /// The Prefix can be a char, a &str, a &wstr, or a &WString.
    fn starts_with<Prefix: IntoCharIter>(&self, prefix: Prefix) -> bool {
        iter_prefixes_iter(prefix.chars(), self.as_char_slice().iter().copied())
    }

    fn strip_prefix<Prefix: IntoCharIter>(&self, prefix: Prefix) -> Option<&wstr> {
        let iter = prefix.chars();
        let prefix_len = iter.clone().count();
        iter_prefixes_iter(iter, self.as_char_slice().iter().copied())
            .then(|| self.slice_from(prefix_len))
    }

    /// Return whether we end with a given Suffix.
    /// The Suffix can be a char, a &str, a &wstr, or a &WString.
    fn ends_with<Suffix: IntoCharIter>(&self, suffix: Suffix) -> bool {
        iter_prefixes_iter(
            suffix.chars().rev(),
            self.as_char_slice().iter().copied().rev(),
        )
    }

    fn strip_suffix<Suffix: IntoCharIter>(&self, suffix: Suffix) -> Option<&wstr> {
        // Note the above trait requires a double ended iterator.
        let iter = suffix.chars().rev();
        let suffix_len = iter.clone().count();
        iter_prefixes_iter(iter, self.as_char_slice().iter().rev().copied())
            .then(|| self.slice_to(self.char_count() - suffix_len))
    }

    fn trim_matches(&self, pat: char) -> &wstr {
        let slice = self.as_char_slice();
        let leading_count = slice.chars().take_while(|&c| c == pat).count();
        let trailing_count = slice.chars().rev().take_while(|&c| c == pat).count();
        if leading_count == slice.len() {
            return L!("");
        }
        let slice = self.slice_from(leading_count);
        slice.slice_to(slice.len() - trailing_count)
    }
}

impl WExt for WString {
    fn as_char_slice(&self) -> &[char] {
        self.as_utfstr().as_char_slice()
    }
}

impl WExt for wstr {
    fn as_char_slice(&self) -> &[char] {
        wstr::as_char_slice(self)
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::wchar::L;
    /// Write some tests.
    #[test]
    fn test_find_char() {
        assert_eq!(Some(0), L!("abc").find_char('a'));
        assert_eq!(Some(1), L!("abc").find_char('b'));
        assert_eq!(None, L!("abc").find_char('X'));
        assert_eq!(None, L!("").find_char('X'));
    }

    #[test]
    fn test_prefix() {
        assert!(L!("").starts_with(L!("")));
        assert!(L!("abc").starts_with(L!("")));
        assert!(L!("abc").starts_with('a'));
        assert!(L!("abc").starts_with("ab"));
        assert!(L!("abc").starts_with(L!("ab")));
        assert!(L!("abc").starts_with(&WString::from_str("abc")));
    }

    #[test]
    fn test_suffix() {
        assert!(L!("").ends_with(L!("")));
        assert!(L!("abc").ends_with(L!("")));
        assert!(L!("abc").ends_with('c'));
        assert!(L!("abc").ends_with("bc"));
        assert!(L!("abc").ends_with(L!("bc")));
        assert!(L!("abc").ends_with(&WString::from_str("abc")));
    }

    #[test]
    fn test_split() {
        fn do_split(s: &wstr, c: char) -> Vec<&wstr> {
            s.split(c).collect()
        }
        assert_eq!(do_split(L!(""), 'b'), &[""]);
        assert_eq!(do_split(L!("abc"), 'b'), &["a", "c"]);
        assert_eq!(do_split(L!("xxb"), 'x'), &["", "", "b"]);
        assert_eq!(do_split(L!("bxxxb"), 'x'), &["b", "", "", "b"]);
        assert_eq!(do_split(L!(""), 'x'), &[""]);
        assert_eq!(do_split(L!("foo,bar,baz"), ','), &["foo", "bar", "baz"]);
        assert_eq!(do_split(L!("foobar"), ','), &["foobar"]);
        assert_eq!(do_split(L!("1,2,3,4,5"), ','), &["1", "2", "3", "4", "5"]);
        assert_eq!(
            do_split(L!("1,2,3,4,5,"), ','),
            &["1", "2", "3", "4", "5", ""]
        );
        assert_eq!(
            do_split(L!("Hello\nworld\nRust"), '\n'),
            &["Hello", "world", "Rust"]
        );
    }

    #[test]
    fn find_prefix() {
        let needle = L!("hello");
        let haystack = L!("hello world");
        assert_eq!(haystack.find(needle), Some(0));
    }

    #[test]
    fn find_one() {
        let needle = L!("ello");
        let haystack = L!("hello world");
        assert_eq!(haystack.find(needle), Some(1));
    }

    #[test]
    fn find_suffix() {
        let needle = L!("world");
        let haystack = L!("hello world");
        assert_eq!(haystack.find(needle), Some(6));
    }

    #[test]
    fn find_none() {
        let needle = L!("worldz");
        let haystack = L!("hello world");
        assert_eq!(haystack.find(needle), None);
    }

    #[test]
    fn find_none_larger() {
        // Notice that `haystack` and `needle` are reversed.
        let haystack = L!("world");
        let needle = L!("hello world");
        assert_eq!(haystack.find(needle), None);
    }

    #[test]
    fn find_none_case_mismatch() {
        let haystack = L!("wOrld");
        let needle = L!("hello world");
        assert_eq!(haystack.find(needle), None);
    }

    #[test]
    fn test_trim_matches() {
        assert_eq!(L!("|foo|").trim_matches('|'), L!("foo"));
        assert_eq!(L!("<foo|").trim_matches('|'), L!("<foo"));
        assert_eq!(L!("|foo>").trim_matches('|'), L!("foo>"));
    }
}
