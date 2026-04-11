//! Support for wide strings.
//!
//! There are two wide string types that are commonly used:
//!   - wstr: a string slice without a nul terminator. Like `&str` but wide chars.
//!   - WString: an owning string without a nul terminator. Like `String` but wide chars.

pub mod word_char;

use std::{
    ffi::{CStr, CString, OsStr, OsString},
    iter,
    os::unix::ffi::{OsStrExt as _, OsStringExt as _},
    slice,
};
pub use widestring::{Utf32Str as wstr, Utf32String as WString, utf32str as L, utfstr::CharsUtf32};

pub mod prelude {
    pub use crate::{IntoCharIter, L, ToWString, WExt, WString, wstr};
}

// Highest legal ASCII value.
pub const ASCII_MAX: char = 127 as char;

// Highest legal 16-bit Unicode value.
pub const UCS2_MAX: char = '\u{FFFF}';

// Highest legal byte value.
pub const BYTE_MAX: char = 0xFF as char;

// Unicode BOM value.
pub const UTF8_BOM_WCHAR: char = '\u{FEFF}';

/// The character to use where the text has been truncated.
pub const ELLIPSIS_CHAR: char = '\u{2026}'; // ('…')

pub const SPECIAL_KEY_ENCODE_BASE: char = '\u{F500}';
// These are in the Unicode private-use range. We really shouldn't use this
// range but have little choice in the matter given how our lexer/parser works.
// We can't use non-characters for these two ranges because there are only 66 of
// them and we need at least 256 + 64.
//
// If sizeof(wchar_t))==4 we could avoid using private-use chars; however, that
// would result in fish having different behavior on machines with 16 versus 32
// bit wchar_t. It's better that fish behave the same on both types of systems.
//
// Note: We don't use the highest 8 bit range (0xF800 - 0xF8FF) because we know
// of at least one use of a codepoint in that range: the Apple symbol (0xF8FF)
// on Mac OS X. See http://www.unicode.org/faq/private_use.html.
pub const ENCODE_DIRECT_BASE: char = char_offset(SPECIAL_KEY_ENCODE_BASE, 256);
pub const ENCODE_DIRECT_END: char = char_offset(ENCODE_DIRECT_BASE, 256);

// Use Unicode "non-characters" for internal characters as much as we can. This
// gives us 32 "characters" for internal use that we can guarantee should not
// appear in our input stream. See http://www.unicode.org/faq/private_use.html.
pub const RESERVED_CHAR_BASE: char = '\u{FDD0}';
pub const RESERVED_CHAR_END: char = '\u{FDF0}';
// Split the available non-character values into two ranges to ensure there are
// no conflicts among the places we use these special characters.
pub const EXPAND_RESERVED_BASE: char = RESERVED_CHAR_BASE;
pub const EXPAND_RESERVED_END: char = char_offset(EXPAND_RESERVED_BASE, 16);
pub const WILDCARD_RESERVED_BASE: char = EXPAND_RESERVED_END;
pub const WILDCARD_RESERVED_END: char = char_offset(WILDCARD_RESERVED_BASE, 16);
// Make sure the ranges defined above don't exceed the range for non-characters.
// This is to make sure we didn't do something stupid in subdividing the
// Unicode range for our needs.
const _: () = assert!(WILDCARD_RESERVED_END <= RESERVED_CHAR_END);

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

/// Character representing a home directory.
pub const HOME_DIRECTORY: char = char_offset(EXPAND_RESERVED_BASE, 0);
/// Character representing process expansion for %self.
pub const PROCESS_EXPAND_SELF: char = char_offset(EXPAND_RESERVED_BASE, 1);
/// Character representing variable expansion.
pub const VARIABLE_EXPAND: char = char_offset(EXPAND_RESERVED_BASE, 2);
/// Character representing variable expansion into a single element.
pub const VARIABLE_EXPAND_SINGLE: char = char_offset(EXPAND_RESERVED_BASE, 3);
/// Character representing the start of a bracket expansion.
pub const BRACE_BEGIN: char = char_offset(EXPAND_RESERVED_BASE, 4);
/// Character representing the end of a bracket expansion.
pub const BRACE_END: char = char_offset(EXPAND_RESERVED_BASE, 5);
/// Character representing separation between two bracket elements.
pub const BRACE_SEP: char = char_offset(EXPAND_RESERVED_BASE, 6);
/// Character that takes the place of any whitespace within non-quoted text in braces
pub const BRACE_SPACE: char = char_offset(EXPAND_RESERVED_BASE, 7);
/// Separate subtokens in a token with this character.
pub const INTERNAL_SEPARATOR: char = char_offset(EXPAND_RESERVED_BASE, 8);
/// Character representing an empty variable expansion. Only used transitively while expanding
/// variables.
pub const VARIABLE_EXPAND_EMPTY: char = char_offset(EXPAND_RESERVED_BASE, 9);

