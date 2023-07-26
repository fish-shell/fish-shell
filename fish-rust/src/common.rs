//! Prototypes for various functions, mostly string utilities, that are used by most parts of fish.

use crate::compat::MB_CUR_MAX;
use crate::expand::{
    BRACE_BEGIN, BRACE_END, BRACE_SEP, BRACE_SPACE, HOME_DIRECTORY, INTERNAL_SEPARATOR,
    PROCESS_EXPAND_SELF, PROCESS_EXPAND_SELF_STR, VARIABLE_EXPAND, VARIABLE_EXPAND_SINGLE,
};
use crate::fallback::fish_wcwidth;
use crate::ffi::{self};
use crate::flog::FLOG;
use crate::future_feature_flags::{feature_test, FeatureFlag};
use crate::global_safety::RelaxedAtomicBool;
use crate::termsize::Termsize;
use crate::wchar::{decode_byte_from_char, encode_byte_to_char, prelude::*};
use crate::wchar_ffi::WCharToFFI;
use crate::wcstringutil::wcs2string_callback;
use crate::wildcard::{ANY_CHAR, ANY_STRING, ANY_STRING_RECURSIVE};
use crate::wutil::encoding::{mbrtowc, wcrtomb, zero_mbstate, AT_LEAST_MB_LEN_MAX};
use crate::wutil::{fish_iswalnum, wwrite_to_fd};
use bitflags::bitflags;
use core::slice;
use cxx::{CxxWString, UniquePtr};
use libc::{EINTR, EIO, O_WRONLY, SIGTTOU, SIG_IGN, STDERR_FILENO, STDIN_FILENO, STDOUT_FILENO};
use num_traits::ToPrimitive;
use once_cell::sync::{Lazy, OnceCell};
use std::env;
use std::ffi::{CStr, CString, OsStr, OsString};
use std::mem;
use std::ops::{Deref, DerefMut};
use std::os::unix::prelude::*;
use std::path::{Path, PathBuf};
use std::sync::atomic::{AtomicI32, AtomicU32, Ordering};
use std::sync::{Arc, Mutex, TryLockError};
use std::time;

// Highest legal ASCII value.
pub const ASCII_MAX: char = 127 as char;

// Highest legal 16-bit Unicode value.
pub const UCS2_MAX: char = '\u{FFFF}';

// Highest legal byte value.
pub const BYTE_MAX: char = 0xFF as char;

// Unicode BOM value.
pub const UTF8_BOM_WCHAR: char = '\u{FEFF}';

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

// The address where bug reports for this package should be sent.
pub const PACKAGE_BUGREPORT: &str = "https://github.com/fish-shell/fish-shell/issues";

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum EscapeStringStyle {
    Script(EscapeFlags),
    Url,
    Var,
    Regex,
}

impl Default for EscapeStringStyle {
    fn default() -> Self {
        Self::Script(EscapeFlags::default())
    }
}

impl TryFrom<&wstr> for EscapeStringStyle {
    type Error = &'static wstr;
    fn try_from(s: &wstr) -> Result<Self, Self::Error> {
        use EscapeStringStyle::*;
        match s {
            s if s == "script" => Ok(Self::default()),
            s if s == "var" => Ok(Var),
            s if s == "url" => Ok(Url),
            s if s == "regex" => Ok(Regex),
            _ => Err(L!("Invalid escape style")),
        }
    }
}

bitflags! {
    /// Flags for the [`escape_string()`] function. These are only applicable when the escape style is
    /// [`EscapeStringStyle::Script`].
    #[derive(Default)]
    pub struct EscapeFlags: u32 {
        /// Do not escape special fish syntax characters like the semicolon. Only escape non-printable
        /// characters and backslashes.
        const NO_PRINTABLES = 1 << 0;
        /// Do not try to use 'simplified' quoted escapes, and do not use empty quotes as the empty
        /// string.
        const NO_QUOTED = 1 << 1;
        /// Do not escape tildes.
        const NO_TILDE = 1 << 2;
        /// Replace non-printable control characters with Unicode symbols.
        const SYMBOLIC = 1 << 3;
    }
}

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum UnescapeStringStyle {
    Script(UnescapeFlags),
    Url,
    Var,
}

impl Default for UnescapeStringStyle {
    fn default() -> Self {
        Self::Script(UnescapeFlags::default())
    }
}

impl TryFrom<&wstr> for UnescapeStringStyle {
    type Error = &'static wstr;
    fn try_from(s: &wstr) -> Result<Self, Self::Error> {
        use UnescapeStringStyle::*;
        match s {
            s if s == "script" => Ok(Self::default()),
            s if s == "var" => Ok(Var),
            s if s == "url" => Ok(Url),
            _ => Err(L!("Invalid escape style")),
        }
    }
}

bitflags! {
    /// Flags for unescape_string functions.
    #[derive(Default)]
    pub struct UnescapeFlags: u32 {
        /// escape special fish syntax characters like the semicolon
        const SPECIAL = 1 << 0;
        /// allow incomplete escape sequences
        const INCOMPLETE = 1 << 1;
        /// don't handle backslash escapes
        const NO_BACKSLASHES = 1 << 2;
    }
}

/// Replace special characters with backslash escape sequences. Newline is replaced with `\n`, etc.
pub fn escape(s: &wstr) -> WString {
    escape_string(s, EscapeStringStyle::Script(EscapeFlags::default()))
}

/// Replace special characters with backslash escape sequences. Newline is replaced with `\n`, etc.
pub fn escape_string(s: &wstr, style: EscapeStringStyle) -> WString {
    match style {
        EscapeStringStyle::Script(flags) => escape_string_script(s, flags),
        EscapeStringStyle::Url => escape_string_url(s),
        EscapeStringStyle::Var => escape_string_var(s),
        EscapeStringStyle::Regex => escape_string_pcre2(s),
    }
}

/// Escape a string in a fashion suitable for using in fish script.
#[widestrs]
fn escape_string_script(input: &wstr, flags: EscapeFlags) -> WString {
    let escape_printables = !flags.contains(EscapeFlags::NO_PRINTABLES);
    let no_quoted = flags.contains(EscapeFlags::NO_QUOTED);
    let no_tilde = flags.contains(EscapeFlags::NO_TILDE);
    let no_qmark = feature_test(FeatureFlag::qmark_noglob);
    let symbolic = flags.contains(EscapeFlags::SYMBOLIC) && MB_CUR_MAX() > 1;

    assert!(
        !symbolic || !escape_printables,
        "symbolic implies escape-no-printables"
    );

    let mut need_escape = false;
    let mut need_complex_escape = false;

    if !no_quoted && input.is_empty() {
        return "''"L.to_owned();
    }

    let mut out = WString::new();

    for c in input.chars() {
        if let Some(val) = decode_byte_from_char(c) {
            out += "\\X";

            let nibble1 = val / 16;
            let nibble2 = val % 16;

            out.push(char::from_digit(nibble1.into(), 16).unwrap());
            out.push(char::from_digit(nibble2.into(), 16).unwrap());
            need_escape = true;
            need_complex_escape = true;
            continue;
        }
        match c {
            '\t' => {
                if symbolic {
                    out.push('␉');
                } else {
                    out += "\\t"L;
                }
                need_escape = true;
                need_complex_escape = true;
            }
            '\n' => {
                if symbolic {
                    out.push('␤');
                } else {
                    out += "\\n"L;
                }
                need_escape = true;
                need_complex_escape = true;
            }
            '\x08' => {
                if symbolic {
                    out.push('␈');
                } else {
                    out += "\\b"L;
                }
                need_escape = true;
                need_complex_escape = true;
            }
            '\r' => {
                if symbolic {
                    out.push('␍');
                } else {
                    out += "\\r"L;
                }
                need_escape = true;
                need_complex_escape = true;
            }
            '\x1B' => {
                if symbolic {
                    out.push('␛');
                } else {
                    out += "\\e"L;
                }
                need_escape = true;
                need_complex_escape = true;
            }
            '\x7F' => {
                if symbolic {
                    out.push('␡');
                } else {
                    out += "\\x7f"L;
                }
                need_escape = true;
                need_complex_escape = true;
            }
            '\\' | '\'' => {
                need_escape = true;
                need_complex_escape = true;
                if escape_printables || (c == '\\' && !symbolic) {
                    out.push('\\');
                }
                out.push(c);
            }
            ANY_CHAR => {
                // See #1614
                out.push('?');
            }
            ANY_STRING => {
                out.push('*');
            }
            ANY_STRING_RECURSIVE => {
                out += "**"L;
            }

            '&' | '$' | ' ' | '#' | '<' | '>' | '(' | ')' | '[' | ']' | '{' | '}' | '?' | '*'
            | '|' | ';' | '"' | '%' | '~' => {
                let char_is_normal = (c == '~' && no_tilde) || (c == '?' && no_qmark);
                if !char_is_normal {
                    need_escape = true;
                    if escape_printables {
                        out.push('\\')
                    };
                }
                out.push(c);
            }
            _ => {
                let cval = u32::from(c);
                if cval < 32 {
                    need_escape = true;
                    need_complex_escape = true;

                    if symbolic {
                        out.push(char::from_u32(0x2400 + cval).unwrap());
                        continue;
                    }

                    if cval < 27 && cval != 0 {
                        out.push('\\');
                        out.push('c');
                        out.push(char::from_u32(u32::from(b'a') + cval - 1).unwrap());
                        continue;
                    }

                    let nibble = cval % 16;
                    out.push('\\');
                    out.push('x');
                    out.push(if cval > 15 { '1' } else { '0' });
                    out.push(char::from_digit(nibble, 16).unwrap());
                } else {
                    out.push(c);
                }
            }
        }
    }

    // Use quoted escaping if possible, since most people find it easier to read.
    if !no_quoted && need_escape && !need_complex_escape && escape_printables {
        let single_quote = '\'';
        out.clear();
        out.reserve(2 + input.len());
        out.push(single_quote);
        out.push_utfstr(input);
        out.push(single_quote);
    }

    out
}

