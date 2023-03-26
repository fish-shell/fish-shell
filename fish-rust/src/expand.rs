use crate::common::{char_offset, EXPAND_RESERVED_BASE, EXPAND_RESERVED_END};
use crate::wchar::wstr;
use widestring_suffix::widestrs;

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
#[widestrs]
pub const PROCESS_EXPAND_SELF_STR: &wstr = "%self"L;
