//! A specialized tokenizer for tokenizing the fish language. In the future, the tokenizer should be
//! extended to support marks, tokenizing multiple strings and disposing of unused string segments.

use crate::ast::unescape_keyword;
use crate::common::valid_var_name_char;
use crate::future_feature_flags::{FeatureFlag, feature_test};
use crate::parse_constants::SOURCE_OFFSET_INVALID;
use crate::parser_keywords::parser_keywords_is_subcommand;
use crate::prelude::*;
use crate::redirection::RedirectionMode;
use libc::{STDIN_FILENO, STDOUT_FILENO};
use nix::fcntl::OFlag;
use std::ops::{BitAnd, BitAndAssign, BitOr, BitOrAssign, Not, Range};
use std::os::fd::RawFd;

/// Token types. XXX Why this isn't ParseTokenType, I'm not really sure.
#[derive(Clone, Copy, Debug, Eq, PartialEq)]
pub enum TokenType {
    /// Error reading token
    Error,
    /// String token
    String,
    /// Pipe token
    Pipe,
    /// && token
    AndAnd,
    /// || token
    OrOr,
    /// End token (semicolon or newline, not literal end)
    End,
    /// opening brace of a compound statement
    LeftBrace,
    /// closing brace of a compound statement
    RightBrace,
    /// redirection token
    Redirect,
    /// send job to bg token
    Background,
    /// comment token
    Comment,
}

#[derive(Copy, Clone, Debug, Eq, PartialEq)]
pub enum TokenizerError {
    None,
    UnterminatedQuote,
    UnterminatedSubshell,
    UnterminatedSlice,
    UnterminatedEscape,
    InvalidRedirect,
    InvalidPipe,
    InvalidPipeAmpersand,
    ClosingUnopenedSubshell,
    IllegalSlice,
    ClosingUnopenedBrace,
    UnterminatedBrace,
    ExpectedPcloseFoundBclose,
    ExpectedBcloseFoundPclose,
}

#[derive(Debug)]
pub struct Tok {
    // Offset of the token.
    pub offset: u32,
    // Length of the token.
    pub length: u32,

    // If an error, this is the offset of the error within the token. A value of 0 means it occurred
    // at 'offset'.
    pub error_offset_within_token: u32,
    pub error_length: u32,

    // If an error, this is the error code.
    pub error: TokenizerError,

    pub is_unterminated_brace: bool,

    // The type of the token.
    pub type_: TokenType,
}
// TODO static_assert(sizeof(Tok) <= 32, "Tok expected to be 32 bytes or less");

/// Struct wrapping up a parsed pipe or redirection.
pub struct PipeOrRedir {
    // The redirected fd, or -1 on overflow.
    // In the common case of a pipe, this is 1 (STDOUT_FILENO).
    // For example, in the case of "3>&1" this will be 3.
    pub fd: i32,

    // Whether we are a pipe (true) or redirection (false).
    pub is_pipe: bool,

    // The redirection mode if the type is redirect.
    // Ignored for pipes.
    pub mode: RedirectionMode,

    // Whether, in addition to this redirection, stderr should also be dup'd to stdout
    // For example &| or &>
    pub stderr_merge: bool,

    // Number of characters consumed when parsing the string.
    pub consumed: usize,
}

#[derive(Clone, Copy)]
pub struct TokFlags(pub u8);

impl BitAnd for TokFlags {
    type Output = bool;
    fn bitand(self, rhs: Self) -> Self::Output {
        (self.0 & rhs.0) != 0
    }
}
impl BitOr for TokFlags {
    type Output = Self;
    fn bitor(self, rhs: Self) -> Self::Output {
        Self(self.0 | rhs.0)
    }
}
impl BitOrAssign for TokFlags {
    fn bitor_assign(&mut self, rhs: Self) {
        self.0 |= rhs.0
    }
}

/// Flag telling the tokenizer to accept incomplete parameters, i.e. parameters with mismatching
/// parenthesis, etc. This is useful for tab-completion.
pub const TOK_ACCEPT_UNFINISHED: TokFlags = TokFlags(1);

/// Flag telling the tokenizer not to remove comments. Useful for syntax highlighting.
pub const TOK_SHOW_COMMENTS: TokFlags = TokFlags(2);

/// Ordinarily, the tokenizer ignores newlines following a newline, or a semicolon. This flag tells
/// the tokenizer to return each of them as a separate END.
pub const TOK_SHOW_BLANK_LINES: TokFlags = TokFlags(4);

/// Make an effort to continue after an error.
pub const TOK_CONTINUE_AFTER_ERROR: TokFlags = TokFlags(8);

/// Consumers want to treat all tokens as arguments, so disable special handling at
/// command-position.
pub const TOK_ARGUMENT_LIST: TokFlags = TokFlags(16);

impl From<TokenizerError> for &'static wstr {
    fn from(err: TokenizerError) -> Self {
        match err {
            TokenizerError::None => L!(""),
            TokenizerError::UnterminatedQuote => {
                wgettext!("Unexpected end of string, quotes are not balanced")
            }
            TokenizerError::UnterminatedSubshell => {
                wgettext!("Unexpected end of string, expecting ')'")
            }
            TokenizerError::UnterminatedSlice => {
                wgettext!("Unexpected end of string, square brackets do not match")
            }
            TokenizerError::UnterminatedEscape => {
                wgettext!("Unexpected end of string, incomplete escape sequence")
            }
            TokenizerError::InvalidRedirect => {
                wgettext!("Invalid input/output redirection")
            }
            TokenizerError::InvalidPipe => {
                wgettext!("Cannot use stdin (fd 0) as pipe output")
            }
            TokenizerError::InvalidPipeAmpersand => {
                wgettext!("|& is not valid. In fish, use &| to pipe both stdout and stderr.")
            }
            TokenizerError::ClosingUnopenedSubshell => {
                wgettext!("Unexpected ')' for unopened parenthesis")
            }
            TokenizerError::IllegalSlice => {
                wgettext!("Unexpected '[' at this location")
            }
            TokenizerError::ClosingUnopenedBrace => {
                wgettext!("Unexpected '}' for unopened brace")
            }
            TokenizerError::UnterminatedBrace => {
                wgettext!("Unexpected end of string, incomplete parameter expansion")
            }
            TokenizerError::ExpectedPcloseFoundBclose => {
                wgettext!("Unexpected '}' found, expecting ')'")
            }
            TokenizerError::ExpectedBcloseFoundPclose => {
                wgettext!("Unexpected ')' found, expecting '}'")
            }
        }
    }
}