/// Test whether the char is a valid hex digit as used by the `escape_string_*()` functions.
/// Note this only considers uppercase characters.
fn is_upper_hex_digit(c: char) -> bool {
    matches!(c, '0'..='9' | 'A'..='F')
}

/// Return the high and low nibbles of a byte, as uppercase hex characters.
fn byte_to_hex(byte: u8) -> (char, char) {
    const HEX: [u8; 16] = *b"0123456789ABCDEF";
    let high = byte >> 4;
    let low = byte & 0xF;
    (HEX[high as usize].into(), HEX[low as usize].into())
}

/// Escape a string in a fashion suitable for using as a URL. Store the result in out_str.
#[widestrs]
fn escape_string_url(input: &wstr) -> WString {
    let narrow = wcs2string(input);
    let mut out = WString::new();
    for byte in narrow.into_iter() {
        if (byte & 0x80) == 0 {
            let c = char::from_u32(u32::from(byte)).unwrap();
            if c.is_alphanumeric() || [b'/', b'.', b'~', b'-', b'_'].contains(&byte) {
                // The above characters don't need to be encoded.
                out.push(c);
                continue;
            }
        }
        // All other chars need to have their narrow representation encoded in hex.
        let (high, low) = byte_to_hex(byte);
        out.push('%');
        out.push(high);
        out.push(low);
    }
    out
}

/// Escape a string in a fashion suitable for using as a fish var name. Store the result in out_str.
fn escape_string_var(input: &wstr) -> WString {
    let mut prev_was_hex_encoded = false;
    let narrow = wcs2string(input);
    let mut out = WString::new();
    for c in narrow.into_iter() {
        let ch: char = c.into();
        if ((c & 0x80) == 0 && ch.is_alphanumeric())
            && (!prev_was_hex_encoded || !is_upper_hex_digit(ch))
        {
            // ASCII alphanumerics don't need to be encoded.
            if prev_was_hex_encoded {
                out.push('_');
                prev_was_hex_encoded = false;
            }
            out.push(ch);
        } else if c == b'_' {
            // Underscores are encoded by doubling them.
            out.push_str("__");
            prev_was_hex_encoded = false;
        } else {
            // All other chars need to have their narrow representation encoded in hex.
            let (high, low) = byte_to_hex(c);
            out.push('_');
            out.push(high);
            out.push(low);
            prev_was_hex_encoded = true;
        }
    }
    if prev_was_hex_encoded {
        out.push('_');
    }
    out
}

/// Escapes a string for use in a regex string. Not safe for use with `eval` as only
/// characters reserved by PCRE2 are escaped.
/// \param in is the raw string to be searched for literally when substituted in a PCRE2 expression.
fn escape_string_pcre2(input: &wstr) -> WString {
    let mut out = WString::new();
    out.reserve(
        (f64::from(u32::try_from(input.len()).unwrap()) * 1.3) // a wild guess
            .to_i128()
            .unwrap()
            .try_into()
            .unwrap(),
    );

    for c in input.chars() {
        if [
            '.', '^', '$', '*', '+', '(', ')', '?', '[', '{', '}', '\\', '|',
            // these two only *need* to be escaped within a character class, and technically it
            // makes no sense to ever use process substitution output to compose a character class,
            // but...
            '-', ']',
        ]
        .contains(&c)
        {
            out.push('\\');
        }
        out.push(c);
    }

    out
}

/// Escape a string so that it may be inserted into a double-quoted string.
/// This permits ownership transfer.
pub fn escape_string_for_double_quotes(input: &wstr) -> WString {
    // We need to escape backslashes, double quotes, and dollars only.
    let mut result = input.to_owned();
    let mut idx = result.len();
    while idx > 0 {
        idx -= 1;
        if ['\\', '$', '"'].contains(&result.char_at(idx)) {
            result.insert(idx, '\\');
        }
    }
    result
}

pub fn unescape_string(input: &wstr, style: UnescapeStringStyle) -> Option<WString> {
    match style {
        UnescapeStringStyle::Script(flags) => unescape_string_internal(input, flags),
        UnescapeStringStyle::Url => unescape_string_url(input),
        UnescapeStringStyle::Var => unescape_string_var(input),
    }
}

// TODO Delete this.
pub fn unescape_string_in_place(s: &mut WString, style: UnescapeStringStyle) -> bool {
    unescape_string(s, style)
        .map(|unescaped| *s = unescaped)
        .is_some()
}

