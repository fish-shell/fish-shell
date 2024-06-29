//! Prototypes for various functions, mostly string utilities, that are used by most parts of fish.

use crate::expand::{
    BRACE_BEGIN, BRACE_END, BRACE_SEP, BRACE_SPACE, HOME_DIRECTORY, INTERNAL_SEPARATOR,
    PROCESS_EXPAND_SELF, PROCESS_EXPAND_SELF_STR, VARIABLE_EXPAND, VARIABLE_EXPAND_SINGLE,
};
use crate::fallback::fish_wcwidth;
use crate::future_feature_flags::{feature_test, FeatureFlag};
use crate::global_safety::AtomicRef;
use crate::global_safety::RelaxedAtomicBool;
use crate::key;
use crate::libc::MB_CUR_MAX;
use crate::parse_util::parse_util_escape_string_with_quote;
use crate::termsize::Termsize;
use crate::wchar::{decode_byte_from_char, encode_byte_to_char, prelude::*};
use crate::wcstringutil::wcs2string_callback;
use crate::wildcard::{ANY_CHAR, ANY_STRING, ANY_STRING_RECURSIVE};
use crate::wutil::encoding::{mbrtowc, wcrtomb, zero_mbstate, AT_LEAST_MB_LEN_MAX};
use crate::wutil::fish_iswalnum;
use bitflags::bitflags;
use core::slice;
use libc::{EIO, O_WRONLY, SIGTTOU, SIG_IGN, STDERR_FILENO, STDIN_FILENO, STDOUT_FILENO};
use once_cell::sync::OnceCell;
use std::env;
use std::ffi::{CStr, CString, OsStr, OsString};
use std::mem;
use std::ops::{Deref, DerefMut};
use std::os::unix::prelude::*;
use std::path::{Path, PathBuf};
use std::sync::atomic::{AtomicI32, AtomicU32, Ordering};
use std::sync::{Arc, MutexGuard};
use std::time;

