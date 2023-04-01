use crate::wchar::{wstr, WString};
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

impl_to_wstring_unsigned!(u8, u16, u32, u64, usize);

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
        let Some(next) = val.checked_mul(-3) else { break; };
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

    /// Return a char slice from a *char index*.
    /// This is different from Rust string slicing, which takes a byte index.
    fn slice_from(&self, start: usize) -> &wstr {
        let chars = self.as_char_slice();
        wstr::from_char_slice(&chars[start..])
    }

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
    fn starts_with<Prefix: IntoCharIter>(&self, prefix: Prefix) -> bool {
        iter_prefixes_iter(prefix.chars(), self.as_char_slice().iter().copied())
    }

    /// \return whether we end with a given Suffix.
    /// The Suffix can be a char, a &str, a &wstr, or a &WString.
    fn ends_with<Suffix: IntoCharIter>(&self, suffix: Suffix) -> bool {
        iter_prefixes_iter(
            suffix.chars().rev(),
            self.as_char_slice().iter().copied().rev(),
        )
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
