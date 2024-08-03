use crate::abbrs::Abbreviation;
use crate::abbrs::{self};
use crate::abbrs::{with_abbrs, with_abbrs_mut};
use crate::complete::{CompletionList, CompletionReceiver};
use crate::env::{EnvMode, EnvStackSetResult};
use crate::expand::{expand_to_receiver, ExpandResultCode};
use crate::operation_context::{no_cancel, EXPANSION_LIMIT_DEFAULT};
use crate::parse_constants::ParseErrorList;
use crate::tests::prelude::*;
use crate::wildcard::ANY_STRING;
use crate::{
    expand::{expand_string, ExpandFlags},
    operation_context::OperationContext,
    wchar::prelude::*,
};
use std::collections::hash_map::RandomState;
use std::collections::HashSet;

fn expand_test_impl(
    input: &wstr,
    flags: ExpandFlags,
    expected: Vec<WString>,
    error_message: Option<&str>,
) {
    let parser = TestParser::new();
    let mut output = CompletionList::new();
    let mut errors = ParseErrorList::new();
    let pwd = PwdEnvironment::default();
    let ctx = OperationContext::test_only_foreground(&parser, &pwd, Box::new(no_cancel));

    if expand_string(
        input.to_owned(),
        &mut output,
        flags,
        &ctx,
        Some(&mut errors),
    ) == ExpandResultCode::error
    {
        assert_ne!(
            errors,
            vec![],
            "Bug: Parse error reported but no error text found."
        );
        panic!(
            "{}",
            errors[0].describe(input, ctx.parser().is_interactive())
        );
    }

    let expected_set: HashSet<WString, RandomState> = HashSet::from_iter(expected);
    let output_set = HashSet::from_iter(output.into_iter().map(|c| c.completion));
    assert_eq!(
        expected_set,
        output_set,
        "{}",
        error_message.unwrap_or("expand mismatch")
    );
}

