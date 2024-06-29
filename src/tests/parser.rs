use crate::ast::{self, Ast, JobPipeline, List, Node, Traversal};
use crate::common::ScopeGuard;
use crate::env::EnvStack;
use crate::expand::ExpandFlags;
use crate::io::{IoBufferfill, IoChain};
use crate::parse_constants::{
    ParseErrorCode, ParseTreeFlags, ParserTestErrorBits, StatementDecoration,
};
use crate::parse_tree::{parse_source, LineCounter};
use crate::parse_util::{parse_util_detect_errors, parse_util_detect_errors_in_argument};
use crate::parser::{CancelBehavior, Parser};
use crate::reader::{reader_pop, reader_push, reader_reset_interrupted, ReaderConfig};
use crate::signal::{signal_clear_cancel, signal_reset_handlers, signal_set_handlers};
use crate::tests::prelude::*;
use crate::threads::iothread_perform;
use crate::wchar::prelude::*;
use crate::wcstringutil::join_strings;
use libc::SIGINT;
use std::rc::Rc;
use std::time::Duration;

#[test]
#[serial]
fn test_parser() {
    let _cleanup = test_init();
    macro_rules! detect_errors {
        ($src:literal) => {
            parse_util_detect_errors(L!($src), None, true /* accept incomplete */)
        };
    }

    fn detect_argument_errors(src: &str) -> Result<(), ParserTestErrorBits> {
        let src = WString::from_str(src);
        let ast = Ast::parse_argument_list(&src, ParseTreeFlags::default(), None);
        if ast.errored() {
            return Err(ParserTestErrorBits::ERROR);
        }
        let args = &ast.top().as_freestanding_argument_list().unwrap().arguments;
        let first_arg = args.get(0).expect("Failed to parse an argument");
        let mut errors = None;
        parse_util_detect_errors_in_argument(first_arg, first_arg.source(&src), &mut errors)
    }

    // Testing block nesting
    assert!(
        detect_errors!("if; end").is_err(),
        "Incomplete if statement undetected"
    );
    assert!(
        detect_errors!("if test; echo").is_err(),
        "Missing end undetected"
    );
    assert!(
        detect_errors!("if test; end; end").is_err(),
        "Unbalanced end undetected"
    );

    // Testing detection of invalid use of builtin commands
    assert!(
        detect_errors!("case foo").is_err(),
        "'case' command outside of block context undetected"
    );
    assert!(
        detect_errors!("switch ggg; if true; case foo;end;end").is_err(),
        "'case' command outside of switch block context undetected"
    );
    assert!(
        detect_errors!("else").is_err(),
        "'else' command outside of conditional block context undetected"
    );
    assert!(
        detect_errors!("else if").is_err(),
        "'else if' command outside of conditional block context undetected"
    );
    assert!(
        detect_errors!("if false; else if; end").is_err(),
        "'else if' missing command undetected"
    );

    assert!(
        detect_errors!("break").is_err(),
        "'break' command outside of loop block context undetected"
    );

    assert!(
        detect_errors!("break --help").is_ok(),
        "'break --help' incorrectly marked as error"
    );

    assert!(
        detect_errors!("while false ; function foo ; break ; end ; end ").is_err(),
        "'break' command inside function allowed to break from loop outside it"
    );

    assert!(
        detect_errors!("exec ls|less").is_err() && detect_errors!("echo|return").is_err(),
        "Invalid pipe command undetected"
    );

    assert!(
        detect_errors!("for i in foo ; switch $i ; case blah ; break; end; end ").is_ok(),
        "'break' command inside switch falsely reported as error"
    );

    assert!(
        detect_errors!("or cat | cat").is_ok() && detect_errors!("and cat | cat").is_ok(),
        "boolean command at beginning of pipeline falsely reported as error"
    );

    assert!(
        detect_errors!("cat | and cat").is_err(),
        "'and' command in pipeline not reported as error"
    );

    assert!(
        detect_errors!("cat | or cat").is_err(),
        "'or' command in pipeline not reported as error"
    );

    assert!(
        detect_errors!("cat | exec").is_err() && detect_errors!("exec | cat").is_err(),
        "'exec' command in pipeline not reported as error"
    );

    assert!(
        detect_errors!("begin ; end arg").is_err(),
        "argument to 'end' not reported as error"
    );

    assert!(
        detect_errors!("switch foo ; end arg").is_err(),
        "argument to 'end' not reported as error"
    );

    assert!(
        detect_errors!("if true; else if false ; end arg").is_err(),
        "argument to 'end' not reported as error"
    );

    assert!(
        detect_errors!("if true; else ; end arg").is_err(),
        "argument to 'end' not reported as error"
    );

    assert!(
        detect_errors!("begin ; end 2> /dev/null").is_ok(),
        "redirection after 'end' wrongly reported as error"
    );

    assert!(
        detect_errors!("true | ") == Err(ParserTestErrorBits::INCOMPLETE),
        "unterminated pipe not reported properly"
    );

    assert!(
        detect_errors!("echo (\nfoo\n  bar") == Err(ParserTestErrorBits::INCOMPLETE),
        "unterminated multiline subshell not reported properly"
    );

    assert!(
        detect_errors!("begin ; true ; end | ") == Err(ParserTestErrorBits::INCOMPLETE),
        "unterminated pipe not reported properly"
    );

    assert!(
        detect_errors!(" | true ") == Err(ParserTestErrorBits::ERROR),
        "leading pipe not reported properly"
    );

    assert!(
        detect_errors!("true | # comment") == Err(ParserTestErrorBits::INCOMPLETE),
        "comment after pipe not reported as incomplete"
    );

    assert!(
        detect_errors!("true | # comment \n false ").is_ok(),
        "comment and newline after pipe wrongly reported as error"
    );

    assert!(
        detect_errors!("true | ; false ") == Err(ParserTestErrorBits::ERROR),
        "semicolon after pipe not detected as error"
    );

    assert!(
        detect_argument_errors("foo").is_ok(),
        "simple argument reported as error"
    );

    assert!(
        detect_argument_errors("''").is_ok(),
        "Empty string reported as error"
    );

    assert!(
        detect_argument_errors("foo$$")
            .unwrap_err()
            .contains(ParserTestErrorBits::ERROR),
        "Bad variable expansion not reported as error"
    );

    assert!(
        detect_argument_errors("foo$@")
            .unwrap_err()
            .contains(ParserTestErrorBits::ERROR),
        "Bad variable expansion not reported as error"
    );

    // Within command substitutions, we should be able to detect everything that
    // parse_util_detect_errors! can detect.
    assert!(
        detect_argument_errors("foo(cat | or cat)")
            .unwrap_err()
            .contains(ParserTestErrorBits::ERROR),
        "Bad command substitution not reported as error"
    );

    assert!(
        detect_errors!("false & ; and cat").is_err(),
        "'and' command after background not reported as error"
    );

    assert!(
        detect_errors!("true & ; or cat").is_err(),
        "'or' command after background not reported as error"
    );

    assert!(
        detect_errors!("true & ; not cat").is_ok(),
        "'not' command after background falsely reported as error"
    );

    assert!(
        detect_errors!("if true & ; end").is_err(),
        "backgrounded 'if' conditional not reported as error"
    );

    assert!(
        detect_errors!("if false; else if true & ; end").is_err(),
        "backgrounded 'else if' conditional not reported as error"
    );

    assert!(
        detect_errors!("while true & ; end").is_err(),
        "backgrounded 'while' conditional not reported as error"
    );

    assert!(
        detect_errors!("true | || false").is_err(),
        "bogus boolean statement error not detected"
    );

    assert!(
        detect_errors!("|| false").is_err(),
        "bogus boolean statement error not detected"
    );

    assert!(
        detect_errors!("&& false").is_err(),
        "bogus boolean statement error not detected"
    );

    assert!(
        detect_errors!("true ; && false").is_err(),
        "bogus boolean statement error not detected"
    );

    assert!(
        detect_errors!("true ; || false").is_err(),
        "bogus boolean statement error not detected"
    );

    assert!(
        detect_errors!("true || && false").is_err(),
        "bogus boolean statement error not detected"
    );

    assert!(
        detect_errors!("true && || false").is_err(),
        "bogus boolean statement error not detected"
    );

    assert!(
        detect_errors!("true && && false").is_err(),
        "bogus boolean statement error not detected"
    );

    assert!(
        detect_errors!("true && ") == Err(ParserTestErrorBits::INCOMPLETE),
        "unterminated conjunction not reported properly"
    );

    assert!(
        detect_errors!("true && \n true").is_ok(),
        "newline after && reported as error"
    );

    assert!(
        detect_errors!("true || \n") == Err(ParserTestErrorBits::INCOMPLETE),
        "unterminated conjunction not reported properly"
    );
}