impl fish_printf::ToArg<'static> for TokenizerError {
    fn to_arg(self) -> fish_printf::Arg<'static> {
        let msg: &'static wstr = self.into();
        fish_printf::Arg::WStr(msg)
    }
}

impl Tok {
    fn new(r#type: TokenType) -> Tok {
        Tok {
            offset: 0,
            length: 0,
            error_offset_within_token: SOURCE_OFFSET_INVALID.try_into().unwrap(),
            error_length: 0,
            error: TokenizerError::None,
            is_unterminated_brace: false,
            type_: r#type,
        }
    }
    pub fn location_in_or_at_end_of_source_range(self: &Tok, loc: usize) -> bool {
        let loc = loc as u32;
        self.offset <= loc && loc - self.offset <= self.length
    }
    pub fn get_source<'a, 'b>(self: &'a Tok, str: &'b wstr) -> &'b wstr {
        &str[self.offset as usize..(self.offset + self.length) as usize]
    }
    pub fn set_offset(&mut self, value: usize) {
        self.offset = value.try_into().unwrap();
    }
    pub fn offset(&self) -> usize {
        self.offset.try_into().unwrap()
    }
    pub fn length(&self) -> usize {
        self.length.try_into().unwrap()
    }
    pub fn set_length(&mut self, value: usize) {
        self.length = value.try_into().unwrap();
    }
    pub fn end(&self) -> usize {
        self.offset() + self.length()
    }
    pub fn range(&self) -> Range<usize> {
        self.offset()..self.end()
    }
    pub fn set_error_offset_within_token(&mut self, value: usize) {
        self.error_offset_within_token = value.try_into().unwrap();
    }
    pub fn error_offset_within_token(&self) -> usize {
        self.error_offset_within_token.try_into().unwrap()
    }
    pub fn error_length(&self) -> usize {
        self.error_length.try_into().unwrap()
    }
    pub fn set_error_length(&mut self, value: usize) {
        self.error_length = value.try_into().unwrap();
    }
}

struct BraceStatementParser {
    at_command_position: bool,
    unclosed_brace_statements: usize,
}

/// The tokenizer struct.
pub struct Tokenizer<'c> {
    /// A pointer into the original string, showing where the next token begins.
    token_cursor: usize,
    /// The start of the original string.
    start: &'c wstr,
    /// Whether we have additional tokens.
    has_next: bool,
    /// Parser state regarding brace statements. None if reading an argument list.
    brace_statement_parser: Option<BraceStatementParser>,
    /// Whether incomplete tokens are accepted.
    accept_unfinished: bool,
    /// Whether comments should be returned.
    show_comments: bool,
    /// Whether all blank lines are returned.
    show_blank_lines: bool,
    /// Whether to attempt to continue after an error.
    continue_after_error: bool,
    /// Whether to continue the previous line after the comment.
    continue_line_after_comment: bool,
    /// Called on every quote change.
    on_quote_toggle: Option<&'c mut dyn FnMut(usize)>,
}

impl<'c> Tokenizer<'c> {
    /// Constructor for a tokenizer. b is the string that is to be tokenized. It is not copied, and
    /// should not be freed by the caller until after the tokenizer is destroyed.
    ///
    /// \param start The string to tokenize
    /// \param flags Flags to the tokenizer. Setting TOK_ACCEPT_UNFINISHED will cause the tokenizer
    /// to accept incomplete tokens, such as a subshell without a closing parenthesis, as a valid
    /// token. Setting TOK_SHOW_COMMENTS will return comments as tokens
    pub fn new(start: &'c wstr, flags: TokFlags) -> Self {
        Self::new_impl(start, flags, None)
    }
    pub fn with_quote_events(
        start: &'c wstr,
        flags: TokFlags,
        on_quote_toggle: &'c mut dyn FnMut(usize),
    ) -> Self {
        Self::new_impl(start, flags, Some(on_quote_toggle))
    }
    fn new_impl(
        start: &'c wstr,
        flags: TokFlags,
        on_quote_toggle: Option<&'c mut dyn FnMut(usize)>,
    ) -> Self {
        Tokenizer {
            token_cursor: 0,
            start,
            has_next: true,
            brace_statement_parser: (!(flags & TOK_ARGUMENT_LIST)).then_some(
                BraceStatementParser {
                    at_command_position: true,
                    unclosed_brace_statements: 0,
                },
            ),
            accept_unfinished: flags & TOK_ACCEPT_UNFINISHED,
            show_comments: flags & TOK_SHOW_COMMENTS,
            show_blank_lines: flags & TOK_SHOW_BLANK_LINES,
            continue_after_error: flags & TOK_CONTINUE_AFTER_ERROR,
            continue_line_after_comment: false,
            on_quote_toggle,
        }
    }
}

