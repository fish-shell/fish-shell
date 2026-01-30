use std::{
    collections::HashSet,
    sync::{LazyLock, Mutex, MutexGuard},
};

use crate::prelude::*;

use crate::parse_constants::SourceRange;
use pcre2::utf32::Regex;

static ABBRS: LazyLock<Mutex<AbbreviationSet>> = LazyLock::new(|| Mutex::new(Default::default()));

pub fn with_abbrs<R>(cb: impl FnOnce(&AbbreviationSet) -> R) -> R {
    let abbrs_g = ABBRS.lock().unwrap();
    cb(&abbrs_g)
}

pub fn with_abbrs_mut<R>(cb: impl FnOnce(&mut AbbreviationSet) -> R) -> R {
    let mut abbrs_g = ABBRS.lock().unwrap();
    cb(&mut abbrs_g)
}

pub fn abbrs_get_set() -> MutexGuard<'static, AbbreviationSet> {
    ABBRS.lock().unwrap()
}

/// Controls where in the command line abbreviations may expand.
#[derive(Debug, PartialEq, Clone, Copy)]
pub enum Position {
    Command,  // expand in command position
    Anywhere, // expand in any token
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
    pub regex: Option<Box<Regex>>,

    /// The commands this abbr is valid for (or empty if any)
    pub commands: Vec<WString>,

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
            commands: vec![],
            replacement,
            replacement_is_function: false,
            position,
            set_cursor_marker: None,
            from_universal,
        }
    }

    // Return true if this is a regex abbreviation.
    pub fn is_regex(&self) -> bool {
        self.regex.is_some()
    }

    // Return true if we match a token at a given position.
    pub fn matches(&self, token: &wstr, position: Position, command: &wstr) -> bool {
        if !self.matches_position(position) {
            return false;
        }

        if !self.commands.is_empty() && !self.commands.contains(&command.to_owned()) {
            return false;
        }

        match &self.regex {
            Some(r) => r
                .is_match(token.as_char_slice())
                .expect("regex match should not error"),
            None => self.key == token,
        }
    }

    // Return if we expand in a given position.
    fn matches_position(&self, position: Position) -> bool {
        self.position == Position::Anywhere || self.position == position
    }
}

/// The result of an abbreviation expansion.
#[derive(Debug, Eq, PartialEq)]
pub struct Replacer {
    /// The string to use to replace the incoming token, either literal or as a function name.
    pub replacement: WString,

    /// If true, treat 'replacement' as the name of a function.
    pub is_function: bool,

    /// If set, the cursor should be moved to the first instance of this string in the expansion.
    pub set_cursor_marker: Option<WString>,
}

pub struct Replacement {
    /// The original range of the token in the command line.
    pub range: SourceRange,

    /// The string to replace with.
    pub text: WString,

    /// The new cursor location, or none to use the default.
    /// This is relative to the original range.
    pub cursor: Option<usize>,
}