pub const PACKAGE_NAME: &str = env!("CARGO_PKG_NAME");

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
    #[derive(Copy, Clone, Debug, Default, Eq, PartialEq)]
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
        /// Escape : and =
        const SEPARATORS = 1 << 4;
        /// Escape ,
        const COMMA = 1 << 5;
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
    #[derive(Copy, Clone, Debug, Default, Eq, PartialEq)]
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
fn escape_string_script(input: &wstr, flags: EscapeFlags) -> WString {
    let escape_printables = !flags.contains(EscapeFlags::NO_PRINTABLES);
    let escape_separators = flags.contains(EscapeFlags::SEPARATORS);
    let escape_comma = flags.contains(EscapeFlags::COMMA);
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
    let mut double_quotes = 0;
    let mut single_quotes = 0;
    let mut dollars = 0;

    if !no_quoted && input.is_empty() {
        return L!("''").to_owned();
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
                    out += L!("\\t");
                }
                need_escape = true;
                need_complex_escape = true;
            }
            '\n' => {
                if symbolic {
                    out.push('␤');
                } else {
                    out += L!("\\n");
                }
                need_escape = true;
                need_complex_escape = true;
            }
            '\x08' => {
                if symbolic {
                    out.push('␈');
                } else {
                    out += L!("\\b");
                }
                need_escape = true;
                need_complex_escape = true;
            }
            '\r' => {
                if symbolic {
                    out.push('␍');
                } else {
                    out += L!("\\r");
                }
                need_escape = true;
                need_complex_escape = true;
            }
            '\x1B' => {
                if symbolic {
                    out.push('␛');
                } else {
                    out += L!("\\e");
                }
                need_escape = true;
                need_complex_escape = true;
            }
            '\x7F' => {
                if symbolic {
                    out.push('␡');
                } else {
                    out += L!("\\x7f");
                }
                need_escape = true;
                need_complex_escape = true;
            }
            '\\' | '\'' => {
                need_escape = true;
                if c == '\'' {
                    single_quotes += 1;
                }
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
                out += L!("**");
            }
            ':' | '=' => {
                if escape_separators {
                    need_escape = true;
                    out.push('\\');
                }
                out.push(c);
            }
            ',' => {
                if escape_comma {
                    need_escape = true;
                    out.push('\\');
                }
                out.push(c);
            }

            '&' | '$' | ' ' | '#' | '<' | '>' | '(' | ')' | '[' | ']' | '{' | '}' | '?' | '*'
            | '|' | ';' | '"' | '%' | '~' => {
                if c == '"' {
                    double_quotes += 1;
                }
                if c == '$' {
                    dollars += 1;
                }
                let char_is_normal = (c == '~' && no_tilde) || (c == '?' && no_qmark);
                if !char_is_normal {
                    need_escape = true;
                    if escape_printables {
                        out.push('\\')
                    };
                }
                out.push(c);
            }
            '\x00'..='\x19' => {
                let cval = u32::from(c);
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
            }
            _ => out.push(c),
        }
    }

    // Use quoted escaping if possible, since most people find it easier to read.
    if !no_quoted && need_escape && !need_complex_escape && escape_printables {
        let quote = if single_quotes > double_quotes + dollars {
            '"'
        } else {
            '\''
        };
        out.clear();
        out.reserve(2 + input.len());
        out.push(quote);
        out.push_utfstr(&parse_util_escape_string_with_quote(
            input,
            Some(quote),
            EscapeFlags::empty(),
        ));
        out.push(quote);
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
    out.reserve(input.len());

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

/// Returns the unescaped version of input, or None on error.
fn unescape_string_internal(input: &wstr, flags: UnescapeFlags) -> Option<WString> {
    let mut result = WString::new();
    result.reserve(input.len());

    let unescape_special = flags.contains(UnescapeFlags::SPECIAL);
    let allow_incomplete = flags.contains(UnescapeFlags::INCOMPLETE);
    let ignore_backslashes = flags.contains(UnescapeFlags::NO_BACKSLASHES);
    let allow_percent_self = !feature_test(FeatureFlag::remove_percent_self);

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
                    if allow_percent_self
                        && unescape_special
                        && input_position == 0
                        && input == PROCESS_EXPAND_SELF_STR
                    {
                        to_append_or_none = Some(PROCESS_EXPAND_SELF);
                        input_position += PROCESS_EXPAND_SELF_STR.len() - 1; // skip over 'self's
                    }
                }
                '*' => {
                    if unescape_special {
                        // In general, this is ANY_STRING. But as a hack, if the last appended char
                        // is ANY_STRING, delete the last char and store ANY_STRING_RECURSIVE to
                        // reflect the fact that ** is the recursive wildcard.
                        if result.chars().next_back() == Some(ANY_STRING) {
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
        if fish_reserved_codepoint(c) {
            return None;
        }
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

/// Exits without invoking destructors (via _exit), useful for code after fork.
pub fn exit_without_destructors(code: libc::c_int) -> ! {
    unsafe { libc::_exit(code) };
}

pub fn shell_modes() -> MutexGuard<'static, libc::termios> {
    crate::reader::SHELL_MODES.lock().unwrap()
}

/// The character to use where the text has been truncated. Is an ellipsis on unicode system and a $
/// on other systems.
pub fn get_ellipsis_char() -> char {
    char::from_u32(ELLIPSIS_CHAR.load(Ordering::Relaxed)).unwrap()
}

static ELLIPSIS_CHAR: AtomicU32 = AtomicU32::new(0);

/// The character or string to use where text has been truncated (ellipsis if possible, otherwise
/// ...)
pub fn get_ellipsis_str() -> &'static wstr {
    ELLIPSIS_STRING.load()
}

static ELLIPSIS_STRING: AtomicRef<wstr> = AtomicRef::new(&L!(""));

/// Character representing an omitted newline at the end of text.
pub fn get_omitted_newline_str() -> &'static wstr {
    OMITTED_NEWLINE_STR.load()
}

static OMITTED_NEWLINE_STR: AtomicRef<wstr> = AtomicRef::new(&L!(""));

pub fn get_omitted_newline_width() -> usize {
    OMITTED_NEWLINE_STR.load().len()
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
        !is_windows_subsystem_for_linux(WSL::V1)
    } else {
        true
    }
}

/// A global, empty string. This is useful for functions which wish to return a reference to an
/// empty string.
pub static EMPTY_STRING: WString = WString::new();

/// A function type to check for cancellation.
/// Return true if execution should cancel.
/// todo!("Maybe remove the box? It is only needed for get_bg_context.")
pub type CancelChecker = Box<dyn Fn() -> bool>;

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
                    fish_reserved_codepoint(c)
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

/// Given an input string, return a prefix of the string up to the first NUL character,
/// or the entire string if there is no NUL character.
pub fn truncate_at_nul(input: &wstr) -> &wstr {
    match input.chars().position(|c| c == '\0') {
        Some(nul_pos) => &input[..nul_pos],
        None => input,
    }
}

pub fn cstr2wcstring(input: &[u8]) -> WString {
    let strlen = input.iter().position(|c| *c == b'\0').unwrap();
    str2wcstring(&input[0..strlen])
}

