//! Prototypes for various functions, mostly string utilities, that are used by most parts of fish.

use crate::expand::{
    BRACE_BEGIN, BRACE_END, BRACE_SEP, BRACE_SPACE, HOME_DIRECTORY, INTERNAL_SEPARATOR,
    PROCESS_EXPAND_SELF, PROCESS_EXPAND_SELF_STR, VARIABLE_EXPAND, VARIABLE_EXPAND_SINGLE,
};
use crate::future_feature_flags::{FeatureFlag, feature_test};
use crate::global_safety::AtomicRef;
use crate::global_safety::RelaxedAtomicBool;
use crate::key;
use crate::parse_util::escape_string_with_quote;
use crate::prelude::*;
use crate::terminal::Output;
use crate::termsize::Termsize;
use crate::wildcard::{ANY_CHAR, ANY_STRING, ANY_STRING_RECURSIVE};
use crate::wutil::fish_iswalnum;
use fish_fallback::fish_wcwidth;
use fish_wcstringutil::str2bytes_callback;
use fish_widestring::{
    ENCODE_DIRECT_END, decode_byte_from_char, encode_byte_to_char, subslice_position,
};
use std::env;
use std::ffi::{CStr, CString, OsString};
use std::os::unix::prelude::*;
use std::sync::atomic::Ordering;
use std::sync::{Arc, MutexGuard, OnceLock};

pub use fish_common::*;

pub const BUILD_DIR: &str = env!("FISH_RESOLVED_BUILD_DIR");