impl Replacement {
    /// Construct a replacement from a replacer.
    /// The `range` is the range of the text matched by the replacer in the command line.
    /// The text is passed in separately as it may be the output of the replacer's function.
    pub fn new(range: SourceRange, mut text: WString, set_cursor_marker: Option<WString>) -> Self {
        let mut cursor = None;
        if let Some(set_cursor_marker) = set_cursor_marker {
            let matched = text
                .as_char_slice()
                .windows(set_cursor_marker.len())
                .position(|w| w == set_cursor_marker.as_char_slice());

            if let Some(start) = matched {
                text.replace_range(start..(start + set_cursor_marker.len()), L!(""));
                cursor = Some(start + range.start as usize);
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

    /// Set of used abbreviation names.
    /// This is to avoid a linear scan when adding new abbreviations.
    used_names: HashSet<WString>,
}

impl AbbreviationSet {
    /// Return the list of replacers for an input token, in priority order.
    /// The `position` is given to describe where the token was found.
    pub fn r#match(&self, token: &wstr, position: Position, cmd: &wstr) -> Vec<Replacer> {
        let mut result = vec![];

        // Later abbreviations take precedence so walk backwards.
        for abbr in self.abbrs.iter().rev() {
            if abbr.matches(token, position, cmd) {
                result.push(Replacer {
                    replacement: abbr.replacement.clone(),
                    is_function: abbr.replacement_is_function,
                    set_cursor_marker: abbr.set_cursor_marker.clone(),
                });
            }
        }
        result
    }

    /// Return whether we would have at least one replacer for a given token.
    pub fn has_match(&self, token: &wstr, position: Position, cmd: &wstr) -> bool {
        self.abbrs
            .iter()
            .any(|abbr| abbr.matches(token, position, cmd))
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
                .position(|a| a.name == abbr.name && a.commands == abbr.commands);

            if let Some(idx) = index {
                // Exact match found (name + commands), delete old abbr.
                self.abbrs.remove(idx);
            }
        }
        self.abbrs.push(abbr);
    }

    /// Rename an abbreviation. This asserts that the old name is used, and the new name is not; the
    /// caller should check these beforehand with has_name().
    pub fn rename(&mut self, old_name: &wstr, new_name: &wstr, commands: &[WString]) {
        self.abbrs
            .iter_mut()
            .find(|a| a.name == old_name && a.commands == commands)
            .expect("Abbreviation to rename was not found.")
            .name = new_name.to_owned();

        self.used_names.insert(new_name.to_owned());
        self.on_remove(old_name);
    }

    /// Erase an abbreviation by name and command restrictions.
    /// Return true if erased, false if not found.
    pub fn erase(&mut self, name: &wstr, commands: &[WString]) -> bool {
        let Some(idx) = self
            .abbrs
            .iter()
            .position(|a| a.name == name && a.commands == commands)
        else {
            return false;
        };

        self.abbrs.remove(idx);
        self.on_remove(name);

        true
    }

    /// Return true if we have an abbreviation with the given name.
    pub fn has_name(&self, name: &wstr) -> bool {
        self.used_names.contains(name)
    }

    /// Return a reference to the abbreviation list.
    pub fn list(&self) -> &[Abbreviation] {
        &self.abbrs
    }

    /// Checks if any other abbreviation still uses name, removing it from used_names if not
    fn on_remove(&mut self, name: &wstr) {
        if self.abbrs.iter().any(|a| a.name == name) {
            return;
        }

        let removed = self.used_names.remove(name);
        assert!(removed, "Name was not in used_names but should have been");
    }
}

/// Return the list of replacers for an input token, in priority order, using the global set.
/// The `position` is given to describe where the token was found.
pub fn abbrs_match(token: &wstr, position: Position, cmd: &wstr) -> Vec<Replacer> {
    with_abbrs(|set| set.r#match(token, position, cmd))
        .into_iter()
        .collect()
}

#[cfg(test)]
mod tests {
    use super::{Abbreviation, Position, abbrs_get_set, abbrs_match, with_abbrs_mut};
    use crate::editable_line::{Edit, apply_edit};
    use crate::highlight::HighlightSpec;
    use crate::prelude::*;
    use crate::reader::reader_expand_abbreviation_at_cursor;
    use crate::tests::prelude::*;

    #[test]
    #[serial]
    fn test_abbreviations() {
        let _cleanup = test_init();
        let parser = TestParser::new();
        {
            let mut abbrs = abbrs_get_set();
            abbrs.add(Abbreviation::new(
                L!("gc").to_owned(),
                L!("gc").to_owned(),
                L!("git checkout").to_owned(),
                Position::Command,
                false,
            ));
            abbrs.add(Abbreviation::new(
                L!("foo").to_owned(),
                L!("foo").to_owned(),
                L!("bar").to_owned(),
                Position::Command,
                false,
            ));
            abbrs.add(Abbreviation::new(
                L!("gx").to_owned(),
                L!("gx").to_owned(),
                L!("git checkout").to_owned(),
                Position::Command,
                false,
            ));
            abbrs.add(Abbreviation::new(
                L!("yin").to_owned(),
                L!("yin").to_owned(),
                L!("yang").to_owned(),
                Position::Anywhere,
                false,
            ));
        }

        // Helper to expand an abbreviation, enforcing we have no more than one result.
        macro_rules! abbr_expand_1 {
            ($token:expr, $position:expr) => {
                let result = abbrs_match(L!($token), $position, L!(""));
                assert_eq!(result, vec![]);
            };
            ($token:expr, $position:expr, $expected:expr) => {
                let result = abbrs_match(L!($token), $position, L!(""));
                assert_eq!(
                    result
                        .into_iter()
                        .map(|a| a.replacement)
                        .collect::<Vec<_>>(),
                    vec![L!($expected).to_owned()]
                );
            };
        }

        let cmd = Position::Command;
        abbr_expand_1!("", cmd);
        abbr_expand_1!("nothing", cmd);

        abbr_expand_1!("gc", cmd, "git checkout");
        abbr_expand_1!("foo", cmd, "bar");

        let expand_abbreviation_in_command =
            |cmdline: &wstr, cursor_pos: Option<usize>| -> Option<WString> {
                let replacement = reader_expand_abbreviation_at_cursor(
                    cmdline,
                    cursor_pos.unwrap_or(cmdline.len()),
                    &parser,
                )?;
                let mut cmdline_expanded = cmdline.to_owned();
                let mut colors = vec![HighlightSpec::new(); cmdline.len()];
                apply_edit(
                    &mut cmdline_expanded,
                    &mut colors,
                    &Edit::new(replacement.range.into(), replacement.text),
                );
                Some(cmdline_expanded)
            };

        macro_rules! validate {
            ($cmdline:expr, $cursor:expr) => {{
                let actual = expand_abbreviation_in_command(L!($cmdline), $cursor);
                assert_eq!(actual, None);
            }};
            ($cmdline:expr, $cursor:expr, $expected:expr) => {{
                let actual = expand_abbreviation_in_command(L!($cmdline), $cursor);
                assert_eq!(actual, Some(L!($expected).to_owned()));
            }};
        }

        validate!("just a command", Some(3));
        validate!("gc somebranch", Some(0), "git checkout somebranch");

        validate!(
            "gc somebranch",
            Some("gc".chars().count()),
            "git checkout somebranch"
        );

        // Space separation.
        validate!(
            "gx somebranch",
            Some("gc".chars().count()),
            "git checkout somebranch"
        );

        validate!(
            "echo hi ; gc somebranch",
            Some("echo hi ; g".chars().count()),
            "echo hi ; git checkout somebranch"
        );

        validate!(
            "echo (echo (echo (echo (gc ",
            Some("echo (echo (echo (echo (gc".chars().count()),
            "echo (echo (echo (echo (git checkout "
        );

        // If commands should be expanded.
        validate!("if gc", None, "if git checkout");

        // Others should not be.
        validate!("of gc", None);

        // Other decorations generally should be.
        validate!("command gc", None, "command git checkout");

        // yin/yang expands everywhere.
        validate!("command yin", None, "command yang");
    }

    #[test]
    #[serial]
    fn rename_abbrs() {
        let _cleanup = test_init();

        with_abbrs_mut(|abbrs_g| {
            let mut add = |name: &wstr, repl: &wstr, position: Position| {
                abbrs_g.add(Abbreviation {
                    name: name.into(),
                    key: name.into(),
                    regex: None,
                    commands: vec![],
                    replacement: repl.into(),
                    replacement_is_function: false,
                    position,
                    set_cursor_marker: None,
                    from_universal: false,
                });
            };
            add(L!("gc"), L!("git checkout"), Position::Command);
            add(L!("foo"), L!("bar"), Position::Command);
            add(L!("gx"), L!("git checkout"), Position::Command);
            add(L!("yin"), L!("yang"), Position::Anywhere);

            assert!(!abbrs_g.has_name(L!("gcc")));
            assert!(abbrs_g.has_name(L!("gc")));

            abbrs_g.rename(L!("gc"), L!("gcc"), &[]);
            assert!(abbrs_g.has_name(L!("gcc")));
            assert!(!abbrs_g.has_name(L!("gc")));

            assert!(!abbrs_g.erase(L!("gc"), &[]));
            assert!(abbrs_g.erase(L!("gcc"), &[]));
            assert!(!abbrs_g.erase(L!("gcc"), &[]));
        });
    }
}
