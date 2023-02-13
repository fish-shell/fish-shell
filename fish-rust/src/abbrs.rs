use std::{
    collections::HashSet,
    sync::{Arc, Mutex},
};

use crate::{
    wchar::L,
    wchar_ffi::{WCharFromFFI, WCharToFFI},
};
use cxx::CxxWString;
use once_cell::sync::Lazy;
pub use widestring::{Utf32Str as wstr, Utf32String as WString};

use crate::abbrs::abbr_ffi::abbrs_replacer_t;
use crate::{ffi::re::regex_t, parse_constants::SourceRange};

use self::abbr_ffi::{abbrs_position_t, abbrs_replacement_t, source_range_t};

#[cxx::bridge]
mod abbr_ffi {
    extern "C++" {
        include!("re.h");
        include!("parse_constants.h");

        type source_range_t = crate::ffi::source_range_t;
    }

    enum abbrs_position_t {
        command,
        anywhere,
    }

    struct abbrs_replacer_t {
        replacement: UniquePtr<CxxWString>,
        is_function: bool,
        set_cursor_marker: UniquePtr<CxxWString>,
        has_cursor_marker: bool,
    }

    struct abbrs_replacement_t {
        range: source_range_t,
        text: UniquePtr<CxxWString>,
        cursor: usize,
        has_cursor: bool,
    }

    extern "Rust" {
        #[cxx_name = "abbrs_match"]
        fn abbrs_match_ffi(token: &CxxWString, position: abbrs_position_t)
            -> Vec<abbrs_replacer_t>;

        #[cxx_name = "abbrs_has_match"]
        fn abbrs_has_match_ffi(token: &CxxWString, position: abbrs_position_t) -> bool;

        #[cxx_name = "abbrs_replacement_from"]
        fn abbrs_replacement_from_ffi(
            range: source_range_t,
            mut text: CxxWString,
            replacer: abbrs_replacer_t,
        ) -> abbrs_replacement_t;
    }
}

const abbrs: Lazy<Arc<Mutex<AbbreviationSet>>> =
    Lazy::new(|| Arc::new(Mutex::new(Default::default())));

/// Controls where in the command line abbreviations may expand.
#[derive(Debug, PartialEq, Clone, Copy)]
enum Position {
    Command,  // expand in command position
    Anywhere, // expand in any token
}

impl From<abbrs_position_t> for Position {
    fn from(value: abbrs_position_t) -> Self {
        match value {
            abbrs_position_t::anywhere => Position::Anywhere,
            abbrs_position_t::command => Position::Command,
            _ => panic!("invalid abbrs_position_t"),
        }
    }
}

struct Abbreviation {
    // Abbreviation name. This is unique within the abbreviation set.
    // This is used as the token to match unless we have a regex.
    name: WString,

    /// The key (recognized token) - either a literal or a regex pattern.
    key: WString,

    /// If set, use this regex to recognize tokens.
    /// If unset, the key is to be interpreted literally.
    /// Note that the fish interface enforces that regexes match the entire token;
    /// we accomplish this by surrounding the regex in ^ and $.
    regex: Option<regex_t>,

    /// Replacement string.
    replacement: WString,

    /// If set, the replacement is a function name.
    replacement_is_function: bool,

    /// Expansion position.
    position: Position,

    /// If set, then move the cursor to the first instance of this string in the expansion.
    set_cursor_marker: Option<WString>,

    /// Mark if we came from a universal variable.
    from_universal: bool,
}

impl Abbreviation {
    // Construct from a name, a key which matches a token, a replacement token, a position, and
    // whether we are derived from a universal variable.
    fn new(
        name: WString,
        key: WString,
        replacement: WString,
        position: Position,
        from_universal: bool,
    ) -> Self {
        Self {
            name,
            key,
            regex: None,
            replacement,
            replacement_is_function: false,
            position,
            set_cursor_marker: None,
            from_universal,
        }
    }

    // \return true if this is a regex abbreviation.
    pub fn is_regex(&self) -> bool {
        self.regex.is_some()
    }

    // \return true if we match a token at a given position.
    pub fn matches(&self, token: &wstr, position: Position) -> bool {
        if !self.matches_position(position) {
            return false;
        }
        self.regex
            .as_ref()
            .map(|r| r.matches_ffi(&token.to_ffi()))
            .unwrap_or(self.key == token)
    }

    // \return if we expand in a given position.
    fn matches_position(&self, position: Position) -> bool {
        return self.position == Position::Anywhere || self.position == position;
    }
}

/// The result of an abbreviation expansion.
struct Replacer {
    /// The string to use to replace the incoming token, either literal or as a function name.
    replacement: WString,

    /// If true, treat 'replacement' as the name of a function.
    is_function: bool,

    /// If set, the cursor should be moved to the first instance of this string in the expansion.
    set_cursor_marker: Option<WString>,
}

impl From<Replacer> for abbrs_replacer_t {
    fn from(value: Replacer) -> Self {
        let has_cursor_marker = value.set_cursor_marker.is_some();
        Self {
            replacement: value.replacement.to_ffi(),
            is_function: value.is_function,
            set_cursor_marker: value.set_cursor_marker.unwrap_or_default().to_ffi(),
            has_cursor_marker,
        }
    }
}

impl Into<Replacer> for abbrs_replacer_t {
    fn into(self) -> Replacer {
        let cursor_marker = if self.has_cursor_marker {
            Some(self.set_cursor_marker.from_ffi())
        } else {
            None
        };
        Replacer {
            replacement: self.replacement.from_ffi(),
            is_function: self.is_function,
            set_cursor_marker: cursor_marker,
        }
    }
}

