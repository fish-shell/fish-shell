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
pub const ENCODE_DIRECT_BASE: char = '\u{F600}';
pub const ENCODE_DIRECT_END: char = char_offset(ENCODE_DIRECT_BASE, 256);

pub const fn char_offset(base: char, offset: u32) -> char {
    match char::from_u32(base as u32 + offset) {
        Some(c) => c,
        None => panic!("not a valid char"),
    }
}

pub fn subslice_position<T: Eq>(a: &[T], b: &[T]) -> Option<usize> {
    if b.is_empty() {
        return Some(0);
    }
    a.windows(b.len()).position(|aw| aw == b)
}
