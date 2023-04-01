//! Support for wide strings.
//!
//! There are two wide string types that are commonly used:
//!   - wstr: a string slice without a nul terminator. Like `&str` but wide chars.
//!   - WString: an owning string without a nul terminator. Like `String` but wide chars.

pub use widestring::{Utf32Str as wstr, Utf32String as WString};

/// Pull in our extensions.
pub use crate::wchar_ext::{IntoCharIter, WExt};

/// Creates a wstr string slice, like the "L" prefix of C++.
/// The result is of type wstr.
/// It is NOT nul-terminated.
macro_rules! L {
    ($string:expr) => {
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

// Use Unicode "non-characters" for internal characters as much as we can. This
// gives us 32 "characters" for internal use that we can guarantee should not
// appear in our input stream. See http://www.unicode.org/faq/private_use.html.
pub const RESERVED_CHAR_BASE: char = '\u{FDD0}';
pub const RESERVED_CHAR_END: char = '\u{FDF0}';
// Split the available non-character values into two ranges to ensure there are
// no conflicts among the places we use these special characters.
pub const EXPAND_RESERVED_BASE: char = RESERVED_CHAR_BASE;
pub const EXPAND_RESERVED_END: char = match char::from_u32(EXPAND_RESERVED_BASE as u32 + 16u32) {
    Some(c) => c,
    None => panic!("private use codepoint in expansion region should be valid char"),
};
pub const WILDCARD_RESERVED_BASE: char = EXPAND_RESERVED_END;
pub const WILDCARD_RESERVED_END: char = match char::from_u32(WILDCARD_RESERVED_BASE as u32 + 16u32)
{
    Some(c) => c,
    None => panic!("private use codepoint in wildcard region should be valid char"),
};

// These are in the Unicode private-use range. We really shouldn't use this
// range but have little choice in the matter given how our lexer/parser works.
// We can't use non-characters for these two ranges because there are only 66 of
// them and we need at least 256 + 64.
//
// If sizeof(wchar_t)==4 we could avoid using private-use chars; however, that
// would result in fish having different behavior on machines with 16 versus 32
// bit wchar_t. It's better that fish behave the same on both types of systems.
//
// Note: We don't use the highest 8 bit range (0xF800 - 0xF8FF) because we know
// of at least one use of a codepoint in that range: the Apple symbol (0xF8FF)
// on Mac OS X. See http://www.unicode.org/faq/private_use.html.
pub const ENCODE_DIRECT_BASE: char = '\u{F600}';
pub const ENCODE_DIRECT_END: char = match char::from_u32(ENCODE_DIRECT_BASE as u32 + 256) {
    Some(c) => c,
    None => panic!("private use codepoint in encode direct region should be valid char"),
};

/// Encode a literal byte in a UTF-32 character. This is required for e.g. the echo builtin, whose
/// escape sequences can be used to construct raw byte sequences which are then interpreted as e.g.
/// UTF-8 by the terminal. If we were to interpret each of those bytes as a codepoint and encode it
/// as a UTF-32 character, printing them would result in several characters instead of one UTF-8
/// character.
///
/// See https://github.com/fish-shell/fish-shell/issues/1894.
pub fn encode_byte_to_char(byte: u8) -> char {
    char::from_u32(u32::from(ENCODE_DIRECT_BASE) + u32::from(byte))
        .expect("private-use codepoint should be valid char")
}