/// Returns the unescaped version of input, or None on error.
fn unescape_string_internal(input: &wstr, flags: UnescapeFlags) -> Option<WString> {
    let mut result = WString::new();
    result.reserve(input.len());

    let unescape_special = flags.contains(UnescapeFlags::SPECIAL);
    let allow_incomplete = flags.contains(UnescapeFlags::INCOMPLETE);
    let ignore_backslashes = flags.contains(UnescapeFlags::NO_BACKSLASHES);

    // The positions of open braces.
    let mut braces = vec![];
    // The positions of variable expansions or brace ","s.
    // We only read braces as expanders if there's a variable expansion or "," in them.
    let mut vars_or_seps = vec![];
    let mut brace_count = 0;

    let mut errored = false;
    #[derive(PartialEq, Eq)]
    enum Mode {
        Unquoted,
        SingleQuotes,
        DoubleQuotes,
    }
    let mut mode = Mode::Unquoted;

    let mut input_position = 0;
    while input_position < input.len() && !errored {
        let c = input.char_at(input_position);
        // Here's the character we'll append to result, or none() to suppress it.
        let mut to_append_or_none = Some(c);
        if mode == Mode::Unquoted {
            match c {
                '\\' => {
                    if !ignore_backslashes {
                        // Backslashes (escapes) are complicated and may result in errors, or
                        // appending INTERNAL_SEPARATORs, so we have to handle them specially.
                        if let Some(escape_chars) = read_unquoted_escape(
                            &input[input_position..],
                            &mut result,
                            allow_incomplete,
                            unescape_special,
                        ) {
                            // Skip over the characters we read, minus one because the outer loop
                            // will increment it.
                            assert!(escape_chars > 0);
                            input_position += escape_chars - 1;
                        } else {
                            // A none() return indicates an error.
                            errored = true;
                        }
                        // We've already appended, don't append anything else.
                        to_append_or_none = None;
                    }
                }
                '~' => {
                    if unescape_special && input_position == 0 {
                        to_append_or_none = Some(HOME_DIRECTORY);
                    }
                }
                '%' => {
                    // Note that this only recognizes %self if the string is literally %self.
                    // %self/foo will NOT match this.
                    if unescape_special && input_position == 0 && input == PROCESS_EXPAND_SELF_STR {
                        to_append_or_none = Some(PROCESS_EXPAND_SELF);
                        input_position += PROCESS_EXPAND_SELF_STR.len() - 1; // skip over 'self's
                    }
                }
                '*' => {
                    if unescape_special {
                        // In general, this is ANY_STRING. But as a hack, if the last appended char
                        // is ANY_STRING, delete the last char and store ANY_STRING_RECURSIVE to
                        // reflect the fact that ** is the recursive wildcard.
                        if result.chars().last() == Some(ANY_STRING) {
                            assert!(!result.is_empty());
                            result.truncate(result.len() - 1);
                            to_append_or_none = Some(ANY_STRING_RECURSIVE);
                        } else {
                            to_append_or_none = Some(ANY_STRING);
                        }
                    }
                }
                '?' => {
                    if unescape_special && !feature_test(FeatureFlag::qmark_noglob) {
                        to_append_or_none = Some(ANY_CHAR);
                    }
                }
                '$' => {
                    if unescape_special {
                        let is_cmdsub = input_position + 1 < input.len()
                            && input.char_at(input_position + 1) == '(';
                        if !is_cmdsub {
                            to_append_or_none = Some(VARIABLE_EXPAND);
                            vars_or_seps.push(input_position);
                        }
                    }
                }
                '{' => {
                    if unescape_special {
                        brace_count += 1;
                        to_append_or_none = Some(BRACE_BEGIN);
                        // We need to store where the brace *ends up* in the output.
                        braces.push(result.len());
                    }
                }
                '}' => {
                    if unescape_special {
                        // HACK: The completion machinery sometimes hands us partial tokens.
                        // We can't parse them properly, but it shouldn't hurt,
                        // so we don't assert here.
                        // See #4954.
                        // assert(brace_count > 0 && "imbalanced brackets are a tokenizer error, we
                        // shouldn't be able to get here");
                        brace_count -= 1;
                        to_append_or_none = Some(BRACE_END);
                        if let Some(brace) = braces.pop() {
                            // HACK: To reduce accidental use of brace expansion, treat a brace
                            // with zero or one items as literal input. See #4632. (The hack is
                            // doing it here and like this.)
                            if vars_or_seps.last().map(|i| *i < brace).unwrap_or(true) {
                                result.as_char_slice_mut()[brace] = '{';
                                // We also need to turn all spaces back.
                                for i in brace + 1..result.len() {
                                    if result.char_at(i) == BRACE_SPACE {
                                        result.as_char_slice_mut()[i] = ' ';
                                    }
                                }
                                to_append_or_none = Some('}');
                            }
                            // Remove all seps inside the current brace pair, so if we have a
                            // surrounding pair we only get seps inside *that*.
                            if !vars_or_seps.is_empty() {
                                while vars_or_seps.last().map(|i| *i > brace).unwrap_or_default() {
                                    vars_or_seps.pop();
                                }
                            }
                        }
                    }
                }
                ',' => {
                    if unescape_special && brace_count > 0 {
                        to_append_or_none = Some(BRACE_SEP);
                        vars_or_seps.push(input_position);
                    }
                }
                ' ' => {
                    if unescape_special && brace_count > 0 {
                        to_append_or_none = Some(BRACE_SPACE);
                    }
                }
                '\'' => {
                    mode = Mode::SingleQuotes;
                    to_append_or_none = if unescape_special {
                        Some(INTERNAL_SEPARATOR)
                    } else {
                        None
                    };
                }
                '"' => {
                    mode = Mode::DoubleQuotes;
                    to_append_or_none = if unescape_special {
                        Some(INTERNAL_SEPARATOR)
                    } else {
                        None
                    };
                }
                _ => (),
            }
        } else if mode == Mode::SingleQuotes {
            if c == '\\' {
                // A backslash may or may not escape something in single quotes.
                match input.char_at(input_position + 1) {
                    '\\' | '\'' => {
                        to_append_or_none = Some(input.char_at(input_position + 1));
                        input_position += 1; // skip over the backslash
                    }
                    '\0' => {
                        if !allow_incomplete {
                            errored = true;
                        } else {
                            // PCA this line had the following cryptic comment: 'We may ever escape
                            // a NULL character, but still appending a \ in case I am wrong.' Not
                            // sure what it means or the importance of this.
                            input_position += 1; /* Skip over the backslash */
                            to_append_or_none = Some('\\');
                        }
                    }
                    _ => {
                        // Literal backslash that doesn't escape anything! Leave things alone; we'll
                        // append the backslash itself.
                    }
                }
            } else if c == '\'' {
                to_append_or_none = if unescape_special {
                    Some(INTERNAL_SEPARATOR)
                } else {
                    None
                };
                mode = Mode::Unquoted;
            }
        } else if mode == Mode::DoubleQuotes {
            match c {
                '"' => {
                    mode = Mode::Unquoted;
                    to_append_or_none = if unescape_special {
                        Some(INTERNAL_SEPARATOR)
                    } else {
                        None
                    };
                }
                '\\' => {
                    match input.char_at(input_position + 1) {
                        '\0' => {
                            if !allow_incomplete {
                                errored = true;
                            } else {
                                to_append_or_none = Some('\0');
                            }
                        }
                        '\\' | '$' | '"' => {
                            to_append_or_none = Some(input.char_at(input_position + 1));
                            input_position += 1; /* Skip over the backslash */
                        }
                        '\n' => {
                            /* Swallow newline */
                            to_append_or_none = None;
                            input_position += 1; /* Skip over the backslash */
                        }
                        _ => {
                            /* Literal backslash that doesn't escape anything! Leave things alone;
                             * we'll append the backslash itself */
                        }
                    }
                }
                '$' => {
                    if unescape_special {
                        to_append_or_none = Some(VARIABLE_EXPAND_SINGLE);
                        vars_or_seps.push(input_position);
                    }
                }
                _ => (),
            }
        }

        // Now maybe append the char.
        if let Some(c) = to_append_or_none {
            result.push(c);
        }
        input_position += 1;
    }

    // Return the string by reference, and then success.
    if errored {
        return None;
    }
    Some(result)
}

/// Reverse the effects of `escape_string_url()`. By definition the input should consist of just
/// ASCII chars.
fn unescape_string_url(input: &wstr) -> Option<WString> {
    let mut result: Vec<u8> = Vec::with_capacity(input.len());
    let mut i = 0;
    while i < input.len() {
        let c = input.char_at(i);
        if c > '\u{7F}' {
            return None; // invalid character means we can't decode the string
        }
        if c == '%' {
            let c1 = input.char_at(i + 1);
            if c1 == '\0' {
                return None;
            } else if c1 == '%' {
                result.push(b'%');
                i += 1;
            } else {
                let d1 = c1.to_digit(16)?;
                let c2 = input.char_at(i + 2);
                let d2 = c2.to_digit(16)?; // also fails if '\0' i.e. premature end
                result.push((16 * d1 + d2) as u8);
                i += 2;
            }
        } else {
            result.push(c as u8);
        }
        i += 1
    }

    Some(str2wcstring(&result))
}

/// Reverse the effects of `escape_string_var()`. By definition the string should consist of just
/// ASCII chars.
fn unescape_string_var(input: &wstr) -> Option<WString> {
    let mut result: Vec<u8> = Vec::with_capacity(input.len());
    let mut prev_was_hex_encoded = false;
    let mut i = 0;
    while i < input.len() {
        let c = input.char_at(i);
        if c > '\u{7F}' {
            return None; // invalid character means we can't decode the string
        }
        if c == '_' {
            let c1 = input.char_at(i + 1);
            if c1 == '\0' {
                if prev_was_hex_encoded {
                    break;
                }
                return None; // found unexpected escape char at end of string
            } else if c1 == '_' {
                result.push(b'_');
                i += 1;
            } else if is_upper_hex_digit(c1) {
                let d1 = c1.to_digit(16)?;
                let c2 = input.char_at(i + 2);
                let d2 = c2.to_digit(16)?; // also fails if '\0' i.e. premature end
                result.push((16 * d1 + d2) as u8);
                i += 2;
                prev_was_hex_encoded = true;
            }
            // No "else" clause because if the first char after an underscore is not another
            // underscore or a valid hex character then the underscore is there to improve
            // readability after we've encoded a character not valid in a var name.
        } else {
            result.push(c as u8);
        }
        i += 1;
    }

    Some(str2wcstring(&result))
}