struct Replacement {
    /// The original range of the token in the command line.
    range: SourceRange,

    /// The string to replace with.
    text: WString,

    /// The new cursor location, or none to use the default.
    /// This is relative to the original range.
    cursor: Option<usize>,
}

impl Replacement {
    /// Construct a replacement from a replacer.
    /// The \p range is the range of the text matched by the replacer in the command line.
    /// The text is passed in separately as it may be the output of the replacer's function.
    fn from(range: SourceRange, mut text: WString, replacer: Replacer) -> Self {
        let mut cursor = None;
        if let Some(set_cursor_marker) = replacer.set_cursor_marker {
            let matched = text
                .as_char_slice()
                .windows(set_cursor_marker.len())
                .enumerate()
                .find(|(_, w)| w.starts_with(set_cursor_marker.as_char_slice()))
                .map(|(i, _)| i);

            if let Some(start) = matched {
                text.replace_range(start..(set_cursor_marker.len()), L!(""));
                cursor = Some(start + range.start as usize)
            }
        }
        Self {
            range,
            text,
            cursor,
        }
    }
}

fn abbrs_replacement_from_ffi(
    range: source_range_t,
    text: CxxWString,
    replacer: abbrs_replacer_t,
) -> abbrs_replacement_t {
    let replacement = Replacement::from(
        SourceRange {
            start: range.start,
            length: range.length,
        },
        text.from_ffi(),
        replacer.into(),
    );

    abbrs_replacement_t {
        range,
        text: replacement.text.to_ffi(),
        cursor: replacement.cursor.unwrap_or_default(),
        has_cursor: replacement.cursor.is_some(),
    }
}

#[derive(Default)]
struct AbbreviationSet {
    /// List of abbreviations, in definition order.
    abbrs: Vec<Abbreviation>,

    /// Set of used abbrevation names.
    /// This is to avoid a linear scan when adding new abbreviations.
    used_names: HashSet<WString>,
}

impl AbbreviationSet {
    /// \return the list of replacers for an input token, in priority order.
    /// The \p position is given to describe where the token was found.
    fn r#match(&self, token: &wstr, position: Position) -> Vec<Replacer> {
        let mut result = vec![];

        // Later abbreviations take precedence so walk backwards.
        for abbr in self.abbrs.iter().rev() {
            if abbr.matches(token, position) {
                result.push(Replacer {
                    replacement: abbr.replacement.clone(),
                    is_function: abbr.replacement_is_function,
                    set_cursor_marker: abbr.set_cursor_marker.clone(),
                });
            }
        }
        return result;
    }

    /// \return whether we would have at least one replacer for a given token.
    fn has_match(&self, token: &wstr, position: Position) -> bool {
        self.abbrs.iter().any(|abbr| abbr.matches(token, position))
    }

    /// Add an abbreviation. Any abbreviation with the same name is replaced.
    fn add(&mut self, abbr: Abbreviation) {
        assert!(abbr.name.is_empty(), "Invalid name");
        let inserted = self.used_names.insert(abbr.name.clone());
        if !inserted {
            // Name was already used, do a linear scan to find it.
            let (index, _) = self
                .abbrs
                .iter()
                .enumerate()
                .find(|a| a.1.name == abbr.name)
                .expect("Abbreviation not found though its name was present");

            self.abbrs.remove(index);
        }
        self.abbrs.push(abbr);
    }

    /// Rename an abbreviation. This asserts that the old name is used, and the new name is not; the
    /// caller should check these beforehand with has_name().
    fn rename(&mut self, old_name: &wstr, new_name: &wstr) {
        let erased = self.used_names.remove(old_name);
        let inserted = self.used_names.insert(new_name.to_owned());
        assert!(
            erased && inserted,
            "Old name not found or new name already present"
        );
        for abbr in self.abbrs.iter_mut() {
            if abbr.name == old_name {
                abbr.name = new_name.to_owned();
                break;
            }
        }
    }

    /// Erase an abbreviation by name.
    /// \return true if erased, false if not found.
    fn erase(&mut self, name: &wstr) -> bool {
        let erased = self.used_names.remove(name);
        if !erased {
            return false;
        }
        for (index, abbr) in self.abbrs.iter().enumerate().rev() {
            if abbr.name == name {
                self.abbrs.remove(index);
                return true;
            }
        }
        assert!(false, "Unable to find named abbreviation");
        return false;
    }

    /// \return true if we have an abbreviation with the given name.
    fn has_name(&self, name: &wstr) -> bool {
        self.used_names.contains(name)
    }

    /// \return a reference to the abbreviation list.
    fn list(&self) -> &[Abbreviation] {
        &self.abbrs
    }
}

/// \return the list of replacers for an input token, in priority order, using the global set.
/// The \p position is given to describe where the token was found.
fn abbrs_match(token: &wstr, position: Position) -> Vec<Replacer> {
    return abbrs.lock().unwrap().r#match(token, position);
}

fn abbrs_match_ffi(token: &CxxWString, position: abbrs_position_t) -> Vec<abbrs_replacer_t> {
    abbrs_match(&token.from_ffi(), position.into())
        .into_iter()
        .map(|r| r.into())
        .collect()
}

fn abbrs_has_match_ffi(token: &CxxWString, position: abbrs_position_t) -> bool {
    abbrs
        .lock()
        .unwrap()
        .has_match(&token.from_ffi(), position.into())
}