#[test]
#[serial]
fn test_new_parser_correctness() {
    let _cleanup = test_init();
    macro_rules! validate {
        ($src:expr, $ok:expr) => {
            let ast = Ast::parse(L!($src), ParseTreeFlags::default(), None);
            assert_eq!(ast.errored(), !$ok);
        };
    }
    validate!("; ; ; ", true);
    validate!("if ; end", false);
    validate!("if true ; end", true);
    validate!("if true; end ; end", false);
    validate!("if end; end ; end", false);
    validate!("if end", false);
    validate!("end", false);
    validate!("for i i", false);
    validate!("for i in a b c ; end", true);
    validate!("begin end", true);
    validate!("begin; end", true);
    validate!("begin if true; end; end;", true);
    validate!("begin if true ; echo hi ; end; end", true);
    validate!("true && false || false", true);
    validate!("true || false; and true", true);
    validate!("true || ||", false);
    validate!("|| true", false);
    validate!("true || \n\n false", true);
}

#[test]
#[serial]
fn test_new_parser_correctness_by_fuzzing() {
    let _cleanup = test_init();
    let fuzzes = [
        L!("if"),
        L!("else"),
        L!("for"),
        L!("in"),
        L!("while"),
        L!("begin"),
        L!("function"),
        L!("switch"),
        L!("case"),
        L!("end"),
        L!("and"),
        L!("or"),
        L!("not"),
        L!("command"),
        L!("builtin"),
        L!("foo"),
        L!("|"),
        L!("^"),
        L!("&"),
        L!(";"),
    ];

    // Generate a list of strings of all keyword / token combinations.
    let mut src = WString::new();
    src.reserve(128);

    // Given that we have an array of 'fuzz_count' strings, we wish to enumerate all permutations of
    // 'len' values. We do this by incrementing an integer, interpreting it as "base fuzz_count".
    fn string_for_permutation(fuzzes: &[&wstr], len: usize, permutation: usize) -> Option<WString> {
        let mut remaining_permutation = permutation;
        let mut out_str = WString::new();
        for _i in 0..len {
            let idx = remaining_permutation % fuzzes.len();
            remaining_permutation /= fuzzes.len();
            out_str.push_utfstr(fuzzes[idx]);
            out_str.push(' ');
        }
        // Return false if we wrapped.
        (remaining_permutation == 0).then_some(out_str)
    }

    let max_len = 5;
    for len in 0..max_len {
        // We wish to look at all permutations of 4 elements of 'fuzzes' (with replacement).
        // Construct an int and keep incrementing it.
        let mut permutation = 0;
        while let Some(src) = string_for_permutation(&fuzzes, len, permutation) {
            permutation += 1;
            Ast::parse(&src, ParseTreeFlags::default(), None);
        }
    }
}