/// Given a string starting with a backslash, read the escape as if it is unquoted, appending
/// to result. Return the number of characters consumed, or none on error.
pub fn read_unquoted_escape(
    input: &wstr,
    result: &mut WString,
    allow_incomplete: bool,
    unescape_special: bool,
) -> Option<usize> {
    assert!(input.char_at(0) == '\\', "not an escape");

    // Here's the character we'll ultimately append, or none. Note that '\0' is a
    // valid thing to append.
    let mut result_char_or_none: Option<char> = None;

    let mut errored = false;
    let mut in_pos = 1; // in_pos always tracks the next character to read (and therefore the number
                        // of characters read so far)

    // For multibyte \X sequences.
    let mut byte_buff: Vec<u8> = vec![];

    loop {
        let c = input.char_at(in_pos);
        in_pos += 1;
        match c {
            // A null character after a backslash is an error.
            '\0' => {
                // Adjust in_pos to only include the backslash.
                assert!(in_pos > 0);
                in_pos -= 1;

                // It's an error, unless we're allowing incomplete escapes.
                if !allow_incomplete {
                    errored = true;
                }
            }
            // Numeric escape sequences. No prefix means octal escape, otherwise hexadecimal.
            '0'..='7' | 'u' | 'U' | 'x' | 'X' => {
                let mut res: u64 = 0;
                let mut chars = 2;
                let mut base = 16;
                let mut byte_literal = false;
                let mut max_val = ASCII_MAX;

                match c {
                    'u' => {
                        chars = 4;
                        max_val = UCS2_MAX;
                    }
                    'U' => {
                        chars = 8;
                        // Don't exceed the largest Unicode code point - see #1107.
                        max_val = char::MAX;
                    }
                    'x' | 'X' => {
                        byte_literal = true;
                        max_val = BYTE_MAX;
                    }
                    _ => {
                        base = 8;
                        chars = 3;
                        // Note that in_pos currently is just after the first post-backslash
                        // character; we want to start our escape from there.
                        assert!(in_pos > 0);
                        in_pos -= 1;
                    }
                }

                for i in 0..chars {
                    let Some(d) = input.char_at(in_pos).to_digit(base) else {
                        // If we have no digit, this is a tokenizer error.
                        if i == 0 {
                            errored = true;
                        }
                        break;
                    };

                    res = (res * u64::from(base)) + u64::from(d);
                    in_pos += 1;
                }

                if !errored && res <= u64::from(max_val) {
                    if byte_literal {
                        // Multibyte encodings necessitate that we keep adjacent byte escapes.
                        // - `\Xc3\Xb6` is "ö", but only together.
                        // (this assumes a valid codepoint can't consist of multiple bytes
                        // that are valid on their own, which is true for UTF-8)
                        byte_buff.push(res.try_into().unwrap());
                        result_char_or_none = None;
                        if input[in_pos..].starts_with("\\X") || input[in_pos..].starts_with("\\x")
                        {
                            in_pos += 1;
                            continue;
                        }
                    } else {
                        result_char_or_none =
                            Some(char::from_u32(res.try_into().unwrap()).unwrap_or('\u{FFFD}'));
                    }
                } else {
                    errored = true;
                }
            }
            // \a means bell (alert).
            'a' => {
                result_char_or_none = Some('\x07');
            }
            // \b means backspace.
            'b' => {
                result_char_or_none = Some('\x08');
            }
            // \cX means control sequence X.
            'c' => {
                let sequence_char = u32::from(input.char_at(in_pos));
                in_pos += 1;
                if sequence_char >= u32::from('a') && sequence_char <= u32::from('a') + 32 {
                    result_char_or_none =
                        Some(char::from_u32(sequence_char - u32::from('a') + 1).unwrap());
                } else if sequence_char >= u32::from('A') && sequence_char <= u32::from('A') + 32 {
                    result_char_or_none =
                        Some(char::from_u32(sequence_char - u32::from('A') + 1).unwrap());
                } else {
                    errored = true;
                }
            }
            // \x1B means escape.
            'e' => {
                result_char_or_none = Some('\x1B');
            }
            // \f means form feed.
            'f' => {
                result_char_or_none = Some('\x0C');
            }
            // \n means newline.
            'n' => {
                result_char_or_none = Some('\n');
            }
            // \r means carriage return.
            'r' => {
                result_char_or_none = Some('\x0D');
            }
            // \t means tab.
            't' => {
                result_char_or_none = Some('\t');
            }
            // \v means vertical tab.
            'v' => {
                result_char_or_none = Some('\x0b');
            }
            // If a backslash is followed by an actual newline, swallow them both.
            '\n' => {
                result_char_or_none = None;
            }
            _ => {
                if unescape_special {
                    result.push(INTERNAL_SEPARATOR);
                }
                result_char_or_none = Some(c);
            }
        }

        if errored {
            return None;
        }

        if !byte_buff.is_empty() {
            result.push_utfstr(&str2wcstring(&byte_buff));
        }

        break;
    }

    if let Some(c) = result_char_or_none {
        result.push(c);
    }

    Some(in_pos)
}

pub const fn char_offset(base: char, offset: u32) -> char {
    match char::from_u32(base as u32 + offset) {
        Some(c) => c,
        None => panic!("not a valid char"),
    }
}

#[no_mangle]
#[inline(never)]
fn debug_thread_error() {
    // Wait for a SIGINT. We can't use sigsuspend() because the signal may be delivered on another
    // thread.
    use crate::signal::SigChecker;
    use crate::topic_monitor::topic_t;
    let sigint = SigChecker::new(topic_t::sighupint);
    sigint.wait();
}

/// Exits without invoking destructors (via _exit), useful for code after fork.
pub fn exit_without_destructors(code: i32) -> ! {
    unsafe { libc::_exit(code) };
}

/// Save the shell mode on startup so we can restore them on exit.
static SHELL_MODES: Lazy<Mutex<libc::termios>> = Lazy::new(|| Mutex::new(unsafe { mem::zeroed() }));

/// The character to use where the text has been truncated. Is an ellipsis on unicode system and a $
/// on other systems.
pub fn get_ellipsis_char() -> char {
    char::from_u32(ELLIPSIS_CHAR.load(Ordering::Relaxed)).unwrap()
}

static ELLIPSIS_CHAR: AtomicU32 = AtomicU32::new(0);

/// The character or string to use where text has been truncated (ellipsis if possible, otherwise
/// ...)
pub fn get_ellipsis_str() -> &'static wstr {
    unsafe { *ELLIPSIS_STRING }
}

static mut ELLIPSIS_STRING: Lazy<&'static wstr> = Lazy::new(|| L!(""));

/// Character representing an omitted newline at the end of text.
pub fn get_omitted_newline_str() -> &'static wstr {
    unsafe { &OMITTED_NEWLINE_STR }
}

static mut OMITTED_NEWLINE_STR: Lazy<&'static wstr> = Lazy::new(|| L!(""));

pub fn get_omitted_newline_width() -> usize {
    unsafe { OMITTED_NEWLINE_STR.len() }
}

static OBFUSCATION_READ_CHAR: AtomicU32 = AtomicU32::new(0);

pub fn get_obfuscation_read_char() -> char {
    char::from_u32(OBFUSCATION_READ_CHAR.load(Ordering::Relaxed)).unwrap()
}

/// Profiling flag. True if commands should be profiled.
pub static PROFILING_ACTIVE: RelaxedAtomicBool = RelaxedAtomicBool::new(false);

/// Name of the current program. Should be set at startup. Used by the debug function.
pub static PROGRAM_NAME: OnceCell<&'static wstr> = OnceCell::new();

/// MS Windows tty devices do not currently have either a read or write timestamp - those respective
/// fields of `struct stat` are always set to the current time, which means we can't rely on them.
/// In this case, we assume no external program has written to the terminal behind our back, making
/// the multiline prompt usable. See #2859 and https://github.com/Microsoft/BashOnWindows/issues/545
pub fn has_working_tty_timestamps() -> bool {
    if cfg!(target_os = "windows") {
        false
    } else if cfg!(target_os = "linux") {
        !is_windows_subsystem_for_linux()
    } else {
        true
    }
}

/// A global, empty string. This is useful for functions which wish to return a reference to an
/// empty string.
pub static EMPTY_STRING: WString = WString::new();

/// A global, empty string list. This is useful for functions which wish to return a reference
/// to an empty string.
pub static EMPTY_STRING_LIST: Vec<WString> = vec![];

/// A function type to check for cancellation.
/// \return true if execution should cancel.
pub type CancelChecker = dyn Fn() -> bool;

/// Converts the narrow character string \c in into its wide equivalent, and return it.
///
/// The string may contain embedded nulls.
///
/// This function encodes illegal character sequences in a reversible way using the private use
/// area.
pub fn str2wcstring(inp: &[u8]) -> WString {
    if inp.is_empty() {
        return WString::new();
    }

    let mut result = WString::new();
    result.reserve(inp.len());
    let mut pos = 0;
    let mut state = zero_mbstate();
    while pos < inp.len() {
        // Append any initial sequence of ascii characters.
        // Note we do not support character sets which are not supersets of ASCII.
        let ascii_prefix_length = count_ascii_prefix(&inp[pos..]);
        result.push_str(std::str::from_utf8(&inp[pos..pos + ascii_prefix_length]).unwrap());
        pos += ascii_prefix_length;
        assert!(pos <= inp.len(), "Position overflowed length");
        if pos == inp.len() {
            break;
        }

        // We have found a non-ASCII character.
        let mut ret = 0;
        let mut c = '\0';

        let use_encode_direct = if inp[pos] & 0xF8 == 0xF8 {
            // Protect against broken mbrtowc() implementations which attempt to encode UTF-8
            // sequences longer than four bytes (e.g., OS X Snow Leopard).
            // TODO This check used to be conditionally compiled only on affected platforms.
            true
        } else {
            const _: () = assert!(mem::size_of::<libc::wchar_t>() == mem::size_of::<char>());
            let mut codepoint = u32::from(c);
            ret = unsafe {
                mbrtowc(
                    std::ptr::addr_of_mut!(codepoint).cast(),
                    std::ptr::addr_of!(inp[pos]).cast(),
                    inp.len() - pos,
                    &mut state,
                )
            };
            match char::from_u32(codepoint) {
                Some(codepoint) => {
                    c = codepoint;
                    // Determine whether to encode this character with our crazy scheme.
                    (c >= ENCODE_DIRECT_BASE && c < ENCODE_DIRECT_END)
                    ||
                    c == INTERNAL_SEPARATOR
                    ||
                    // Incomplete sequence.
                    ret == 0_usize.wrapping_sub(2)
                    ||
                    // Invalid data.
                    ret == 0_usize.wrapping_sub(1)
                    ||
                    // Other error codes? Terrifying, should never happen.
                    ret > inp.len() - pos
                }
                None => true,
            }
        };

        if use_encode_direct {
            c = encode_byte_to_char(inp[pos]);
            result.push(c);
            pos += 1;
            state = zero_mbstate();
        } else if ret == 0 {
            // embedded null byte!
            result.push('\0');
            pos += 1;
            state = zero_mbstate();
        } else {
            // normal case
            result.push(c);
            pos += ret;
        }
    }
    result
}