pub(crate) fn charptr2wcstring(input: *const libc::c_char) -> WString {
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

/// Same as [`wcs2string`]. Meant to be used when we need a zero-terminated string to feed legacy APIs.
/// Note: if `input` contains any interior NUL bytes, the result will be truncated at the first!
pub fn wcs2zstring(input: &wstr) -> CString {
    if input.is_empty() {
        return CString::default();
    }

    let mut vec = Vec::with_capacity(input.len() + 1);
    wcs2string_callback(input, |buff| {
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

/// Like wcs2string, but appends to `receiver` instead of returning a new string.
pub fn wcs2string_appending(output: &mut Vec<u8>, input: &wstr) {
    output.reserve(input.len());
    wcs2string_callback(input, |buff| {
        output.extend_from_slice(buff);
        true
    });
}

/// Return the count of initial characters in `in` which are ASCII.
fn count_ascii_prefix(inp: &[u8]) -> usize {
    // The C++ version had manual vectorization.
    inp.iter().take_while(|c| c.is_ascii()).count()
}

// Check if we are running in the test mode, where we should suppress error output
pub const TESTS_PROGRAM_NAME: &wstr = L!("(ignore)");

/// Hack to not print error messages in the tests. Do not call this from functions in this module
/// like `debug()`. It is only intended to suppress diagnostic noise from testing things like the
/// fish parser where we expect a lot of diagnostic messages due to testing error conditions.
pub fn should_suppress_stderr_for_tests() -> bool {
    PROGRAM_NAME
        .get()
        .map(|p| p == TESTS_PROGRAM_NAME)
        .unwrap_or_default()
}

/// Stored in blocks to reference the file which created the block.
pub type FilenameRef = Arc<WString>;

/// This function should be called after calling `setlocale()` to perform fish specific locale
/// initialization.
pub fn fish_setlocale() {
    // Helper to make a static reference to a static &'wstr, from a string literal.
    // This is necessary to store them in global atomics, as these can't handle fat pointers.
    macro_rules! LL {
        ($s:literal) => {{
            const S: &'static wstr = L!($s);
            &S
        }};
    }

    // Use various Unicode symbols if they can be encoded using the current locale, else a simple
    // ASCII char alternative. All of the can_be_encoded() invocations should return the same
    // true/false value since the code points are in the BMP but we're going to be paranoid. This
    // is also technically wrong if we're not in a Unicode locale but we expect (or hope)
    // can_be_encoded() will return false in that case.
    if can_be_encoded('\u{2026}') {
        ELLIPSIS_CHAR.store(u32::from('\u{2026}'), Ordering::Relaxed);
        ELLIPSIS_STRING.store(LL!("\u{2026}"));
    } else {
        ELLIPSIS_CHAR.store(u32::from('$'), Ordering::Relaxed); // "horizontal ellipsis"
        ELLIPSIS_STRING.store(LL!("..."));
    }

    if is_windows_subsystem_for_linux(WSL::Any) {
        // neither of \u23CE and \u25CF can be displayed in the default fonts on Windows, though
        // they can be *encoded* just fine. Use alternative glyphs.
        OMITTED_NEWLINE_STR.store(LL!("\u{00b6}")); // "pilcrow"
        OBFUSCATION_READ_CHAR.store(u32::from('\u{2022}'), Ordering::Relaxed); // "bullet"
    } else if is_console_session() {
        OMITTED_NEWLINE_STR.store(LL!("^J"));
        OBFUSCATION_READ_CHAR.store(u32::from('*'), Ordering::Relaxed);
    } else {
        if can_be_encoded('\u{23CE}') {
            OMITTED_NEWLINE_STR.store(LL!("\u{23CE}")); // "return symbol" (⏎)
        } else {
            OMITTED_NEWLINE_STR.store(LL!("^J"));
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
}

/// Test if the character can be encoded using the current locale.
fn can_be_encoded(wc: char) -> bool {
    let mut converted = [0 as libc::c_char; AT_LEAST_MB_LEN_MAX];
    let mut state = zero_mbstate();
    unsafe {
        wcrtomb(converted.as_mut_ptr(), wc as libc::wchar_t, &mut state) != 0_usize.wrapping_sub(1)
    }
}

/// Call read, blocking and repeating on EINTR. Exits on EAGAIN.
/// Return the number of bytes read, or 0 on EOF, or an error.
pub fn read_blocked(fd: RawFd, buf: &mut [u8]) -> nix::Result<usize> {
    loop {
        let res = nix::unistd::read(fd, buf);
        if let Err(nix::Error::EINTR) = res {
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
        match nix::unistd::write(unsafe { BorrowedFd::borrow_raw(fd) }, &buf[total..]) {
            Ok(written) => {
                total += written;
            }
            Err(err) => {
                if matches!(err, nix::Error::EAGAIN | nix::Error::EINTR) {
                    continue;
                }
                return Err(std::io::Error::from(err));
            }
        }
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
        match nix::unistd::read(fd, buf) {
            Ok(read) => {
                return Ok(read);
            }
            Err(err) => {
                if matches!(err, nix::Error::EAGAIN | nix::Error::EINTR) {
                    continue;
                }
                return Err(std::io::Error::from(err));
            }
        }
    }
}

/// Write the given paragraph of output, redoing linebreaks to fit `termsize`.
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
                let width = fish_wcwidth(msg.char_at(pos));
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
                buff += &sprintf!("%ls-\n", token)[..];
                line_width = 0;
            } else {
                // Print the token.
                let token = &msg[start..pos];
                let line_width_unit = if line_width != 0 { 1 } else { 0 };
                if (line_width + line_width_unit + tok_width) > screen_width {
                    buff.push('\n');
                    line_width = 0;
                }
                if line_width != 0 {
                    buff += L!(" ");
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

#[allow(unused)]
// This function is unused in some configurations/on some platforms
fn slice_contains_slice<T: Eq>(a: &[T], b: &[T]) -> bool {
    subslice_position(a, b).is_some()
}

pub fn subslice_position<T: Eq>(a: &[T], b: &[T]) -> Option<usize> {
    if b.is_empty() {
        return Some(0);
    }
    a.windows(b.len()).position(|aw| aw == b)
}

#[derive(Copy, Debug, Clone, PartialEq, Eq)]
pub enum WSL {
    Any,
    V1,
    V2,
}

/// Determines if we are running under Microsoft's Windows Subsystem for Linux to work around
/// some known limitations and/or bugs.
///
/// See https://github.com/Microsoft/WSL/issues/423 and Microsoft/WSL#2997
#[inline(always)]
#[cfg(not(target_os = "linux"))]
pub fn is_windows_subsystem_for_linux(_: WSL) -> bool {
    false
}

/// Determines if we are running under Microsoft's Windows Subsystem for Linux to work around
/// some known limitations and/or bugs.
///
/// See https://github.com/Microsoft/WSL/issues/423 and Microsoft/WSL#2997
#[cfg(target_os = "linux")]
pub fn is_windows_subsystem_for_linux(v: WSL) -> bool {
    use std::sync::OnceLock;
    static RESULT: OnceLock<Option<WSL>> = OnceLock::new();

    // This is called post-fork from [`report_setpgid_error()`], so the fast path must not involve
    // any allocations or mutexes. We can't rely on all the std functions to be alloc-free in both
    // Debug and Release modes, so we just mandate that the result already be available.
    //
    // is_wsl() is called by has_working_timestamps() which is called by `screen.cpp` in the main
    // process. If that's not good enough, we can call is_wsl() manually at shell startup.
    if crate::threads::is_forked_child() {
        debug_assert!(
            RESULT.get().is_some(),
            "is_wsl() should be called by main before forking!"
        );
    }

    let wsl = RESULT.get_or_init(|| {
        let mut info: libc::utsname = unsafe { mem::zeroed() };
        let release: &[u8] = unsafe {
            libc::uname(&mut info);
            std::mem::transmute(&info.release[..])
        };

        // Sample utsname.release under WSLv2, testing for something like `4.19.104-microsoft-standard`
        // or `5.10.16.3-microsoft-standard-WSL2`
        if slice_contains_slice(release, b"microsoft-standard") {
            return Some(WSL::V2);
        }
        // Sample utsname.release under WSL, testing for something like `4.4.0-17763-Microsoft`
        if !slice_contains_slice(release, b"Microsoft") {
            return None;
        }

        let release: Vec<_> = release
            .iter()
            .copied()
            .skip_while(|c| *c != b'-')
            .skip(1) // the dash itself
            .take_while(|c| c.is_ascii_digit())
            .collect();
        let build: Result<u32, _> = std::str::from_utf8(&release).unwrap().parse();
        match build {
            Ok(17763..) => return Some(WSL::V1),
            Ok(_) => (),      // return true, but first warn (see below)
            _ => return None, // if parsing fails, assume this isn't WSL
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
        use crate::flog::FLOG;
        if env::var_os("FISH_NO_WSL_CHECK").is_none() {
            FLOG!(
                error,
                concat!(
                    "This version of WSL has known bugs that prevent fish from working.\n",
                    "Please upgrade to Windows 10 1809 (17763) or higher to use fish!"
                )
            );
        }
        Some(WSL::V1)
    });

    wsl.map(|wsl| v == WSL::Any || wsl == v).unwrap_or(false)
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
        || (c >= key::Backspace && c < ENCODE_DIRECT_END)
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
/// use fish::common::ScopeGuard;
///
/// let file = std::fs::File::create("/dev/null").unwrap();
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

    /// Cancels the invocation of the callback, returning the original wrapped value.
    pub fn cancel(mut guard: Self) -> T {
        let (value, _) = guard.0.take().expect("Should always have Some value");
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

/// Similar to scoped_push but takes a function like "std::mem::replace" instead of a function
/// that returns a mutable reference.
pub fn scoped_push_replacer<Replacer, T>(
    replacer: Replacer,
    new_value: T,
) -> impl ScopeGuarding<Target = ()>
where
    Replacer: Fn(T) -> T,
{
    let saved = replacer(new_value);
    let restore_saved = move |_ctx: &mut ()| {
        replacer(saved);
    };
    ScopeGuard::new((), restore_saved)
}

pub fn scoped_push_replacer_ctx<Context, Replacer, T>(
    mut ctx: Context,
    replacer: Replacer,
    new_value: T,
) -> impl ScopeGuarding<Target = Context>
where
    Replacer: Fn(&mut Context, T) -> T,
{
    let saved = replacer(&mut ctx, new_value);
    let restore_saved = move |ctx: &mut Context| {
        replacer(ctx, saved);
    };
    ScopeGuard::new(ctx, restore_saved)
}

pub const fn assert_send<T: Send>() {}
pub const fn assert_sync<T: Sync>() {}

/// This function attempts to distinguish between a console session (at the actual login vty) and a
/// session within a terminal emulator inside a desktop environment or over SSH. Unfortunately
/// there are few values of $TERM that we can interpret as being exclusively console sessions, and
/// most common operating systems do not use them. The value is cached for the duration of the fish
/// session. We err on the side of assuming it's not a console session. This approach isn't
/// bullet-proof and that's OK.
pub fn is_console_session() -> bool {
    static IS_CONSOLE_SESSION: OnceCell<bool> = OnceCell::new();
    *IS_CONSOLE_SESSION.get_or_init(|| {
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
    })
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
/// ```
/// use fish::wchar::prelude::*;
/// use fish::assert_sorted_by_name;
///
/// const COLORS: &[(&wstr, u32)] = &[
///     // must be in alphabetical order
///     (L!("blue"), 0x0000ff),
///     (L!("green"), 0x00ff00),
///     (L!("red"), 0xff0000),
/// ];
///
/// assert_sorted_by_name!(COLORS, 0);
/// ```
///
/// While this example would fail to compile:
///
/// ```compile_fail
/// use fish::wchar::prelude::*;
/// use fish::assert_sorted_by_name;
///
/// const COLORS: &[(&wstr, u32)] = &[
///     // not in alphabetical order
///     (L!("green"), 0x00ff00),
///     (L!("blue"), 0x0000ff),
///     (L!("red"), 0xff0000),
/// ];
///
/// assert_sorted_by_name!(COLORS, 0);
/// ```
#[macro_export]
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

/// Return a pointer to the first entry with the given name, assuming the entries are sorted by
/// name. Return nullptr if not found.
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
#[deprecated = "use printf!, eprintf! or fprintf"]
macro_rules! fwprintf {
    ($args:tt) => {
        panic!()
    };
}

// test-only
#[allow(unused_macros)]
#[deprecated = "use printf!"]
macro_rules! err {
    ($format:expr $(, $args:expr)* $(,)? ) => {
        printf!($format $(, $args )*);
    }
}

#[macro_export]
macro_rules! fprintf {
    ($fd:expr, $format:expr $(, $arg:expr)* $(,)?) => {
        {
            let wide = $crate::wutil::sprintf!($format, $( $arg ),*);
            $crate::wutil::wwrite_to_fd(&wide, $fd);
        }
    }
}

#[macro_export]
macro_rules! printf {
    ($format:expr $(, $arg:expr)* $(,)?) => {
        fprintf!(libc::STDOUT_FILENO, $format $(, $arg)*)
    }
}

#[macro_export]
macro_rules! eprintf {
    ($format:expr $(, $arg:expr)* $(,)?) => {
        fprintf!(libc::STDERR_FILENO, $format $(, $arg)*)
    }
}