impl<'c> Iterator for Tokenizer<'c> {
    type Item = Tok;

    fn next(&mut self) -> Option<Self::Item> {
        if !self.has_next {
            return None;
        }

        // Consume non-newline whitespace. If we get an escaped newline, mark it and continue past
        // it.
        loop {
            let i = self.token_cursor;
            if self.start.get(i..i + 2) == Some(L!("\\\n")) {
                self.token_cursor += 2;
                self.continue_line_after_comment = true;
            } else if i < self.start.len() && iswspace_not_nl(self.start.char_at(i)) {
                self.token_cursor += 1;
            } else {
                break;
            }
        }

        while self.start.char_at(self.token_cursor) == '#' {
            // We have a comment, walk over the comment.
            let comment_start = self.token_cursor;
            self.token_cursor = comment_end(self.start, self.token_cursor);
            let comment_len = self.token_cursor - comment_start;

            // If we are going to continue after the comment, skip any trailing newline.
            if self.start.as_char_slice().get(self.token_cursor) == Some(&'\n')
                && self.continue_line_after_comment
            {
                self.token_cursor += 1;
            }

            // Maybe return the comment.
            if self.show_comments {
                let mut result = Tok::new(TokenType::Comment);
                result.offset = comment_start as u32;
                result.length = comment_len as u32;
                return Some(result);
            }

            while self.token_cursor < self.start.len()
                && iswspace_not_nl(self.start.char_at(self.token_cursor))
            {
                self.token_cursor += 1;
            }
        }

        // We made it past the comments and ate any trailing newlines we wanted to ignore.
        self.continue_line_after_comment = false;
        let start_pos = self.token_cursor;

        let this_char = self.start.char_at(self.token_cursor);
        let next_char = self
            .start
            .as_char_slice()
            .get(self.token_cursor + 1)
            .copied();
        let buff = &self.start[self.token_cursor..];
        let mut at_cmd_pos = false;
        let token = match this_char {
            '\0'=> {
                self.has_next = false;
                None
            }
            '\r'|  // carriage-return
            '\n'|  // newline
            ';'=> {
                let mut result = Tok::new(TokenType::End);
                result.offset = start_pos as u32;
                result.length = 1;
                self.token_cursor += 1;
                at_cmd_pos = true;
                // Hack: when we get a newline, swallow as many as we can. This compresses multiple
                // subsequent newlines into a single one.
                if !self.show_blank_lines {
                    while self.token_cursor < self.start.len() {
                        let c = self.start.char_at(self.token_cursor);
                        if c != '\n' && c != '\r' && c != ' ' && c != '\t' {
                            break
                        }
                        self.token_cursor += 1;
                    }
                }
                Some(result)
            }
            '{' if self.brace_statement_parser.as_ref()
    				.is_some_and(|parser| parser.at_command_position) =>
			{
                self.brace_statement_parser.as_mut().unwrap().unclosed_brace_statements += 1;
                let mut result = Tok::new(TokenType::LeftBrace);
                result.offset = start_pos as u32;
                result.length = 1;
                self.token_cursor += 1;
                at_cmd_pos = true;
                Some(result)
            }
            '}' => {
                let brace_count = self.brace_statement_parser.as_mut()
                    .map(|parser| &mut parser.unclosed_brace_statements);
                if brace_count.as_ref().is_none_or(|count| **count == 0) {
                    return Some(self.call_error(
                        TokenizerError::ClosingUnopenedBrace,
                        self.token_cursor,
                        self.token_cursor,
                        Some(1),
                        1,
                    ));
                }
                brace_count.map(|count| *count -= 1);
                let mut result = Tok::new(TokenType::RightBrace);
                result.offset = start_pos as u32;
                result.length = 1;
                self.token_cursor += 1;
                Some(result)
            }
            '&'=> {
                if next_char == Some('&') {
                    // && is and.
                    let mut result = Tok::new(TokenType::AndAnd);
                    result.offset = start_pos as u32;
                    result.length = 2;
                    self.token_cursor += 2;
                    at_cmd_pos = true;
                    Some(result)
                } else if next_char == Some('>') || next_char == Some('|') {
                    // &> and &| redirect both stdout and stderr.
                    let redir = PipeOrRedir::try_from(buff).
                        expect("Should always succeed to parse a &> or &| redirection");
                    let mut result = Tok::new(redir.token_type());
                    result.offset = start_pos as u32;
                    result.length = redir.consumed as u32;
                    self.token_cursor += redir.consumed;
                    at_cmd_pos = next_char == Some('|');
                    Some(result)
                } else {
                    let mut result = Tok::new(TokenType::Background);
                    result.offset = start_pos as u32;
                    result.length = 1;
                    self.token_cursor += 1;
                    at_cmd_pos = true;
                    Some(result)
                }
            }
            '|'=> {
                if next_char == Some('|') {
                    // || is or.
                        let mut result=Tok::new(TokenType::OrOr);
                    result.offset = start_pos as u32;
                    result.length = 2;
                    self.token_cursor += 2;
                    at_cmd_pos = true;
                    Some(result)
                } else if next_char == Some('&') {
                    // |& is a bashism; in fish it's &|.
                    Some(self.call_error(TokenizerError::InvalidPipeAmpersand,
                                            self.token_cursor, self.token_cursor, Some(2), 2))
                } else {
                    let pipe = PipeOrRedir::try_from(buff).
                        expect("Should always succeed to parse a | pipe");
                    let mut result = Tok::new(pipe.token_type());
                    result.offset = start_pos as u32;
                    result.length = pipe.consumed as u32;
                    self.token_cursor += pipe.consumed;
                    at_cmd_pos = true;
                    Some(result)
                }
            }
            '>'| '<' => {
                // There's some duplication with the code in the default case below. The key
                // difference here is that we must never parse these as a string; a failed
                // redirection is an error!
                 match PipeOrRedir::try_from(buff) {
                    Ok(redir_or_pipe) => {
                        if redir_or_pipe.fd < 0 {
                            Some(self.call_error(TokenizerError::InvalidRedirect, self.token_cursor,
                                            self.token_cursor,
                                            Some(redir_or_pipe.consumed),
                                            redir_or_pipe.consumed))
                        } else {
                            let mut result = Tok::new(redir_or_pipe.token_type());
                            result.offset = start_pos as u32;
                            result.length = redir_or_pipe.consumed as u32;
                            self.token_cursor += redir_or_pipe.consumed;
                            Some(result)
                        }
                    }
                    Err(()) => Some(self.call_error(TokenizerError::InvalidRedirect, self.token_cursor,
                                            self.token_cursor,
                                            Some(0),
                                            0))
                }
            }
                _ => {
                    // Maybe a redirection like '2>&1', maybe a pipe like 2>|, maybe just a string.
                    let error_location = self.token_cursor;
                    let redir_or_pipe = if this_char.is_ascii_digit() {
                        PipeOrRedir::try_from(buff).ok()
                    } else {
                        None
                    };

                    match redir_or_pipe {
                        Some(redir_or_pipe) => {
                            // It looks like a redirection or a pipe. But we don't support piping fd 0. Note
                            // tSome(hat fd 0 may be -1, indicating overflow; but we don't treat that as a
                            // tokenizer error.
                            if redir_or_pipe.is_pipe && redir_or_pipe.fd == 0 {
                                Some(self.call_error(TokenizerError::InvalidPipe, error_location,
                                                        error_location, Some(redir_or_pipe.consumed),
                                                        redir_or_pipe.consumed))
                            }
                            else {
                                let mut result = Tok::new(redir_or_pipe.token_type());
                                result.offset = start_pos as u32;
                                result.length = redir_or_pipe.consumed as u32;
                                self.token_cursor += redir_or_pipe.consumed;
                                at_cmd_pos = redir_or_pipe.is_pipe;
                                Some(result)
                            }
                        }
                        None => {
                            // Not a redirection or pipe, so just a string.
                            let s = self.read_string();
                            at_cmd_pos = self.brace_statement_parser.as_ref()
                                .is_some_and(|parser| parser.at_command_position) && {
                                let text = self.text_of(&s);
                                parser_keywords_is_subcommand(&unescape_keyword(
                                    TokenType::String,
                                    text)
                                ) ||
                                variable_assignment_equals_pos(text).is_some()
                            };
                            Some(s)
                    }
                }
            }
        };
        if let Some(parser) = self.brace_statement_parser.as_mut() {
            parser.at_command_position = at_cmd_pos;
        }
        token
    }
}