// Test globbing and other parameter expansion.
#[test]
#[serial]
fn test_expand() {
    let _cleanup = test_init();
    let parser = TestParser::new();
    /// Perform parameter expansion and test if the output equals the zero-terminated parameter list /// supplied.
    ///
    /// \param in the string to expand
    /// \param flags the flags to send to expand_string
    /// \param ... A zero-terminated parameter list of values to test.
    /// After the zero terminator comes one more arg, a string, which is the error
    /// message to print if the test fails.
    macro_rules! expand_test {
        ($input:expr, $flags:expr, ( $($expected:expr),* $(,)? )) => {
            expand_test_impl(L!($input), $flags, vec![$( $expected.into(), )*], None)
        };
        ($input:expr, $flags:expr, ( $($expected:expr),* $(,)? ), $error:literal) => {
            expand_test_impl(L!($input), $flags, vec![$( $expected.into(), )*], Some($error))
        };
        ($input:expr, $flags:expr, $expected:expr) => {
            expand_test_impl(L!($input), $flags, vec![ $expected.into() ], None)
        };
        ($input:expr, $flags:expr, $expected:expr, $error:literal) => {
            expand_test_impl(L!($input), $flags, vec![ $expected.into() ], Some($error))
        };
    }

    // Testing parameter expansion
    let noflags = ExpandFlags::default();

    expand_test!("foo", noflags, "foo", "Strings do not expand to themselves");

    expand_test!(
        "a{b,c,d}e",
        noflags,
        ("abe", "ace", "ade"),
        "Bracket expansion is broken"
    );
    expand_test!(
        "a*",
        ExpandFlags::SKIP_WILDCARDS,
        "a*",
        "Cannot skip wildcard expansion"
    );
    expand_test!(
        "/bin/l\\0",
        ExpandFlags::FOR_COMPLETIONS,
        (),
        "Failed to handle null escape in expansion"
    );
    expand_test!(
        "foo\\$bar",
        ExpandFlags::SKIP_VARIABLES,
        "foo$bar",
        "Failed to handle dollar sign in variable-skipping expansion"
    );

    // bb
    //    x
    // bar
    // baz
    //    xxx
    //    yyy
    // bax
    //    xxx
    // lol
    //    nub
    //       q
    // .foo
    // aaa
    // aaa2
    //    x
    std::fs::create_dir_all("test/fish_expand_test/").unwrap();
    std::fs::create_dir_all("test/fish_expand_test/bb/").unwrap();
    std::fs::create_dir_all("test/fish_expand_test/baz/").unwrap();
    std::fs::create_dir_all("test/fish_expand_test/bax/").unwrap();
    std::fs::create_dir_all("test/fish_expand_test/lol/nub/").unwrap();
    std::fs::create_dir_all("test/fish_expand_test/aaa/").unwrap();
    std::fs::create_dir_all("test/fish_expand_test/aaa2/").unwrap();
    std::fs::write("test/fish_expand_test/.foo", []).unwrap();
    std::fs::write("test/fish_expand_test/bb/x", []).unwrap();
    std::fs::write("test/fish_expand_test/bar", []).unwrap();
    std::fs::write("test/fish_expand_test/bax/xxx", []).unwrap();
    std::fs::write("test/fish_expand_test/baz/xxx", []).unwrap();
    std::fs::write("test/fish_expand_test/baz/yyy", []).unwrap();
    std::fs::write("test/fish_expand_test/lol/nub/q", []).unwrap();
    std::fs::write("test/fish_expand_test/aaa2/x", []).unwrap();

    // This is checking that .* does NOT match . and ..
    // (https://github.com/fish-shell/fish-shell/issues/270). But it does have to match literal
    // components (e.g. "./*" has to match the same as "*".
    expand_test!(
        "test/fish_expand_test/.*",
        noflags,
        "test/fish_expand_test/.foo",
        "Expansion not correctly handling dotfiles"
    );

    expand_test!(
        "test/fish_expand_test/./.*",
        noflags,
        "test/fish_expand_test/./.foo",
        "Expansion not correctly handling literal path components in dotfiles"
    );

    expand_test!(
        "test/fish_expand_test/*/xxx",
        noflags,
        (
            "test/fish_expand_test/bax/xxx",
            "test/fish_expand_test/baz/xxx"
        ),
        "Glob did the wrong thing 1"
    );

    expand_test!(
        "test/fish_expand_test/*z/xxx",
        noflags,
        "test/fish_expand_test/baz/xxx",
        "Glob did the wrong thing 2"
    );

    expand_test!(
        "test/fish_expand_test/**z/xxx",
        noflags,
        "test/fish_expand_test/baz/xxx",
        "Glob did the wrong thing 3"
    );

    expand_test!(
        "test/fish_expand_test////baz/xxx",
        noflags,
        "test/fish_expand_test////baz/xxx",
        "Glob did the wrong thing 3"
    );

    expand_test!(
        "test/fish_expand_test/b**",
        noflags,
        (
            "test/fish_expand_test/bb",
            "test/fish_expand_test/bb/x",
            "test/fish_expand_test/bar",
            "test/fish_expand_test/bax",
            "test/fish_expand_test/bax/xxx",
            "test/fish_expand_test/baz",
            "test/fish_expand_test/baz/xxx",
            "test/fish_expand_test/baz/yyy"
        ),
        "Glob did the wrong thing 4"
    );

    // A trailing slash should only produce directories.
    expand_test!(
        "test/fish_expand_test/b*/",
        noflags,
        (
            "test/fish_expand_test/bb/",
            "test/fish_expand_test/baz/",
            "test/fish_expand_test/bax/"
        ),
        "Glob did the wrong thing 5"
    );

    expand_test!(
        "test/fish_expand_test/b**/",
        noflags,
        (
            "test/fish_expand_test/bb/",
            "test/fish_expand_test/baz/",
            "test/fish_expand_test/bax/"
        ),
        "Glob did the wrong thing 6"
    );

    expand_test!(
        "test/fish_expand_test/**/q",
        noflags,
        "test/fish_expand_test/lol/nub/q",
        "Glob did the wrong thing 7"
    );

    expand_test!(
        "test/fish_expand_test/BA",
        ExpandFlags::FOR_COMPLETIONS,
        (
            "test/fish_expand_test/bar",
            "test/fish_expand_test/bax/",
            "test/fish_expand_test/baz/"
        ),
        "Case insensitive test did the wrong thing"
    );

    expand_test!(
        "test/fish_expand_test/BA",
        ExpandFlags::FOR_COMPLETIONS,
        (
            "test/fish_expand_test/bar",
            "test/fish_expand_test/bax/",
            "test/fish_expand_test/baz/"
        ),
        "Case insensitive test did the wrong thing"
    );

    expand_test!(
        "test/fish_expand_test/bb/yyy",
        ExpandFlags::FOR_COMPLETIONS,
        (), /* nothing! */
        "Wrong fuzzy matching 1"
    );

    expand_test!(
        "test/fish_expand_test/bb/x",
        ExpandFlags::FOR_COMPLETIONS | ExpandFlags::FUZZY_MATCH,
        "",
        // we just expect the empty string since this is an exact match
        "Wrong fuzzy matching 2"
    );

    // Some vswprintfs refuse to append ANY_STRING in a format specifiers, so don't use
    // format_string here.
    let fuzzy_comp = ExpandFlags::FOR_COMPLETIONS | ExpandFlags::FUZZY_MATCH;
    let any_str_str = ANY_STRING.to_string();
    expand_test!(
        "test/fish_expand_test/b/xx*",
        fuzzy_comp,
        (
            (String::from("test/fish_expand_test/bax/xx") + &any_str_str),
            (String::from("test/fish_expand_test/baz/xx") + &any_str_str)
        ),
        "Wrong fuzzy matching 3"
    );

    expand_test!(
        "test/fish_expand_test/b/yyy",
        fuzzy_comp,
        "test/fish_expand_test/baz/yyy",
        "Wrong fuzzy matching 4"
    );

    expand_test!(
        "test/fish_expand_test/aa/x",
        fuzzy_comp,
        "test/fish_expand_test/aaa2/x",
        "Wrong fuzzy matching 5"
    );

    expand_test!(
        "test/fish_expand_test/aaa/x",
        fuzzy_comp,
        (),
        "Wrong fuzzy matching 6 - shouldn't remove valid directory names (#3211)"
    );

    // Dotfiles
    expand_test!(
        "test/fish_expand_test/.*",
        noflags,
        "test/fish_expand_test/.foo",
        ""
    );

    // Literal path components in dotfiles.
    expand_test!(
        "test/fish_expand_test/./.*",
        noflags,
        "test/fish_expand_test/./.foo",
        ""
    );

    parser.pushd("test/fish_expand_test");

    expand_test!(
        "b/xx",
        fuzzy_comp,
        ("bax/xxx", "baz/xxx"),
        "Wrong fuzzy matching 5"
    );

    // multiple slashes with fuzzy matching - #3185
    expand_test!("l///n", fuzzy_comp, "lol///nub/", "Wrong fuzzy matching 6");

    parser.popd();
}