const _: () = assert!(
    EXPAND_RESERVED_END as u32 > VARIABLE_EXPAND_EMPTY as u32,
    "Characters used in expansions must stay within private use area"
);

/// The string represented by PROCESS_EXPAND_SELF
pub const PROCESS_EXPAND_SELF_STR: &wstr = L!("%self");

/// Return true if the character is in a range reserved for fish's private use.
///
/// NOTE: This is used when tokenizing the input. It is also used when reading input, before
/// tokenization, to replace such chars with REPLACEMENT_WCHAR if they're not part of a quoted
/// string. We don't want external input to be able to feed reserved characters into our
/// lexer/parser or code evaluator.
//
// TODO: Actually implement the replacement as documented above.
pub fn fish_reserved_codepoint(c: char) -> bool {
    (c >= RESERVED_CHAR_BASE && c < RESERVED_CHAR_END)
        || (c >= SPECIAL_KEY_ENCODE_BASE && c < ENCODE_DIRECT_END)
}

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

/// Returns a newly allocated multibyte character string equivalent of the specified wide character
/// string.
///
/// This function decodes illegal character sequences in a reversible way using the private use
/// area.
pub fn wcs2bytes(input: impl IntoCharIter) -> Vec<u8> {
    let mut result = vec![];
    wcs2bytes_appending(&mut result, input);
    result
}

pub fn wcs2osstring(input: &wstr) -> OsString {
    if input.is_empty() {
        return OsString::new();
    }

    let mut result = vec![];
    wcs2bytes_appending(&mut result, input);
    OsString::from_vec(result)
}

/// Same as [`wcs2bytes`]. Meant to be used when we need a zero-terminated string to feed legacy APIs.
/// Note: if `input` contains any interior NUL bytes, the result will be truncated at the first!
pub fn wcs2zstring(input: &wstr) -> CString {
    if input.is_empty() {
        return CString::default();
    }

    let mut vec = Vec::with_capacity(input.len() + 1);
    str2bytes_callback(input, |buff| {
        vec.extend_from_slice(buff);
        true
    });
    vec.push(b'\0');

    match CString::from_vec_with_nul(vec) {
        Ok(cstr) => cstr,
        Err(err) => {
            // `input` contained a NUL in the middle; we can retrieve `vec`, though
            let mut vec = err.into_bytes();
            let pos = vec.iter().position(|c| *c == b'\0').unwrap();
            vec.truncate(pos + 1);
            // Safety: We truncated after the first NUL
            unsafe { CString::from_vec_with_nul_unchecked(vec) }
        }
    }
}

/// Like [`wcs2bytes`], but appends to `output` instead of returning a new string.
pub fn wcs2bytes_appending(output: &mut Vec<u8>, input: impl IntoCharIter) {
    str2bytes_callback(input, |buff| {
        output.extend_from_slice(buff);
        true
    });
}

