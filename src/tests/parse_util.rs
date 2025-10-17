use pcre2::utf32::Regex;

use crate::common::EscapeFlags;
use crate::parse_constants::{
    ERROR_BAD_VAR_CHAR1, ERROR_BRACKETED_VARIABLE_QUOTED1, ERROR_BRACKETED_VARIABLE1,
    ERROR_NO_VAR_NAME, ERROR_NOT_ARGV_AT, ERROR_NOT_ARGV_COUNT, ERROR_NOT_ARGV_STAR, ERROR_NOT_PID,
    ERROR_NOT_STATUS,
};
use crate::parse_util::{
    BOOL_AFTER_BACKGROUND_ERROR_MSG, parse_util_cmdsubst_extent, parse_util_compute_indents,
    parse_util_detect_errors, parse_util_escape_string_with_quote, parse_util_process_extent,
    parse_util_slice_length,
};
use crate::tests::prelude::*;
use crate::wchar::prelude::*;

#[test]
#[serial]
fn test_error_messages() {
    let _cleanup = test_init();
    // Given a format string, returns a list of non-empty strings separated by format specifiers. The
    // format specifiers themselves are omitted.
    fn separate_by_format_specifiers(format: &wstr) -> Vec<&wstr> {
        let format_specifier_regex = Regex::new(L!(r"%[cds]").as_char_slice()).unwrap();
        let mut result = vec![];
        let mut offset = 0;
        for mtch in format_specifier_regex.find_iter(format.as_char_slice()) {
            let mtch = mtch.unwrap();
            let component = &format[offset..mtch.start()];
            result.push(component);
            offset = mtch.end();
        }
        result.push(&format[offset..]);
        // Avoid mismatch from localized  quotes.
        for component in &mut result {
            *component = component.trim_matches('\'');
        }
        result
    }

    // Given a format string 'format', return true if the string may have been produced by that format
    // string. We do this by splitting the format string around the format specifiers, and then ensuring
    // that each of the remaining chunks is found (in order) in the string.
    fn string_matches_format(s: &wstr, format: &wstr) -> bool {
        let components = separate_by_format_specifiers(format);
        assert!(!components.is_empty());
        let mut idx = 0;
        for component in components {
            let Some(relpos) = s[idx..].find(component) else {
                return false;
            };
            idx += relpos + component.len();
            assert!(idx <= s.len());
        }
        true
    }

    macro_rules! validate {
        ($src:expr, $error_text_format:expr) => {
            let mut errors = vec![];
            let res = parse_util_detect_errors(L!($src), Some(&mut errors), false);
            let fmt = wgettext!($error_text_format);
            assert!(res.is_err());
            assert!(
                string_matches_format(&errors[0].text, fmt),
                "command '{}' is expected to match error pattern '{}' but is '{}'",
                $src,
                $error_text_format.localize(),
                &errors[0].text
            );
        };
    }

    validate!("echo $^", ERROR_BAD_VAR_CHAR1);
    validate!("echo foo${a}bar", ERROR_BRACKETED_VARIABLE1);
    validate!("echo foo\"${a}\"bar", ERROR_BRACKETED_VARIABLE_QUOTED1);
    validate!("echo foo\"${\"bar", ERROR_BAD_VAR_CHAR1);
    validate!("echo $?", ERROR_NOT_STATUS);
    validate!("echo $$", ERROR_NOT_PID);
    validate!("echo $#", ERROR_NOT_ARGV_COUNT);
    validate!("echo $@", ERROR_NOT_ARGV_AT);
    validate!("echo $*", ERROR_NOT_ARGV_STAR);
    validate!("echo $", ERROR_NO_VAR_NAME);
    validate!("echo foo\"$\"bar", ERROR_NO_VAR_NAME);
    validate!("echo \"foo\"$\"bar\"", ERROR_NO_VAR_NAME);
    validate!("echo foo $ bar", ERROR_NO_VAR_NAME);
    validate!("echo 1 & && echo 2", BOOL_AFTER_BACKGROUND_ERROR_MSG);
    validate!(
        "echo 1 && echo 2 & && echo 3",
        BOOL_AFTER_BACKGROUND_ERROR_MSG
    );
}

#[test]
fn test_parse_util_process_extent() {
    macro_rules! validate {
        ($commandline:literal, $cursor:expr, $expected_range:expr) => {
            assert_eq!(
                parse_util_process_extent(L!($commandline), $cursor, None),
                $expected_range
            );
        };
    }
    validate!("for file in (path base\necho", 22, 13..22);
    validate!("begin\n\n\nec", 10, 6..10);
    validate!("begin; echo; end", 12, 12..16);
}

