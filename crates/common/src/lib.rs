use bitflags::bitflags;
use fish_feature_flags::{FeatureFlag, feature_test};
use fish_widestring::{
    ANY_CHAR, ANY_STRING, ANY_STRING_RECURSIVE, ASCII_MAX, BRACE_BEGIN, BRACE_END, BRACE_SEP,
    BRACE_SPACE, BYTE_MAX, HOME_DIRECTORY, INTERNAL_SEPARATOR, L, PROCESS_EXPAND_SELF,
    PROCESS_EXPAND_SELF_STR, UCS2_MAX, VARIABLE_EXPAND, VARIABLE_EXPAND_SINGLE, WExt as _, WString,
    bytes2wcstring, decode_byte_from_char, fish_reserved_codepoint, wcs2bytes, wstr,
};
use libc::{SIG_IGN, SIGTTOU, STDIN_FILENO};
use std::{
    cell::{Cell, RefCell},
    env,
    io::Read,
    mem,
    ops::{Deref, DerefMut},
    os::{
        fd::{AsRawFd, BorrowedFd, RawFd},
        unix::ffi::OsStrExt as _,
    },
    rc::Rc,
    sync::{
        Arc, LazyLock,
        atomic::{AtomicI32, AtomicU32, Ordering},
    },
    time,
};

pub const PACKAGE_NAME: &str = env!("CARGO_PKG_NAME");

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
        /// Escape ,
        const COMMA = 1 << 4;
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

    let mut out = WString::with_capacity(input.len());

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
                        out.push('\\');
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