/// Implementation of wcs2bytes that accepts a callback.
/// The first argument can be either a `&str` or `&wstr`.
/// This invokes `func` with byte slices containing the UTF-8 encoding of the characters in the
/// input, doing one invocation per character.
/// If `func` returns false, it stops; otherwise it continues.
/// Return false if the callback returned false, otherwise true.
pub fn str2bytes_callback(input: impl IntoCharIter, mut func: impl FnMut(&[u8]) -> bool) -> bool {
    // A `char` represents an Unicode scalar value, which takes up at most 4 bytes when encoded in UTF-8.
    let mut converted = [0_u8; 4];

    for c in input.chars() {
        let bytes = if let Some(byte) = decode_byte_from_char(c) {
            converted[0] = byte;
            &converted[..=0]
        } else {
            c.encode_utf8(&mut converted).as_bytes()
        };
        if !func(bytes) {
            return false;
        }
    }
    true
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

/// A trait to make it more convenient to pass ascii/Unicode strings to functions that can take
/// non-Unicode values. The result is nul-terminated and can be passed to OS functions.
///
/// This is only implemented for owned types where an owned instance will skip allocations (e.g.
/// `CString` can return `self`) but not implemented for owned instances where a new allocation is
/// always required (e.g. implemented for `&wstr` but not `WideString`) because you might as well be
/// left with the original item if we're going to allocate from scratch in all cases.
pub trait ToCString {
    /// Correctly convert to a nul-terminated [`CString`] that can be passed to OS functions.
    fn to_cstring(self) -> CString;
}

impl ToCString for CString {
    fn to_cstring(self) -> CString {
        self
    }
}

impl ToCString for &CStr {
    fn to_cstring(self) -> CString {
        self.to_owned()
    }
}

/// Safely converts from `&wstr` to a `CString` to a nul-terminated `CString` that can be passed to
/// OS functions, taking into account non-Unicode values that have been shifted into the private-use
/// range by using [`wcs2zstring()`].
impl ToCString for &wstr {
    /// The wide string may contain non-Unicode bytes mapped to the private-use Unicode range, so we
    /// have to use [`wcs2zstring()`](self::wcs2zstring) to convert it correctly.
    fn to_cstring(self) -> CString {
        self::wcs2zstring(self)
    }
}

/// Safely converts from `&WString` to a nul-terminated `CString` that can be passed to OS
/// functions, taking into account non-Unicode values that have been shifted into the private-use
/// range by using [`wcs2zstring()`].
impl ToCString for &WString {
    fn to_cstring(self) -> CString {
        self.as_utfstr().to_cstring()
    }
}

/// Convert a (probably ascii) string to CString that can be passed to OS functions.
impl ToCString for Vec<u8> {
    fn to_cstring(mut self) -> CString {
        self.push(b'\0');
        CString::from_vec_with_nul(self).unwrap()
    }
}

/// Convert a (probably ascii) string to nul-terminated CString that can be passed to OS functions.
impl ToCString for &[u8] {
    fn to_cstring(self) -> CString {
        CString::new(self).unwrap()
    }
}

mod decoder {
    use crate::{ENCODE_DIRECT_BASE, ENCODE_DIRECT_END, char_offset, wstr};
    use buffer::Buffer;
    use std::{char::REPLACEMENT_CHARACTER, ops::Range};
    use widestring::utfstr::CharsUtf32;

    mod buffer {
        // The size required for a PUA-encoded character from our special PUA range - 1,
        // since that is the maximum number characters our look-ahead needs to check.
        const MAX_SIZE: usize = 2;
        pub(super) struct Buffer {
            buffer: [char; MAX_SIZE],
            length: usize,
        }

        impl Buffer {
            pub(super) fn empty() -> Self {
                Self {
                    buffer: ['\0'; MAX_SIZE],
                    length: 0,
                }
            }

            pub(super) fn push(&mut self, c: char) {
                self.buffer[self.length] = c;
                self.length += 1;
            }

            pub(super) fn pop(&mut self) -> Option<char> {
                if self.length == 0 {
                    return None;
                }
                self.length -= 1;
                Some(self.buffer[self.length])
            }

            pub(super) fn pop_front(&mut self) -> Option<char> {
                if self.length == 0 {
                    return None;
                }
                self.buffer.rotate_left(1);
                self.length -= 1;
                Some(self.buffer[MAX_SIZE - 1])
            }
        }
    }

    const PUA_ENCODE_RANGE: Range<char> = ENCODE_DIRECT_BASE..ENCODE_DIRECT_END;
    const ENCODED_PUA_CHAR_FIRST_CHAR: char = char_offset(ENCODE_DIRECT_BASE, 0xef);
    // The second UTF-8 byte of a character in our special PUA range is in the range
    // 0x98..0x9c.
    const ENCODED_PUA_CHAR_SECOND_CHAR_RANGE: Range<char> =
        char_offset(ENCODE_DIRECT_BASE, 0x98)..char_offset(ENCODE_DIRECT_BASE, 0x9c);

    /// This serves as the data container for building a double-ended iterator which decodes our
    /// PUA-encoded chars into a char iterator where each encoded non-UTF-8 byte is replaced by the
    /// replacement character, and each encoded PUA codepoint is turned back into a single char
    /// whose value is the original PUA codepoint.
    ///
    /// The latter part makes the decoding logic somewhat complicated, because encoded PUA chars
    /// take up 3 chars in our encoding. Therefore, in some cases, we need to take more than 1 char
    /// from the `encoded_chars` iterator before we know whether to decode 3 chars together into a
    /// single char, or whether the chars should be replaced by the replacement char individually.
    /// In cases where we took more than 1 char and then notice that individual replacement is
    /// warranted, we return a replacement char for the first char we took from the iterator, and
    /// cache the 1 or 2 other chars we read in `buffer_front` or `buffer_back`, depending on the
    /// reading direction. Buffers store elements in such an order that getting the next character
    /// requires `pop` when using the buffer associated with the current reading direction, and
    /// `pop_front` when using the other buffer. This is done to optimize the common case of
    /// iterating in a single direction.
    ///
    /// The buffers have to be considered before taking more chars from `encoded_chars`.
    /// If the iterator is only read in one direction, the buffer for the other direction will not
    /// be used. But because it's possible that the iterator is read from both ends, it can happen
    /// that when `encoded_chars` runs out, the buffer for the opposite reading direction is
    /// non-empty. In the [`Self::next`] and [`Self::next_back`] implementations, this logic is
    /// encapsulated into closures for getting the next char from the appropriate source.
    /// At most 2 chars will ever be stored in a buffer, so they are implemented using a fixed-size
    /// array, requiring no heap allocations.
    ///
    /// Note that in most cases, we can avoid using the buffers, and simply forward the char
    /// obtained from `encoded_chars`. Only chars in [`PUA_ENCODE_RANGE`] can possibly encode PUA
    /// chars, so if we read any other char, we know that it's not part of such an encoding and can
    /// return it directly. If we read in the forward direction, we can also exploit knowledge about
    /// the possible values of our PUA encoding. Specifically, the first char in such an encoding
    /// will always be [`ENCODED_PUA_CHAR_FIRST_CHAR`], and the second char will be in the range
    /// [`ENCODED_PUA_CHAR_SECOND_CHAR_RANGE`].
    pub(super) struct Decoder<'a> {
        encoded_chars: CharsUtf32<'a>,
        buffer_front: Buffer,
        buffer_back: Buffer,
    }

    impl<'a> Decoder<'a> {
        pub(super) fn new(encoded_str: &'a wstr) -> Self {
            Self {
                encoded_chars: encoded_str.chars(),
                buffer_front: Buffer::empty(),
                buffer_back: Buffer::empty(),
            }
        }
    }

    fn try_pua_decoding(encoding: &[char; 3]) -> Option<char> {
        let mut bytes = [0u8; 3];
        for (index, &c) in encoding.iter().enumerate() {
            bytes[index] = super::decode_byte_from_char(c)?;
        }
        let first_decoded_char =
            std::str::from_utf8(&bytes).ok()?.chars().next().expect(
                "Non-empty byte slice which is valid UTF-8 must result in at least one char.",
            );
        // For strings whose width we compute, we only expect invalid UTF-8 and codepoints from the
        // PUA encoding range to be PUA encoded.
        // If we reach this point, the encoded bytes are valid UTF-8, so the only remaining
        // expected case are codepoints from the PUA encoding range.
        // These all take 3 bytes to represent in UTF-8, so if we check that the first parsed
        // codepoint is in the expected range, we know that exactly 3 bytes were consumed for
        // parsing this codepoint.
        assert!(PUA_ENCODE_RANGE.contains(&first_decoded_char));
        Some(first_decoded_char)
    }

    fn replace_if_pua_encoded(c: char) -> char {
        if PUA_ENCODE_RANGE.contains(&c) {
            REPLACEMENT_CHARACTER
        } else {
            c
        }
    }

    impl<'a> Iterator for Decoder<'a> {
        type Item = char;

        fn next(&mut self) -> Option<Self::Item> {
            let mut get_next_char = || {
                self.buffer_front
                    .pop()
                    .or_else(|| self.encoded_chars.next())
                    .or_else(|| self.buffer_back.pop_front())
            };
            let c_0 = get_next_char()?;
            if c_0 != ENCODED_PUA_CHAR_FIRST_CHAR {
                return Some(replace_if_pua_encoded(c_0));
            }
            if let Some(c_1) = get_next_char() {
                if ENCODED_PUA_CHAR_SECOND_CHAR_RANGE.contains(&c_1) {
                    if let Some(c_2) = get_next_char() {
                        if let Some(decoded_pua_char) = try_pua_decoding(&[c_0, c_1, c_2]) {
                            return Some(decoded_pua_char);
                        }
                        self.buffer_front.push(c_2);
                    }
                }
                self.buffer_front.push(c_1);
            }
            // If decoding 3 consecutive PUA chars into the encoded PUA char fails, `c_0` should be
            // returned. `c_0` is `ENCODED_PUA_CHAR_FIRST_CHAR` if we reach this point, so return
            // the `REPLACEMENT_CHARACTER` in these cases.
            Some(REPLACEMENT_CHARACTER)
        }
    }

    impl<'a> DoubleEndedIterator for Decoder<'a> {
        fn next_back(&mut self) -> Option<Self::Item> {
            let mut get_next_char = || {
                self.buffer_back
                    .pop()
                    .or_else(|| self.encoded_chars.next_back())
                    .or_else(|| self.buffer_front.pop_front())
            };
            let c_2 = get_next_char()?;
            if !PUA_ENCODE_RANGE.contains(&c_2) {
                return Some(c_2);
            }
            if let Some(c_1) = get_next_char() {
                if PUA_ENCODE_RANGE.contains(&c_1) {
                    if let Some(c_0) = get_next_char() {
                        if let Some(decoded_pua_char) = try_pua_decoding(&[c_0, c_1, c_2]) {
                            return Some(decoded_pua_char);
                        }
                        self.buffer_back.push(c_0);
                    }
                    self.buffer_back.push(c_1);
                }
            }
            // If decoding 3 consecutive PUA chars into the encoded PUA char fails, `c_2` should be
            // returned. `c_2` is in `PUA_ENCODE_RANGE` if we reach this point, so return the
            // `REPLACEMENT_CHARACTER` in these cases.
            Some(REPLACEMENT_CHARACTER)
        }
    }
}

