use crate::ffi;
use crate::wchar_ffi::c_str;
use crate::wchar_ffi::{wstr, WCharFromFFI, WString};
use std::{ffi::c_uint, mem};

/// A scoped manager to save the current value of some variable, and optionally set it to a new
/// value. When dropped, it restores the variable to its old value.
///
/// This can be handy when there are multiple code paths to exit a block.
pub struct ScopedPush<'a, T> {
    var: &'a mut T,
    saved_value: Option<T>,
}

impl<'a, T> ScopedPush<'a, T> {
    pub fn new(var: &'a mut T, new_value: T) -> Self {
        let saved_value = mem::replace(var, new_value);

        Self {
            var,
            saved_value: Some(saved_value),
        }
    }

    pub fn restore(&mut self) {
        if let Some(saved_value) = self.saved_value.take() {
            *self.var = saved_value;
        }
    }
}

impl<'a, T> Drop for ScopedPush<'a, T> {
    fn drop(&mut self) {
        self.restore()
    }
}

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum EscapeStringStyle {
    Script(EscapeFlags),
    Url,
    Var,
    Regex,
}

/// Flags for the [`escape_string()`] function. These are only applicable when the escape style is
/// [`EscapeStringStyle::Script`].
#[derive(Debug, Clone, Copy, PartialEq, Eq, Default)]
pub struct EscapeFlags {
    /// Do not escape special fish syntax characters like the semicolon. Only escape non-printable
    /// characters and backslashes.
    pub no_printables: bool,
    /// Do not try to use 'simplified' quoted escapes, and do not use empty quotes as the empty
    /// string.
    pub no_quoted: bool,
    /// Do not escape tildes.
    pub no_tilde: bool,
    /// Replace non-printable control characters with Unicode symbols.
    pub symbolic: bool,
}

/// Replace special characters with backslash escape sequences. Newline is replaced with `\n`, etc.
pub fn escape_string(s: &wstr, style: EscapeStringStyle) -> WString {
    let mut flags_int = 0;

    let style = match style {
        EscapeStringStyle::Script(flags) => {
            const ESCAPE_NO_PRINTABLES: c_uint = 1 << 0;
            const ESCAPE_NO_QUOTED: c_uint = 1 << 1;
            const ESCAPE_NO_TILDE: c_uint = 1 << 2;
            const ESCAPE_SYMBOLIC: c_uint = 1 << 3;

            if flags.no_printables {
                flags_int |= ESCAPE_NO_PRINTABLES;
            }
            if flags.no_quoted {
                flags_int |= ESCAPE_NO_QUOTED;
            }
            if flags.no_tilde {
                flags_int |= ESCAPE_NO_TILDE;
            }
            if flags.symbolic {
                flags_int |= ESCAPE_SYMBOLIC;
            }

            ffi::escape_string_style_t::STRING_STYLE_SCRIPT
        }
        EscapeStringStyle::Url => ffi::escape_string_style_t::STRING_STYLE_URL,
        EscapeStringStyle::Var => ffi::escape_string_style_t::STRING_STYLE_VAR,
        EscapeStringStyle::Regex => ffi::escape_string_style_t::STRING_STYLE_REGEX,
    };

    ffi::escape_string(c_str!(s), flags_int.into(), style).from_ffi()
}