/// Attempts to escape the string 'cmd' using the given quote type, as determined by the quote
/// character. The quote can be a single quote or double quote, or L'\0' to indicate no quoting (and
/// thus escaping should be with backslashes). Optionally do not escape tildes.
pub fn escape_string_with_quote(
    cmd: &wstr,
    quote: Option<char>,
    escape_flags: EscapeFlags,
) -> WString {
    let Some(quote) = quote else {
        return escape_string(cmd, EscapeStringStyle::Script(escape_flags));
    };
    // Here we are going to escape a string with quotes.
    // A few characters cannot be represented inside quotes, e.g. newlines. In that case,
    // terminate the quote and then re-enter it.
    let mut result = WString::new();
    result.reserve(cmd.len());
    for c in cmd.chars() {
        match c {
            '\n' => {
                for c in [quote, '\\', 'n', quote] {
                    result.push(c);
                }
            }
            '\t' => {
                for c in [quote, '\\', 't', quote] {
                    result.push(c);
                }
            }
            '\x08' => {
                for c in [quote, '\\', 'b', quote] {
                    result.push(c);
                }
            }
            '\r' => {
                for c in [quote, '\\', 'r', quote] {
                    result.push(c);
                }
            }
            '\\' => {
                result.push_str("\\\\");
            }
            '$' => {
                if quote == '"' {
                    result.push('\\');
                }
                result.push('$');
            }
            _ => {
                if c == quote {
                    result.push('\\');
                }
                result.push(c);
            }
        }
    }
    result
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
            if c.is_alphanumeric() || b"/.~-_".contains(&byte) {
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
                '\\'
                    if !ignore_backslashes => {
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
                '~'
                    if unescape_special
                        && (input_position == 0 || Some(input_position) == potential_word_start)
                    => {
                        to_append_or_none = Some(HOME_DIRECTORY);
                    }
                '%'
                    // Note that this only recognizes %self if the string is literally %self.
                    // %self/foo will NOT match this.
                    if allow_percent_self
                        && unescape_special
                        && input_position == 0
                        && input == PROCESS_EXPAND_SELF_STR
                    => {
                        to_append_or_none = Some(PROCESS_EXPAND_SELF);
                        input_position += PROCESS_EXPAND_SELF_STR.len() - 1; // skip over 'self's
                    }
                '*'
                    if unescape_special => {
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
                '?'
                    if unescape_special && !feature_test(FeatureFlag::QuestionMarkNoGlob) => {
                        to_append_or_none = Some(ANY_CHAR);
                    }
                '$'
                    if unescape_special => {
                        let is_cmdsub = input_position + 1 < input.len()
                            && input.char_at(input_position + 1) == '(';
                        if !is_cmdsub {
                            to_append_or_none = Some(VARIABLE_EXPAND);
                            vars_or_seps.push(input_position);
                        }
                    }
                '{'
                    if unescape_special => {
                        brace_count += 1;
                        to_append_or_none = Some(BRACE_BEGIN);
                        // We need to store where the brace *ends up* in the output.
                        braces.push(result.len());
                        potential_word_start = Some(input_position + 1);
                    }
                '}'
                    if unescape_special => {
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
                ','
                    if unescape_special && brace_count > 0 => {
                        to_append_or_none = Some(BRACE_SEP);
                        vars_or_seps.push(input_position);
                        potential_word_start = Some(input_position + 1);
                    }
                ' '
                    if unescape_special && brace_count > 0 => {
                        to_append_or_none = Some(BRACE_SPACE);
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
                '$' if unescape_special => {
                    to_append_or_none = Some(VARIABLE_EXPAND_SINGLE);
                    vars_or_seps.push(input_position);
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
        i += 1;
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

/// This function attempts to distinguish between a console session (at the actual login vty) and a
/// session within a terminal emulator inside a desktop environment or over SSH. Unfortunately
/// there are few values of $TERM that we can interpret as being exclusively console sessions, and
/// most common operating systems do not use them. The value is cached for the duration of the fish
/// session. We err on the side of assuming it's not a console session. This approach isn't
/// bullet-proof and that's OK.
pub fn is_console_session() -> bool {
    // TODO(terminal-workaround)
    static IS_CONSOLE_SESSION: LazyLock<bool> = LazyLock::new(||
        // No console session on Apple, and ttyname may hang (#12506).
        !cfg!(apple)
            && nix::unistd::ttyname(unsafe { std::os::fd::BorrowedFd::borrow_raw(STDIN_FILENO) })
                .is_ok_and(|buf| {
                    // Check if the tty matches /dev/(console|dcons|tty[uv\d])
                    let is_console_tty = match buf.as_os_str().as_bytes() {
                        b"/dev/console" => true,
                        b"/dev/dcons" => true,
                        bytes => bytes.strip_prefix(b"/dev/tty").is_some_and(|rest| {
                            matches!(rest.first(), Some(b'u' | b'v' | b'0'..=b'9'))
                        }),
                    };

                    // and that $TERM is simple, e.g. `xterm` or `vt100`, not `xterm-something` or `sun-color`.
                    is_console_tty
                        && env::var_os("TERM").is_none_or(|t| !t.as_bytes().contains(&b'-'))
                }));
    *IS_CONSOLE_SESSION
}

/// Exits without invoking destructors (via _exit), useful for code after fork.
pub fn exit_without_destructors(code: libc::c_int) -> ! {
    unsafe { libc::_exit(code) };
}

// Only pub for `src/common.rs`
pub static OBFUSCATION_READ_CHAR: AtomicU32 = AtomicU32::new(0);

pub fn get_obfuscation_read_char() -> char {
    char::from_u32(OBFUSCATION_READ_CHAR.load(Ordering::Relaxed)).unwrap()
}

/// Call read, blocking and repeating on EINTR. Exits on EAGAIN.
/// Return the number of bytes read, or 0 on EOF, or an error.
pub fn read_blocked(fd: RawFd, buf: &mut [u8]) -> nix::Result<usize> {
    loop {
        let res = nix::unistd::read(unsafe { BorrowedFd::borrow_raw(fd) }, buf);
        if let Err(nix::Error::EINTR) = res {
            continue;
        }
        return res;
    }
}

pub trait ReadExt {
    /// Like [`std::io::Read::read_to_end`], but does not retry on EINTR.
    fn read_to_end_interruptible(&mut self, buf: &mut Vec<u8>) -> std::io::Result<()>;
}

impl<T: Read + ?Sized> ReadExt for T {
    fn read_to_end_interruptible(&mut self, buf: &mut Vec<u8>) -> std::io::Result<()> {
        let mut chunk = [0_u8; 4096];
        loop {
            match self.read(&mut chunk)? {
                0 => return Ok(()),
                n => buf.extend_from_slice(&chunk[..n]),
            }
        }
    }
}

/// A rusty port of the C++ `write_loop()` function from `common.cpp`. This should be deprecated in
/// favor of native rust read/write methods at some point.
pub fn write_loop<Fd: AsRawFd>(fd: &Fd, buf: &[u8]) -> std::io::Result<()> {
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
    Ok(())
}

pub const fn help_section_exists(section: &str) -> bool {
    let haystack = include_str!("../../../share/help_sections");
    let needle = section;

    let needle = needle.as_bytes();
    let haystack = haystack.as_bytes();
    let nlen = needle.len();
    let mut line_start = 0;
    let mut i = 0;
    while i <= haystack.len() {
        if i == haystack.len() || haystack[i] == b'\n' {
            let line_len = i - line_start;

            if line_len == nlen {
                let mut j = 0;
                while j < nlen && haystack[line_start + j] == needle[j] {
                    j += 1;
                }
                if j == nlen {
                    return true;
                }
            }
            line_start = i + 1;
        }
        i += 1;
    }
    false
}

#[macro_export]
macro_rules! help_section {
    ($section:expr) => {{
        const {
            assert!($crate::help_section_exists($section));
        }

        $section
    }};
}

/// Stored in blocks to reference the file which created the block.
pub type FilenameRef = Arc<WString>;

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

/// A wrapper around `Rc<Cell>` which supports modifying the contents, scoped to a region of code.
/// This provides a somewhat nicer API than ScopedRefCell because you can directly modify the
/// value, instead of requiring an accessor function which returns a mutable reference to a field.
pub struct ScopedCell<T>(Rc<Cell<T>>);

impl<T> Deref for ScopedCell<T> {
    type Target = Cell<T>;

    fn deref(&self) -> &Self::Target {
        &self.0
    }
}

impl<T: Copy> ScopedCell<T> {
    pub fn new(value: T) -> Self {
        Self(Rc::new(Cell::new(value)))
    }

    /// Temporarily modify a value in the ScopedCell, restoring it when the returned object is dropped.
    ///
    /// This is useful when you want to apply a change for the duration of a scope
    /// without having to manually restore the previous value.
    ///
    /// # Example
    ///
    /// ```
    /// use fish_common::ScopedCell;
    ///
    /// let cell = ScopedCell::new(5);
    /// assert_eq!(cell.get(), 5);
    ///
    /// {
    ///     let _guard = cell.scoped_mod(|v| *v += 10);
    ///     assert_eq!(cell.get(), 15);
    /// }
    ///
    /// // Restored after scope
    /// assert_eq!(cell.get(), 5);
    /// ```
    pub fn scoped_mod<Modifier: FnOnce(&mut T)>(
        &self,
        modifier: Modifier,
    ) -> impl DerefMut + use<T, Modifier> {
        let mut val = self.get();
        modifier(&mut val);
        let saved = self.replace(val);
        let inner = Rc::clone(&self.0);
        ScopeGuard::new((), move |()| inner.set(saved))
    }
}

/// A wrapper around `Rc<RefCell>` which supports modifying the contents, scoped to a region
/// of code.
#[derive(Default)]
pub struct ScopedRefCell<T>(Rc<RefCell<T>>);

impl<T> Deref for ScopedRefCell<T> {
    type Target = RefCell<T>;

    fn deref(&self) -> &Self::Target {
        &self.0
    }
}

impl<T> ScopedRefCell<T> {
    pub fn new(value: T) -> Self {
        Self(Rc::new(RefCell::new(value)))
    }

    /// Temporarily modify a field in the ScopedRefCell, restoring it when the returned guard is dropped.
    ///
    /// This is useful when you want to change part of a data structure for the duration of a scope,
    /// and automatically restore the original value afterward.
    ///
    /// The `accessor` function selects the field to modify by returning a mutable reference to it.
    ///
    /// # Example
    /// ```
    /// use fish_common::ScopedRefCell;
    ///
    /// struct State { flag: bool }
    ///
    /// let cell = ScopedRefCell::new(State { flag: false });
    /// assert_eq!(cell.borrow().flag, false);
    ///
    /// {
    ///     let _guard = cell.scoped_set(true, |s| &mut s.flag);
    ///     assert_eq!(cell.borrow().flag, true);
    /// }
    ///
    /// // Restored after scope
    /// assert_eq!(cell.borrow().flag, false);
    /// ```
    pub fn scoped_set<Accessor, Value>(
        &self,
        value: Value,
        accessor: Accessor,
    ) -> impl DerefMut + use<T, Accessor, Value>
    where
        Accessor: Fn(&mut T) -> &mut Value,
    {
        let mut data = self.borrow_mut();
        let mut saved = std::mem::replace(accessor(&mut data), value);
        let inner = Rc::clone(&self.0);
        ScopeGuard::new((), move |()| {
            let mut inner = inner.borrow_mut();
            std::mem::swap((accessor)(&mut inner), &mut saved);
        })
    }

    /// Convenience method for replacing the entire contents of the ScopedRefCell, restoring it when dropped.
    ///
    /// Equivalent to `scoped_set(value, |s| s)`.
    ///
    /// # Example
    /// ```
    /// use fish_common::ScopedRefCell;
    ///
    /// let cell = ScopedRefCell::new(10);
    /// assert_eq!(*cell.borrow(), 10);
    ///
    /// {
    ///     let _guard = cell.scoped_replace(99);
    ///     assert_eq!(*cell.borrow(), 99);
    /// }
    ///
    /// assert_eq!(*cell.borrow(), 10);
    /// ```
    pub fn scoped_replace(&self, value: T) -> impl DerefMut + use<T> {
        self.scoped_set(value, |s| s)
    }
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
/// use fish_common::ScopeGuard;
///
/// let file = std::fs::File::create("/dev/null").unwrap();
/// // Create a scope guard to write to the file when the scope expires.
/// // To be able to still use the file, shadow `file` with the ScopeGuard itself.
/// let mut file = ScopeGuard::new(file, |mut file| file.write_all(b"goodbye\n").unwrap());
/// // Now write to the file normally "through" the capturing ScopeGuard instance.
/// file.write_all(b"hello\n").unwrap();
///
/// // hello will be written first, then goodbye.
/// ```
pub struct ScopeGuard<T, F: FnOnce(T)>(Option<(T, F)>);

impl<T, F: FnOnce(T)> ScopeGuard<T, F> {
    /// Creates a new `ScopeGuard` wrapping `value`. The `on_drop` callback is executed when the
    /// ScopeGuard's lifetime expires or when it is manually dropped.
    pub fn new(value: T, on_drop: F) -> Self {
        Self(Some((value, on_drop)))
    }
}

impl<T, F: FnOnce(T)> Deref for ScopeGuard<T, F> {
    type Target = T;

    fn deref(&self) -> &Self::Target {
        &self.0.as_ref().unwrap().0
    }
}

impl<T, F: FnOnce(T)> DerefMut for ScopeGuard<T, F> {
    fn deref_mut(&mut self) -> &mut Self::Target {
        &mut self.0.as_mut().unwrap().0
    }
}

impl<T, F: FnOnce(T)> Drop for ScopeGuard<T, F> {
    fn drop(&mut self) {
        if let Some((value, on_drop)) = self.0.take() {
            on_drop(value);
        }
    }
}

pub const fn assert_send<T: Send>() {}
pub const fn assert_sync<T: Sync>() {}

/// Asserts that a slice is alphabetically sorted by a <code>&[wstr]</code> `name` field.
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
/// use fish_widestring::{L, wstr};
/// use fish_common::assert_sorted_by_name;
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
/// use fish_widestring::{L, wstr};
/// use fish_common::assert_sorted_by_name;
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

/// Return a reference to the first entry with the given name, assuming the entries are sorted by
/// name. Return None if not found.
pub fn get_by_sorted_name<T: Named>(name: &wstr, vals: &'static [T]) -> Option<&'static T> {
    match vals.binary_search_by_key(&name, |val| val.name()) {
        Ok(index) => Some(&vals[index]),
        Err(_) => None,
    }
}

/// Given an input string, return a prefix of the string up to the first NUL character,
/// or the entire string if there is no NUL character.
pub fn truncate_at_nul(input: &wstr) -> &wstr {
    match input.chars().position(|c| c == '\0') {
        Some(nul_pos) => &input[..nul_pos],
        None => input,
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_scoped_cell() {
        let cell = ScopedCell::new(42);

        {
            let _guard = cell.scoped_mod(|x| *x += 1);
            assert_eq!(cell.get(), 43);
        }

        assert_eq!(cell.get(), 42);
    }

    #[test]
    fn test_scoped_refcell() {
        #[derive(Debug, PartialEq, Clone)]
        struct Data {
            x: i32,
            y: i32,
        }

        let cell = ScopedRefCell::new(Data { x: 1, y: 2 });

        {
            let _guard = cell.scoped_set(10, |d| &mut d.x);
            assert_eq!(cell.borrow().x, 10);
        }
        assert_eq!(cell.borrow().x, 1);

        {
            let _guard = cell.scoped_replace(Data { x: 42, y: 99 });
            assert_eq!(*cell.borrow(), Data { x: 42, y: 99 });
        }
        assert_eq!(*cell.borrow(), Data { x: 1, y: 2 });
    }

    #[test]
    fn test_scope_guard() {
        let relaxed = std::sync::atomic::Ordering::Relaxed;
        let counter = std::sync::atomic::AtomicUsize::new(0);
        {
            let guard = ScopeGuard::new(123, |arg| {
                assert_eq!(arg, 123);
                counter.fetch_add(1, relaxed);
            });
            assert_eq!(counter.load(relaxed), 0);
            drop(guard);
            assert_eq!(counter.load(relaxed), 1);
        }
        // The callback is invoked on drop
        {
            let guard = ScopeGuard::new(123, |arg| {
                assert_eq!(arg, 123);
                counter.fetch_add(1, relaxed);
            });
            assert_eq!(counter.load(relaxed), 1);
            drop(guard);
            assert_eq!(counter.load(relaxed), 2);
        }
    }

    #[test]
    fn test_truncate_at_nul() {
        assert_eq!(truncate_at_nul(L!("abc\0def")), L!("abc"));
        assert_eq!(truncate_at_nul(L!("abc")), L!("abc"));
        assert_eq!(truncate_at_nul(L!("\0abc")), L!(""));
    }
}
