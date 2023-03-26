// Enumeration of all wildcard types.

use crate::common::{char_offset, WILDCARD_RESERVED_BASE};

/// Character representing any character except '/' (slash).
pub const ANY_CHAR: char = char_offset(WILDCARD_RESERVED_BASE, 0);
/// Character representing any character string not containing '/' (slash).
pub const ANY_STRING: char = char_offset(WILDCARD_RESERVED_BASE, 1);
/// Character representing any character string.
pub const ANY_STRING_RECURSIVE: char = char_offset(WILDCARD_RESERVED_BASE, 2);
/// This is a special pseudo-char that is not used other than to mark the
/// end of the the special characters so we can sanity check the enum range.
pub const ANY_SENTINEL: char = char_offset(WILDCARD_RESERVED_BASE, 3);
