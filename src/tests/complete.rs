use crate::abbrs::{self, with_abbrs_mut, Abbreviation};
use crate::complete::{
    complete, complete_add, complete_add_wrapper, complete_get_wrap_targets,
    complete_remove_wrapper, sort_and_prioritize, CompleteFlags, CompleteOptionType,
    CompletionMode, CompletionRequestOptions,
};
use crate::env::{EnvMode, Environment};
use crate::io::IoChain;
use crate::operation_context::{
    no_cancel, OperationContext, EXPANSION_LIMIT_BACKGROUND, EXPANSION_LIMIT_DEFAULT,
};
use crate::reader::completion_apply_to_command_line;
use crate::tests::prelude::*;
use crate::wchar::prelude::*;
use crate::wcstringutil::join_strings;
use std::collections::HashMap;
use std::ffi::CString;

/// Joins a std::vector<wcstring> via commas.
fn comma_join(lst: Vec<WString>) -> WString {
    join_strings(&lst, ',')
}

#[test]
#[serial]
fn test_complete() {
    let _cleanup = test_init();
    let vars = PwdEnvironment {
        parent: TestEnvironment {
            vars: HashMap::from([
                (WString::from_str("Foo1"), WString::new()),
                (WString::from_str("Foo2"), WString::new()),
                (WString::from_str("Foo3"), WString::new()),
                (WString::from_str("Bar1"), WString::new()),
                (WString::from_str("Bar2"), WString::new()),
                (WString::from_str("Bar3"), WString::new()),
                (WString::from_str("alpha"), WString::new()),
                (WString::from_str("ALPHA!"), WString::new()),
                (WString::from_str("gamma1"), WString::new()),
                (WString::from_str("GAMMA2"), WString::new()),
            ]),
        },
    };

    let parser = TestParser::new();
    let ctx = OperationContext::test_only_foreground(&parser, &vars, Box::new(no_cancel));

    let do_complete = |cmd: &wstr, flags: CompletionRequestOptions| complete(cmd, flags, &ctx).0;

    let mut completions = do_complete(L!("$"), CompletionRequestOptions::default());
    sort_and_prioritize(&mut completions, CompletionRequestOptions::default());
    assert_eq!(
        completions
            .into_iter()
            .map(|c| c.completion.to_string())
            .collect::<Vec<_>>(),
        [
            "alpha", "ALPHA!", "Bar1", "Bar2", "Bar3", "Foo1", "Foo2", "Foo3", "gamma1", "GAMMA2",
            "PWD"
        ]
        .into_iter()
        .map(|s| s.to_owned())
        .collect::<Vec<_>>()
    );

    // Smartcase test. Lowercase inputs match both lowercase and uppercase.
    let mut completions = do_complete(L!("$a"), CompletionRequestOptions::default());
    sort_and_prioritize(&mut completions, CompletionRequestOptions::default());

    assert_eq!(completions.len(), 2);
    assert_eq!(completions[0].completion, L!("$ALPHA!"));
    assert_eq!(completions[1].completion, L!("lpha"));

    let mut completions = do_complete(L!("$F"), CompletionRequestOptions::default());
    sort_and_prioritize(&mut completions, CompletionRequestOptions::default());
    assert_eq!(completions.len(), 3);
    assert_eq!(completions[0].completion, L!("oo1"));
    assert_eq!(completions[1].completion, L!("oo2"));
    assert_eq!(completions[2].completion, L!("oo3"));

    completions = do_complete(L!("$1"), CompletionRequestOptions::default());
    sort_and_prioritize(&mut completions, CompletionRequestOptions::default());
    assert_eq!(completions, vec![]);

    let mut fuzzy_options = CompletionRequestOptions::default();
    fuzzy_options.fuzzy_match = true;
    let mut completions = do_complete(L!("$1"), fuzzy_options);
    sort_and_prioritize(&mut completions, fuzzy_options);
    assert_eq!(completions.len(), 3);
    assert_eq!(completions[0].completion, L!("$Bar1"));
    assert_eq!(completions[1].completion, L!("$Foo1"));
    assert_eq!(completions[2].completion, L!("$gamma1"));

    std::fs::create_dir_all("test/complete_test").unwrap();
    std::fs::write("test/complete_test/has space", []).unwrap();
    std::fs::write("test/complete_test/bracket[abc]", []).unwrap();
    std::fs::write(r"test/complete_test/gnarlybracket\[abc]", []).unwrap();
    #[cfg(not(windows))] // Square brackets are not legal path characters on WIN32/CYGWIN
    {
        std::fs::write(r"test/complete_test/gnarlybracket\[abc]", []).unwrap();
        std::fs::write(r"test/complete_test/colon:abc", []).unwrap();
    }
    std::fs::write(r"test/complete_test/equal=abc", []).unwrap();
    std::fs::write("test/complete_test/testfile", []).unwrap();
    let testfile = CString::new("test/complete_test/testfile").unwrap();
    assert_eq!(unsafe { libc::chmod(testfile.as_ptr(), 0o700,) }, 0);
    std::fs::create_dir_all("test/complete_test/foo1").unwrap();
    std::fs::create_dir_all("test/complete_test/foo2").unwrap();
    std::fs::create_dir_all("test/complete_test/foo3").unwrap();

    completions = do_complete(
        L!("echo (test/complete_test/testfil"),
        CompletionRequestOptions::default(),
    );
    assert_eq!(completions.len(), 1);
    assert_eq!(completions[0].completion, L!("e"));

    completions = do_complete(
        L!("echo (ls test/complete_test/testfil"),
        CompletionRequestOptions::default(),
    );
    assert_eq!(completions.len(), 1);
    assert_eq!(completions[0].completion, L!("e"));

    completions = do_complete(
        L!("echo (command ls test/complete_test/testfil"),
        CompletionRequestOptions::default(),
    );
    assert_eq!(completions.len(), 1);
    assert_eq!(completions[0].completion, L!("e"));

    // Completing after spaces - see #2447
    completions = do_complete(
        L!("echo (ls test/complete_test/has\\ "),
        CompletionRequestOptions::default(),
    );
    assert_eq!(completions.len(), 1);
    assert_eq!(completions[0].completion, L!("space"));

    macro_rules! unique_completion_applies_as {
        ( $cmdline:expr, $completion_result:expr, $applied:expr $(,)? ) => {
            let cmdline = L!($cmdline);
            let completions = do_complete(cmdline, CompletionRequestOptions::default());
            assert_eq!(completions.len(), 1);
            assert_eq!(
                completions[0].completion,
                L!($completion_result),
                "completion mismatch"
            );
            let mut cursor = cmdline.len();
            let newcmdline = completion_apply_to_command_line(
                &completions[0].completion,
                completions[0].flags,
                cmdline,
                &mut cursor,
                false,
            );
            assert_eq!(newcmdline, L!($applied), "apply result mismatch");
        };
    }

    // Brackets - see #5831
    unique_completion_applies_as!(
        "touch test/complete_test/bracket[",
        r"'test/complete_test/bracket[abc]'",
        "touch 'test/complete_test/bracket[abc]' ",
    );
    unique_completion_applies_as!(
        "echo (ls test/complete_test/bracket[",
        r"'test/complete_test/bracket[abc]'",
        "echo (ls 'test/complete_test/bracket[abc]' ",
    );
    #[cfg(not(windows))] // Square brackets are not legal path characters on WIN32/CYGWIN
    {
        unique_completion_applies_as!(
            r"touch test/complete_test/gnarlybracket\\[",
            r"'test/complete_test/gnarlybracket\\[abc]'",
            r"touch 'test/complete_test/gnarlybracket\\[abc]' ",
        );
        unique_completion_applies_as!(
            r"a=test/complete_test/bracket[",
            r"a='test/complete_test/bracket[abc]'",
            r"a='test/complete_test/bracket[abc]' ",
        );
    }

    #[cfg(not(windows))]
    {
        unique_completion_applies_as!(
            r"touch test/complete_test/colon",
            r"\:abc",
            r"touch test/complete_test/colon\:abc ",
        );
        unique_completion_applies_as!(
            r"touch test/complete_test/colon\:",
            r"abc",
            r"touch test/complete_test/colon\:abc ",
        );
        unique_completion_applies_as!(
            r#"touch "test/complete_test/colon:"#,
            r"abc",
            r#"touch "test/complete_test/colon:abc" "#,
        );
    }

    // #8820
    let mut cursor_pos = 11;
    let newcmdline = completion_apply_to_command_line(
        L!("Debug/"),
        CompleteFlags::REPLACES_TOKEN | CompleteFlags::NO_SPACE,
        L!("mv debug debug"),
        &mut cursor_pos,
        true,
    );
    assert_eq!(newcmdline, L!("mv debug Debug/"));

    // Add a function and test completing it in various ways.
    parser.eval(L!("function scuttlebutt; end"), &IoChain::new());

    // Complete a function name.
    completions = do_complete(L!("echo (scuttlebut"), CompletionRequestOptions::default());
    assert_eq!(completions.len(), 1);
    assert_eq!(completions[0].completion, L!("t"));

    // But not with the command prefix.
    completions = do_complete(
        L!("echo (command scuttlebut"),
        CompletionRequestOptions::default(),
    );
    assert_eq!(&completions, &[]);

    // Not with the builtin prefix.
    let completions = do_complete(
        L!("echo (builtin scuttlebut"),
        CompletionRequestOptions::default(),
    );
    assert_eq!(&completions, &[]);

    // Not after a redirection.
    let completions = do_complete(
        L!("echo hi > scuttlebut"),
        CompletionRequestOptions::default(),
    );
    assert_eq!(&completions, &[]);

    // Trailing spaces (#1261).
    let mut no_files = CompletionMode::default();
    no_files.no_files = true;
    complete_add(
        L!("foobarbaz").into(),
        false,
        WString::new(),
        CompleteOptionType::ArgsOnly,
        no_files,
        vec![],
        L!("qux").into(),
        WString::new(),
        CompleteFlags::AUTO_SPACE,
    );
    let completions = do_complete(L!("foobarbaz "), CompletionRequestOptions::default());
    assert_eq!(completions.len(), 1);
    assert_eq!(completions[0].completion, L!("qux"));

    // Don't complete variable names in single quotes (#1023).
    let completions = do_complete(L!("echo '$Foo"), CompletionRequestOptions::default());
    assert_eq!(completions, vec![]);
    let completions = do_complete(L!("echo \\$Foo"), CompletionRequestOptions::default());
    assert_eq!(completions, vec![]);

    // File completions.
    let completions = do_complete(
        L!("cat test/complete_test/te"),
        CompletionRequestOptions::default(),
    );
    assert_eq!(completions.len(), 1);
    assert_eq!(completions[0].completion, L!("stfile"));
    let completions = do_complete(
        L!("echo sup > test/complete_test/te"),
        CompletionRequestOptions::default(),
    );
    assert_eq!(completions.len(), 1);
    assert_eq!(completions[0].completion, L!("stfile"));
    let completions = do_complete(
        L!("echo sup > test/complete_test/te"),
        CompletionRequestOptions::default(),
    );
    assert_eq!(completions.len(), 1);
    assert_eq!(completions[0].completion, L!("stfile"));

    parser.pushd("test/complete_test");
    let completions = do_complete(L!("cat te"), CompletionRequestOptions::default());
    assert_eq!(completions.len(), 1);
    assert_eq!(completions[0].completion, L!("stfile"));
    assert!(!(completions[0].flags.contains(CompleteFlags::REPLACES_TOKEN)));
    assert!(
        !(completions[0]
            .flags
            .contains(CompleteFlags::DUPLICATES_ARGUMENT))
    );
    let completions = do_complete(L!("cat testfile te"), CompletionRequestOptions::default());
    assert_eq!(completions.len(), 1);
    assert_eq!(completions[0].completion, L!("stfile"));
    assert!(completions[0]
        .flags
        .contains(CompleteFlags::DUPLICATES_ARGUMENT));
    let completions = do_complete(L!("cat testfile TE"), CompletionRequestOptions::default());
    assert_eq!(completions.len(), 1);
    assert_eq!(completions[0].completion, L!("testfile"));
    assert!(completions[0].flags.contains(CompleteFlags::REPLACES_TOKEN));
    assert!(completions[0]
        .flags
        .contains(CompleteFlags::DUPLICATES_ARGUMENT));
    let completions = do_complete(
        L!("something --abc=te"),
        CompletionRequestOptions::default(),
    );
    assert_eq!(completions.len(), 1);
    assert_eq!(completions[0].completion, L!("stfile"));
    let completions = do_complete(L!("something -abc=te"), CompletionRequestOptions::default());
    assert_eq!(completions.len(), 1);
    assert_eq!(completions[0].completion, L!("stfile"));
    let completions = do_complete(L!("something abc=te"), CompletionRequestOptions::default());
    assert_eq!(completions.len(), 1);
    assert_eq!(completions[0].completion, L!("stfile"));
    let completions = do_complete(
        L!("something abc=stfile"),
        CompletionRequestOptions::default(),
    );
    assert_eq!(&completions, &[]);
    let completions = do_complete(L!("something abc=stfile"), fuzzy_options);
    assert_eq!(completions.len(), 1);
    assert_eq!(completions[0].completion, L!("abc=testfile"));

    // Zero escapes can cause problems. See issue #1631.
    let completions = do_complete(L!("cat foo\\0"), CompletionRequestOptions::default());
    assert_eq!(&completions, &[]);
    let completions = do_complete(L!("cat foo\\0bar"), CompletionRequestOptions::default());
    assert_eq!(&completions, &[]);
    let completions = do_complete(L!("cat \\0"), CompletionRequestOptions::default());
    assert_eq!(&completions, &[]);
    let mut completions = do_complete(L!("cat te\\0"), CompletionRequestOptions::default());
    assert_eq!(&completions, &[]);

    parser.popd();
    completions.clear();

    // Test abbreviations.
    parser.eval(
        L!("function testabbrsonetwothreefour; end"),
        &IoChain::new(),
    );
    with_abbrs_mut(|abbrset| {
        abbrset.add(Abbreviation::new(
            L!("somename").into(),
            L!("testabbrsonetwothreezero").into(),
            L!("expansion").into(),
            abbrs::Position::Command,
            false,
        ))
    });

    let completions = complete(
        L!("testabbrsonetwothree"),
        CompletionRequestOptions::default(),
        &parser.context(),
    )
    .0;
    assert_eq!(completions.len(), 2);
    assert_eq!(completions[0].completion, L!("four"));
    assert!(!completions[0].flags.contains(CompleteFlags::NO_SPACE));
    // Abbreviations should not have a space after them.
    assert_eq!(completions[1].completion, L!("zero"));
    assert!(completions[1].flags.contains(CompleteFlags::NO_SPACE));
    with_abbrs_mut(|abbrset| {
        abbrset.erase(L!("testabbrsonetwothreezero"));
    });

    // Test wraps.
    assert!(comma_join(complete_get_wrap_targets(L!("wrapper1"))).is_empty());
    complete_add_wrapper(L!("wrapper1").into(), L!("wrapper2").into());
    assert_eq!(
        comma_join(complete_get_wrap_targets(L!("wrapper1"))),
        L!("wrapper2")
    );
    complete_add_wrapper(L!("wrapper2").into(), L!("wrapper3").into());
    assert_eq!(
        comma_join(complete_get_wrap_targets(L!("wrapper1"))),
        L!("wrapper2")
    );
    assert_eq!(
        comma_join(complete_get_wrap_targets(L!("wrapper2"))),
        L!("wrapper3")
    );
    complete_add_wrapper(L!("wrapper3").into(), L!("wrapper1").into()); // loop!
    assert_eq!(
        comma_join(complete_get_wrap_targets(L!("wrapper1"))),
        L!("wrapper2")
    );
    assert_eq!(
        comma_join(complete_get_wrap_targets(L!("wrapper2"))),
        L!("wrapper3")
    );
    assert_eq!(
        comma_join(complete_get_wrap_targets(L!("wrapper3"))),
        L!("wrapper1")
    );
    complete_remove_wrapper(L!("wrapper1").into(), L!("wrapper2"));
    assert!(comma_join(complete_get_wrap_targets(L!("wrapper1"))).is_empty());
    assert_eq!(
        comma_join(complete_get_wrap_targets(L!("wrapper2"))),
        L!("wrapper3")
    );
    assert_eq!(
        comma_join(complete_get_wrap_targets(L!("wrapper3"))),
        L!("wrapper1")
    );

    // Test cd wrapping chain
    parser.pushd("test/complete_test");

    complete_add_wrapper(L!("cdwrap1").into(), L!("cd").into());
    complete_add_wrapper(L!("cdwrap2").into(), L!("cdwrap1").into());

    let mut cd_compl = do_complete(L!("cd "), CompletionRequestOptions::default());
    sort_and_prioritize(&mut cd_compl, CompletionRequestOptions::default());

    let mut cdwrap1_compl = do_complete(L!("cdwrap1 "), CompletionRequestOptions::default());
    sort_and_prioritize(&mut cdwrap1_compl, CompletionRequestOptions::default());

    let mut cdwrap2_compl = do_complete(L!("cdwrap2 "), CompletionRequestOptions::default());
    sort_and_prioritize(&mut cdwrap2_compl, CompletionRequestOptions::default());

    let min_compl_size = cd_compl
        .len()
        .min(cdwrap1_compl.len().min(cdwrap2_compl.len()));

    assert_eq!(cd_compl.len(), min_compl_size);
    assert_eq!(cdwrap1_compl.len(), min_compl_size);
    assert_eq!(cdwrap2_compl.len(), min_compl_size);
    for i in 0..min_compl_size {
        assert_eq!(cd_compl[i].completion, cdwrap1_compl[i].completion);
        assert_eq!(cdwrap1_compl[i].completion, cdwrap2_compl[i].completion);
    }

    complete_remove_wrapper(L!("cdwrap1").into(), L!("cd"));
    complete_remove_wrapper(L!("cdwrap2").into(), L!("cdwrap1"));
    parser.popd();
}

