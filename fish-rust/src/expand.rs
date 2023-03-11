use crate::wchar::{EXPAND_RESERVED_BASE, EXPAND_RESERVED_END};

/// Private use area characters used in expansions
#[repr(u32)]
pub enum ExpandChars {
    /// Character representing a home directory.
    HomeDirectory = EXPAND_RESERVED_BASE as u32,
    /// Character representing process expansion for %self.
    ProcessExpandSelf,
    /// Character representing variable expansion.
    VariableExpand,
    /// Character representing variable expansion into a single element.
    VariableExpandSingle,
    /// Character representing the start of a bracket expansion.
    BraceBegin,
    /// Character representing the end of a bracket expansion.
    BraceEnd,
    /// Character representing separation between two bracket elements.
    BraceSep,
    /// Character that takes the place of any whitespace within non-quoted text in braces
    BraceSpace,
    /// Separate subtokens in a token with this character.
    InternalSeparator,
    /// Character representing an empty variable expansion. Only used transitively while expanding
    /// variables.
    VariableExpandEmpty,
}

const _: () = assert!(
    EXPAND_RESERVED_END as u32 > ExpandChars::VariableExpandEmpty as u32,
    "Characters used in expansions must stay within private use area"
);

impl From<ExpandChars> for char {
    fn from(val: ExpandChars) -> Self {
        // We know this is safe because we limit the the range of this enum
        unsafe { char::from_u32_unchecked(val as _) }
    }
}