/// Only exists for tests. Exported because the encoding functionality is not available in this
/// crate. Do not use for non-testing purposes.
pub fn decode_with_replacement(encoded_str: &wstr) -> impl DoubleEndedIterator<Item = char> {
    decoder::Decoder::new(encoded_str)
}

/// Takes a PUA-encoded string, decodes it by restoring encoded PUA codepoints and replacing encoded
/// non-UTF-8 bytes by the replacement character U+FFFD.
/// The result is passed to the [`unicode_width`] crate, which will compute its width, which will be
/// the return value of this function.
pub fn decoded_width(encoded_str: &wstr) -> usize {
    // TODO: Avoid constructing String by using `unicode_width::char_iter_width` once that is
    // available in a released version of the crate (it's already on the crate's master branch).
    use unicode_width::UnicodeWidthStr as _;
    decoder::Decoder::new(encoded_str)
        .collect::<String>()
        .width()
}

pub const fn char_offset(base: char, offset: u32) -> char {
    match char::from_u32(base as u32 + offset) {
        Some(c) => c,
        None => panic!("not a valid char"),
    }
}

/// Encodes the bytes in `input` into a [`WString`], encoding non-UTF-8 bytes into private-use-area
/// code-points. Bytes which would be parsed into our reserved PUA range are encoded individually,
/// to allow for correct round-tripping.
pub fn bytes2wcstring(mut input: &[u8]) -> WString {
    if input.is_empty() {
        return WString::new();
    }

    let mut result = WString::with_capacity(input.len());

    fn append_escaped_str(output: &mut WString, input: &str) {
        for (i, c) in input.char_indices() {
            if fish_reserved_codepoint(c) {
                for byte in &input.as_bytes()[i..i + c.len_utf8()] {
                    output.push(encode_byte_to_char(*byte));
                }
            } else {
                output.push(c);
            }
        }
    }

    while !input.is_empty() {
        match std::str::from_utf8(input) {
            Ok(parsed_str) => {
                append_escaped_str(&mut result, parsed_str);
                // The entire remaining input could be parsed, so we are done.
                break;
            }
            Err(e) => {
                let (valid, after_valid) = input.split_at(e.valid_up_to());
                // SAFETY: The previous `str::from_utf8` call established that the prefix `valid`
                // is valid UTF-8. This prefix may be empty.
                let parsed_str = unsafe { std::str::from_utf8_unchecked(valid) };
                append_escaped_str(&mut result, parsed_str);
                // The length of the prefix of `after_valid` which is invalid UTF-8.
                // The remaining bytes of `input` (if any) will be parsed in subsequent iterations
                // of the loop, starting from the first byte that starts a valid UTF-8-encoded codepoint.
                // `error_len` can return `None`, if it sees a byte sequence that could be the
                // prefix of a valid code-point encoding at the end of the byte slice.
                // This is useful when the input is chunked, but we don't do that, so in this case
                // we use our custom encoding for all remaining bytes (at most 3).
                let error_len = e.error_len().unwrap_or(after_valid.len());
                for byte in &after_valid[..error_len] {
                    result.push(encode_byte_to_char(*byte));
                }
                input = &after_valid[error_len..];
            }
        }
    }
    result
}