pub fn cstr2wcstring(input: &[u8]) -> WString {
    let strlen = input.iter().position(|c| *c == b'\0').unwrap();
    str2wcstring(&input[0..strlen])
}

pub fn charptr2wcstring(input: *const libc::c_char) -> WString {
    let input: &[u8] = unsafe {
        let strlen = libc::strlen(input);
        slice::from_raw_parts(input.cast(), strlen)
    };
    str2wcstring(input)
}

/// Returns a newly allocated multibyte character string equivalent of the specified wide character
/// string.
///
/// This function decodes illegal character sequences in a reversible way using the private use
/// area.
pub fn wcs2string(input: &wstr) -> Vec<u8> {
    if input.is_empty() {
        return vec![];
    }

    let mut result = vec![];
    wcs2string_appending(&mut result, input);
    result
}

pub fn wcs2osstring(input: &wstr) -> OsString {
    if input.is_empty() {
        return OsString::new();
    }

    let mut result = vec![];
    wcs2string_appending(&mut result, input);
    OsString::from_vec(result)
}

pub fn wcs2zstring(input: &wstr) -> CString {
    if input.is_empty() {
        return CString::default();
    }

    let mut result = vec![];
    wcs2string_callback(input, |buff| {
        result.extend_from_slice(buff);
        true
    });
    let until_nul = match result.iter().position(|c| *c == b'\0') {
        Some(pos) => &result[..pos],
        None => &result[..],
    };
    CString::new(until_nul).unwrap()
}

/// Like wcs2string, but appends to \p receiver instead of returning a new string.
pub fn wcs2string_appending(output: &mut Vec<u8>, input: &wstr) {
    output.reserve(input.len());
    wcs2string_callback(input, |buff| {
        output.extend_from_slice(buff);
        true
    });
}

/// \return the count of initial characters in \p in which are ASCII.
fn count_ascii_prefix(inp: &[u8]) -> usize {
    // The C++ version had manual vectorization.
    inp.iter().take_while(|c| c.is_ascii()).count()
}

// Check if we are running in the test mode, where we should suppress error output
#[widestrs]
pub const TESTS_PROGRAM_NAME: &wstr = "(ignore)"L;

/// Hack to not print error messages in the tests. Do not call this from functions in this module
/// like `debug()`. It is only intended to suppress diagnostic noise from testing things like the
/// fish parser where we expect a lot of diagnostic messages due to testing error conditions.
pub fn should_suppress_stderr_for_tests() -> bool {
    PROGRAM_NAME
        .get()
        .map(|p| p == TESTS_PROGRAM_NAME)
        .unwrap_or_default()
}

#[deprecated(note = "Use threads::assert_is_main_thread() instead")]
pub fn assert_is_main_thread() {
    crate::threads::assert_is_main_thread()
}

#[deprecated(note = "Use threads::assert_is_background_thread() instead")]
pub fn assert_is_background_thread() {
    crate::threads::assert_is_background_thread()
}

/// Format the specified size (in bytes, kilobytes, etc.) into the specified stringbuffer.
#[widestrs]
pub fn format_size(mut sz: i64) -> WString {
    let mut result = WString::new();
    const sz_names: [&wstr; 8] = ["kB"L, "MB"L, "GB"L, "TB"L, "PB"L, "EB"L, "ZB"L, "YB"L];
    if sz < 0 {
        result += "unknown"L;
    } else if sz == 0 {
        result += wgettext!("empty");
    } else if sz < 1024 {
        result += &sprintf!("%lldB"L, sz)[..];
    } else {
        for (i, sz_name) in sz_names.iter().enumerate() {
            if sz < (1024 * 1024) || i == sz_names.len() - 1 {
                let isz = sz / 1024;
                if isz > 9 {
                    result += &sprintf!("%ld%ls"L, isz, *sz_name)[..];
                } else {
                    result += &sprintf!("%.1f%ls"L, sz as f64 / 1024.0, *sz_name)[..];
                }
                break;
            }
            sz /= 1024;
        }
    }

    result
}

/// Version of format_size that does not allocate memory.
fn format_size_safe(buff: &mut [u8; 128], mut sz: u64) {
    let buff_size = 128;
    let max_len = buff_size - 1; // need to leave room for a null terminator
    buff.fill(0);
    let mut idx = 0;
    const sz_names: [&str; 8] = ["kB", "MB", "GB", "TB", "PB", "EB", "ZB", "YB"];
    if sz == 0 {
        let empty = "empty".as_bytes();
        buff[..empty.len()].copy_from_slice(empty);
    } else if sz < 1024 {
        append_ull(buff, &mut sz, &mut idx, max_len);
        append_str(buff, "B", &mut idx, max_len);
    } else {
        for (i, sz_name) in sz_names.iter().enumerate() {
            if sz < (1024 * 1024) || i == sz_names.len() - 1 {
                let mut isz = sz / 1024;
                append_ull(buff, &mut isz, &mut idx, max_len);
                if isz <= 9 {
                    // Maybe append a single fraction digit.
                    let mut remainder = sz % 1024;
                    if remainder > 0 {
                        let tmp = [b'.', extract_most_significant_digit(&mut remainder)];
                        let tmp = std::str::from_utf8(&tmp).unwrap();
                        append_str(buff, tmp, &mut idx, max_len);
                    }
                }
                append_str(buff, sz_name, &mut idx, max_len);
                break;
            }
            sz /= 1024;
        }
    }
}

/// Writes out a long safely.
pub fn format_llong_safe<CharT: From<u8>>(buff: &mut [CharT; 64], val: i64) {
    let uval = val.unsigned_abs();
    if val >= 0 {
        format_safe_impl(buff, 64, uval);
    } else {
        buff[0] = CharT::from(b'-');
        format_safe_impl(&mut buff[1..], 63, uval);
    }
}

pub fn format_ullong_safe<CharT: From<u8>>(buff: &mut [CharT; 64], val: u64) {
    format_safe_impl(buff, 64, val);
}

fn format_safe_impl<CharT: From<u8>>(buff: &mut [CharT], size: usize, mut val: u64) {
    let mut idx = 0;
    if val == 0 {
        buff[idx] = CharT::from(b'0');
    } else {
        // Generate the string backwards, then reverse it.
        while val != 0 {
            buff[idx] = CharT::from((val % 10) as u8 + b'0');
            val /= 10;
        }
        buff[..idx].reverse();
    }
    buff[idx] = CharT::from(b'\0');
    idx += 1;
    assert!(idx <= size, "Buffer overflowed");
}

fn append_ull(buff: &mut [u8], val: &mut u64, inout_idx: &mut usize, max_len: usize) {
    let mut idx = *inout_idx;
    while *val > 0 && idx < max_len {
        buff[idx] = extract_most_significant_digit(val);
        idx += 1;
    }
    *inout_idx = idx;
}

fn append_str(buff: &mut [u8], s: &str, inout_idx: &mut usize, max_len: usize) {
    let mut idx = *inout_idx;
    let bytes = s.as_bytes();
    while idx < bytes.len().min(max_len) {
        buff[idx] = bytes[idx];
        idx += 1;
    }
    *inout_idx = idx;
}

/// Crappy function to extract the most significant digit of an unsigned long long value.
fn extract_most_significant_digit(xp: &mut u64) -> u8 {
    let mut place_value = 1;
    let mut x = *xp;
    while x >= 10 {
        x /= 10;
        place_value *= 10;
    }
    *xp -= place_value * x;
    x as u8 + b'0'
}

/// "Narrows" a wide character string. This just grabs any ASCII characters and truncates.
pub fn narrow_string_safe(buff: &mut [u8; 64], s: &wstr) {
    let mut idx = 0;
    for c in s.chars() {
        if c as u32 <= 127 {
            buff[idx] = c as u8;
            idx += 1;
            if idx + 1 == 64 {
                break;
            }
        }
    }
    buff[idx] = b'\0';
}

/// Stored in blocks to reference the file which created the block.
pub type FilenameRef = Arc<WString>;