// Test the LL2 (two token lookahead) nature of the parser by exercising the special builtin and
// command handling. In particular, 'command foo' should be a decorated statement 'foo' but 'command
// -help' should be an undecorated statement 'command' with argument '--help', and NOT attempt to
// run a command called '--help'.
#[test]
#[serial]
fn test_new_parser_ll2() {
    let _cleanup = test_init();
    // Parse a statement, returning the command, args (joined by spaces), and the decoration. Returns
    // true if successful.
    fn test_1_parse_ll2(src: &wstr) -> Option<(WString, WString, StatementDecoration)> {
        let ast = Ast::parse(src, ParseTreeFlags::default(), None);
        if ast.errored() {
            return None;
        }

        // Get the statement. Should only have one.
        let mut statement = None;
        for n in Traversal::new(ast.top()) {
            if let Some(tmp) = n.as_decorated_statement() {
                assert!(
                    statement.is_none(),
                    "More than one decorated statement found in '{}'",
                    src
                );
                statement = Some(tmp);
            }
        }
        let statement = statement.expect("No decorated statement found");

        // Return its decoration and command.
        let out_deco = statement.decoration();
        let out_cmd = statement.command.source(src).to_owned();

        // Return arguments separated by spaces.
        let out_joined_args = join_strings(
            &statement
                .args_or_redirs
                .iter()
                .filter(|a| a.is_argument())
                .map(|a| a.source(src))
                .collect::<Vec<_>>(),
            ' ',
        );

        Some((out_cmd, out_joined_args, out_deco))
    }
    macro_rules! validate {
        ($src:expr, $cmd:expr, $args:expr, $deco:expr) => {
            let (cmd, args, deco) = test_1_parse_ll2(L!($src)).unwrap();
            assert_eq!(cmd, L!($cmd));
            assert_eq!(args, L!($args));
            assert_eq!(deco, $deco);
        };
    }

    validate!("echo hello", "echo", "hello", StatementDecoration::none);
    validate!(
        "command echo hello",
        "echo",
        "hello",
        StatementDecoration::command
    );
    validate!(
        "exec echo hello",
        "echo",
        "hello",
        StatementDecoration::exec
    );
    validate!(
        "command command hello",
        "command",
        "hello",
        StatementDecoration::command
    );
    validate!(
        "builtin command hello",
        "command",
        "hello",
        StatementDecoration::builtin
    );
    validate!(
        "command --help",
        "command",
        "--help",
        StatementDecoration::none
    );
    validate!("command -h", "command", "-h", StatementDecoration::none);
    validate!("command", "command", "", StatementDecoration::none);
    validate!("command -", "command", "-", StatementDecoration::none);
    validate!("command --", "command", "--", StatementDecoration::none);
    validate!(
        "builtin --names",
        "builtin",
        "--names",
        StatementDecoration::none
    );
    validate!("function", "function", "", StatementDecoration::none);
    validate!(
        "function --help",
        "function",
        "--help",
        StatementDecoration::none
    );

    // Verify that 'function -h' and 'function --help' are plain statements but 'function --foo' is
    // not (issue #1240).
    macro_rules! check_function_help {
        ($src:expr, $typ:expr) => {
            let ast = Ast::parse(L!($src), ParseTreeFlags::default(), None);
            assert!(!ast.errored());
            assert_eq!(
                Traversal::new(ast.top())
                    .filter(|n| n.typ() == $typ)
                    .count(),
                1
            );
        };
    }
    check_function_help!("function -h", ast::Type::decorated_statement);
    check_function_help!("function --help", ast::Type::decorated_statement);
    check_function_help!("function --foo; end", ast::Type::function_header);
    check_function_help!("function foo; end", ast::Type::function_header);
}