/// Test if a character is whitespace. Differs from iswspace in that it does not consider a
/// newline to be whitespace.
fn iswspace_not_nl(c: char) -> bool {
    match c {
        ' ' | '\t' | '\r' => true,
        '\n' => false,
        _ => c.is_whitespace(),
    }
}

impl<'c> Tokenizer<'c> {
    /// Returns the text of a token, as a string.
    pub fn text_of(&self, tok: &Tok) -> &wstr {
        tok.get_source(self.start)
    }

    /// Return an error token and mark that we no longer have a next token.
    fn call_error(
        &mut self,
        error_type: TokenizerError,
        token_start: usize,
        error_loc: usize,
        token_length: Option<usize>,
        error_len: usize,
    ) -> Tok {
        assert_ne!(
            error_type,
            TokenizerError::None,
            "TokenizerError::none passed to call_error"
        );
        assert!(error_loc >= token_start, "Invalid error location");
        assert!(self.token_cursor >= token_start, "Invalid buff location");

        // If continue_after_error is set and we have a real token length, then skip past it.
        // Otherwise give up.
        match token_length {
            Some(token_length) if self.continue_after_error => {
                assert!(
                    self.token_cursor < error_loc + token_length,
                    "Unable to continue past error"
                );
                self.token_cursor = error_loc + token_length;
            }
            _ => self.has_next = false,
        }

        Tok {
            offset: token_start as u32,
            length: token_length.unwrap_or(self.token_cursor - token_start) as u32,
            error_offset_within_token: (error_loc - token_start) as u32,
            error_length: error_len as u32,
            error: error_type,
            is_unterminated_brace: false,
            type_: TokenType::Error,
        }
    }
}

