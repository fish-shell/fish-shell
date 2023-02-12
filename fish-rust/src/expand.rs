use crate::wchar::EXPAND_RESERVED_BASE;

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
        Self::try_from(value as u32)
    }
}

impl TryFrom<u32> for SpecialUnicodeValues {
    type Error = &'static str;

    fn try_from(value: u32) -> Result<Self, Self::Error> {
        match value {
            c if c == Self::HomeDirectory as u32 => Ok(Self::HomeDirectory),
            c if c == Self::ProcessExpandSelf as u32 => Ok(Self::ProcessExpandSelf),
            c if c == Self::VariableExpand as u32 => Ok(Self::VariableExpand),
            c if c == Self::VariableExpandSingle as u32 => Ok(Self::VariableExpandSingle),
            c if c == Self::BraceBegin as u32 => Ok(Self::BraceBegin),
            c if c == Self::BraceEnd as u32 => Ok(Self::BraceEnd),
            c if c == Self::BraceSep as u32 => Ok(Self::BraceSep),
            c if c == Self::BraceSpace as u32 => Ok(Self::BraceSpace),
            c if c == Self::InternalSeparator as u32 => Ok(Self::InternalSeparator),
            c if c == Self::VariableExpandEmpty as u32 => Ok(Self::VariableExpandEmpty),
            c if c == Self::ExpandSentinel as u32 => Ok(Self::ExpandSentinel),
            _ => Err("unsigned integer is not a valid Private Use Area value in Fish"),
        }
    }
}