#[test]
#[serial]
fn test_expand_overflow() {
    let _cleanup = test_init();
    // Testing overflowing expansions
    // Ensure that we have sane limits on number of expansions - see #7497.

    // Make a list of 64 elements, then expand it cartesian-style 64 times.
    // This is far too large to expand.
    let vals: Vec<WString> = (1..=64).map(|i| i.to_wstring()).collect();
    let expansion = WString::from_str(&str::repeat("$bigvar", 64));

    let parser = TestParser::new();
    parser.vars().push(true);
    let set = parser.vars().set(L!("bigvar"), EnvMode::LOCAL, vals);
    assert_eq!(set, EnvStackSetResult::Ok);

    let mut errors = ParseErrorList::new();
    let ctx = OperationContext::foreground(&parser, Box::new(no_cancel), EXPANSION_LIMIT_DEFAULT);

    // We accept only 1024 completions.
    let mut output = CompletionReceiver::new(1024);

    let res = expand_to_receiver(
        expansion,
        &mut output,
        ExpandFlags::default(),
        &ctx,
        Some(&mut errors),
    );
    assert_ne!(errors, vec![]);
    assert_eq!(res, ExpandResultCode::error);

    parser.vars().pop();
}

#[test]
#[serial]
fn test_abbreviations() {
    let _cleanup = test_init();
    // Testing abbreviations

    with_abbrs_mut(|abbrset| {
        abbrset.add(Abbreviation::new(
            L!("gc").to_owned(),
            L!("gc").to_owned(),
            L!("git checkout").to_owned(),
            abbrs::Position::Command,
            false,
        ));
        abbrset.add(Abbreviation::new(
            L!("foo").to_owned(),
            L!("foo").to_owned(),
            L!("bar").to_owned(),
            abbrs::Position::Command,
            false,
        ));
        abbrset.add(Abbreviation::new(
            L!("gx").to_owned(),
            L!("gx").to_owned(),
            L!("git checkout").to_owned(),
            abbrs::Position::Command,
            false,
        ));
        abbrset.add(Abbreviation::new(
            L!("yin").to_owned(),
            L!("yin").to_owned(),
            L!("yang").to_owned(),
            abbrs::Position::Anywhere,
            false,
        ));
    });

    // Helper to expand an abbreviation, enforcing we have no more than one result.
    let abbr_expand_1 = |token, pos| -> Option<WString> {
        let result = with_abbrs(|abbrset| abbrset.r#match(token, pos, L!("")));
        if result.is_empty() {
            return None;
        }
        assert_eq!(
            &result[1..],
            &[],
            "abbreviation expansion for {token} returned more than 1 result"
        );
        Some(result.into_iter().next().unwrap().replacement)
    };

    let cmd = abbrs::Position::Command;
    assert!(
        abbr_expand_1(L!(""), cmd).is_none(),
        "Unexpected success with empty abbreviation"
    );
    assert!(
        abbr_expand_1(L!("nothing"), cmd).is_none(),
        "Unexpected success with missing abbreviation"
    );

    assert_eq!(
        abbr_expand_1(L!("gc"), cmd),
        Some(L!("git checkout").into())
    );

    assert_eq!(abbr_expand_1(L!("foo"), cmd), Some(L!("bar").into()));
}