#[test]
#[serial]
fn test_parse_util_cmdsubst_extent() {
    let _cleanup = test_init();
    const a: &wstr = L!("echo (echo (echo hi");
    assert_eq!(parse_util_cmdsubst_extent(a, 0), 0..a.len());
    assert_eq!(parse_util_cmdsubst_extent(a, 1), 0..a.len());
    assert_eq!(parse_util_cmdsubst_extent(a, 2), 0..a.len());
    assert_eq!(parse_util_cmdsubst_extent(a, 3), 0..a.len());
    assert_eq!(
        parse_util_cmdsubst_extent(a, 8),
        "echo (".chars().count()..a.len()
    );
    assert_eq!(
        parse_util_cmdsubst_extent(a, 17),
        "echo (echo (".chars().count()..a.len()
    );
}

#[test]
#[serial]
fn test_parse_util_slice_length() {
    let _cleanup = test_init();
    assert_eq!(parse_util_slice_length(L!("[2]")), Some(3));
    assert_eq!(parse_util_slice_length(L!("[12]")), Some(4));
    assert_eq!(parse_util_slice_length(L!("[\"foo\"]")), Some(7));
    assert_eq!(parse_util_slice_length(L!("[\"foo\"")), None);
}

#[test]
#[serial]
fn test_escape_quotes() {
    let _cleanup = test_init();
    macro_rules! validate {
        ($cmd:expr, $quote:expr, $no_tilde:expr, $expected:expr) => {
            assert_eq!(
                parse_util_escape_string_with_quote(
                    L!($cmd),
                    $quote,
                    if $no_tilde {
                        EscapeFlags::NO_TILDE
                    } else {
                        EscapeFlags::empty()
                    }
                ),
                L!($expected)
            );
        };
    }
    macro_rules! validate_no_quoted {
        ($cmd:expr, $quote:expr, $no_tilde:expr, $expected:expr) => {
            assert_eq!(
                parse_util_escape_string_with_quote(
                    L!($cmd),
                    $quote,
                    EscapeFlags::NO_QUOTED
                        | if $no_tilde {
                            EscapeFlags::NO_TILDE
                        } else {
                            EscapeFlags::empty()
                        }
                ),
                L!($expected)
            );
        };
    }

    validate!("abc~def", None, false, "'abc~def'");
    validate!("abc~def", None, true, "abc~def");
    validate!("~abc", None, false, "'~abc'");
    validate!("~abc", None, true, "~abc");

    // These are "raw string literals"
    validate_no_quoted!("abc", None, false, "abc");
    validate_no_quoted!("abc~def", None, false, "abc\\~def");
    validate_no_quoted!("abc~def", None, true, "abc~def");
    validate_no_quoted!("abc\\~def", None, false, "abc\\\\\\~def");
    validate_no_quoted!("abc\\~def", None, true, "abc\\\\~def");
    validate_no_quoted!("~abc", None, false, "\\~abc");
    validate_no_quoted!("~abc", None, true, "~abc");
    validate_no_quoted!("~abc|def", None, false, "\\~abc\\|def");
    validate_no_quoted!("|abc~def", None, false, "\\|abc\\~def");
    validate_no_quoted!("|abc~def", None, true, "\\|abc~def");
    validate_no_quoted!("foo\nbar", None, false, "foo\\nbar");

    // Note tildes are not expanded inside quotes, so no_tilde is ignored with a quote.
    validate_no_quoted!("abc", Some('\''), false, "abc");
    validate_no_quoted!("abc\\def", Some('\''), false, "abc\\\\def");
    validate_no_quoted!("abc'def", Some('\''), false, "abc\\'def");
    validate_no_quoted!("~abc'def", Some('\''), false, "~abc\\'def");
    validate_no_quoted!("~abc'def", Some('\''), true, "~abc\\'def");
    validate_no_quoted!("foo\nba'r", Some('\''), false, "foo'\\n'ba\\'r");
    validate_no_quoted!("foo\\\\bar", Some('\''), false, "foo\\\\\\\\bar");

    validate_no_quoted!("abc", Some('"'), false, "abc");
    validate_no_quoted!("abc\\def", Some('"'), false, "abc\\\\def");
    validate_no_quoted!("~abc'def", Some('"'), false, "~abc'def");
    validate_no_quoted!("~abc'def", Some('"'), true, "~abc'def");
    validate_no_quoted!("foo\nba'r", Some('"'), false, "foo\"\\n\"ba'r");
    validate_no_quoted!("foo\\\\bar", Some('"'), false, "foo\\\\\\\\bar");
}