/// This function should be called after calling `setlocale()` to perform fish specific locale
/// initialization.
#[widestrs]
pub fn fish_setlocale() {
    // Use various Unicode symbols if they can be encoded using the current locale, else a simple
    // ASCII char alternative. All of the can_be_encoded() invocations should return the same
    // true/false value since the code points are in the BMP but we're going to be paranoid. This
    // is also technically wrong if we're not in a Unicode locale but we expect (or hope)
    // can_be_encoded() will return false in that case.
    if can_be_encoded('\u{2026}') {
        ELLIPSIS_CHAR.store(u32::from('\u{2026}'), Ordering::Relaxed);
        unsafe {
            ELLIPSIS_STRING = Lazy::new(|| "\u{2026}"L);
        }
    } else {
        ELLIPSIS_CHAR.store(u32::from('$'), Ordering::Relaxed); // "horizontal ellipsis"
        unsafe {
            ELLIPSIS_STRING = Lazy::new(|| "..."L);
        }
    }

    if is_windows_subsystem_for_linux() {
        // neither of \u23CE and \u25CF can be displayed in the default fonts on Windows, though
        // they can be *encoded* just fine. Use alternative glyphs.
        unsafe {
            OMITTED_NEWLINE_STR = Lazy::new(|| "\u{00b6}"L); // "pilcrow"
        }
        OBFUSCATION_READ_CHAR.store(u32::from('\u{2022}'), Ordering::Relaxed); // "bullet"
    } else if is_console_session() {
        unsafe {
            OMITTED_NEWLINE_STR = Lazy::new(|| "^J"L);
        }
        OBFUSCATION_READ_CHAR.store(u32::from('*'), Ordering::Relaxed);
    } else {
        if can_be_encoded('\u{23CE}') {
            unsafe {
                OMITTED_NEWLINE_STR = Lazy::new(|| "\u{23CE}"L); // "return symbol" (⏎)
            }
        } else {
            unsafe {
                OMITTED_NEWLINE_STR = Lazy::new(|| "^J"L);
            }
        }
        OBFUSCATION_READ_CHAR.store(
            u32::from(if can_be_encoded('\u{25CF}') {
                '\u{25CF}' // "black circle"
            } else {
                '#'
            }),
            Ordering::Relaxed,
        );
    }
    PROFILING_ACTIVE.store(true);

    // Until no C++ code uses the variables init in the C++ version of fish_setlocale(), we need to
    // also call that one or otherwise we'll segfault trying to read those uninit values.
    extern "C" {
        fn fish_setlocale_ffi();
    }
    unsafe {
        fish_setlocale_ffi();
    }
}

/// Test if the character can be encoded using the current locale.
fn can_be_encoded(wc: char) -> bool {
    let mut converted = [0_i8; AT_LEAST_MB_LEN_MAX];
    let mut state = zero_mbstate();
    unsafe {
        wcrtomb(&mut converted[0], wc as libc::wchar_t, &mut state) != 0_usize.wrapping_sub(1)
    }
}

/// Call read, blocking and repeating on EINTR. Exits on EAGAIN.
/// \return the number of bytes read, or 0 on EOF. On EAGAIN, returns -1 if nothing was read.
pub fn read_blocked(fd: RawFd, buf: &mut [u8]) -> isize {
    loop {
        let res = unsafe { libc::read(fd, buf.as_mut_ptr().cast(), buf.len()) };
        if res < 0 && errno::errno().0 == EINTR {
            continue;
        }
        return res;
    }
}

/// Test if the string is a valid function name.
pub fn valid_func_name(name: &wstr) -> bool {
    !(name.is_empty()
    || name.starts_with('-')
    // A function name needs to be a valid path, so no / and no NULL.
    || name.contains('/')
    || name.contains('\0'))
}

/// A rusty port of the C++ `write_loop()` function from `common.cpp`. This should be deprecated in
/// favor of native rust read/write methods at some point.
///
/// Returns the number of bytes written or an IO error.
pub fn write_loop<Fd: AsRawFd>(fd: &Fd, buf: &[u8]) -> std::io::Result<usize> {
    let fd = fd.as_raw_fd();
    let mut total = 0;
    while total < buf.len() {
        let written =
            unsafe { libc::write(fd, buf[total..].as_ptr() as *const _, buf.len() - total) };
        if written < 0 {
            let errno = errno::errno().0;
            if matches!(errno, libc::EAGAIN | libc::EINTR) {
                continue;
            }
            return Err(std::io::Error::from_raw_os_error(errno));
        }
        total += written as usize;
    }
    Ok(total)
}

/// A rusty port of the C++ `read_loop()` function from `common.cpp`. This should be deprecated in
/// favor of native rust read/write methods at some point.
///
/// Returns the number of bytes read or an IO error.
pub fn read_loop<Fd: AsRawFd>(fd: &Fd, buf: &mut [u8]) -> std::io::Result<usize> {
    let fd = fd.as_raw_fd();
    loop {
        let read = unsafe { libc::read(fd, buf.as_mut_ptr() as *mut _, buf.len()) };
        if read < 0 {
            let errno = errno::errno().0;
            if matches!(errno, libc::EAGAIN | libc::EINTR) {
                continue;
            }
            return Err(std::io::Error::from_raw_os_error(errno));
        }
        return Ok(read as usize);
    }
}

/// Write the given paragraph of output, redoing linebreaks to fit \p termsize.
#[widestrs]
pub fn reformat_for_screen(msg: &wstr, termsize: &Termsize) -> WString {
    let mut buff = WString::new();

    let screen_width = termsize.width;
    if screen_width != 0 {
        let mut start = 0;
        let mut pos = start;
        let mut line_width = 0;
        while pos < msg.len() {
            let mut overflow = false;
            let mut tok_width = 0;

            // Tokenize on whitespace, and also calculate the width of the token.
            while pos < msg.len() && ![' ', '\n', '\r', '\t'].contains(&msg.char_at(pos)) {
                // Check is token is wider than one line. If so we mark it as an overflow and break
                // the token.
                let width = fish_wcwidth(msg.char_at(pos)) as isize;
                if (tok_width + width) > (screen_width - 1) {
                    overflow = true;
                    break;
                }
                tok_width += width;
                pos += 1;
            }

            // If token is zero character long, we don't do anything.
            if pos == start {
                pos += 1;
            } else if overflow {
                // In case of overflow, we print a newline, except if we already are at position 0.
                let token = &msg[start..pos];
                if line_width != 0 {
                    buff.push('\n');
                }
                buff += &sprintf!("%ls-\n"L, token)[..];
                line_width = 0;
            } else {
                // Print the token.
                let token = &msg[start..pos];
                let line_width_unit = (if line_width != 0 { 1 } else { 0 });
                if (line_width + line_width_unit + tok_width) > screen_width {
                    buff.push('\n');
                    line_width = 0;
                }
                if line_width != 0 {
                    buff += " "L;
                }
                buff += token;
                line_width += line_width_unit + tok_width;
            }

            start = pos;
        }
    } else {
        buff += msg;
    }
    buff.push('\n');
    buff
}

pub type Timepoint = f64;

/// Return the number of seconds from the UNIX epoch, with subsecond precision. This function uses
/// the gettimeofday function and will have the same precision as that function.
pub fn timef() -> Timepoint {
    match time::SystemTime::now().duration_since(time::UNIX_EPOCH) {
        Ok(difference) => difference.as_secs() as f64,
        Err(until_epoch) => -(until_epoch.duration().as_secs() as f64),
    }
}

#[deprecated(note = "Use threads::is_main_thread() instead")]
pub fn is_main_thread() -> bool {
    crate::threads::is_main_thread()
}

/// Call the following function early in main to set the main thread. This is our replacement for
/// pthread_main_np().
#[deprecated(note = "This function is no longer called manually!")]
pub fn set_main_thread() {
    eprintln!("set_main_thread() is removed in favor of `main_thread_id()` and co. in threads.rs!")
}

#[deprecated(note = "Use threads::configure_thread_assertions_for_testing() instead")]
pub fn configure_thread_assertions_for_testing() {
    crate::threads::configure_thread_assertions_for_testing();
}

#[deprecated(note = "This should no longer be called manually")]
pub fn setup_fork_guards() {}

#[deprecated(note = "Use threads::is_forked_child() instead")]
pub fn is_forked_child() -> bool {
    crate::threads::is_forked_child()
}

/// Be able to restore the term's foreground process group.
/// This is set during startup and not modified after.
static INITIAL_FG_PROCESS_GROUP: AtomicI32 = AtomicI32::new(-1); // HACK, should be pid_t
const _: () = assert!(mem::size_of::<i32>() >= mem::size_of::<libc::pid_t>());

/// Save the value of tcgetpgrp so we can restore it on exit.
pub fn save_term_foreground_process_group() {
    INITIAL_FG_PROCESS_GROUP.store(unsafe { libc::tcgetpgrp(STDIN_FILENO) }, Ordering::Relaxed);
}

