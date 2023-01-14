use crate::ffi;
pub use cxx::CxxWString;
pub use ffi::{wchar_t, wcharz_t};
pub use widestring::utf32str;
pub use widestring::{Utf32Str as wstr, Utf32String as WString};

/// Support for wide strings.
/// There are two wide string types that are commonly used:
///   - wstr: a string slice without a nul terminator. Like `&str` but wide chars.
///   - WString: an owning string without a nul terminator. Like `String` but wide chars.

/// Creates a wstr string slice, like the "L" prefix of C++.
/// The result is of type wstr.
/// It is NOT nul-terminated.
macro_rules! L {
    ($string:literal) => {
        widestring::utf32str!($string)
    };
}
pub(crate) use L;

/// A proc-macro for creating wide string literals using an L *suffix*.
///  Example usage:
/// ```
///  #[widestrs]
///  pub fn func() {
///     let s = "hello"L; // type &'static wstr
///  }
/// ```
/// Note: the resulting string is NOT nul-terminated.
pub use widestring_suffix::widestrs;

/// Pull in our extensions.
pub use crate::wchar_ext::{CharPrefixSuffix, WExt};