#[test]
#[serial]
fn test_indents() {
    let _cleanup = test_init();
    // A struct which is either text or a new indent.
    struct Segment {
        // The indent to set
        indent: i32,
        text: &'static str,
    }
    fn do_validate(segments: &[Segment]) {
        // Compute the indents.
        let mut expected_indents = vec![];
        let mut text = WString::new();
        for segment in segments {
            text.push_str(segment.text);
            for _ in segment.text.chars() {
                expected_indents.push(segment.indent);
            }
        }
        let indents = parse_util_compute_indents(&text);
        assert_eq!(indents, expected_indents);
    }
    macro_rules! validate {
        ( $( $(,)? $indent:literal, $text:literal )* $(,)?  ) => {
            let segments = vec![
                $(
                    Segment{ indent: $indent, text: $text },
                )*
            ];
            do_validate(&segments);
        };
    }

    #[rustfmt::skip]
    #[allow(clippy::redundant_closure_call)]
    (|| {
        validate!(
            0, "if", 1, " foo",
            0, "\nend"
        );
        validate!(
            0, "if", 1, " foo",
            1, "\nfoo",
            0, "\nend"
        );

        validate!(
            0, "if", 1, " foo",
            1, "\nif", 2, " bar",
            1, "\nend",
            0, "\nend"
        );

        validate!(
            0, "if", 1, " foo",
            1, "\nif", 2, " bar",
            2, "\n",
            1, "\nend\n"
        );

        validate!(
            0, "if", 1, " foo",
            1, "\nif", 2, " bar",
            2, "\n"
        );

        validate!(
            0, "begin",
            1, "\nfoo",
            1, "\n"
        );

        validate!(
            0, "begin",
            1, "\n;",
            0, "end",
            0, "\nfoo", 0, "\n"
        );

        validate!(
            0, "begin",
            1, "\n;",
            0, "end",
            0, "\nfoo", 0, "\n"
        );

        validate!(
            0, "if", 1, " foo",
            1, "\nif", 2, " bar",
            2, "\nbaz",
            1, "\nend", 1, "\n"
        );

        validate!(
            0, "switch foo",
            1, "\n"
        );

        validate!(
            0, "switch foo",
            1, "\ncase bar",
            1, "\ncase baz",
            2, "\nquux",
            2, "\nquux"
        );

        validate!(
            0,
            "switch foo",
            1,
            "\ncas" // parse error indentation handling
        );

        validate!(
            0, "while",
            1, " false",
            1, "\n# comment", // comment indentation handling
            1, "\ncommand",
            1, "\n# comment 2"
        );

        validate!(
            0, "begin",
            1, "\n", // "begin" is special because this newline belongs to the block header
            1, "\n"
        );

        // Continuation lines.
        validate!(
            0, "echo 'continuation line' \\",
            1, "\ncont",
            0, "\n"
        );
        validate!(
            0, "echo 'empty continuation line' \\",
            1, "\n"
        );
        validate!(
            0, "begin # continuation line in block",
            1, "\necho \\",
            2, "\ncont"
        );
        validate!(
            0, "begin # empty continuation line in block",
            1, "\necho \\",
            2, "\n",
            0, "\nend"
        );
        validate!(
            0, "echo 'multiple continuation lines' \\",
            1, "\nline1 \\",
            1, "\n# comment",
            1, "\n# more comment",
            1, "\nline2 \\",
            1, "\n"
        );
        validate!(
            0, "echo # inline comment ending in \\",
            0, "\nline"
        );
        validate!(
            0, "# line comment ending in \\",
            0, "\nline"
        );
        validate!(
            0, "echo 'multiple empty continuation lines' \\",
            1, "\n\\",
            1, "\n",
            0, "\n"
        );
        validate!(
            0, "echo 'multiple statements with continuation lines' \\",
            1, "\nline 1",
            0, "\necho \\",
            1, "\n"
        );
        // This is an edge case, probably okay to change the behavior here.
        validate!(
            0, "begin",
            1, " \\",
            2, "\necho 'continuation line in block header' \\",
            2, "\n",
            1, "\n",
            0, "\nend"
        );
        validate!(
            0, "if", 1,  " true",
            1, "\n    begin",
            2,  "\n        echo",
            1,  "\n    end",
            0,  "\nend",
        );

        // Quotes and command substitutions.
        validate!(
            0, "if", 1, " foo \"",
            0, "\nquoted",
        );
        validate!(
            0, "if", 1, " foo \"",
            0, "\n",
        );
        validate!(
            0, "echo (",
            1, "\n", // )
        );
        validate!(
            0, "echo \"$(",
            1, "\n" // )
        );
        validate!(
            0, "echo (", // )
            1, "\necho \"",
            0, "\n"
        );
        validate!(
            0, "echo (", // )
            1, "\necho (", // )
            2, "\necho"
        );
        validate!(
            0, "if", 1, " true",
            1, "\n    echo \"line1",
            0, "\nline2 ", 1, "$(",
            2, "\n    echo line3",
            0, "\n) line4",
            0, "\nline5\"",
        );
        validate!(
            0, r#"echo "$()"'"#,
            0, "\n"
        );
        validate!(
            0, r#"""#,
            0, "\n",
            0, r#"$()"$() ""#
        );
    })();
}