pub fn restore_term_foreground_process_group_for_exit() {
    // We wish to restore the tty to the initial owner. There's two ways this can go wrong:
    //  1. We may steal the tty from someone else (#7060).
    //  2. The call to tcsetpgrp may deliver SIGSTOP to us, and we will not exit.
    // Hanging on exit seems worse, so ensure that SIGTTOU is ignored so we do not get SIGSTOP.
    // Note initial_fg_process_group == 0 is possible with Linux pid namespaces.
    // This is called during shutdown and from a signal handler. We don't bother to complain on
    // failure because doing so is unlikely to be noticed.
    // Safety: All of getpgrp, signal, and tcsetpgrp are async-signal-safe.
    let initial_fg_process_group = INITIAL_FG_PROCESS_GROUP.load(Ordering::Relaxed);
    if initial_fg_process_group > 0 && initial_fg_process_group != unsafe { libc::getpgrp() } {
        unsafe {
            libc::signal(SIGTTOU, SIG_IGN);
            libc::tcsetpgrp(STDIN_FILENO, initial_fg_process_group);
        }
    }
}

fn slice_contains_slice<T: Eq>(a: &[T], b: &[T]) -> bool {
    a.windows(b.len()).any(|aw| aw == b)
}

#[cfg(target_os = "linux")]
static IS_WINDOWS_SUBSYSTEM_FOR_LINUX: once_cell::race::OnceBool = once_cell::race::OnceBool::new();

/// Determines if we are running under Microsoft's Windows Subsystem for Linux to work around
/// some known limitations and/or bugs.
/// See https://github.com/Microsoft/WSL/issues/423 and Microsoft/WSL#2997
pub fn is_windows_subsystem_for_linux() -> bool {
    // We are purposely not using std::call_once as it may invoke locking, which is an unnecessary
    // overhead since there's no actual race condition here - even if multiple threads call this
    // routine simultaneously the first time around, we just end up needlessly querying uname(2) one
    // more time.
    #[cfg(not(target_os = "linux"))]
    {
        false
    }

    #[cfg(target_os = "linux")]
    IS_WINDOWS_SUBSYSTEM_FOR_LINUX.get_or_init(|| {
        let mut info: libc::utsname = unsafe { mem::zeroed() };
        let release: &[u8] = unsafe {
            libc::uname(&mut info);
            std::mem::transmute(&info.release[..])
        };

        // Sample utsname.release under WSL, testing for something like `4.4.0-17763-Microsoft`
        if !slice_contains_slice(release, b"Microsoft") {
            return false;
        }

        let release: Vec<_> = release
            .iter()
            .skip_while(|c| **c != b'-')
            .skip(1) // the dash itself
            .take_while(|c| c.is_ascii_digit())
            .copied()
            .collect();
        let build: Result<u32, _> = std::str::from_utf8(&release).unwrap().parse();
        match build {
            Ok(17763..) => return true,
            Ok(_) => (),       // return true, but first warn (see below)
            _ => return false, // if parsing fails, assume this isn't WSL
        };

        // #5298, #5661: There are acknowledged, published, and (later) fixed issues with
        // job control under early WSL releases that prevent fish from running correctly,
        // with unexpected failures when piping. Fish 3.0 nightly builds worked around this
        // issue with some needlessly complicated code that was later stripped from the
        // fish 3.0 release, so we just bail. Note that fish 2.0 was also broken, but we
        // just didn't warn about it.

        // #6038 & 5101bde: It's been requested that there be some sort of way to disable
        // this check: if the environment variable FISH_NO_WSL_CHECK is present, this test
        // is bypassed. We intentionally do not include this in the error message because
        // it'll only allow fish to run but not to actually work. Here be dragons!
        if env::var_os("FISH_NO_WSL_CHECK").is_none() {
            FLOG!(
                error,
                concat!(
                    "This version of WSL has known bugs that prevent fish from working.\n",
                    "Please upgrade to Windows 10 1809 (17763) or higher to use fish!"
                )
            );
        }
        true
    })
}

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
        || (c >= ENCODE_DIRECT_BASE && c < ENCODE_DIRECT_END)
}

pub fn redirect_tty_output() {
    unsafe {
        let mut t: libc::termios = mem::zeroed();
        let s = CString::new("/dev/null").unwrap();
        let fd = libc::open(s.as_ptr(), O_WRONLY);
        assert!(fd != -1, "Could not open /dev/null!");
        for stdfd in [STDIN_FILENO, STDOUT_FILENO, STDERR_FILENO] {
            if libc::tcgetattr(stdfd, &mut t) == -1 && errno::errno().0 == EIO {
                libc::dup2(fd, stdfd);
            }
        }
    }
}

/// Test if the given char is valid in a variable name.
pub fn valid_var_name_char(chr: char) -> bool {
    fish_iswalnum(chr) || chr == '_'
}

/// Test if the given string is a valid variable name.
pub fn valid_var_name(s: &wstr) -> bool {
    // Note do not use c_str(), we want to fail on embedded nul bytes.
    !s.is_empty() && s.chars().all(valid_var_name_char)
}

/// Get the absolute path to the fish executable itself
pub fn get_executable_path(argv0: impl AsRef<Path>) -> PathBuf {
    if let Ok(path) = std::env::current_exe() {
        if path.exists() {
            return path;
        }

        // When /proc/self/exe points to a file that was deleted (or overwritten on update!)
        // then linux adds a " (deleted)" suffix.
        // If that's not a valid path, let's remove that awkward suffix.
        let pathstr = path.to_str().unwrap_or("");
        if !pathstr.ends_with(" (deleted)") {
            return path;
        }

        if let (Some(filename), Some(parent)) = (path.file_name(), path.parent()) {
            if let Some(filename) = filename.to_str() {
                let corrected_filename = OsStr::new(filename.strip_suffix(" (deleted)").unwrap());
                return parent.join(corrected_filename);
            }
        }
        return path;
    }
    argv0.as_ref().to_owned()
}

/// A RAII cleanup object. Unlike in C++ where there is no borrow checker, we can't just provide a
/// callback that modifies live objects willy-nilly because then there would be two &mut references
/// to the same object - the original variables we keep around to use and their captured references
/// held by the closure until its scope expires.
///
/// Instead we have a `ScopeGuard` type that takes exclusive ownership of (a mutable reference to)
/// the object to be managed. In lieu of keeping the original value around, we obtain a regular or
/// mutable reference to it via ScopeGuard's [`Deref`] and [`DerefMut`] impls.
///
/// The `ScopeGuard` is considered to be the exclusively owner of the passed value for the
/// duration of its lifetime. If you need to use the value again, use `ScopeGuard` to shadow the
/// value and obtain a reference to it via the `ScopeGuard` itself:
///
/// ```rust
/// use std::io::prelude::*;
///
/// let file = std::fs::File::open("/dev/null");
/// // Create a scope guard to write to the file when the scope expires.
/// // To be able to still use the file, shadow `file` with the ScopeGuard itself.
/// let mut file = ScopeGuard::new(file, |file| file.write_all(b"goodbye\n").unwrap());
/// // Now write to the file normally "through" the capturing ScopeGuard instance.
/// file.write_all(b"hello\n").unwrap();
///
/// // hello will be written first, then goodbye.
/// ```
pub struct ScopeGuard<T, F: FnOnce(&mut T)>(Option<(T, F)>);

impl<T, F: FnOnce(&mut T)> ScopeGuard<T, F> {
    /// Creates a new `ScopeGuard` wrapping `value`. The `on_drop` callback is executed when the
    /// ScopeGuard's lifetime expires or when it is manually dropped.
    pub fn new(value: T, on_drop: F) -> Self {
        Self(Some((value, on_drop)))
    }

    /// Invokes the callback and returns the wrapped value, consuming the ScopeGuard.
    pub fn commit(mut guard: Self) -> T {
        let (mut value, on_drop) = guard.0.take().expect("Should always have Some value");
        on_drop(&mut value);
        value
    }
}

impl<T, F: FnOnce(&mut T)> Deref for ScopeGuard<T, F> {
    type Target = T;

    fn deref(&self) -> &Self::Target {
        &self.0.as_ref().unwrap().0
    }
}

impl<T, F: FnOnce(&mut T)> DerefMut for ScopeGuard<T, F> {
    fn deref_mut(&mut self) -> &mut Self::Target {
        &mut self.0.as_mut().unwrap().0
    }
}

impl<T, F: FnOnce(&mut T)> Drop for ScopeGuard<T, F> {
    fn drop(&mut self) {
        if let Some((mut value, on_drop)) = self.0.take() {
            on_drop(&mut value);
        }
    }
}

/// A trait expressing what ScopeGuard can do. This is necessary because scoped_push returns an
/// `impl Trait` object and therefore methods on ScopeGuard which take a self parameter cannot be
/// used.
pub trait ScopeGuarding: DerefMut {
    /// Invokes the callback and returns the wrapped value, consuming the ScopeGuard.
    fn commit(guard: Self) -> Self::Target;
}

