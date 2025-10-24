use crate::redirection::RedirectionMode;
use crate::tokenizer::{
    MoveWordStateMachine, MoveWordStyle, PipeOrRedir, TokFlags, TokenType, Tokenizer,
    TokenizerError,
};
use crate::wchar::prelude::*;
use libc::{STDERR_FILENO, STDOUT_FILENO};
use std::collections::HashSet;

#[test]
fn test_tokenizer() {
    {
        let s = L!("alpha beta");
        let mut t = Tokenizer::new(s, TokFlags(0));

        let token = t.next(); // alpha
        assert!(token.is_some());
        let token = token.unwrap();
        assert_eq!(token.type_, TokenType::string);
        assert_eq!(token.length, 5);
        assert_eq!(t.text_of(&token), "alpha");

        let token = t.next(); // beta
        assert!(token.is_some());
        let token = token.unwrap();
        assert_eq!(token.type_, TokenType::string);
        assert_eq!(token.offset, 6);
        assert_eq!(token.length, 4);
        assert_eq!(t.text_of(&token), "beta");

        assert!(t.next().is_none());
    }

    {
        let s = L!("{ echo");
        let mut t = Tokenizer::new(s, TokFlags(0));

        let token = t.next(); // {
        assert!(token.is_some());
        let token = token.unwrap();
        assert_eq!(token.type_, TokenType::left_brace);
        assert_eq!(token.length, 1);
        assert_eq!(t.text_of(&token), "{");

        let token = t.next(); // echo
        assert!(token.is_some());
        let token = token.unwrap();
        assert_eq!(token.type_, TokenType::string);
        assert_eq!(token.offset, 2);
        assert_eq!(token.length, 4);
        assert_eq!(t.text_of(&token), "echo");

        assert!(t.next().is_none());
    }

    {
        let s = L!("{echo, foo}");
        let mut t = Tokenizer::new(s, TokFlags(0));
        let token = t.next().unwrap();
        assert_eq!(token.type_, TokenType::left_brace);
        assert_eq!(token.length, 1);
    }
    {
        let s = L!("{ echo; foo}");
        let mut t = Tokenizer::new(s, TokFlags(0));
        let token = t.next().unwrap();
        assert_eq!(token.type_, TokenType::left_brace);
    }

    {
        let s = L!("{ | { name } '");
        let mut t = Tokenizer::new(s, TokFlags(0));
        let mut next_type = || t.next().unwrap().type_;
        assert_eq!(next_type(), TokenType::left_brace);
        assert_eq!(next_type(), TokenType::pipe);
        assert_eq!(next_type(), TokenType::left_brace);
        assert_eq!(next_type(), TokenType::string);
        assert_eq!(next_type(), TokenType::right_brace);
        assert_eq!(next_type(), TokenType::error);
        assert!(t.next().is_none());
    }

    let s = L!(concat!(
        "string <redirection  2>&1 'nested \"quoted\" '(string containing subshells ",
        "){and,brackets}$as[$well (as variable arrays)] not_a_redirect^ ^ ^^is_a_redirect ",
        "&| &> ",
        "&&& ||| ",
        "&& || & |",
        "Compress_Newlines\n  \n\t\n   \nInto_Just_One",
    ));
    type tt = TokenType;
    #[rustfmt::skip]
    let types = [
        tt::string, tt::redirect, tt::string, tt::redirect, tt::string, tt::string, tt::string,
        tt::string, tt::string, tt::pipe, tt::redirect, tt::andand, tt::background, tt::oror,
        tt::pipe, tt::andand, tt::oror, tt::background, tt::pipe, tt::string, tt::end, tt::string,
    ];

    {
        let t = Tokenizer::new(s, TokFlags(0));
        let mut actual_types = vec![];
        for token in t {
            actual_types.push(token.type_);
        }
        assert_eq!(&actual_types[..], types);
    }

    // Test some errors.

    {
        let mut t = Tokenizer::new(L!("abc\\"), TokFlags(0));
        let token = t.next().unwrap();
        assert_eq!(token.type_, TokenType::error);
        assert_eq!(token.error, TokenizerError::unterminated_escape);
        assert_eq!(token.error_offset_within_token, 3);
    }

    {
        let mut t = Tokenizer::new(L!("abc )defg(hij"), TokFlags(0));
        let _token = t.next().unwrap();
        let token = t.next().unwrap();
        assert_eq!(token.type_, TokenType::error);
        assert_eq!(token.error, TokenizerError::closing_unopened_subshell);
        assert_eq!(token.offset, 4);
        assert_eq!(token.error_offset_within_token, 0);
    }

    {
        let mut t = Tokenizer::new(L!("abc defg(hij (klm)"), TokFlags(0));
        let _token = t.next().unwrap();
        let token = t.next().unwrap();
        assert_eq!(token.type_, TokenType::error);
        assert_eq!(token.error, TokenizerError::unterminated_subshell);
        assert_eq!(token.error_offset_within_token, 4);
    }

    {
        let mut t = Tokenizer::new(L!("abc defg[hij (klm)"), TokFlags(0));
        let _token = t.next().unwrap();
        let token = t.next().unwrap();
        assert_eq!(token.type_, TokenType::error);
        assert_eq!(token.error, TokenizerError::unterminated_slice);
        assert_eq!(token.error_offset_within_token, 4);
    }

    // Test some redirection parsing.
    macro_rules! pipe_or_redir {
        ($s:literal) => {
            PipeOrRedir::try_from(L!($s)).unwrap()
        };
    }

    assert!(pipe_or_redir!("|").is_pipe);
    assert!(pipe_or_redir!("0>|").is_pipe);
    assert_eq!(pipe_or_redir!("0>|").fd, 0);
    assert!(pipe_or_redir!("2>|").is_pipe);
    assert_eq!(pipe_or_redir!("2>|").fd, 2);
    assert!(pipe_or_redir!(">|").is_pipe);
    assert_eq!(pipe_or_redir!(">|").fd, STDOUT_FILENO);
    assert!(!pipe_or_redir!(">").is_pipe);
    assert_eq!(pipe_or_redir!(">").fd, STDOUT_FILENO);
    assert_eq!(pipe_or_redir!("2>").fd, STDERR_FILENO);
    assert_eq!(pipe_or_redir!("9999999999999>").fd, -1);
    assert_eq!(pipe_or_redir!("9999999999999>&2").fd, -1);
    assert!(!pipe_or_redir!("9999999999999>&2").is_valid());
    assert!(!pipe_or_redir!("9999999999999>&2").is_valid());

    assert!(pipe_or_redir!("&|").is_pipe);
    assert!(pipe_or_redir!("&|").stderr_merge);
    assert!(!pipe_or_redir!("&>").is_pipe);
    assert!(pipe_or_redir!("&>").stderr_merge);
    assert!(pipe_or_redir!("&>>").stderr_merge);
    assert!(pipe_or_redir!("&>?").stderr_merge);

    macro_rules! get_redir_mode {
        ($s:literal) => {
            pipe_or_redir!($s).mode
        };
    }

    assert_eq!(get_redir_mode!("<"), RedirectionMode::input);
    assert_eq!(get_redir_mode!(">"), RedirectionMode::overwrite);
    assert_eq!(get_redir_mode!("2>"), RedirectionMode::overwrite);
    assert_eq!(get_redir_mode!(">>"), RedirectionMode::append);
    assert_eq!(get_redir_mode!("2>>"), RedirectionMode::append);
    assert_eq!(get_redir_mode!("2>?"), RedirectionMode::noclob);
    assert_eq!(
        get_redir_mode!("9999999999999999>?"),
        RedirectionMode::noclob
    );
    assert_eq!(get_redir_mode!("2>&3"), RedirectionMode::fd);
    assert_eq!(get_redir_mode!("3<&0"), RedirectionMode::fd);
    assert_eq!(get_redir_mode!("3</tmp/filetxt"), RedirectionMode::input);
}

