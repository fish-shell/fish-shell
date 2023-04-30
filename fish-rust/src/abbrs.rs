#![allow(clippy::extra_unused_lifetimes, clippy::needless_lifetimes)]
use std::{
    collections::HashSet,
    sync::{Mutex, MutexGuard},
};

use crate::wchar::{wstr, WString, L};
use crate::wchar_ffi::{AsWstr, WCharFromFFI, WCharToFFI};
use cxx::CxxWString;
use once_cell::sync::Lazy;

use crate::abbrs::abbrs_ffi::abbrs_replacer_t;
use crate::parse_constants::SourceRange;
use pcre2::utf32::Regex;

use self::abbrs_ffi::{abbreviation_t, abbrs_position_t, abbrs_replacement_t};

#[cxx::bridge]
mod abbrs_ffi {
    extern "C++" {
        include!("re.h");
        include!("parse_constants.h");

        type SourceRange = crate::parse_constants::SourceRange;
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
        range: SourceRange,
        text: UniquePtr<CxxWString>,
        cursor: usize,
        has_cursor: bool,
    }

    struct abbreviation_t {
        key: UniquePtr<CxxWString>,
        replacement: UniquePtr<CxxWString>,
        is_regex: bool,
    }

    extern "Rust" {
        type GlobalAbbrs<'a>;

        #[cxx_name = "abbrs_list"]
        fn abbrs_list_ffi() -> Vec<abbreviation_t>;

        #[cxx_name = "abbrs_match"]
        fn abbrs_match_ffi(token: &CxxWString, position: abbrs_position_t)
            -> Vec<abbrs_replacer_t>;

        #[cxx_name = "abbrs_has_match"]
        fn abbrs_has_match_ffi(token: &CxxWString, position: abbrs_position_t) -> bool;

        #[cxx_name = "abbrs_replacement_from"]
        fn abbrs_replacement_from_ffi(
            range: SourceRange,
            text: &CxxWString,
            set_cursor_marker: &CxxWString,
            has_cursor_marker: bool,
        ) -> abbrs_replacement_t;

        #[cxx_name = "abbrs_get_set"]
        unsafe fn abbrs_get_set_ffi<'a>() -> Box<GlobalAbbrs<'a>>;
        unsafe fn add<'a>(
            self: &mut GlobalAbbrs<'_>,
            name: &CxxWString,
            key: &CxxWString,
            replacement: &CxxWString,
            position: abbrs_position_t,
            from_universal: bool,
        );
        unsafe fn erase<'a>(self: &mut GlobalAbbrs<'_>, name: &CxxWString);
    }
}

static abbrs: Lazy<Mutex<AbbreviationSet>> = Lazy::new(|| Mutex::new(Default::default()));

pub fn with_abbrs<R>(cb: impl FnOnce(&AbbreviationSet) -> R) -> R {
    let abbrs_g = abbrs.lock().unwrap();
    cb(&abbrs_g)
}

pub fn with_abbrs_mut<R>(cb: impl FnOnce(&mut AbbreviationSet) -> R) -> R {
    let mut abbrs_g = abbrs.lock().unwrap();
    cb(&mut abbrs_g)
}

pub fn abbrs_get_set() -> MutexGuard<'static, AbbreviationSet> {
    abbrs.lock().unwrap()
}

/// Controls where in the command line abbreviations may expand.
#[derive(Debug, PartialEq, Clone, Copy)]
pub enum Position {
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

#[derive(Debug)]
pub struct Abbreviation {
    // Abbreviation name. This is unique within the abbreviation set.
    // This is used as the token to match unless we have a regex.
    pub name: WString,

    /// The key (recognized token) - either a literal or a regex pattern.
    pub key: WString,

    /// If set, use this regex to recognize tokens.
    /// If unset, the key is to be interpreted literally.
    /// Note that the fish interface enforces that regexes match the entire token;
    /// we accomplish this by surrounding the regex in ^ and $.
    pub regex: Option<Regex>,

    /// Replacement string.
    pub replacement: WString,

    /// If set, the replacement is a function name.
    pub replacement_is_function: bool,

    /// Expansion position.
    pub position: Position,

    /// If set, then move the cursor to the first instance of this string in the expansion.
    pub set_cursor_marker: Option<WString>,

    /// Mark if we came from a universal variable.
    pub from_universal: bool,
}

impl Abbreviation {
    // Construct from a name, a key which matches a token, a replacement token, a position, and
    // whether we are derived from a universal variable.
    pub fn new(
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
        match &self.regex {
            Some(r) => r
                .is_match(token.as_char_slice())
                .expect("regex match should not error"),
            None => self.key == token,
        }
    }