// Testing test_autosuggest_suggest_special, in particular for properly handling quotes and
// backslashes.
#[test]
#[serial]
fn test_autosuggest_suggest_special() {
    let _cleanup = test_init();
    let parser = TestParser::new();
    macro_rules! perform_one_autosuggestion_cd_test {
        ($command:literal, $expected:literal, $vars:expr) => {
            let command = L!($command);
            let mut comps = complete(
                command,
                CompletionRequestOptions::autosuggest(),
                &OperationContext::background($vars, EXPANSION_LIMIT_BACKGROUND),
            )
            .0;

            let expects_error = $expected == "<error>";

            assert_eq!(expects_error, comps.is_empty());
            if !expects_error {
                sort_and_prioritize(&mut comps, CompletionRequestOptions::default());
                let suggestion = &comps[0];
                let mut cursor = command.len();
                let newcmdline = completion_apply_to_command_line(
                    &suggestion.completion,
                    suggestion.flags,
                    command,
                    &mut cursor,
                    /*append_only=*/ true,
                );
                assert_eq!(newcmdline.strip_prefix(command).unwrap(), L!($expected));
            }
        };
    }

    macro_rules! perform_one_completion_cd_test {
        ($command:literal, $expected:literal) => {
            let mut comps = complete(
                L!($command),
                CompletionRequestOptions::default(),
                &OperationContext::foreground(
                    &parser,
                    Box::new(no_cancel),
                    EXPANSION_LIMIT_DEFAULT,
                ),
            )
            .0;

            let expects_error = $expected == "<error>";

            assert_eq!(expects_error, comps.is_empty());
            if !expects_error {
                sort_and_prioritize(&mut comps, CompletionRequestOptions::default());
                let suggestion = &comps[0];
                assert_eq!(suggestion.completion, L!($expected));
            }
        };
    }

    std::fs::create_dir_all("test/autosuggest_test/0foobar").unwrap();
    std::fs::create_dir_all("test/autosuggest_test/1foo bar").unwrap();
    std::fs::create_dir_all("test/autosuggest_test/2foo  bar").unwrap();
    // Cygwin disallows backslashes in filenames.
    #[cfg(not(windows))]
    std::fs::create_dir_all("test/autosuggest_test/3foo\\bar").unwrap();
    // a path with a single quote
    std::fs::create_dir_all("test/autosuggest_test/4foo'bar").unwrap();
    // a path with a double quote
    std::fs::create_dir_all("test/autosuggest_test/5foo\"bar").unwrap();
    // This is to ensure tilde expansion is handled. See the `cd ~/test_autosuggest_suggest_specia`
    // test below.
    // Fake out the home directory
    parser.vars().set_one(
        L!("HOME"),
        EnvMode::LOCAL | EnvMode::EXPORT,
        L!("test/test-home").to_owned(),
    );
    std::fs::create_dir_all("test/test-home/test_autosuggest_suggest_special/").unwrap();
    std::fs::create_dir_all("test/autosuggest_test/start/unique2/unique3/multi4").unwrap();
    std::fs::create_dir_all("test/autosuggest_test/start/unique2/unique3/multi42").unwrap();
    std::fs::create_dir_all("test/autosuggest_test/start/unique2/.hiddenDir/moreStuff").unwrap();

    // Ensure symlink don't cause us to chase endlessly.
    std::fs::create_dir_all("test/autosuggest_test/has_loop/loopy").unwrap();
    let _ = std::fs::remove_file("test/autosuggest_test/has_loop/loopy/loop");
    std::os::unix::fs::symlink("../loopy", "test/autosuggest_test/has_loop/loopy/loop").unwrap();

    let wd = "test/autosuggest_test";

    let mut vars = PwdEnvironment::default();
    vars.parent.vars.insert(
        L!("HOME").into(),
        parser.vars().get(L!("HOME")).unwrap().as_string(),
    );

    perform_one_autosuggestion_cd_test!("cd test/autosuggest_test/0", "foobar/", &vars);
    perform_one_autosuggestion_cd_test!("cd \"test/autosuggest_test/0", "foobar/", &vars);
    perform_one_autosuggestion_cd_test!("cd 'test/autosuggest_test/0", "foobar/", &vars);
    perform_one_autosuggestion_cd_test!("cd test/autosuggest_test/1", r"foo\ bar/", &vars);
    perform_one_autosuggestion_cd_test!("cd \"test/autosuggest_test/1", r"foo bar/", &vars);
    perform_one_autosuggestion_cd_test!("cd 'test/autosuggest_test/1", r"foo bar/", &vars);
    perform_one_autosuggestion_cd_test!("cd test/autosuggest_test/2", r"foo\ \ bar/", &vars);
    perform_one_autosuggestion_cd_test!("cd \"test/autosuggest_test/2", r"foo  bar/", &vars);
    perform_one_autosuggestion_cd_test!("cd 'test/autosuggest_test/2", r"foo  bar/", &vars);
    #[cfg(not(windows))]
    {
        perform_one_autosuggestion_cd_test!("cd test/autosuggest_test/3", r"foo\\bar/", &vars);
        perform_one_autosuggestion_cd_test!("cd \"test/autosuggest_test/3", r"foo\\bar/", &vars);
        perform_one_autosuggestion_cd_test!("cd 'test/autosuggest_test/3", r"foo\\bar/", &vars);
    }
    perform_one_autosuggestion_cd_test!("cd test/autosuggest_test/4", r"foo\'bar/", &vars);
    perform_one_autosuggestion_cd_test!("cd \"test/autosuggest_test/4", "foo'bar/", &vars);
    perform_one_autosuggestion_cd_test!("cd 'test/autosuggest_test/4", r"foo\'bar/", &vars);
    perform_one_autosuggestion_cd_test!("cd test/autosuggest_test/5", r#"foo\"bar/"#, &vars);
    perform_one_autosuggestion_cd_test!("cd \"test/autosuggest_test/5", r#"foo\"bar/"#, &vars);
    perform_one_autosuggestion_cd_test!("cd 'test/autosuggest_test/5", r#"foo"bar/"#, &vars);

    vars.parent
        .vars
        .insert(L!("AUTOSUGGEST_TEST_LOC").to_owned(), WString::from_str(wd));
    perform_one_autosuggestion_cd_test!("cd $AUTOSUGGEST_TEST_LOC/0", "foobar/", &vars);
    perform_one_autosuggestion_cd_test!("cd ~/test_autosuggest_suggest_specia", "l/", &vars);

    perform_one_autosuggestion_cd_test!(
        "cd test/autosuggest_test/start/",
        "unique2/unique3/",
        &vars
    );

    perform_one_autosuggestion_cd_test!("cd test/autosuggest_test/has_loop/", "loopy/loop/", &vars);

    parser.pushd(wd);
    perform_one_autosuggestion_cd_test!("cd 0", "foobar/", &vars);
    perform_one_autosuggestion_cd_test!("cd \"0", "foobar/", &vars);
    perform_one_autosuggestion_cd_test!("cd '0", "foobar/", &vars);
    perform_one_autosuggestion_cd_test!("cd 1", r"foo\ bar/", &vars);
    perform_one_autosuggestion_cd_test!("cd \"1", "foo bar/", &vars);
    perform_one_autosuggestion_cd_test!("cd '1", "foo bar/", &vars);
    perform_one_autosuggestion_cd_test!("cd 2", r"foo\ \ bar/", &vars);
    perform_one_autosuggestion_cd_test!("cd \"2", "foo  bar/", &vars);
    perform_one_autosuggestion_cd_test!("cd '2", "foo  bar/", &vars);
    #[cfg(not(windows))]
    {
        perform_one_autosuggestion_cd_test!("cd 3", r"foo\\bar/", &vars);
        perform_one_autosuggestion_cd_test!("cd \"3", r"foo\\bar/", &vars);
        perform_one_autosuggestion_cd_test!("cd '3", r"foo\\bar/", &vars);
    }
    perform_one_autosuggestion_cd_test!("cd 4", r"foo\'bar/", &vars);
    perform_one_autosuggestion_cd_test!("cd \"4", r"foo'bar/", &vars);
    perform_one_autosuggestion_cd_test!("cd '4", r"foo\'bar/", &vars);
    perform_one_autosuggestion_cd_test!("cd 5", r#"foo\"bar/"#, &vars);
    perform_one_autosuggestion_cd_test!("cd \"5", r#"foo\"bar/"#, &vars);
    perform_one_autosuggestion_cd_test!("cd '5", r#"foo"bar/"#, &vars);

    // A single quote should defeat tilde expansion.
    perform_one_autosuggestion_cd_test!("cd '~/test_autosuggest_suggest_specia'", "<error>", &vars);

    // Don't crash on ~ (issue #2696). Note this is cwd dependent.
    std::fs::create_dir_all("~absolutelynosuchuser/path1/path2/").unwrap();
    perform_one_autosuggestion_cd_test!("cd ~absolutelynosuchus", "er/path1/path2/", &vars);
    perform_one_autosuggestion_cd_test!("cd ~absolutelynosuchuser/", "path1/path2/", &vars);
    perform_one_completion_cd_test!("cd ~absolutelynosuchus", "er/");
    perform_one_completion_cd_test!("cd ~absolutelynosuchuser/", "path1/");

    parser
        .vars()
        .remove(L!("HOME"), EnvMode::LOCAL | EnvMode::EXPORT);
    parser.popd();
}

#[test]
#[serial]
fn test_autosuggestion_ignores() {
    let _cleanup = test_init();
    // Testing scenarios that should produce no autosuggestions
    macro_rules! perform_one_autosuggestion_should_ignore_test {
        ($command:literal) => {
            let comps = complete(
                L!($command),
                CompletionRequestOptions::autosuggest(),
                &OperationContext::empty(),
            )
            .0;
            assert_eq!(&comps, &[]);
        };
    }
    // Do not do file autosuggestions immediately after certain statement terminators - see #1631.
    perform_one_autosuggestion_should_ignore_test!("echo PIPE_TEST|");
    perform_one_autosuggestion_should_ignore_test!("echo PIPE_TEST&");
    perform_one_autosuggestion_should_ignore_test!("echo PIPE_TEST#comment");
    perform_one_autosuggestion_should_ignore_test!("echo PIPE_TEST;");
}
