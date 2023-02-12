use crate::wchar::{wstr, WString};
use widestring::utfstr::CharsUtf32;

/// A thing that a wide string can start with or end with.
/// It must have a chars() method which returns a double-ended char iterator.
pub trait CharPrefixSuffix {
    type Iter: DoubleEndedIterator<Item = char>;
    fn chars(self) -> Self::Iter;
}

impl CharPrefixSuffix for char {
    type Iter = std::iter::Once<char>;
    fn chars(self) -> Self::Iter {
        std::iter::once(self)
    }
}

impl<'a> CharPrefixSuffix for &'a str {
    type Iter = std::str::Chars<'a>;
    fn chars(self) -> Self::Iter {
        str::chars(self)
    }
}

impl<'a> CharPrefixSuffix for &'a wstr {
    type Iter = CharsUtf32<'a>;
    fn chars(self) -> Self::Iter {
        wstr::chars(self)
    }
}

impl<'a> CharPrefixSuffix for &'a WString {
    type Iter = CharsUtf32<'a>;
    fn chars(self) -> Self::Iter {
        wstr::chars(self)
    }
}

/// \return true if \p prefix is a prefix of \p contents.
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

/// Convenience functions for WString.
pub trait WExt {
    /// Access the chars of a WString or wstr.
    fn as_char_slice(&self) -> &[char];

    /// \return the char at an index.
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

    /// \return the index of the first occurrence of the given char, or None.
    fn find_char(&self, c: char) -> Option<usize> {
        self.as_char_slice().iter().position(|&x| x == c)
    }

    /// \return whether we start with a given Prefix.
    /// The Prefix can be a char, a &str, a &wstr, or a &WString.
    fn starts_with<Prefix: CharPrefixSuffix>(&self, prefix: Prefix) -> bool {
        iter_prefixes_iter(prefix.chars(), self.as_char_slice().iter().copied())
    }

    /// \return whether we end with a given Suffix.
    /// The Suffix can be a char, a &str, a &wstr, or a &WString.
    fn ends_with<Suffix: CharPrefixSuffix>(&self, suffix: Suffix) -> bool {
        iter_prefixes_iter(
            suffix.chars().rev(),
            self.as_char_slice().iter().copied().rev(),
        )
    }

    /// Equivalent of `basic_string.rfind()` from C++
    fn rfind(&self, c: char) -> Option<usize> {
        self.as_char_slice().iter().rposition(|&x| x == c)
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
    use super::WExt;
    use crate::wchar::{WString, L};
    /// Write some tests.
    #[cfg(test)]
    fn test_find_char() {
        assert_eq!(Some(0), L!("abc").find_char('a'));
        assert_eq!(Some(1), L!("abc").find_char('b'));
        assert_eq!(None, L!("abc").find_char('X'));
        assert_eq!(None, L!("").find_char('X'));
    }
    
    #[cfg(test)]
    fn test_rfind() {
        assert_eq!(Some(3), L!("a/b/c").rfind('/'));
        assert_eq!(Some(2), L!("a/b/c").rfind('b'));
        assert_eq!(Some(0), L!("a/b/c").rfind('a'));
        assert_eq!(None, L!("a/b/c").rfind('z'));
        assert_eq!(None, L!("").rfind('z'));
        assert_eq!(None, WString::new().rfind('a'));
    }

    #[cfg(test)]
    fn test_prefix() {
        assert!(L!("").starts_with(L!("")));
        assert!(L!("abc").starts_with(L!("")));
        assert!(L!("abc").starts_with('a'));
        assert!(L!("abc").starts_with("ab"));
        assert!(L!("abc").starts_with(L!("ab")));
        assert!(L!("abc").starts_with(&WString::from_str("abc")));
    }

    #[cfg(test)]
    fn test_suffix() {
        assert!(L!("").ends_with(L!("")));
        assert!(L!("abc").ends_with(L!("")));
        assert!(L!("abc").ends_with('c'));
        assert!(L!("abc").ends_with("bc"));
        assert!(L!("abc").ends_with(L!("bc")));
        assert!(L!("abc").ends_with(&WString::from_str("abc")));
    }
}