/// Use this rather than [`WString::from_str`] when the input could contain PUA bytes we use to
/// encode non-UTF-8 bytes. Otherwise, when decoding the resulting [`WString`], the PUA bytes in
/// the input would be converted to non-UTF-8 bytes.
pub fn str2wcstring<S: AsRef<str>>(input: S) -> WString {
    bytes2wcstring(input.as_ref().as_bytes())
}

pub fn cstr2wcstring<C: AsRef<CStr>>(input: C) -> WString {
    bytes2wcstring(input.as_ref().to_bytes())
}

pub fn osstr2wcstring<O: AsRef<OsStr>>(input: O) -> WString {
    bytes2wcstring(input.as_ref().as_bytes())
}

/// # SAFETY
///
/// `input` must point to a valid NUL-terminated string.
pub unsafe fn charptr2wcstring(input: *const libc::c_char) -> WString {
    let input: &[u8] = unsafe { CStr::from_ptr(input).to_bytes() };
    bytes2wcstring(input)
}

/// Finds `needle` in a `haystack` and returns the index of the first matching element, if any.
///
/// # Examples
///
/// ```
/// use fish_widestring::subslice_position;
/// let haystack = b"ABC ABCDAB ABCDABCDABDE";
///
/// assert_eq!(subslice_position(haystack, b"ABCDABD"), Some(15));
/// assert_eq!(subslice_position(haystack, b"ABCDE"), None);
/// ```
pub fn subslice_position<T: PartialEq>(
    haystack: impl AsRef<[T]>,
    needle: impl AsRef<[T]>,
) -> Option<usize> {
    let needle = needle.as_ref();
    if needle.is_empty() {
        return Some(0);
    }
    let haystack = haystack.as_ref();
    haystack.windows(needle.len()).position(|w| w == needle)
}

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