impl<T, F: FnOnce(&mut T)> ScopeGuarding for ScopeGuard<T, F> {
    fn commit(guard: Self) -> T {
        ScopeGuard::commit(guard)
    }
}

/// A scoped manager to save the current value of some variable, and set it to a new value. When
/// dropped, it restores the variable to its old value.
pub fn scoped_push<Context, Accessor, T>(
    mut ctx: Context,
    accessor: Accessor,
    new_value: T,
) -> impl ScopeGuarding<Target = Context>
where
    Accessor: Fn(&mut Context) -> &mut T,
{
    let saved = mem::replace(accessor(&mut ctx), new_value);
    let restore_saved = move |ctx: &mut Context| {
        *accessor(ctx) = saved;
    };
    ScopeGuard::new(ctx, restore_saved)
}

pub const fn assert_send<T: Send>() {}
pub const fn assert_sync<T: Sync>() {}

pub fn assert_is_locked_impl_do_not_use_directly<T>(
    mutex: &Mutex<T>,
    who: &str,
    lineno: usize,
    filename: &str,
) {
    match mutex.try_lock() {
        Err(TryLockError::WouldBlock) => {
            // Expected case.
        }
        Err(TryLockError::Poisoned(_)) => {
            panic!(
                "Mutex {} is poisoned in {} at line {}",
                who, filename, lineno
            );
        }
        Ok(_) => {
            FLOG!(
                error,
                who,
                "is not locked when it should be in",
                filename,
                "at line",
                lineno
            );
            FLOG!(error, "Break on debug_thread_error to debug.");
            debug_thread_error();
        }
    }
}

macro_rules! assert_is_locked {
    ($lock:expr) => {
        crate::common::assert_is_locked_impl_do_not_use_directly(
            $lock,
            stringify!($lock),
            line!() as usize,
            file!(),
        )
    };
}
pub(crate) use assert_is_locked;

/// This function attempts to distinguish between a console session (at the actual login vty) and a
/// session within a terminal emulator inside a desktop environment or over SSH. Unfortunately
/// there are few values of $TERM that we can interpret as being exclusively console sessions, and
/// most common operating systems do not use them. The value is cached for the duration of the fish
/// session. We err on the side of assuming it's not a console session. This approach isn't
/// bullet-proof and that's OK.
pub fn is_console_session() -> bool {
    static IS_CONSOLE_SESSION: Lazy<bool> = Lazy::new(|| {
        const PATH_MAX: usize = libc::PATH_MAX as usize;
        let mut tty_name = [0u8; PATH_MAX];
        unsafe {
            if libc::ttyname_r(STDIN_FILENO, tty_name.as_mut_ptr().cast(), tty_name.len()) != 0 {
                return false;
            }
        }
        // Check if the tty matches /dev/(console|dcons|tty[uv\d])
        const LEN: usize = b"/dev/tty".len();
        (
        (
            tty_name.starts_with(b"/dev/tty") &&
                ([b'u', b'v'].contains(&tty_name[LEN]) || tty_name[LEN].is_ascii_digit())
        ) ||
        tty_name.starts_with(b"/dev/dcons\0") ||
        tty_name.starts_with(b"/dev/console\0"))
        // and that $TERM is simple, e.g. `xterm` or `vt100`, not `xterm-something` or `sun-color`.
        && match env::var_os("TERM") {
            Some(term) => !term.as_bytes().contains(&b'-'),
            None => true,
        }
    });

    *IS_CONSOLE_SESSION
}

/// Asserts that a slice is alphabetically sorted by a [`&wstr`] `name` field.
///
/// Mainly useful for static asserts/const eval.
///
/// # Panics
///
/// This function panics if the given slice is unsorted.
///
/// # Examples
///
/// ```rust
/// const COLORS: &[(&wstr, u32)] = &[
///     // must be in alphabetical order
///     (L!("blue"), 0x0000ff),
///     (L!("green"), 0x00ff00),
///     (L!("red"), 0xff0000),
/// ];
///
/// assert_sorted_by_name!(COLORS, 0);
/// ```
macro_rules! assert_sorted_by_name {
    ($slice:expr, $field:tt) => {
        const _: () = {
            use std::cmp::Ordering;

            // ugly const eval workarounds below.
            const fn cmp_i32(lhs: i32, rhs: i32) -> Ordering {
                match lhs - rhs {
                    ..=-1 => Ordering::Less,
                    0 => Ordering::Equal,
                    1.. => Ordering::Greater,
                }
            }

            const fn cmp_slice(s1: &[char], s2: &[char]) -> Ordering {
                let mut i = 0;
                while i < s1.len() && i < s2.len() {
                    match cmp_i32(s1[i] as i32, s2[i] as i32) {
                        Ordering::Equal => i += 1,
                        other => return other,
                    }
                }
                cmp_i32(s1.len() as i32, s2.len() as i32)
            }

            let mut i = 1;
            while i < $slice.len() {
                let prev = $slice[i - 1].$field.as_char_slice();
                let cur = $slice[i].$field.as_char_slice();
                if matches!(cmp_slice(prev, cur), Ordering::Greater) {
                    panic!("array must be sorted");
                }
                i += 1;
            }
        };
    };
    ($slice:expr) => {
        assert_sorted_by_name!($slice, name);
    };
}

pub trait Named {
    fn name(&self) -> &'static wstr;
}

/// \return a pointer to the first entry with the given name, assuming the entries are sorted by
/// name. \return nullptr if not found.
pub fn get_by_sorted_name<T: Named>(name: &wstr, vals: &'static [T]) -> Option<&'static T> {
    match vals.binary_search_by_key(&name, |val| val.name()) {
        Ok(index) => Some(&vals[index]),
        Err(_) => None,
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

#[allow(unused_macros)]
macro_rules! fwprintf {
    ($fd:expr, $format:literal $(, $arg:expr)*) => {
        {
            let wide = crate::wutil::sprintf!($format $(, $arg )*);
            crate::wutil::wwrite_to_fd(&wide, $fd);
        }
    }
}

pub fn fputws(s: &wstr, fd: RawFd) {
    wwrite_to_fd(s, fd);
}

#[cxx::bridge]
mod common_ffi {
    extern "C++" {
        include!("wutil.h");
        include!("common.h");
        type escape_string_style_t = crate::ffi::escape_string_style_t;
    }
    extern "Rust" {
        #[cxx_name = "rust_unescape_string"]
        fn unescape_string_ffi(
            input: *const wchar_t,
            len: usize,
            escape_special: u32,
            style: escape_string_style_t,
        ) -> UniquePtr<CxxWString>;

        #[cxx_name = "rust_escape_string_script"]
        fn escape_string_script_ffi(
            input: *const wchar_t,
            len: usize,
            flags: u32,
        ) -> UniquePtr<CxxWString>;

        #[cxx_name = "rust_escape_string_url"]
        fn escape_string_url_ffi(input: *const wchar_t, len: usize) -> UniquePtr<CxxWString>;

        #[cxx_name = "rust_escape_string_var"]
        fn escape_string_var_ffi(input: *const wchar_t, len: usize) -> UniquePtr<CxxWString>;

    }
}

fn unescape_string_ffi(
    input: *const ffi::wchar_t,
    len: usize,
    escape_special: u32,
    style: ffi::escape_string_style_t,
) -> UniquePtr<CxxWString> {
    let style = match style {
        ffi::escape_string_style_t::STRING_STYLE_SCRIPT => {
            UnescapeStringStyle::Script(UnescapeFlags::from_bits(escape_special).unwrap())
        }
        ffi::escape_string_style_t::STRING_STYLE_URL => UnescapeStringStyle::Url,
        ffi::escape_string_style_t::STRING_STYLE_VAR => UnescapeStringStyle::Var,
        _ => panic!(),
    };
    let input = unsafe { slice::from_raw_parts(input, len) };
    let input = wstr::from_slice(input).unwrap();
    match unescape_string(input, style) {
        Some(result) => result.to_ffi(),
        None => UniquePtr::null(),
    }
}

fn escape_string_script_ffi(
    input: *const ffi::wchar_t,
    len: usize,
    flags: u32,
) -> UniquePtr<CxxWString> {
    let input = unsafe { slice::from_raw_parts(input, len) };
    escape_string_script(
        wstr::from_slice(input).unwrap(),
        EscapeFlags::from_bits(flags).unwrap(),
    )
    .to_ffi()
}

fn escape_string_var_ffi(input: *const ffi::wchar_t, len: usize) -> UniquePtr<CxxWString> {
    let input = unsafe { slice::from_raw_parts(input, len) };
    escape_string_var(wstr::from_slice(input).unwrap()).to_ffi()
}

fn escape_string_url_ffi(input: *const ffi::wchar_t, len: usize) -> UniquePtr<CxxWString> {
    let input = unsafe { slice::from_raw_parts(input, len) };
    escape_string_url(wstr::from_slice(input).unwrap()).to_ffi()
}
