use crate::abbrs::{self, abbrs_get_set, abbrs_match, Abbreviation};
use crate::editable_line::{apply_edit, Edit};
use crate::highlight::HighlightSpec;
use crate::reader::reader_expand_abbreviation_at_cursor;
use crate::tests::prelude::*;
use crate::wchar::prelude::*;

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
            abbrs::Position::Command,
            false,
        ));
        abbrs.add(Abbreviation::new(
            L!("foo").to_owned(),
            L!("foo").to_owned(),
            L!("bar").to_owned(),
            abbrs::Position::Command,
            false,
        ));
        abbrs.add(Abbreviation::new(
            L!("gx").to_owned(),
            L!("gx").to_owned(),
            L!("git checkout").to_owned(),
            abbrs::Position::Command,
            false,
        ));
        abbrs.add(Abbreviation::new(
            L!("yin").to_owned(),
            L!("yin").to_owned(),
            L!("yang").to_owned(),
            abbrs::Position::Anywhere,
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

    let cmd = abbrs::Position::Command;
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
