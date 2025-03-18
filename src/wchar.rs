//! Support for wide strings.
//!
//! There are two wide string types that are commonly used:
//!   - wstr: a string slice without a nul terminator. Like `&str` but wide chars.
//!   - WString: an owning string without a nul terminator. Like `String` but wide chars.

use crate::common::{ENCODE_DIRECT_BASE, ENCODE_DIRECT_END};
pub use widestring::{Utf32Str as wstr, Utf32String as WString};

/// Pull in our extensions.
pub use crate::wchar_ext::IntoCharIter;

pub mod prelude {
    pub use crate::{
        wchar::{wstr, IntoCharIter, WString, L},
        wchar_ext::{ToWString, WExt},
        wutil::{eprintf, sprintf, wgettext, wgettext_fmt, wgettext_maybe_fmt, wgettext_str},
    };
}

/// Creates a wstr string slice, like the "L" prefix of C++.
/// The result is of type wstr.
/// It is NOT nul-terminated.
#[macro_export]
macro_rules! L {
    ($string:expr) => {
        widestring::utf32str!($string)
    };
}
pub use L;

/// Encode a literal byte in a UTF-32 character. This is required for e.g. the echo builtin, whose
/// escape sequences can be used to construct raw byte sequences which are then interpreted as e.g.
/// UTF-8 by the terminal. If we were to interpret each of those bytes as a codepoint and encode it
/// as a UTF-32 character, printing them would result in several characters instead of one UTF-8
/// character.
///
/// See <https://github.com/fish-shell/fish-shell/issues/1894>.
pub fn encode_byte_to_char(byte: u8) -> char {
    char::from_u32(u32::from(ENCODE_DIRECT_BASE) + u32::from(byte))
        .expect("private-use codepoint should be valid char")
}

/// Decode a literal byte from a UTF-32 character.
pub fn decode_byte_from_char(c: char) -> Option<u8> {
    if c >= ENCODE_DIRECT_BASE && c < ENCODE_DIRECT_END {
        Some(
            (u32::from(c) - u32::from(ENCODE_DIRECT_BASE))
                .try_into()
                .unwrap(),
        )
    } else {
        None
    }
}