    // \return if we expand in a given position.
    fn matches_position(&self, position: Position) -> bool {
        return self.position == Position::Anywhere || self.position == position;
    }
}

/// The result of an abbreviation expansion.
pub struct Replacer {
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
    fn from(range: SourceRange, mut text: WString, set_cursor_marker: Option<WString>) -> Self {
        let mut cursor = None;
        if let Some(set_cursor_marker) = set_cursor_marker {
            let matched = text
                .as_char_slice()
                .windows(set_cursor_marker.len())
                .position(|w| w == set_cursor_marker.as_char_slice());

            if let Some(start) = matched {
                text.replace_range(start..(start + set_cursor_marker.len()), L!(""));
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

#[derive(Default)]
pub struct AbbreviationSet {
    /// List of abbreviations, in definition order.
    abbrs: Vec<Abbreviation>,

    /// Set of used abbrevation names.
    /// This is to avoid a linear scan when adding new abbreviations.
    used_names: HashSet<WString>,
}

impl AbbreviationSet {
    /// \return the list of replacers for an input token, in priority order.
    /// The \p position is given to describe where the token was found.
    pub fn r#match(&self, token: &wstr, position: Position) -> Vec<Replacer> {
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
    pub fn has_match(&self, token: &wstr, position: Position) -> bool {
        self.abbrs.iter().any(|abbr| abbr.matches(token, position))
    }

    /// Add an abbreviation. Any abbreviation with the same name is replaced.
    pub fn add(&mut self, abbr: Abbreviation) {
        assert!(!abbr.name.is_empty(), "Invalid name");
        let inserted = self.used_names.insert(abbr.name.clone());
        if !inserted {
            // Name was already used, do a linear scan to find it.
            let index = self
                .abbrs
                .iter()
                .position(|a| a.name == abbr.name)
                .expect("Abbreviation not found though its name was present");

            self.abbrs.remove(index);
        }
        self.abbrs.push(abbr);
    }

    /// Rename an abbreviation. This asserts that the old name is used, and the new name is not; the
    /// caller should check these beforehand with has_name().
    pub fn rename(&mut self, old_name: &wstr, new_name: &wstr) {
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
    pub fn erase(&mut self, name: &wstr) -> bool {
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
        panic!("Unable to find named abbreviation");
    }

    /// \return true if we have an abbreviation with the given name.
    pub fn has_name(&self, name: &wstr) -> bool {
        self.used_names.contains(name)
    }

    /// \return a reference to the abbreviation list.
    pub fn list(&self) -> &[Abbreviation] {
        &self.abbrs
    }
}

/// \return the list of replacers for an input token, in priority order, using the global set.
/// The \p position is given to describe where the token was found.
fn abbrs_match_ffi(token: &CxxWString, position: abbrs_position_t) -> Vec<abbrs_replacer_t> {
    with_abbrs(|set| set.r#match(token.as_wstr(), position.into()))
        .into_iter()
        .map(|r| r.into())
        .collect()
}

fn abbrs_has_match_ffi(token: &CxxWString, position: abbrs_position_t) -> bool {
    with_abbrs(|set| set.has_match(token.as_wstr(), position.into()))
}

fn abbrs_list_ffi() -> Vec<abbreviation_t> {
    with_abbrs(|set| -> Vec<abbreviation_t> {
        let list = set.list();
        let mut result = Vec::with_capacity(list.len());
        for abbr in list {
            result.push(abbreviation_t {
                key: abbr.key.to_ffi(),
                replacement: abbr.replacement.to_ffi(),
                is_regex: abbr.is_regex(),
            })
        }

        result
    })
}

fn abbrs_get_set_ffi<'a>() -> Box<GlobalAbbrs<'a>> {
    let abbrs_g = abbrs.lock().unwrap();
    Box::new(GlobalAbbrs { g: abbrs_g })
}

fn abbrs_replacement_from_ffi(
    range: SourceRange,
    text: &CxxWString,
    set_cursor_marker: &CxxWString,
    has_cursor_marker: bool,
) -> abbrs_replacement_t {
    let cursor_marker = if has_cursor_marker {
        Some(set_cursor_marker.from_ffi())
    } else {
        None
    };

    let replacement = Replacement::from(range, text.from_ffi(), cursor_marker);

    abbrs_replacement_t {
        range,
        text: replacement.text.to_ffi(),
        cursor: replacement.cursor.unwrap_or_default(),
        has_cursor: replacement.cursor.is_some(),
    }
}

pub struct GlobalAbbrs<'a> {
    g: MutexGuard<'a, AbbreviationSet>,
}

impl<'a> GlobalAbbrs<'a> {
    fn add(
        &mut self,
        name: &CxxWString,
        key: &CxxWString,
        replacement: &CxxWString,
        position: abbrs_position_t,
        from_universal: bool,
    ) {
        self.g.add(Abbreviation::new(
            name.from_ffi(),
            key.from_ffi(),
            replacement.from_ffi(),
            position.into(),
            from_universal,
        ));
    }

    fn erase(&mut self, name: &CxxWString) {
        self.g.erase(name.as_wstr());
    }
}
use crate::ffi_tests::add_test;
add_test!("rename_abbrs", || {
    use crate::wchar::wstr;
    use crate::{
        abbrs::{Abbreviation, Position},
        wchar::L,
    };

    with_abbrs_mut(|abbrs_g| {
        let mut add = |name: &wstr, repl: &wstr, position: Position| {
            abbrs_g.add(Abbreviation {
                name: name.into(),
                key: name.into(),
                regex: None,
                replacement: repl.into(),
                replacement_is_function: false,
                position,
                set_cursor_marker: None,
                from_universal: false,
            })
        };
        add(L!("gc"), L!("git checkout"), Position::Command);
        add(L!("foo"), L!("bar"), Position::Command);
        add(L!("gx"), L!("git checkout"), Position::Command);
        add(L!("yin"), L!("yang"), Position::Anywhere);

        assert!(!abbrs_g.has_name(L!("gcc")));
        assert!(abbrs_g.has_name(L!("gc")));

        abbrs_g.rename(L!("gc"), L!("gcc"));
        assert!(abbrs_g.has_name(L!("gcc")));
        assert!(!abbrs_g.has_name(L!("gc")));

        assert!(!abbrs_g.erase(L!("gc")));
        assert!(abbrs_g.erase(L!("gcc")));
        assert!(!abbrs_g.erase(L!("gcc")));
    })
});