impl<'c> Tokenizer<'c> {
    /// Read the next token as a string.
    fn read_string(&mut self) -> Tok {
        let mut mode = TOK_MODE_REGULAR_TEXT;
        let mut paran_offsets = vec![];
        let mut brace_offsets = vec![];
        let mut expecting = vec![];
        let mut quoted_cmdsubs = vec![];
        let mut slice_offset = 0;
        let buff_start = self.token_cursor;
        let mut is_token_begin = true;

        fn process_opening_quote(
            zelf: &mut Tokenizer,
            quoted_cmdsubs: &mut Vec<usize>,
            paran_offsets: &[usize],
            quote: char,
        ) -> Result<(), usize> {
            zelf.on_quote_toggle
                .as_mut()
                .map(|cb| (cb)(zelf.token_cursor));
            if let Some(end) = quote_end(zelf.start, zelf.token_cursor, quote) {
                let mut one_past_end = end + 1;
                if zelf.start.char_at(end) == '$' {
                    one_past_end = end;
                    quoted_cmdsubs.push(paran_offsets.len());
                }
                zelf.token_cursor = end;
                zelf.on_quote_toggle.as_mut().map(|cb| (cb)(one_past_end));
                Ok(())
            } else {
                let error_loc = zelf.token_cursor;
                zelf.token_cursor = zelf.start.len();
                Err(error_loc)
            }
        }

        while self.token_cursor != self.start.len() {
            let c = self.start.char_at(self.token_cursor);

            // Make sure this character isn't being escaped before anything else
            if mode & TOK_MODE_CHAR_ESCAPE {
                mode &= !TOK_MODE_CHAR_ESCAPE;
                // and do nothing more
            } else if myal(c) {
                // Early exit optimization in case the character is just a letter,
                // which has no special meaning to the tokenizer, i.e. the same mode continues.
            }
            // Now proceed with the evaluation of the token, first checking to see if the token
            // has been explicitly ignored (escaped).
            else if c == '\\' {
                mode |= TOK_MODE_CHAR_ESCAPE;
            } else if c == '#' && is_token_begin {
                self.token_cursor = comment_end(self.start, self.token_cursor) - 1;
            } else if c == '(' {
                paran_offsets.push(self.token_cursor);
                expecting.push(')');
                mode |= TOK_MODE_SUBSHELL;
            } else if c == '{' {
                brace_offsets.push(self.token_cursor);
                expecting.push('}');
                mode |= TOK_MODE_CURLY_BRACES;
            } else if c == ')' {
                if expecting.last() == Some(&'}') {
                    return self.call_error(
                        TokenizerError::ExpectedBcloseFoundPclose,
                        self.token_cursor,
                        self.token_cursor,
                        Some(1),
                        1,
                    );
                }
                if paran_offsets.pop().is_none() {
                    return self.call_error(
                        TokenizerError::ClosingUnopenedSubshell,
                        self.token_cursor,
                        self.token_cursor,
                        Some(1),
                        1,
                    );
                }
                if paran_offsets.is_empty() {
                    mode &= !TOK_MODE_SUBSHELL;
                }
                expecting.pop();
                // Check if the ) completed a quoted command substitution.
                if quoted_cmdsubs.last() == Some(&paran_offsets.len()) {
                    quoted_cmdsubs.pop();
                    // The "$(" part of a quoted command substitution closes double quotes. To keep
                    // quotes balanced, act as if there was an invisible double quote after the ")".
                    if let Err(error_loc) =
                        process_opening_quote(self, &mut quoted_cmdsubs, &paran_offsets, '"')
                    {
                        if !self.accept_unfinished {
                            return self.call_error(
                                TokenizerError::UnterminatedQuote,
                                buff_start,
                                error_loc,
                                None,
                                0,
                            );
                        }
                        break;
                    }
                }
            } else if c == '}' {
                if expecting.last() == Some(&')') {
                    return self.call_error(
                        TokenizerError::ExpectedPcloseFoundBclose,
                        self.token_cursor,
                        self.token_cursor,
                        Some(1),
                        1,
                    );
                }
                if brace_offsets.pop().is_none() {
                    // Let the caller throw an error.
                    break;
                }
                if brace_offsets.is_empty() {
                    mode &= !TOK_MODE_CURLY_BRACES;
                }
                expecting.pop();
            } else if c == '[' {
                if self.token_cursor != buff_start {
                    mode |= TOK_MODE_ARRAY_BRACKETS;
                    slice_offset = self.token_cursor;
                } else {
                    // This is actually allowed so the test operator `[` can be used as the head of a
                    // command
                }
            }
            // Only exit bracket mode if we are in bracket mode.
            // Reason: `]` can be a parameter, e.g. last parameter to `[` test alias.
            // e.g. echo $argv[([ $x -eq $y ])] # must not end bracket mode on first bracket
            else if c == ']' && (mode & TOK_MODE_ARRAY_BRACKETS) {
                mode &= !TOK_MODE_ARRAY_BRACKETS;
            } else if c == '\'' || c == '"' {
                if let Err(error_loc) =
                    process_opening_quote(self, &mut quoted_cmdsubs, &paran_offsets, c)
                {
                    if !self.accept_unfinished {
                        return self.call_error(
                            TokenizerError::UnterminatedQuote,
                            buff_start,
                            error_loc,
                            None,
                            1,
                        );
                    }
                    break;
                }
            } else if mode == TOK_MODE_REGULAR_TEXT
                && !tok_is_string_character(
                    c,
                    self.start
                        .as_char_slice()
                        .get(self.token_cursor + 1)
                        .copied(),
                )
            {
                break;
            }

            let next = self
                .start
                .as_char_slice()
                .get(self.token_cursor + 1)
                .copied();
            is_token_begin = is_token_delimiter(c, next);
            self.token_cursor += 1;
        }

        if !self.accept_unfinished && mode != TOK_MODE_REGULAR_TEXT {
            // These are all "unterminated", so the only char we can mark as an error
            // is the opener (the closing char could be anywhere!)
            //
            // (except for TOK_MODE_CHAR_ESCAPE, which is one long by definition)
            if mode & TOK_MODE_CHAR_ESCAPE {
                return self.call_error(
                    TokenizerError::UnterminatedEscape,
                    buff_start,
                    self.token_cursor - 1,
                    None,
                    1,
                );
            } else if mode & TOK_MODE_ARRAY_BRACKETS {
                return self.call_error(
                    TokenizerError::UnterminatedSlice,
                    buff_start,
                    slice_offset,
                    None,
                    1,
                );
            } else if mode & TOK_MODE_SUBSHELL {
                let offset_of_open_paran = *paran_offsets.last().expect("paran_offsets is empty");

                return self.call_error(
                    TokenizerError::UnterminatedSubshell,
                    buff_start,
                    offset_of_open_paran,
                    None,
                    1,
                );
            } else if mode & TOK_MODE_CURLY_BRACES {
                let offset_of_open_brace = *brace_offsets.last().expect("brace_offsets is empty");

                return self.call_error(
                    TokenizerError::UnterminatedBrace,
                    buff_start,
                    offset_of_open_brace,
                    None,
                    1,
                );
            } else {
                panic!("Unknown non-regular-text mode");
            }
        }

        let mut result = Tok::new(TokenType::String);
        result.set_offset(buff_start);
        result.set_length(self.token_cursor - buff_start);
        result.is_unterminated_brace = mode & TOK_MODE_CURLY_BRACES;
        result
    }
}