/// A trait for a thing that can produce a double-ended, cloneable
/// iterator of chars.
/// Common implementations include char, &str, &wstr, &WString.
pub trait IntoCharIter {
    type Iter: DoubleEndedIterator<Item = char> + Clone;
    fn chars(self) -> Self::Iter;
    fn extend_wstring(&self, _out: &mut WString) -> bool {
        false
    }
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

impl<'a> IntoCharIter for &'a String {
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
    fn extend_wstring(&self, out: &mut WString) -> bool {
        out.push_utfstr(wstr::from_char_slice(self));
        true
    }
}

impl<'a> IntoCharIter for &'a wstr {
    type Iter = CharsUtf32<'a>;
    fn chars(self) -> Self::Iter {
        wstr::chars(self)
    }
    fn extend_wstring(&self, out: &mut WString) -> bool {
        self.as_char_slice().extend_wstring(out)
    }
}

impl<'a> IntoCharIter for &'a WString {
    type Iter = CharsUtf32<'a>;
    fn chars(self) -> Self::Iter {
        wstr::chars(self)
    }
    fn extend_wstring(&self, out: &mut WString) -> bool {
        self.as_char_slice().extend_wstring(out)
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

impl<'a: 'b, 'b> IntoCharIter for &'b std::borrow::Cow<'a, str> {
    type Iter = std::str::Chars<'b>;
    fn chars(self) -> Self::Iter {
        str::chars(self)
    }
}

impl<'a: 'b, 'b> IntoCharIter for &'b std::borrow::Cow<'a, wstr> {
    type Iter = CharsUtf32<'b>;
    fn chars(self) -> Self::Iter {
        wstr::chars(self)
    }
    fn extend_wstring(&self, out: &mut WString) -> bool {
        self.as_char_slice().extend_wstring(out)
    }
}

impl<A: DoubleEndedIterator<Item = char> + Clone, B: DoubleEndedIterator<Item = char> + Clone>
    IntoCharIter for std::iter::Chain<A, B>
{
    type Iter = std::iter::Chain<A, B>;
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
            Some(wstr::from_char_slice(prefix))
        } else {
            self.chars = None;
            Some(wstr::from_char_slice(chars))
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
        assert!(L!("abc").starts_with(L!("abc")));
    }

    #[test]
    fn test_suffix() {
        assert!(L!("").ends_with(L!("")));
        assert!(L!("abc").ends_with(L!("")));
        assert!(L!("abc").ends_with('c'));
        assert!(L!("abc").ends_with("bc"));
        assert!(L!("abc").ends_with(L!("bc")));
        assert!(L!("abc").ends_with(L!("abc")));
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
