#[derive(Debug, Copy, Clone, PartialEq)]
pub enum Error {
    // The value overflowed.
    Overflow,

    // The input string was empty.
    Empty,

    // The input string contained an invalid char.
    // Note this may not be returned for conversions which stop at invalid chars.
    InvalidChar,
}
