use crate::ast::{Ast, List, Node};
use crate::io::{IoBufferfill, IoChain};
use crate::parse_constants::{ParseTreeFlags, ParserTestErrorBits};
use crate::parse_util::{parse_util_detect_errors, parse_util_detect_errors_in_argument};
use crate::parser::Parser;
use crate::reader::reader_reset_interrupted;
use crate::signal::{signal_clear_cancel, signal_reset_handlers, signal_set_handlers};
use crate::threads::{iothread_drain_all, iothread_perform};
use crate::wchar::prelude::*;
use libc::SIGINT;
use std::time::Duration;

use crate::ffi_tests::add_test;
add_test!("test_parser", || {
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
});

fn test_1_cancellation(src: &wstr) {
    let filler = IoBufferfill::create().unwrap();
    let delay = Duration::from_millis(500);
    let thread = unsafe { libc::pthread_self() };
    iothread_perform(move || {
        // Wait a while and then SIGINT the main thread.
        std::thread::sleep(delay);
        unsafe {
            libc::pthread_kill(thread, SIGINT);
        }
    });
    let mut io = IoChain::new();
    io.push(filler.clone());
    let res = Parser::principal_parser().eval(src, &io);
    let buffer = IoBufferfill::finish(filler);
    assert_eq!(
        buffer.len(),
        0,
        "Expected 0 bytes in out_buff, but instead found {} bytes, for command {}",
        buffer.len(),
        src
    );
    assert!(res.status.signal_exited() && res.status.signal_code() == SIGINT);
    unsafe {
        iothread_drain_all();
    }
}

add_test!("test_cancellation", || {
    println!("Testing Ctrl-C cancellation. If this hangs, that's a bug!");

    // Enable fish's signal handling here.
    signal_set_handlers(true);

    // This tests that we can correctly ctrl-C out of certain loop constructs, and that nothing gets
    // printed if we do.

    // Here the command substitution is an infinite loop. echo never even gets its argument, so when
    // we cancel we expect no output.
    test_1_cancellation(L!("echo (while true ; echo blah ; end)"));

    // Nasty infinite loop that doesn't actually execute anything.
    test_1_cancellation(L!(
        "echo (while true ; end) (while true ; end) (while true ; end)"
    ));
    test_1_cancellation(L!("while true ; end"));
    test_1_cancellation(L!("while true ; echo nothing > /dev/null; end"));
    test_1_cancellation(L!("for i in (while true ; end) ; end"));

    signal_reset_handlers();

    // Ensure that we don't think we should cancel.
    reader_reset_interrupted();
    signal_clear_cancel();
});