pub fn quote_end(s: &wstr, mut pos: usize, quote: char) -> Option<usize> {
    loop {
        pos += 1;

        let c = s.try_char_at(pos)?;
        if c == '\\' {
            pos += 1;
        } else if c == quote ||
                // Command substitutions also end a double quoted string.  This is how we
                // support command substitutions inside double quotes.
                (quote == '"' && c == '$' && s.as_char_slice().get(pos+1) == Some(&'('))
        {
            return Some(pos);
        }
    }
}

pub fn comment_end(s: &wstr, mut pos: usize) -> usize {
    loop {
        pos += 1;
        if pos == s.len() || s.char_at(pos) == '\n' {
            return pos;
        }
    }
}

/// Tests if this character can be a part of a string. Hash (#) starts a comment if it's the first
/// character in a token; otherwise it is considered a string character. See issue #953.
pub fn tok_is_string_character(c: char, next: Option<char>) -> bool {
    match c {
        // Unconditional separators.
        '\0' | ' ' | '\n' | '|' | '\t' | ';' | '\r' | '<' | '>' => false,
        '&' => {
            if feature_test(FeatureFlag::AmpersandNoBgInToken) {
                // Unlike in other shells, '&' is not special if followed by a string character.
                next.is_some_and(|nc| tok_is_string_character(nc, None))
            } else {
                false
            }
        }
        _ => true,
    }
}

/// Quick test to catch the most common 'non-magical' characters, makes read_string slightly faster
/// by adding a fast path for the most common characters. This is obviously not a suitable
/// replacement for iswalpha.
fn myal(c: char) -> bool {
    c.is_ascii_alphabetic()
}

#[derive(Clone, Copy, PartialEq, Eq)]
struct TokModes(u8);

const TOK_MODE_REGULAR_TEXT: TokModes = TokModes(0); // regular text
const TOK_MODE_SUBSHELL: TokModes = TokModes(1 << 0); // inside of subshell parentheses
const TOK_MODE_ARRAY_BRACKETS: TokModes = TokModes(1 << 1); // inside of array brackets
const TOK_MODE_CURLY_BRACES: TokModes = TokModes(1 << 2);
const TOK_MODE_CHAR_ESCAPE: TokModes = TokModes(1 << 3);

impl BitAnd for TokModes {
    type Output = bool;
    fn bitand(self, rhs: Self) -> Self::Output {
        (self.0 & rhs.0) != 0
    }
}
impl BitAndAssign for TokModes {
    fn bitand_assign(&mut self, rhs: Self) {
        self.0 &= rhs.0
    }
}
impl BitOrAssign for TokModes {
    fn bitor_assign(&mut self, rhs: Self) {
        self.0 |= rhs.0
    }
}
impl Not for TokModes {
    type Output = TokModes;
    fn not(self) -> Self::Output {
        TokModes(!self.0)
    }
}

/// Tests if this character can delimit tokens.
pub fn is_token_delimiter(c: char, next: Option<char>) -> bool {
    c == '(' || !tok_is_string_character(c, next)
}

/// Return the first token from the string, skipping variable assignments like A=B.
pub fn tok_command(str: &wstr) -> WString {
    let mut t = Tokenizer::new(str, TokFlags(0));
    while let Some(token) = t.next() {
        if token.type_ != TokenType::String {
            return WString::new();
        }
        let text = t.text_of(&token);
        if variable_assignment_equals_pos(text).is_some() {
            continue;
        }
        return text.to_owned();
    }
    WString::new()
}

impl TryFrom<&wstr> for PipeOrRedir {
    type Error = ();