#[test]
#[serial]
fn test_new_parser_ad_hoc() {
    let _cleanup = test_init();
    // Very ad-hoc tests for issues encountered.

    // Ensure that 'case' terminates a job list.
    let src = L!("switch foo ; case bar; case baz; end");
    let ast = Ast::parse(src, ParseTreeFlags::default(), None);
    assert!(!ast.errored());
    // Expect two case_item_lists. The bug was that we'd
    // try to run a command 'case'.
    assert_eq!(
        Traversal::new(ast.top())
            .filter(|n| n.typ() == ast::Type::case_item)
            .count(),
        2
    );

    // Ensure that naked variable assignments don't hang.
    // The bug was that "a=" would produce an error but not be consumed,
    // leading to an infinite loop.

    // By itself it should produce an error.
    let ast = Ast::parse(L!("a="), ParseTreeFlags::default(), None);
    assert!(ast.errored());

    // If we are leaving things unterminated, this should not produce an error.
    // i.e. when typing "a=" at the command line, it should be treated as valid
    // because we don't want to color it as an error.
    let ast = Ast::parse(L!("a="), ParseTreeFlags::LEAVE_UNTERMINATED, None);
    assert!(!ast.errored());

    let mut errors = vec![];
    Ast::parse(
        L!("begin; echo ("),
        ParseTreeFlags::LEAVE_UNTERMINATED,
        Some(&mut errors),
    );
    assert!(errors.len() == 1);
    assert!(errors[0].code == ParseErrorCode::tokenizer_unterminated_subshell);

    errors.clear();
    Ast::parse(
        L!("for x in ("),
        ParseTreeFlags::LEAVE_UNTERMINATED,
        Some(&mut errors),
    );
    assert!(errors.len() == 1);
    assert!(errors[0].code == ParseErrorCode::tokenizer_unterminated_subshell);

    errors.clear();
    Ast::parse(
        L!("begin; echo '"),
        ParseTreeFlags::LEAVE_UNTERMINATED,
        Some(&mut errors),
    );
    assert!(errors.len() == 1);
    assert!(errors[0].code == ParseErrorCode::tokenizer_unterminated_quote);
}

