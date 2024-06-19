use crate::common::ScopeGuard;
use crate::env::EnvMode;
use crate::future_feature_flags::{self, FeatureFlag};
use crate::tests::prelude::*;
use crate::wchar::prelude::*;
use crate::{
    env::EnvStack,
    highlight::{highlight_shell, is_potential_path, HighlightRole, HighlightSpec, PathFlags},
    operation_context::{OperationContext, EXPANSION_LIMIT_BACKGROUND, EXPANSION_LIMIT_DEFAULT},
};
use libc::PATH_MAX;

// Helper to return a string whose length greatly exceeds PATH_MAX.
fn get_overlong_path() -> String {
    let path_max = usize::try_from(PATH_MAX).unwrap();
    let mut longpath = String::with_capacity(path_max * 2 + 10);
    while longpath.len() <= path_max * 2 {
        longpath += "/overlong";
    }
    longpath
}

#[test]
#[serial]
fn test_is_potential_path() {
    let _cleanup = test_init();
    // Directories
    std::fs::create_dir_all("test/is_potential_path_test/alpha/").unwrap();
    std::fs::create_dir_all("test/is_potential_path_test/beta/").unwrap();

    // Files
    std::fs::write("test/is_potential_path_test/aardvark", []).unwrap();
    std::fs::write("test/is_potential_path_test/gamma", []).unwrap();

    let wd = L!("test/is_potential_path_test/").to_owned();
    let wds = [L!(".").to_owned(), wd];

    let vars = EnvStack::new();
    let ctx = OperationContext::background(&vars, EXPANSION_LIMIT_DEFAULT);

    assert!(is_potential_path(
        L!("al"),
        true,
        &wds[..],
        &ctx,
        PathFlags::PATH_REQUIRE_DIR
    ));

    assert!(is_potential_path(
        L!("alpha/"),
        true,
        &wds[..],
        &ctx,
        PathFlags::PATH_REQUIRE_DIR
    ));
    assert!(is_potential_path(
        L!("aard"),
        true,
        &wds[..],
        &ctx,
        PathFlags::empty()
    ));
    assert!(!is_potential_path(
        L!("aard"),
        false,
        &wds[..],
        &ctx,
        PathFlags::empty()
    ));
    assert!(!is_potential_path(
        L!("alp/"),
        true,
        &wds[..],
        &ctx,
        PathFlags::PATH_REQUIRE_DIR | PathFlags::PATH_FOR_CD
    ));

    assert!(!is_potential_path(
        L!("balpha/"),
        true,
        &wds[..],
        &ctx,
        PathFlags::PATH_REQUIRE_DIR
    ));
    assert!(!is_potential_path(
        L!("aard"),
        true,
        &wds[..],
        &ctx,
        PathFlags::PATH_REQUIRE_DIR
    ));
    assert!(!is_potential_path(
        L!("aarde"),
        true,
        &wds[..],
        &ctx,
        PathFlags::PATH_REQUIRE_DIR
    ));
    assert!(!is_potential_path(
        L!("aarde"),
        true,
        &wds[..],
        &ctx,
        PathFlags::empty()
    ));

    assert!(is_potential_path(
        L!("test/is_potential_path_test/aardvark"),
        true,
        &wds[..],
        &ctx,
        PathFlags::empty()
    ));
    assert!(is_potential_path(
        L!("test/is_potential_path_test/al"),
        true,
        &wds[..],
        &ctx,
        PathFlags::PATH_REQUIRE_DIR
    ));
    assert!(is_potential_path(
        L!("test/is_potential_path_test/aardv"),
        true,
        &wds[..],
        &ctx,
        PathFlags::empty()
    ));

    assert!(!is_potential_path(
        L!("test/is_potential_path_test/aardvark"),
        true,
        &wds[..],
        &ctx,
        PathFlags::PATH_REQUIRE_DIR
    ));
    assert!(!is_potential_path(
        L!("test/is_potential_path_test/al/"),
        true,
        &wds[..],
        &ctx,
        PathFlags::empty()
    ));
    assert!(!is_potential_path(
        L!("test/is_potential_path_test/ar"),
        true,
        &wds[..],
        &ctx,
        PathFlags::empty()
    ));
    assert!(is_potential_path(
        L!("/usr"),
        true,
        &wds[..],
        &ctx,
        PathFlags::PATH_REQUIRE_DIR
    ));
}