    /// Examples of supported syntaxes.
    ///    Note we are only responsible for parsing the redirection part, not 'cmd' or 'file'.
    ///
    /// ```text
    ///     cmd | cmd        normal pipe
    ///     cmd &| cmd       normal pipe plus stderr-merge
    ///     cmd >| cmd       pipe with explicit fd
    ///     cmd 2>| cmd      pipe with explicit fd
    ///     cmd < file       stdin redirection
    ///     cmd > file       redirection
    ///     cmd >> file      appending redirection
    ///     cmd >? file      noclobber redirection
    ///     cmd >>? file     appending noclobber redirection
    ///     cmd 2> file      file redirection with explicit fd
    ///     cmd >&2          fd redirection with no explicit src fd (stdout is used)
    ///     cmd 1>&2         fd redirection with an explicit src fd
    ///     cmd <&2          fd redirection with no explicit src fd (stdin is used)
    ///     cmd 3<&0         fd redirection with an explicit src fd
    ///     cmd &> file      redirection with stderr merge
    ///     cmd ^ file       caret (stderr) redirection, perhaps disabled via feature flags
    ///     cmd ^^ file      caret (stderr) redirection, perhaps disabled via feature flags
    /// ```
    fn try_from(buff: &wstr) -> Result<PipeOrRedir, ()> {
        // Extract a range of leading fd.
        let mut cursor = buff.chars().take_while(|c| c.is_ascii_digit()).count();
        let fd_buff = &buff[..cursor];
        let has_fd = !fd_buff.is_empty();

        // Try consuming a given character.
        // Return true if consumed. On success, advances cursor.
        let try_consume = |cursor: &mut usize, c| -> bool {
            if buff.char_at(*cursor) != c {
                false
            } else {
                *cursor += 1;
                true
            }
        };

        // Like try_consume, but asserts on failure.
        let consume = |cursor: &mut usize, c| {
            assert_eq!(buff.char_at(*cursor), c, "Failed to consume char");
            *cursor += 1;
        };

        let c = buff.char_at(cursor);
        let mut result = PipeOrRedir {
            fd: -1,
            is_pipe: false,
            mode: RedirectionMode::Overwrite,
            stderr_merge: false,
            consumed: 0,
        };
        match c {
            '|' => {
                if has_fd {
                    // Like 123|
                    return Err(());
                }
                consume(&mut cursor, '|');
                assert_ne!(
                    buff.char_at(cursor),
                    '|',
                    "|| passed as redirection, this should have been handled as 'or' by the caller"
                );
                result.fd = STDOUT_FILENO;
                result.is_pipe = true;
            }
            '>' => {
                consume(&mut cursor, '>');
                if try_consume(&mut cursor, '>') {
                    result.mode = RedirectionMode::Append;
                }
                if try_consume(&mut cursor, '|') {
                    // Note we differ from bash here.
                    // Consider `echo foo 2>| bar`
                    // In fish, this is a *pipe*. Run bar as a command and attach foo's stderr to bar's
                    // stdin, while leaving stdout as tty.
                    // In bash, this is a *redirection* to bar as a file. It is like > but ignores
                    // noclobber.
                    result.is_pipe = true;
                    result.fd = if has_fd {
                        parse_fd(fd_buff) // like 2>|
                    } else {
                        STDOUT_FILENO
                    }; // like >|
                } else if try_consume(&mut cursor, '&') {
                    // This is a redirection to an fd.
                    // Note that we allow ">>&", but it's still just writing to the fd - "appending" to
                    // it doesn't make sense.
                    result.mode = RedirectionMode::Fd;
                    result.fd = if has_fd {
                        parse_fd(fd_buff) // like 1>&2
                    } else {
                        STDOUT_FILENO // like >&2
                    };
                } else {
                    // This is a redirection to a file.
                    result.fd = if has_fd {
                        parse_fd(fd_buff) // like 1> file.txt
                    } else {
                        STDOUT_FILENO // like > file.txt
                    };
                    if result.mode != RedirectionMode::Append {
                        result.mode = RedirectionMode::Overwrite;
                    }
                    // Note 'echo abc >>? file' is valid: it means append and noclobber.
                    // But here "noclobber" means the file must not exist, so appending
                    // can be ignored.
                    if try_consume(&mut cursor, '?') {
                        result.mode = RedirectionMode::NoClob;
                    }
                }
            }
            '<' => {
                consume(&mut cursor, '<');
                if try_consume(&mut cursor, '&') {
                    result.mode = RedirectionMode::Fd;
                } else if try_consume(&mut cursor, '?') {
                    // <? foo try-input redirection (uses /dev/null if file can't be used).
                    result.mode = RedirectionMode::TryInput;
                } else {
                    result.mode = RedirectionMode::Input;
                }
                result.fd = if has_fd {
                    parse_fd(fd_buff) // like 1<&3 or 1< /tmp/file.txt
                } else {
                    STDIN_FILENO // like <&3 or < /tmp/file.txt
                };
            }
            '&' => {
                consume(&mut cursor, '&');
                if try_consume(&mut cursor, '|') {
                    // &| is pipe with stderr merge.
                    result.fd = STDOUT_FILENO;
                    result.is_pipe = true;
                    result.stderr_merge = true;
                } else if try_consume(&mut cursor, '>') {
                    result.fd = STDOUT_FILENO;
                    result.stderr_merge = true;
                    result.mode = RedirectionMode::Overwrite;
                    if try_consume(&mut cursor, '>') {
                        result.mode = RedirectionMode::Append; // like &>>
                    }
                    if try_consume(&mut cursor, '?') {
                        result.mode = RedirectionMode::NoClob; // like &>? or &>>?
                    }
                } else {
                    return Err(());
                }
            }
            _ => {
                // Not a redirection.
                return Err(());
            }
        }

        result.consumed = cursor;
        assert!(
            result.consumed > 0,
            "Should have consumed at least one character on success"
        );
        Ok(result)
    }
}

impl PipeOrRedir {
    /// Return the oflags (as in open(2)) for this redirection.
    pub fn oflags(&self) -> Option<OFlag> {
        self.mode.oflags()
    }

    // Return if we are "valid". Here "valid" means only that the source fd did not overflow.
    // For example 99999999999> is invalid.
    pub fn is_valid(&self) -> bool {
        self.fd >= 0
    }

    // Return the token type for this redirection.
    pub fn token_type(&self) -> TokenType {
        if self.is_pipe {
            TokenType::Pipe
        } else {
            TokenType::Redirect
        }
    }
}

// Parse an fd from the non-empty string [start, end), all of which are digits.
// Return the fd, or -1 on overflow.
fn parse_fd(s: &wstr) -> RawFd {
    assert!(!s.is_empty());
    let chars: Vec<u8> = s
        .chars()
        .map(|c| {
            assert!(c.is_ascii_digit());
            c as u8
        })
        .collect();
    let s = std::str::from_utf8(chars.as_slice()).unwrap();
    s.parse().unwrap_or(-1)
}