pub const PACKAGE_NAME: &str = env!("CARGO_PKG_NAME");

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
    let escape_comma = flags.contains(EscapeFlags::COMMA);
    let no_quoted = flags.contains(EscapeFlags::NO_QUOTED);
    let no_tilde = flags.contains(EscapeFlags::NO_TILDE);
    let no_qmark = feature_test(FeatureFlag::QuestionMarkNoGlob);
    let symbolic = flags.contains(EscapeFlags::SYMBOLIC);

    assert!(
        !symbolic || !escape_printables,
        "symbolic implies escape-no-printables"
    );

    let mut need_escape = false;
    let mut need_complex_escape = no_tilde && input.char_at(0) == '~';
    let mut double_quotes = 0;
    let mut single_quotes = 0;
    let mut dollars = 0;

    if !no_quoted && input.is_empty() {
        return L!("''").to_owned();
    }

    let mut out = WString::new();

    for (i, c) in input.chars().enumerate() {
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
                let char_is_normal = (c == '~' && no_tilde)
                    || (c == '?' && no_qmark)
                    || (c == '#'
                        && i.checked_sub(1)
                            .map(|previ| input.char_at(previ).is_alphanumeric())
                            .unwrap_or_default());
                if !char_is_normal {
                    need_escape = true;
                    if escape_printables {
                        out.push('\\')
                    }
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
        out.push_utfstr(&escape_string_with_quote(
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
    let narrow = wcs2bytes(input);
    let mut out = WString::new();
    for byte in narrow {
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
    let narrow = wcs2bytes(input);
    let mut out = WString::new();
    for c in narrow {
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
    out.reserve(input.len() + input.len() / 2);

    for c in input.chars() {
        if c == '\n' {
            out.push_str("\\n");
            continue;
        }
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
    let allow_percent_self = !feature_test(FeatureFlag::RemovePercentSelf);

    // The positions of open braces.
    let mut braces = vec![];
    // The positions of variable expansions or brace ","s.
    // We only read braces as expanders if there's a variable expansion or "," in them.
    let mut vars_or_seps = vec![];
    let mut brace_count = 0;
    let mut potential_word_start = None;

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
                    if unescape_special
                        && (input_position == 0 || Some(input_position) == potential_word_start)
                    {
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
                    if unescape_special && !feature_test(FeatureFlag::QuestionMarkNoGlob) {
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
                        potential_word_start = Some(input_position + 1);
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
                            if vars_or_seps.last().is_none_or(|i| *i < brace) {
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
                        potential_word_start = Some(input_position + 1);
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

    Some(bytes2wcstring(&result))
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

    Some(bytes2wcstring(&result))
}

/// Given a string starting with a backslash, read the escape as if it is unquoted, appending
/// to result. Return the number of characters consumed, or none on error.
pub fn read_unquoted_escape(
    input: &wstr,
    result: &mut WString,
    allow_incomplete: bool,
    unescape_special: bool,
) -> Option<usize> {
    assert_eq!(input.char_at(0), '\\', "not an escape");

    // Here's the character we'll ultimately append, or none. Note that '\0' is a
    // valid thing to append.
    let mut result_char_or_none: Option<char> = None;

    let mut errored = false;
    // in_pos always tracks the next character to read
    // (and therefore the number of characters read so far)
    let mut in_pos = 1;

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
            result.push_utfstr(&bytes2wcstring(&byte_buff));
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

pub fn shell_modes() -> MutexGuard<'static, libc::termios> {
    crate::reader::SHELL_MODES.lock().unwrap()
}

/// Character representing an omitted newline at the end of text.
pub fn get_omitted_newline_str() -> &'static str {
    OMITTED_NEWLINE_STR.load()
}

static OMITTED_NEWLINE_STR: AtomicRef<str> = AtomicRef::new(&"");

/// Profiling flag. True if commands should be profiled.
pub static PROFILING_ACTIVE: RelaxedAtomicBool = RelaxedAtomicBool::new(false);

/// Name of the current program. Should be set at startup. Used by the debug function.
pub static PROGRAM_NAME: OnceLock<&'static wstr> = OnceLock::new();

pub fn get_program_name() -> &'static wstr {
    PROGRAM_NAME.get().unwrap()
}

/// MS Windows tty devices do not currently have either a read or write timestamp - those respective
/// fields of `struct stat` are always set to the current time, which means we can't rely on them.
/// In this case, we assume no external program has written to the terminal behind our back, making
/// the multiline prompt usable. See [#2859](https://github.com/fish-shell/fish-shell/issues/2859)
/// and <https://github.com/Microsoft/BashOnWindows/issues/545>
pub fn has_working_tty_timestamps() -> bool {
    if cfg!(any(target_os = "windows", cygwin)) {
        false
    } else if cfg!(target_os = "linux") {
        !is_windows_subsystem_for_linux(WSL::V1)
    } else {
        true
    }
}

/// A function type to check for cancellation.
/// Return true if execution should cancel.
/// todo!("Maybe remove the box? It is only needed for get_bg_context.")
pub type CancelChecker = Box<dyn Fn() -> bool>;

/// Encodes the bytes in `input` into a [`WString`], encoding non-UTF-8 bytes into private-use-area
/// code-points. Bytes which would be parsed into our reserved PUA range are encoded individually,
/// to allow for correct round-tripping.
pub fn bytes2wcstring(mut input: &[u8]) -> WString {
    if input.is_empty() {
        return WString::new();
    }

    let mut result = WString::new();

    fn append_escaped_str(output: &mut WString, input: &str) {
        for (i, c) in input.char_indices() {
            if fish_reserved_codepoint(c) {
                for byte in &input.as_bytes()[i..i + c.len_utf8()] {
                    output.push(encode_byte_to_char(*byte));
                }
            } else {
                output.push(c);
            }
        }
    }

    while !input.is_empty() {
        match std::str::from_utf8(input) {
            Ok(parsed_str) => {
                append_escaped_str(&mut result, parsed_str);
                // The entire remaining input could be parsed, so we are done.
                break;
            }
            Err(e) => {
                let (valid, after_valid) = input.split_at(e.valid_up_to());
                // SAFETY: The previous `str::from_utf8` call established that the prefix `valid`
                // is valid UTF-8. This prefix may be empty.
                let parsed_str = unsafe { std::str::from_utf8_unchecked(valid) };
                append_escaped_str(&mut result, parsed_str);
                // The length of the prefix of `after_valid` which is invalid UTF-8.
                // The remaining bytes of `input` (if any) will be parsed in subsequent iterations
                // of the loop, starting from the first byte that starts a valid UTF-8-encoded codepoint.
                // `error_len` can return `None`, if it sees a byte sequence that could be the
                // prefix of a valid code-point encoding at the end of the byte slice.
                // This is useful when the input is chunked, but we don't do that, so in this case
                // we use our custom encoding for all remaining bytes (at most 3).
                let error_len = e.error_len().unwrap_or(after_valid.len());
                for byte in &after_valid[..error_len] {
                    result.push(encode_byte_to_char(*byte));
                }
                input = &after_valid[error_len..];
            }
        }
    }
    result
}

pub fn cstr2wcstring(input: &[u8]) -> WString {
    let input = CStr::from_bytes_until_nul(input).unwrap().to_bytes();
    bytes2wcstring(input)
}

pub(crate) fn charptr2wcstring(input: *const libc::c_char) -> WString {
    let input: &[u8] = unsafe { CStr::from_ptr(input).to_bytes() };
    bytes2wcstring(input)
}

/// Returns a newly allocated multibyte character string equivalent of the specified wide character
/// string.
///
/// This function decodes illegal character sequences in a reversible way using the private use
/// area.
pub fn wcs2bytes(input: impl IntoCharIter) -> Vec<u8> {
    let mut result = vec![];
    wcs2bytes_appending(&mut result, input);
    result
}

pub fn wcs2osstring(input: &wstr) -> OsString {
    if input.is_empty() {
        return OsString::new();
    }

    let mut result = vec![];
    wcs2bytes_appending(&mut result, input);
    OsString::from_vec(result)
}

/// Same as [`wcs2bytes`]. Meant to be used when we need a zero-terminated string to feed legacy APIs.
/// Note: if `input` contains any interior NUL bytes, the result will be truncated at the first!
pub fn wcs2zstring(input: &wstr) -> CString {
    if input.is_empty() {
        return CString::default();
    }

    let mut vec = Vec::with_capacity(input.len() + 1);
    str2bytes_callback(input, |buff| {
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

/// Like [`wcs2bytes`], but appends to `output` instead of returning a new string.
pub fn wcs2bytes_appending(output: &mut Vec<u8>, input: impl IntoCharIter) {
    str2bytes_callback(input, |buff| {
        output.extend_from_slice(buff);
        true
    });
}

/// Stored in blocks to reference the file which created the block.
pub type FilenameRef = Arc<WString>;

pub fn init_special_chars_once() {
    if is_windows_subsystem_for_linux(WSL::Any) {
        // neither of \u23CE and \u25CF can be displayed in the default fonts on Windows, though
        // they can be *encoded* just fine. Use alternative glyphs.
        OMITTED_NEWLINE_STR.store(&"\u{00b6}"); // "pilcrow"
        OBFUSCATION_READ_CHAR.store(u32::from('\u{2022}'), Ordering::Relaxed); // "bullet" (•)
    } else if is_console_session() {
        OMITTED_NEWLINE_STR.store(&"^J");
        OBFUSCATION_READ_CHAR.store(u32::from('*'), Ordering::Relaxed);
    } else {
        OMITTED_NEWLINE_STR.store(&"\u{23CE}"); // "return symbol" (⏎)
        OBFUSCATION_READ_CHAR.store(
            u32::from(
                '\u{25CF}', // "black circle" (●)
            ),
            Ordering::Relaxed,
        );
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

// Output writes always succeed; this adapter allows us to use it in a write-like macro.
struct OutputWriteAdapter<'a, T: Output>(&'a mut T);

impl<'a, T: Output> std::fmt::Write for OutputWriteAdapter<'a, T> {
    fn write_str(&mut self, s: &str) -> std::fmt::Result {
        self.0.write_bytes(s.as_bytes());
        Ok(())
    }
}

impl<'a, T: Output> std::io::Write for OutputWriteAdapter<'a, T> {
    fn write(&mut self, buf: &[u8]) -> std::io::Result<usize> {
        self.0.write_bytes(buf);
        Ok(buf.len())
    }
    fn flush(&mut self) -> std::io::Result<()> {
        Ok(())
    }
}

pub(crate) fn do_write_to_output(writer: &mut impl Output, args: std::fmt::Arguments<'_>) {
    let mut adapter = OutputWriteAdapter(writer);
    std::fmt::write(&mut adapter, args).unwrap()
}

#[macro_export]
macro_rules! write_to_output {
    ($out:expr, $($arg:tt)*) => {{
        $crate::common::do_write_to_output($out, format_args!($($arg)*));
    }};
}

/// Write the given paragraph of output, redoing linebreaks to fit `termsize`.
pub fn reformat_for_screen(msg: &wstr, termsize: &Termsize) -> WString {
    let mut buff = WString::new();

    let screen_width = isize::try_from(termsize.width()).unwrap();
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
                buff += &sprintf!("%s-\n", token)[..];
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

#[allow(unused)]
// This function is unused in some configurations/on some platforms
fn slice_contains_slice<T: Eq>(a: &[T], b: &[T]) -> bool {
    subslice_position(a, b).is_some()
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
/// See <https://github.com/Microsoft/WSL/issues/423> and [Microsoft/WSL#2997](https://github.com/Microsoft/WSL/issues/2997)
#[inline(always)]
#[cfg(not(target_os = "linux"))]
pub fn is_windows_subsystem_for_linux(_: WSL) -> bool {
    false
}

/// Determines if we are running under Microsoft's Windows Subsystem for Linux to work around
/// some known limitations and/or bugs.
///
/// See <https://github.com/Microsoft/WSL/issues/423> and [Microsoft/WSL#2997](https://github.com/Microsoft/WSL/issues/2997)
#[cfg(target_os = "linux")]
pub fn is_windows_subsystem_for_linux(v: WSL) -> bool {
    use std::sync::OnceLock;
    static RESULT: OnceLock<Option<WSL>> = OnceLock::new();

    // This is called post-fork from [`report_setpgid_error()`], so the fast path must not involve
    // any allocations or mutexes. We can't rely on all the std functions to be alloc-free in both
    // Debug and Release modes, so we just mandate that the result already be available.
    //
    // is_wsl() is called by has_working_tty_timestamps() which is called by `screen.rs` in the main
    // process. If that's not good enough, we can call is_wsl() manually at shell startup.
    if crate::threads::is_forked_child() {
        debug_assert!(
            RESULT.get().is_some(),
            "is_wsl() should be called by main before forking!"
        );
    }

    let wsl = RESULT.get_or_init(|| {
        let release = unsafe {
            let mut info = std::mem::MaybeUninit::uninit();
            libc::uname(info.as_mut_ptr());
            let info = info.assume_init();
            info.release
        };
        let release: &[u8] = unsafe { std::mem::transmute(&release[..]) };

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
        }

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
        use crate::flog::flog;
        if env::var_os("FISH_NO_WSL_CHECK").is_none() {
            flog!(
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

/// Test if the given char is valid in a variable name.
pub fn valid_var_name_char(chr: char) -> bool {
    fish_iswalnum(chr) || chr == '_'
}

/// Test if the given string is a valid variable name.
pub fn valid_var_name(s: &wstr) -> bool {
    // Note do not use c_str(), we want to fail on embedded nul bytes.
    !s.is_empty() && s.chars().all(valid_var_name_char)
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

#[macro_export]
macro_rules! env_stack_set_from_env {
    ($vars:ident, $var_name:literal) => {{
        use std::os::unix::ffi::OsStrExt;
        if let Some(var) = std::env::var_os($var_name) {
            $vars.set_one(
                L!($var_name),
                $crate::env::EnvSetMode::new_at_early_startup($crate::env::EnvMode::GLOBAL),
                $crate::common::bytes2wcstring(var.as_bytes()),
            );
        }
    }};
}

/// The highest character number of character to try and escape.
#[cfg(test)]
pub const ESCAPE_TEST_CHAR: usize = 4000;

#[cfg(test)]
mod tests {

    use super::{
        ENCODE_DIRECT_END, ESCAPE_TEST_CHAR, EscapeFlags, EscapeStringStyle, UnescapeStringStyle,
        bytes2wcstring, escape_string, unescape_string, wcs2bytes,
    };
    use fish_util::get_seeded_rng;
    use fish_widestring::{ENCODE_DIRECT_BASE, L, WString, wstr};
    use rand::{Rng, RngCore};
    use std::fmt::Write as _;

    #[test]
    fn test_escape_string() {
        let regex = |input| escape_string(input, EscapeStringStyle::Regex);

        // plain text should not be needlessly escaped
        assert_eq!(regex(L!("hello world!")), L!("hello world!"));

        // all the following are intended to be ultimately matched literally - even if they
        // don't look like that's the intent - so we escape them.
        assert_eq!(regex(L!(".ext")), L!("\\.ext"));
        assert_eq!(regex(L!("{word}")), L!("\\{word\\}"));
        assert_eq!(regex(L!("hola-mundo")), L!("hola\\-mundo"));
        assert_eq!(
            regex(L!("$17.42 is your total?")),
            L!("\\$17\\.42 is your total\\?")
        );
        assert_eq!(
            regex(L!("not really escaped\\?")),
            L!("not really escaped\\\\\\?")
        );
    }

    #[test]
    pub fn test_unescape_sane() {
        const TEST_CASES: &[(&wstr, &wstr)] = &[
            (L!("abcd"), L!("abcd")),
            (L!("'abcd'"), L!("abcd")),
            (L!("'abcd\\n'"), L!("abcd\\n")),
            (L!("\"abcd\\n\""), L!("abcd\\n")),
            (L!("\"abcd\\n\""), L!("abcd\\n")),
            (L!("\\143"), L!("c")),
            (L!("'\\143'"), L!("\\143")),
            (L!("\\n"), L!("\n")), // \n normally becomes newline
        ];

        for (input, expected) in TEST_CASES {
            let Some(output) = unescape_string(input, UnescapeStringStyle::default()) else {
                panic!("Failed to unescape string {input:?}");
            };

            assert_eq!(
                output, *expected,
                "In unescaping {input:?}, expected {expected:?} but got {output:?}\n"
            );
        }
    }

    #[test]
    fn test_escape_var() {
        const TEST_CASES: &[(&wstr, &wstr)] = &[
            (L!(" a"), L!("_20_a")),
            (L!("a B "), L!("a_20_42_20_")),
            (L!("a b "), L!("a_20_b_20_")),
            (L!(" B"), L!("_20_42_")),
            (L!(" f"), L!("_20_f")),
            (L!(" 1"), L!("_20_31_")),
            (L!("a\nghi_"), L!("a_0A_ghi__")),
        ];

        for (input, expected) in TEST_CASES {
            let output = escape_string(input, EscapeStringStyle::Var);

            assert_eq!(
                output, *expected,
                "In escaping {input:?} with style var, expected {expected:?} but got {output:?}\n"
            );
        }
    }

    fn escape_test(escape_style: EscapeStringStyle, unescape_style: UnescapeStringStyle) {
        let seed = rand::rng().next_u64();
        let mut rng = get_seeded_rng(seed);

        let mut random_string = WString::new();
        let mut escaped_string;
        for _ in 0..(ESCAPE_TEST_COUNT as u32) {
            random_string.clear();
            let length = rng.random_range(0..=(2 * ESCAPE_TEST_LENGTH));
            for _ in 0..length {
                random_string
                    .push(char::from_u32((rng.next_u32() % ESCAPE_TEST_CHAR as u32) + 1).unwrap());
            }

            escaped_string = escape_string(&random_string, escape_style);
            let Some(unescaped_string) = unescape_string(&escaped_string, unescape_style) else {
                let slice = escaped_string.as_char_slice();
                panic!("Failed to unescape string {slice:?}. Generated from seed {seed}.");
            };
            assert_eq!(
                random_string, unescaped_string,
                "Escaped and then unescaped string {random_string:?}, but got back a different string {unescaped_string:?}. \
                The intermediate escape looked like {escaped_string:?}. \
                Generated from seed {seed}."
            );
        }
    }

    #[test]
    fn test_escape_random_script() {
        escape_test(EscapeStringStyle::default(), UnescapeStringStyle::default());
    }

    #[test]
    fn test_escape_random_var() {
        escape_test(EscapeStringStyle::Var, UnescapeStringStyle::Var);
    }

    #[test]
    fn test_escape_random_url() {
        escape_test(EscapeStringStyle::Url, UnescapeStringStyle::Url);
    }

    #[test]
    fn test_escape_no_printables() {
        // Verify that ESCAPE_NO_PRINTABLES also escapes backslashes so we don't regress on issue #3892.
        let random_string = L!("line 1\\n\nline 2").to_owned();
        let escaped_string = escape_string(
            &random_string,
            EscapeStringStyle::Script(EscapeFlags::NO_PRINTABLES | EscapeFlags::NO_QUOTED),
        );
        let Some(unescaped_string) =
            unescape_string(&escaped_string, UnescapeStringStyle::default())
        else {
            panic!("Failed to unescape string <{escaped_string}>");
        };

        assert_eq!(
            random_string, unescaped_string,
            "Escaped and then unescaped string '{random_string}', but got back a different string '{unescaped_string}'"
        );
    }

    /// The number of tests to run.
    const ESCAPE_TEST_COUNT: usize = 20_000;
    /// The average length of strings to unescape.
    const ESCAPE_TEST_LENGTH: usize = 100;

    /// Helper to convert a narrow string to a sequence of hex digits.
    fn bytes2hex(input: &[u8]) -> String {
        let mut output = String::with_capacity(input.len() * 5);
        for byte in input {
            write!(output, "0x{:2X} ", *byte).unwrap();
        }
        output
    }

    /// Test wide/narrow conversion by creating random strings and verifying that the original
    /// string comes back through double conversion.
    #[test]
    fn test_convert() {
        let seed = rand::rng().next_u64();
        let mut rng = get_seeded_rng(seed);
        let mut origin = Vec::new();

        for _ in 0..ESCAPE_TEST_COUNT {
            let length: usize = rng.random_range(0..=(2 * ESCAPE_TEST_LENGTH));
            origin.resize(length, 0);
            rng.fill_bytes(&mut origin);

            let w = bytes2wcstring(&origin[..]);
            let n = wcs2bytes(&w);
            assert_eq!(
                origin,
                n,
                "Conversion cycle of string:\n{:4} chars: {}\n\
                    produced different string:\n\
                    {:4} chars: {}\n
                    Use this seed to reproduce: {}",
                origin.len(),
                &bytes2hex(&origin),
                n.len(),
                &bytes2hex(&n),
                seed,
            );
        }
    }

    /// Verify correct behavior with embedded nulls.
    #[test]
    fn test_convert_nulls() {
        let input = L!("AAA\0BBB");
        let out_str = wcs2bytes(input);
        assert_eq!(
            input.chars().collect::<Vec<_>>(),
            std::str::from_utf8(&out_str)
                .unwrap()
                .chars()
                .collect::<Vec<_>>()
        );

        let out_wstr = bytes2wcstring(&out_str);
        assert_eq!(input, &out_wstr);
    }

    /// Verify that ASCII narrow->wide conversions are correct.
    #[test]
    fn test_convert_ascii() {
        let mut s = vec![b'\0'; 4096];
        for (i, c) in s.iter_mut().enumerate() {
            *c = u8::try_from(i % 10).unwrap() + b'0';
        }

        // Test a variety of alignments.
        for left in 0..16 {
            for right in 0..16 {
                let len = s.len() - left - right;
                let input = &s[left..left + len];
                let wide = bytes2wcstring(input);
                let narrow = wcs2bytes(&wide);
                assert_eq!(narrow, input);
            }
        }

        // Put some non-ASCII bytes in and ensure it all still works.
        for i in 0..s.len() {
            let saved = s[i];
            s[i] = 0xF7;
            assert_eq!(wcs2bytes(&bytes2wcstring(&s)), s);
            s[i] = saved;
        }
    }

    /// fish uses the private-use range to encode bytes that are not valid UTF-8.
    /// If the input decodes to these private-use codepoints,
    /// then fish should also use the direct encoding for those bytes.
    /// Verify that characters in the private use area are correctly round-tripped. See #7723.
    #[test]
    fn test_convert_private_use() {
        for c in ENCODE_DIRECT_BASE..ENCODE_DIRECT_END {
            // A `char` represents an Unicode scalar value, which takes up at most 4 bytes when encoded in UTF-8.
            // TODO MSRV(1.92?) replace 4 by `char::MAX_LEN_UTF8` once that's available in our MSRV.
            // https://doc.rust-lang.org/std/primitive.char.html#associatedconstant.MAX_LEN_UTF8
            let mut converted = [0_u8; 4];
            let s = c.encode_utf8(&mut converted).as_bytes();
            // Ask fish to decode this via bytes2wcstring.
            // bytes2wcstring should notice that the decoded form collides with its private use
            // and encode it directly.
            let ws = bytes2wcstring(s);

            // Each byte should be encoded directly, and round tripping should work.
            assert_eq!(ws.len(), s.len());
            assert_eq!(wcs2bytes(&ws), s);
        }
    }
}

#[cfg(feature = "benchmark")]
#[cfg(test)]
mod bench {
    extern crate test;
    use crate::common::bytes2wcstring;
    use test::Bencher;

    #[bench]
    fn bench_convert_ascii(b: &mut Bencher) {
        let s: [u8; 128 * 1024] = std::array::from_fn(|i| b'0' + u8::try_from(i % 10).unwrap());
        b.bytes = u64::try_from(s.len()).unwrap();
        b.iter(|| bytes2wcstring(&s));
    }
}