/// Test word motion (forward-word, etc.). Carets represent cursor stops.
#[test]
fn test_word_motion() {
    #[derive(Eq, PartialEq)]
    pub enum Direction {
        Left,
        Right,
    }

    macro_rules! validate {
        ($direction:expr, $style:expr, $line:expr) => {
            let mut command = WString::new();
            let mut stops = HashSet::new();

            // Carets represent stops and should be cut out of the command.
            for c in $line.chars() {
                if c == '^' {
                    stops.insert(command.len());
                } else {
                    command.push(c);
                }
            }

            let (mut idx, end) = if $direction == Direction::Left {
                (stops.iter().max().unwrap().clone(), 0)
            } else {
                (stops.iter().min().unwrap().clone(), command.len())
            };
            stops.remove(&idx);

            let mut sm = MoveWordStateMachine::new($style);
            while idx != end {
                let char_idx = if $direction == Direction::Left {
                    idx - 1
                } else {
                    idx
                };
                let c = command.as_char_slice()[char_idx];
                let will_stop = !sm.consume_char(c);
                let expected_stop = stops.contains(&idx);
                assert_eq!(will_stop, expected_stop);
                // We don't expect to stop here next time.
                if expected_stop {
                    stops.remove(&idx);
                    sm.reset();
                } else {
                    if $direction == Direction::Left {
                        idx -= 1;
                    } else {
                        idx += 1;
                    }
                }
            }
        };
    }

    validate!(
        Direction::Left,
        MoveWordStyle::Punctuation,
        "^echo ^hello_^world.^txt^"
    );
    validate!(
        Direction::Right,
        MoveWordStyle::Punctuation,
        "^echo^ hello^_world^.txt^"
    );

    validate!(
        Direction::Left,
        MoveWordStyle::Punctuation,
        "echo ^foo_^foo_^foo/^/^/^/^/^    ^"
    );
    validate!(
        Direction::Right,
        MoveWordStyle::Punctuation,
        "^echo^ foo^_foo^_foo^/^/^/^/^/    ^"
    );

    validate!(
        Direction::Left,
        MoveWordStyle::PathComponents,
        "^/^foo/^bar/^baz/^"
    );
    validate!(
        Direction::Left,
        MoveWordStyle::PathComponents,
        "^echo ^--foo ^--bar^"
    );
    validate!(
        Direction::Left,
        MoveWordStyle::PathComponents,
        "^echo ^hi ^> ^/^dev/^null^"
    );

    validate!(
        Direction::Left,
        MoveWordStyle::PathComponents,
        "^echo ^/^foo/^bar{^aaa,^bbb,^ccc}^bak/^"
    );
    validate!(
        Direction::Left,
        MoveWordStyle::PathComponents,
        "^echo ^bak ^///^"
    );
    validate!(
        Direction::Left,
        MoveWordStyle::PathComponents,
        "^aaa ^@ ^@^aaa^"
    );
    validate!(
        Direction::Left,
        MoveWordStyle::PathComponents,
        "^aaa ^a ^@^aaa^"
    );
    validate!(
        Direction::Left,
        MoveWordStyle::PathComponents,
        "^aaa ^@@@ ^@@^aa^"
    );
    validate!(
        Direction::Left,
        MoveWordStyle::PathComponents,
        "^aa^@@  ^aa@@^a^"
    );

    validate!(Direction::Right, MoveWordStyle::Punctuation, "^a^ bcd^");
    validate!(Direction::Right, MoveWordStyle::Punctuation, "a^b^ cde^");
    validate!(Direction::Right, MoveWordStyle::Punctuation, "^ab^ cde^");
    validate!(
        Direction::Right,
        MoveWordStyle::Punctuation,
        "^ab^&cd^ ^& ^e^ f^&"
    );

    validate!(
        Direction::Right,
        MoveWordStyle::Whitespace,
        "^^a-b-c^ d-e-f"
    );
    validate!(
        Direction::Right,
        MoveWordStyle::Whitespace,
        "^a-b-c^\n d-e-f^ "
    );
    validate!(
        Direction::Right,
        MoveWordStyle::Whitespace,
        "^a-b-c^\n\nd-e-f^ "
    );
}