/// The position of the equal sign in a variable assignment like foo=bar.
///
/// Return the location of the equals sign, or none if the string does
/// not look like a variable assignment like FOO=bar.  The detection
/// works similar as in some POSIX shells: only letters and numbers qre
/// allowed on the left hand side, no quotes or escaping.
pub fn variable_assignment_equals_pos(txt: &wstr) -> Option<usize> {
    let mut found_potential_variable = false;

    // TODO bracket indexing
    for (i, c) in txt.chars().enumerate() {
        if !found_potential_variable {
            if !valid_var_name_char(c) {
                return None;
            }
            found_potential_variable = true;
        } else {
            if c == '=' {
                return Some(i);
            }
            if !valid_var_name_char(c) {
                return None;
            }
        }
    }
    None
}

#[cfg(test)]
mod tests {
    use super::{PipeOrRedir, TokFlags, TokenType, Tokenizer, TokenizerError};
    use crate::prelude::*;
    use crate::redirection::RedirectionMode;
    use libc::{STDERR_FILENO, STDOUT_FILENO};

    #[test]
    fn test_tokenizer() {
        {
            let s = L!("alpha beta");
            let mut t = Tokenizer::new(s, TokFlags(0));

            let token = t.next(); // alpha
            assert!(token.is_some());
            let token = token.unwrap();
            assert_eq!(token.type_, TokenType::String);
            assert_eq!(token.length, 5);
            assert_eq!(t.text_of(&token), "alpha");

            let token = t.next(); // beta
            assert!(token.is_some());
            let token = token.unwrap();
            assert_eq!(token.type_, TokenType::String);
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
            assert_eq!(token.type_, TokenType::LeftBrace);
            assert_eq!(token.length, 1);
            assert_eq!(t.text_of(&token), "{");

            let token = t.next(); // echo
            assert!(token.is_some());
            let token = token.unwrap();
            assert_eq!(token.type_, TokenType::String);
            assert_eq!(token.offset, 2);
            assert_eq!(token.length, 4);
            assert_eq!(t.text_of(&token), "echo");

            assert!(t.next().is_none());
        }

        {
            let s = L!("{echo, foo}");
            let mut t = Tokenizer::new(s, TokFlags(0));
            let token = t.next().unwrap();
            assert_eq!(token.type_, TokenType::LeftBrace);
            assert_eq!(token.length, 1);
        }
        {
            let s = L!("{ echo; foo}");
            let mut t = Tokenizer::new(s, TokFlags(0));
            let token = t.next().unwrap();
            assert_eq!(token.type_, TokenType::LeftBrace);
        }

        {
            let s = L!("{ | { name } '");
            let mut t = Tokenizer::new(s, TokFlags(0));
            let mut next_type = || t.next().unwrap().type_;
            assert_eq!(next_type(), TokenType::LeftBrace);
            assert_eq!(next_type(), TokenType::Pipe);
            assert_eq!(next_type(), TokenType::LeftBrace);
            assert_eq!(next_type(), TokenType::String);
            assert_eq!(next_type(), TokenType::RightBrace);
            assert_eq!(next_type(), TokenType::Error);
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
            tt::String, tt::Redirect, tt::String, tt::Redirect, tt::String, tt::String, tt::String,
            tt::String, tt::String, tt::Pipe, tt::Redirect, tt::AndAnd, tt::Background, tt::OrOr,
            tt::Pipe, tt::AndAnd, tt::OrOr, tt::Background, tt::Pipe, tt::String, tt::End,
            tt::String,
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
            assert_eq!(token.type_, TokenType::Error);
            assert_eq!(token.error, TokenizerError::UnterminatedEscape);
            assert_eq!(token.error_offset_within_token, 3);
        }

        {
            let mut t = Tokenizer::new(L!("abc )defg(hij"), TokFlags(0));
            let _token = t.next().unwrap();
            let token = t.next().unwrap();
            assert_eq!(token.type_, TokenType::Error);
            assert_eq!(token.error, TokenizerError::ClosingUnopenedSubshell);
            assert_eq!(token.offset, 4);
            assert_eq!(token.error_offset_within_token, 0);
        }

        {
            let mut t = Tokenizer::new(L!("abc defg(hij (klm)"), TokFlags(0));
            let _token = t.next().unwrap();
            let token = t.next().unwrap();
            assert_eq!(token.type_, TokenType::Error);
            assert_eq!(token.error, TokenizerError::UnterminatedSubshell);
            assert_eq!(token.error_offset_within_token, 4);
        }

        {
            let mut t = Tokenizer::new(L!("abc defg[hij (klm)"), TokFlags(0));
            let _token = t.next().unwrap();
            let token = t.next().unwrap();
            assert_eq!(token.type_, TokenType::Error);
            assert_eq!(token.error, TokenizerError::UnterminatedSlice);
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

        assert_eq!(get_redir_mode!("<"), RedirectionMode::Input);
        assert_eq!(get_redir_mode!(">"), RedirectionMode::Overwrite);
        assert_eq!(get_redir_mode!("2>"), RedirectionMode::Overwrite);
        assert_eq!(get_redir_mode!(">>"), RedirectionMode::Append);
        assert_eq!(get_redir_mode!("2>>"), RedirectionMode::Append);
        assert_eq!(get_redir_mode!("2>?"), RedirectionMode::NoClob);
        assert_eq!(
            get_redir_mode!("9999999999999999>?"),
            RedirectionMode::NoClob
        );
        assert_eq!(get_redir_mode!("2>&3"), RedirectionMode::Fd);
        assert_eq!(get_redir_mode!("3<&0"), RedirectionMode::Fd);
        assert_eq!(get_redir_mode!("3</tmp/filetxt"), RedirectionMode::Input);
    }
}