#[test]
#[serial]
fn test_new_parser_errors() {
    let _cleanup = test_init();
    macro_rules! validate {
        ($src:expr, $expected_code:expr) => {
            let mut errors = vec![];
            let ast = Ast::parse(L!($src), ParseTreeFlags::default(), Some(&mut errors));
            assert!(ast.errored());
            assert_eq!(
                errors.into_iter().map(|e| e.code).collect::<Vec<_>>(),
                vec![$expected_code],
            );
        };
    }

    validate!("echo 'abc", ParseErrorCode::tokenizer_unterminated_quote);
    validate!("'", ParseErrorCode::tokenizer_unterminated_quote);
    validate!("echo (abc", ParseErrorCode::tokenizer_unterminated_subshell);

    validate!("end", ParseErrorCode::unbalancing_end);
    validate!("echo hi ; end", ParseErrorCode::unbalancing_end);

    validate!("else", ParseErrorCode::unbalancing_else);
    validate!("if true ; end ; else", ParseErrorCode::unbalancing_else);

    validate!("case", ParseErrorCode::unbalancing_case);
    validate!("if true ; case ; end", ParseErrorCode::generic);

    validate!("true | and", ParseErrorCode::andor_in_pipeline);

    validate!("a=", ParseErrorCode::bare_variable_assignment);
}

#[test]
#[serial]
fn test_eval_recursion_detection() {
    let _cleanup = test_init();
    // Ensure that we don't crash on infinite self recursion and mutual recursion.
    let parser = TestParser::new();
    parser.eval(
        L!("function recursive ; recursive ; end ; recursive; "),
        &IoChain::new(),
    );

    parser.eval(
        L!(concat!(
            "function recursive1 ; recursive2 ; end ; ",
            "function recursive2 ; recursive1 ; end ; recursive1; ",
        )),
        &IoChain::new(),
    );
}

#[test]
#[serial]
fn test_eval_illegal_exit_code() {
    let _cleanup = test_init();
    let parser = TestParser::new();
    macro_rules! validate {
        ($cmd:expr, $result:expr) => {
            parser.eval($cmd, &IoChain::new());
            let exit_status = parser.get_last_status();
            assert_eq!(exit_status, parser.get_last_status());
        };
    }

    // We need to be in an empty directory so that none of the wildcards match a file that might be
    // in the fish source tree. In particular we need to ensure that "?" doesn't match a file
    // named by a single character. See issue #3852.
    parser.pushd("test/temp");
    validate!(L!("echo -n"), STATUS_CMD_OK.unwrap());
    validate!(L!("pwd"), STATUS_CMD_OK.unwrap());
    validate!(
        L!("UNMATCHABLE_WILDCARD*"),
        STATUS_UNMATCHED_WILDCARD.unwrap()
    );
    validate!(
        L!("UNMATCHABLE_WILDCARD**"),
        STATUS_UNMATCHED_WILDCARD.unwrap()
    );
    validate!(L!("?"), STATUS_UNMATCHED_WILDCARD.unwrap());
    validate!(L!("abc?def"), STATUS_UNMATCHED_WILDCARD.unwrap());
    parser.popd();
}

#[test]
#[serial]
fn test_eval_empty_function_name() {
    let _cleanup = test_init();
    let parser = TestParser::new();
    parser.eval(
        L!("function '' ; echo fail; exit 42 ; end ; ''"),
        &IoChain::new(),
    );
}

#[test]
#[serial]
fn test_expand_argument_list() {
    let _cleanup = test_init();
    let parser = TestParser::new();
    let comps: Vec<WString> = Parser::expand_argument_list(
        L!("alpha 'beta gamma' delta"),
        ExpandFlags::default(),
        &parser.context(),
    )
    .into_iter()
    .map(|c| c.completion)
    .collect();
    assert_eq!(comps, &[L!("alpha"), L!("beta gamma"), L!("delta"),]);
}

fn test_1_cancellation(parser: &Parser, src: &wstr) {
    let filler = IoBufferfill::create().unwrap();
    let delay = Duration::from_millis(100);
    let thread = unsafe { libc::pthread_self() } as usize;
    iothread_perform(move || {
        // Wait a while and then SIGINT the main thread.
        std::thread::sleep(delay);
        unsafe {
            libc::pthread_kill(thread as libc::pthread_t, SIGINT);
        }
    });
    let mut io = IoChain::new();
    io.push(filler.clone());
    let res = parser.eval(src, &io);
    let buffer = IoBufferfill::finish(filler);
    assert_eq!(
        buffer.len(),
        0,
        "Expected 0 bytes in out_buff, but instead found {} bytes, for command {}",
        buffer.len(),
        src
    );
    assert!(res.status.signal_exited() && res.status.signal_code() == SIGINT);
}

