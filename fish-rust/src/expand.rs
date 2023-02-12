use crate::common::EXPAND_RESERVED_BASE;

#[derive(Debug, Clone, Copy)]
#[repr(u32)]
pub(crate) enum SpecialUnicodeValues {
    HomeDirectory = EXPAND_RESERVED_BASE,
    ProcessExpandSelf,
    VariableExpand,
    VariableExpandSingle,
    BraceBegin,
    BraceEnd,
    BraceSep,
    BraceSpace,
    InternalSeparator,
    VariableExpandEmpty,
    ExpandSentinel,
}

impl TryFrom<char> for SpecialUnicodeValues {
    type Error = &'static str;

    fn try_from(value: char) -> Result<Self, Self::Error> {
        match value {
            c if c as u32 == Self::HomeDirectory as u32 => Ok(Self::HomeDirectory),
            c if c as u32 == Self::ProcessExpandSelf as u32 => Ok(Self::ProcessExpandSelf),
            c if c as u32 == Self::VariableExpand as u32 => Ok(Self::VariableExpand),
            c if c as u32 == Self::VariableExpandSingle as u32 => Ok(Self::VariableExpandSingle),
            c if c as u32 == Self::BraceBegin as u32 => Ok(Self::BraceBegin),
            c if c as u32 == Self::BraceEnd as u32 => Ok(Self::BraceEnd),
            c if c as u32 == Self::BraceSep as u32 => Ok(Self::BraceSep),
            c if c as u32 == Self::BraceSpace as u32 => Ok(Self::BraceSpace),
            c if c as u32 == Self::InternalSeparator as u32 => Ok(Self::InternalSeparator),
            c if c as u32 == Self::VariableExpandEmpty as u32 => Ok(Self::VariableExpandEmpty),
            c if c as u32 == Self::ExpandSentinel as u32 => Ok(Self::ExpandSentinel),
            _ => Err("Character is not a valid Private Use Area value in Fish"),
        }
    }
}
