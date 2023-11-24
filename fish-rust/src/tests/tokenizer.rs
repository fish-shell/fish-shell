use crate::redirection::RedirectionMode;
use crate::tokenizer::{PipeOrRedir, TokFlags, TokenType, Tokenizer, TokenizerError};
use crate::wchar::prelude::*;
use libc::{STDERR_FILENO, STDOUT_FILENO};

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
    assert_eq!(pipe_or_redir!("9999999999999>&2").is_valid(), false);
    assert_eq!(pipe_or_redir!("9999999999999>&2").is_valid(), false);

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