#[test]
#[serial]
fn test_highlighting() {
    let _cleanup = test_init();
    let parser = TestParser::new();
    // Testing syntax highlighting
    parser.pushd("test/fish_highlight_test/");
    let _popd = ScopeGuard::new((), |_| parser.popd());
    std::fs::create_dir_all("dir").unwrap();
    std::fs::create_dir_all("cdpath-entry/dir-in-cdpath").unwrap();
    std::fs::write("foo", []).unwrap();
    std::fs::write("bar", []).unwrap();

    // Here are the components of our source and the colors we expect those to be.
    #[derive(Debug)]
    struct HighlightComponent<'a> {
        text: &'a str,
        color: HighlightSpec,
        nospace: bool,
    }

    macro_rules! component {
        ( ( $text:expr, $color:expr) ) => {
            HighlightComponent {
                text: $text,
                color: $color,
                nospace: false,
            }
        };
        ( ( $text:literal, $color:expr, ns ) ) => {
            HighlightComponent {
                text: $text,
                color: $color,
                nospace: true,
            }
        };
    }

    macro_rules! validate {
        ( $($comp:tt),* $(,)? ) => {
            let components = [
                $(
                    component!($comp),
                )*
            ];
            let vars = parser.vars();
            // Generate the text.
            let mut text = WString::new();
            let mut expected_colors = vec![];
            for comp in &components {
                if !text.is_empty() && !comp.nospace {
                    text.push(' ');
                    expected_colors.push(HighlightSpec::new());
                }
                text.push_str(comp.text);
                expected_colors.resize(text.len(), comp.color);
            }
            assert_eq!(text.len(), expected_colors.len());

            let mut colors = vec![];
            highlight_shell(
                &text,
                &mut colors,
                &OperationContext::background(vars, EXPANSION_LIMIT_BACKGROUND),
                true, /* io_ok */
                Some(text.len()),
            );
            assert_eq!(colors.len(), expected_colors.len());

            for (i, c) in text.chars().enumerate() {
                // Hackish space handling. We don't care about the colors in spaces.
                if c == ' ' {
                    continue;
                }

                assert_eq!(colors[i], expected_colors[i], "Failed at position {i}, char {c}");
            }
        };
    }

    let mut param_valid_path = HighlightSpec::with_fg(HighlightRole::param);
    param_valid_path.valid_path = true;

    let saved_flag = future_feature_flags::test(FeatureFlag::ampersand_nobg_in_token);
    future_feature_flags::set(FeatureFlag::ampersand_nobg_in_token, true);
    let _restore_saved_flag = ScopeGuard::new((), |_| {
        future_feature_flags::set(FeatureFlag::ampersand_nobg_in_token, saved_flag);
    });

    let fg = HighlightSpec::with_fg;

    // Verify variables and wildcards in commands using /bin/cat.
    let vars = parser.vars();
    vars.set_one(
        L!("CDPATH"),
        EnvMode::LOCAL,
        L!("./cdpath-entry").to_owned(),
    );

    vars.set_one(
        L!("VARIABLE_IN_COMMAND"),
        EnvMode::LOCAL,
        L!("a").to_owned(),
    );
    vars.set_one(
        L!("VARIABLE_IN_COMMAND2"),
        EnvMode::LOCAL,
        L!("at").to_owned(),
    );

    let _cleanup = ScopeGuard::new((), |_| {
        vars.remove(L!("VARIABLE_IN_COMMAND"), EnvMode::default());
        vars.remove(L!("VARIABLE_IN_COMMAND2"), EnvMode::default());
    });

    validate!(
        ("echo", fg(HighlightRole::command)),
        ("./foo", param_valid_path),
        ("&", fg(HighlightRole::statement_terminator)),
    );

    validate!(
        ("command", fg(HighlightRole::keyword)),
        ("echo", fg(HighlightRole::command)),
        ("abc", fg(HighlightRole::param)),
        ("foo", param_valid_path),
        ("&", fg(HighlightRole::statement_terminator)),
    );

    validate!(
        ("echo", fg(HighlightRole::command)),
        ("foo&bar", fg(HighlightRole::param)),
        ("foo", fg(HighlightRole::param), ns),
        ("&", fg(HighlightRole::statement_terminator)),
        ("echo", fg(HighlightRole::command)),
        ("&>", fg(HighlightRole::redirection)),
    );

    validate!(
        ("if command", fg(HighlightRole::keyword)),
        ("ls", fg(HighlightRole::command)),
        ("; ", fg(HighlightRole::statement_terminator)),
        ("echo", fg(HighlightRole::command)),
        ("abc", fg(HighlightRole::param)),
        ("; ", fg(HighlightRole::statement_terminator)),
        ("/bin/definitely_not_a_command", fg(HighlightRole::error)),
        ("; ", fg(HighlightRole::statement_terminator)),
        ("end", fg(HighlightRole::keyword)),
    );

    // Verify that cd shows errors for non-directories.
    validate!(
        ("cd", fg(HighlightRole::command)),
        ("dir", param_valid_path),
    );

    validate!(
        ("cd", fg(HighlightRole::command)),
        ("foo", fg(HighlightRole::error)),
    );

    validate!(
        ("cd", fg(HighlightRole::command)),
        ("--help", fg(HighlightRole::option)),
        ("-h", fg(HighlightRole::option)),
        ("definitely_not_a_directory", fg(HighlightRole::error)),
    );

    validate!(
        ("cd", fg(HighlightRole::command)),
        ("dir-in-cdpath", param_valid_path),
    );

    // Command substitutions.
    validate!(
        ("echo", fg(HighlightRole::command)),
        ("param1", fg(HighlightRole::param)),
        ("-l", fg(HighlightRole::option)),
        ("--", fg(HighlightRole::option)),
        ("-l", fg(HighlightRole::param)),
        ("(", fg(HighlightRole::operat)),
        ("ls", fg(HighlightRole::command)),
        ("-l", fg(HighlightRole::option)),
        ("--", fg(HighlightRole::option)),
        ("-l", fg(HighlightRole::param)),
        ("param2", fg(HighlightRole::param)),
        (")", fg(HighlightRole::operat)),
        ("|", fg(HighlightRole::statement_terminator)),
        ("cat", fg(HighlightRole::command)),
    );
    validate!(
        ("true", fg(HighlightRole::command)),
        ("$(", fg(HighlightRole::operat)),
        ("true", fg(HighlightRole::command)),
        (")", fg(HighlightRole::operat)),
    );
    validate!(
        ("true", fg(HighlightRole::command)),
        ("\"before", fg(HighlightRole::quote)),
        ("$(", fg(HighlightRole::operat)),
        ("true", fg(HighlightRole::command)),
        ("param1", fg(HighlightRole::param)),
        (")", fg(HighlightRole::operat)),
        ("after\"", fg(HighlightRole::quote)),
        ("param2", fg(HighlightRole::param)),
    );
    validate!(
        ("true", fg(HighlightRole::command)),
        ("\"", fg(HighlightRole::error)),
        ("unclosed quote", fg(HighlightRole::quote)),
        ("$(", fg(HighlightRole::operat)),
        ("true", fg(HighlightRole::command)),
        (")", fg(HighlightRole::operat)),
    );

    // Redirections substitutions.
    validate!(
        ("echo", fg(HighlightRole::command)),
        ("param1", fg(HighlightRole::param)),
        // Input redirection.
        ("<", fg(HighlightRole::redirection)),
        ("/bin/echo", fg(HighlightRole::redirection)),
        // Output redirection to a valid fd.
        ("1>&2", fg(HighlightRole::redirection)),
        // Output redirection to an invalid fd.
        ("2>&", fg(HighlightRole::redirection)),
        ("LO", fg(HighlightRole::error)),
        // Just a param, not a redirection.
        ("test/blah", fg(HighlightRole::param)),
        // Input redirection from directory.
        ("<", fg(HighlightRole::redirection)),
        ("test/", fg(HighlightRole::error)),
        // Output redirection to an invalid path.
        ("3>", fg(HighlightRole::redirection)),
        ("/not/a/valid/path/nope", fg(HighlightRole::error)),
        // Output redirection to directory.
        ("3>", fg(HighlightRole::redirection)),
        ("test/nope/", fg(HighlightRole::error)),
        // Redirections to overflow fd.
        ("99999999999999999999>&2", fg(HighlightRole::error)),
        ("2>&", fg(HighlightRole::redirection)),
        ("99999999999999999999", fg(HighlightRole::error)),
        // Output redirection containing a command substitution.
        ("4>", fg(HighlightRole::redirection)),
        ("(", fg(HighlightRole::operat)),
        ("echo", fg(HighlightRole::command)),
        ("test/somewhere", fg(HighlightRole::param)),
        (")", fg(HighlightRole::operat)),
        // Just another param.
        ("param2", fg(HighlightRole::param)),
    );

    validate!(
        ("for", fg(HighlightRole::keyword)),
        ("x", fg(HighlightRole::param)),
        ("in", fg(HighlightRole::keyword)),
        ("set-by-for-1", fg(HighlightRole::param)),
        ("set-by-for-2", fg(HighlightRole::param)),
        (";", fg(HighlightRole::statement_terminator)),
        ("echo", fg(HighlightRole::command)),
        (">", fg(HighlightRole::redirection)),
        ("$x", fg(HighlightRole::redirection)),
        (";", fg(HighlightRole::statement_terminator)),
        ("end", fg(HighlightRole::keyword)),
    );

    validate!(
        ("set", fg(HighlightRole::command)),
        ("x", fg(HighlightRole::param)),
        ("set-by-set", fg(HighlightRole::param)),
        (";", fg(HighlightRole::statement_terminator)),
        ("echo", fg(HighlightRole::command)),
        (">", fg(HighlightRole::redirection)),
        ("$x", fg(HighlightRole::redirection)),
        ("2>", fg(HighlightRole::redirection)),
        ("$totally_not_x", fg(HighlightRole::error)),
        ("<", fg(HighlightRole::redirection)),
        ("$x_but_its_an_impostor", fg(HighlightRole::error)),
    );

    validate!(
        ("x", fg(HighlightRole::param), ns),
        ("=", fg(HighlightRole::operat), ns),
        ("set-by-variable-override", fg(HighlightRole::param), ns),
        ("echo", fg(HighlightRole::command)),
        (">", fg(HighlightRole::redirection)),
        ("$x", fg(HighlightRole::redirection)),
    );

    validate!(
        ("end", fg(HighlightRole::error)),
        (";", fg(HighlightRole::statement_terminator)),
        ("if", fg(HighlightRole::keyword)),
        ("end", fg(HighlightRole::error)),
    );

    validate!(
        ("echo", fg(HighlightRole::command)),
        ("'", fg(HighlightRole::error)),
        ("single_quote", fg(HighlightRole::quote)),
        ("$stuff", fg(HighlightRole::quote)),
    );

    validate!(
        ("echo", fg(HighlightRole::command)),
        ("\"", fg(HighlightRole::error)),
        ("double_quote", fg(HighlightRole::quote)),
        ("$stuff", fg(HighlightRole::operat)),
    );

    validate!(
        ("echo", fg(HighlightRole::command)),
        ("$foo", fg(HighlightRole::operat)),
        ("\"", fg(HighlightRole::quote)),
        ("$bar", fg(HighlightRole::operat)),
        ("\"", fg(HighlightRole::quote)),
        ("$baz[", fg(HighlightRole::operat)),
        ("1 2..3", fg(HighlightRole::param)),
        ("]", fg(HighlightRole::operat)),
    );

    validate!(
        ("for", fg(HighlightRole::keyword)),
        ("i", fg(HighlightRole::param)),
        ("in", fg(HighlightRole::keyword)),
        ("1 2 3", fg(HighlightRole::param)),
        (";", fg(HighlightRole::statement_terminator)),
        ("end", fg(HighlightRole::keyword)),
    );

    validate!(
        ("echo", fg(HighlightRole::command)),
        ("$$foo[", fg(HighlightRole::operat)),
        ("1", fg(HighlightRole::param)),
        ("][", fg(HighlightRole::operat)),
        ("2", fg(HighlightRole::param)),
        ("]", fg(HighlightRole::operat)),
        ("[3]", fg(HighlightRole::param)), // two dollar signs, so last one is not an expansion
    );

    validate!(
        ("cat", fg(HighlightRole::command)),
        ("/dev/null", param_valid_path),
        ("|", fg(HighlightRole::statement_terminator)),
        // This is bogus, but we used to use "less" here and that doesn't have to be installed.
        ("cat", fg(HighlightRole::command)),
        ("2>", fg(HighlightRole::redirection)),
    );

    // Highlight path-prefixes only at the cursor.
    validate!(
        ("cat", fg(HighlightRole::command)),
        ("/dev/nu", fg(HighlightRole::param)),
        ("/dev/nu", param_valid_path),
    );

    validate!(
        ("if", fg(HighlightRole::keyword)),
        ("true", fg(HighlightRole::command)),
        ("&&", fg(HighlightRole::operat)),
        ("false", fg(HighlightRole::command)),
        (";", fg(HighlightRole::statement_terminator)),
        ("or", fg(HighlightRole::operat)),
        ("false", fg(HighlightRole::command)),
        ("||", fg(HighlightRole::operat)),
        ("true", fg(HighlightRole::command)),
        (";", fg(HighlightRole::statement_terminator)),
        ("and", fg(HighlightRole::operat)),
        ("not", fg(HighlightRole::operat)),
        ("!", fg(HighlightRole::operat)),
        ("true", fg(HighlightRole::command)),
        (";", fg(HighlightRole::statement_terminator)),
        ("end", fg(HighlightRole::keyword)),
    );

    validate!(
        ("echo", fg(HighlightRole::command)),
        ("%self", fg(HighlightRole::operat)),
        ("not%self", fg(HighlightRole::param)),
        ("self%not", fg(HighlightRole::param)),
    );

    validate!(
        ("false", fg(HighlightRole::command)),
        ("&|", fg(HighlightRole::statement_terminator)),
        ("true", fg(HighlightRole::command)),
    );

    validate!(
        ("HOME", fg(HighlightRole::param)),
        ("=", fg(HighlightRole::operat), ns),
        (".", fg(HighlightRole::param), ns),
        ("VAR1", fg(HighlightRole::param)),
        ("=", fg(HighlightRole::operat), ns),
        ("VAL1", fg(HighlightRole::param), ns),
        ("VAR", fg(HighlightRole::param)),
        ("=", fg(HighlightRole::operat), ns),
        ("false", fg(HighlightRole::command)),
        ("|&", fg(HighlightRole::error)),
        ("true", fg(HighlightRole::command)),
        ("stuff", fg(HighlightRole::param)),
    );

    validate!(
        ("echo", fg(HighlightRole::command)), // (
        (")", fg(HighlightRole::error)),
    );

    validate!(
        ("echo", fg(HighlightRole::command)),
        ("stuff", fg(HighlightRole::param)),
        ("# comment", fg(HighlightRole::comment)),
    );

    validate!(
        ("echo", fg(HighlightRole::command)),
        ("--", fg(HighlightRole::option)),
        ("-s", fg(HighlightRole::param)),
    );

    // Overlong paths don't crash (#7837).
    let overlong = get_overlong_path();
    validate!(
        ("touch", fg(HighlightRole::command)),
        (&overlong, fg(HighlightRole::param)),
    );

    validate!(
        ("a", fg(HighlightRole::param)),
        ("=", fg(HighlightRole::operat), ns),
    );

    // Highlighting works across escaped line breaks (#8444).
    validate!(
        ("echo", fg(HighlightRole::command)),
        ("$FISH_\\\n", fg(HighlightRole::operat)),
        ("VERSION", fg(HighlightRole::operat), ns),
    );

    validate!(
        ("/bin/ca", fg(HighlightRole::command), ns),
        ("*", fg(HighlightRole::operat), ns)
    );

    validate!(
        ("/bin/c", fg(HighlightRole::command), ns),
        ("*", fg(HighlightRole::operat), ns)
    );

    validate!(
        ("/bin/c", fg(HighlightRole::command), ns),
        ("{$VARIABLE_IN_COMMAND}", fg(HighlightRole::operat), ns),
        ("*", fg(HighlightRole::operat), ns)
    );

    validate!(
        ("/bin/c", fg(HighlightRole::command), ns),
        ("$VARIABLE_IN_COMMAND2", fg(HighlightRole::operat), ns)
    );

    validate!(("$EMPTY_VARIABLE", fg(HighlightRole::error)));
    validate!(("\"$EMPTY_VARIABLE\"", fg(HighlightRole::error)));

    validate!(
        ("echo", fg(HighlightRole::command)),
        ("\\UFDFD", fg(HighlightRole::escape)),
    );
    validate!(
        ("echo", fg(HighlightRole::command)),
        ("\\U10FFFF", fg(HighlightRole::escape)),
    );
    validate!(
        ("echo", fg(HighlightRole::command)),
        ("\\U110000", fg(HighlightRole::error)),
    );

    validate!(
        (">", fg(HighlightRole::error)),
        ("echo", fg(HighlightRole::error)),
    );
}