#[test]
#[serial]
fn test_cancellation() {
    let _cleanup = test_init();
    let parser = Parser::new(Rc::new(EnvStack::new()), CancelBehavior::Clear);
    reader_push(&parser, L!(""), ReaderConfig::default());
    let _pop = ScopeGuard::new((), |()| reader_pop());

    println!("Testing Ctrl-C cancellation. If this hangs, that's a bug!");

    // Enable fish's signal handling here.
    signal_set_handlers(true);

    // This tests that we can correctly ctrl-C out of certain loop constructs, and that nothing gets
    // printed if we do.

    // Here the command substitution is an infinite loop. echo never even gets its argument, so when
    // we cancel we expect no output.
    test_1_cancellation(&parser, L!("echo (while true ; echo blah ; end)"));

    // Nasty infinite loop that doesn't actually execute anything.
    test_1_cancellation(
        &parser,
        L!("echo (while true ; end) (while true ; end) (while true ; end)"),
    );
    test_1_cancellation(&parser, L!("while true ; end"));
    test_1_cancellation(&parser, L!("while true ; echo nothing > /dev/null; end"));
    test_1_cancellation(&parser, L!("for i in (while true ; end) ; end"));

    signal_reset_handlers();

    // Ensure that we don't think we should cancel.
    reader_reset_interrupted();
    signal_clear_cancel();
}

#[test]
fn test_line_counter() {
    let src = L!("echo line1; echo still_line_1;\n\necho line3");
    let ps = parse_source(src.to_owned(), ParseTreeFlags::default(), None)
        .expect("Failed to parse source");
    assert!(!ps.ast.errored());
    let mut line_counter = ps.line_counter();

    // Test line_offset_of_character_at_offset, both forwards and backwards to exercise the cache.
    let mut expected = 0;
    for (idx, c) in src.chars().enumerate() {
        let line_offset = line_counter.line_offset_of_character_at_offset(idx);
        assert_eq!(line_offset, expected);
        if c == '\n' {
            expected += 1;
        }
    }
    for (idx, c) in src.chars().enumerate().rev() {
        if c == '\n' {
            expected -= 1;
        }
        let line_offset = line_counter.line_offset_of_character_at_offset(idx);
        assert_eq!(line_offset, expected);
    }

    fn ref_eq<T>(a: Option<&T>, b: Option<&T>) -> bool {
        match (a, b) {
            (Some(a), Some(b)) => std::ptr::eq(a, b),
            (None, None) => true,
            _ => false,
        }
    }

    let pipelines: Vec<_> = ps.ast.walk().filter_map(|n| n.as_job_pipeline()).collect();
    assert_eq!(pipelines.len(), 3);
    let src_offsets = [0, 0, 2];
    assert_eq!(line_counter.source_offset_of_node(), None);
    assert_eq!(line_counter.line_offset_of_node(), None);

    let mut last_set = None;
    for (idx, &node) in pipelines.iter().enumerate() {
        let orig = line_counter.set_node(Some(node));
        assert!(ref_eq(orig, last_set));
        last_set = Some(node);
        assert_eq!(
            line_counter.source_offset_of_node(),
            Some(node.source_range().start())
        );
        assert_eq!(line_counter.line_offset_of_node(), Some(src_offsets[idx]));
    }

    for (idx, &node) in pipelines.iter().enumerate().rev() {
        let orig = line_counter.set_node(Some(node));
        assert!(ref_eq(orig, last_set));
        last_set = Some(node);
        assert_eq!(
            line_counter.source_offset_of_node(),
            Some(node.source_range().start())
        );
        assert_eq!(line_counter.line_offset_of_node(), Some(src_offsets[idx]));
    }
}

#[test]
fn test_line_counter_empty() {
    let mut line_counter = LineCounter::<JobPipeline>::empty();
    assert_eq!(line_counter.line_offset_of_character_at_offset(0), 0);
    assert_eq!(line_counter.line_offset_of_node(), None);
    assert_eq!(line_counter.source_offset_of_node(), None);
}
