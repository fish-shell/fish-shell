//! A specialized tokenizer for tokenizing the fish language. In the future, the tokenizer should be
//! extended to support marks, tokenizing multiple strings and disposing of unused string segments.

use crate::common::valid_var_name_char;
use crate::ffi::wcharz_t;
use crate::future_feature_flags::{feature_test, FeatureFlag};
use crate::parse_constants::SOURCE_OFFSET_INVALID;
use crate::redirection::RedirectionMode;
use crate::wchar::prelude::*;
use crate::wchar_ffi::{wchar_t, AsWstr, WCharToFFI};
use cxx::{CxxWString, SharedPtr, UniquePtr};
use libc::{c_int, STDIN_FILENO, STDOUT_FILENO};
use std::ops::{BitAnd, BitAndAssign, BitOr, BitOrAssign, Not};
use std::os::fd::RawFd;

#[cxx::bridge]
mod tokenizer_ffi {
    extern "C++" {
        include!("wutil.h");
        include!("redirection.h");
        type wcharz_t = super::wcharz_t;
        type RedirectionMode = super::RedirectionMode;
    }

    /// Token types. XXX Why this isn't ParseTokenType, I'm not really sure.
    enum TokenType {
        /// Error reading token
        error,
        /// String token
        string,
        /// Pipe token
        pipe,
        /// && token
        andand,
        /// || token
        oror,
        /// End token (semicolon or newline, not literal end)
        end,
        /// redirection token
        redirect,
        /// send job to bg token
        background,
        /// comment token
        comment,
    }

    enum TokenizerError {
        none,
        unterminated_quote,
        unterminated_subshell,
        unterminated_slice,
        unterminated_escape,
        invalid_redirect,
        invalid_pipe,
        invalid_pipe_ampersand,
        closing_unopened_subshell,
        illegal_slice,
        closing_unopened_brace,
        unterminated_brace,
        expected_pclose_found_bclose,
        expected_bclose_found_pclose,
    }

    extern "Rust" {
        fn tokenizer_get_error_message(err: TokenizerError) -> UniquePtr<CxxWString>;
    }

    struct Tok {
        // Offset of the token.
        offset: u32,
        // Length of the token.
        length: u32,

        // If an error, this is the offset of the error within the token. A value of 0 means it occurred
        // at 'offset'.
        error_offset_within_token: u32,
        error_length: u32,

        // If an error, this is the error code.
        error: TokenizerError,

        // The type of the token.
        type_: TokenType,
    }
    // TODO static_assert(sizeof(Tok) <= 32, "Tok expected to be 32 bytes or less");

    extern "Rust" {
        fn location_in_or_at_end_of_source_range(self: &Tok, loc: usize) -> bool;
        #[cxx_name = "get_source"]
        fn get_source_ffi(self: &Tok, str: &CxxWString) -> UniquePtr<CxxWString>;
    }

    extern "Rust" {
        type Tokenizer;
        fn new_tokenizer(start: wcharz_t, flags: u8) -> Box<Tokenizer>;
        #[cxx_name = "next"]
        fn next_ffi(self: &mut Tokenizer) -> UniquePtr<Tok>;
        #[cxx_name = "text_of"]
        fn text_of_ffi(self: &Tokenizer, tok: &Tok) -> UniquePtr<CxxWString>;
        #[cxx_name = "is_token_delimiter"]
        fn is_token_delimiter_ffi(c: wchar_t, next: SharedPtr<wchar_t>) -> bool;
    }

    extern "Rust" {
        #[cxx_name = "tok_command"]
        fn tok_command_ffi(str: &CxxWString) -> UniquePtr<CxxWString>;
    }

    /// Struct wrapping up a parsed pipe or redirection.
    struct PipeOrRedir {
        // The redirected fd, or -1 on overflow.
        // In the common case of a pipe, this is 1 (STDOUT_FILENO).
        // For example, in the case of "3>&1" this will be 3.
        fd: i32,

        // Whether we are a pipe (true) or redirection (false).
        is_pipe: bool,

        // The redirection mode if the type is redirect.
        // Ignored for pipes.
        mode: RedirectionMode,

        // Whether, in addition to this redirection, stderr should also be dup'd to stdout
        // For example &| or &>
        stderr_merge: bool,

        // Number of characters consumed when parsing the string.
        consumed: usize,
    }

    extern "Rust" {
        fn pipe_or_redir_from_string(buff: wcharz_t) -> UniquePtr<PipeOrRedir>;
        fn is_valid(self: &PipeOrRedir) -> bool;
        fn oflags(self: &PipeOrRedir) -> i32;
        fn token_type(self: &PipeOrRedir) -> TokenType;
    }

    enum MoveWordStyle {
        move_word_style_punctuation,     // stop at punctuation
        move_word_style_path_components, // stops at path components
        move_word_style_whitespace,      // stops at whitespace
    }

    /// Our state machine that implements "one word" movement or erasure.
    struct MoveWordStateMachine {
        state: u8,
        style: MoveWordStyle,
    }

    extern "Rust" {
        fn new_move_word_state_machine(syl: MoveWordStyle) -> Box<MoveWordStateMachine>;
        #[cxx_name = "consume_char"]
        fn consume_char_ffi(self: &mut MoveWordStateMachine, c: wchar_t) -> bool;
        fn reset(self: &mut MoveWordStateMachine);
    }

    extern "Rust" {
        #[cxx_name = "variable_assignment_equals_pos"]
        fn variable_assignment_equals_pos_ffi(txt: &CxxWString) -> SharedPtr<usize>;
    }
}

pub use tokenizer_ffi::{
    MoveWordStateMachine, MoveWordStyle, PipeOrRedir, Tok, TokenType, TokenizerError,
};

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

/// Get the error message for an error \p err.
pub fn tokenizer_get_error_message(err: TokenizerError) -> UniquePtr<CxxWString> {
    let s: &'static wstr = err.into();
    s.to_ffi()
}

impl From<TokenizerError> for &'static wstr {
    #[widestrs]
    fn from(err: TokenizerError) -> Self {
        match err {
            TokenizerError::none => ""L,
            TokenizerError::unterminated_quote => {
                wgettext!("Unexpected end of string, quotes are not balanced")
            }
            TokenizerError::unterminated_subshell => {
                wgettext!("Unexpected end of string, expecting ')'")
            }
            TokenizerError::unterminated_slice => {
                wgettext!("Unexpected end of string, square brackets do not match")
            }
            TokenizerError::unterminated_escape => {
                wgettext!("Unexpected end of string, incomplete escape sequence")
            }
            TokenizerError::invalid_redirect => {
                wgettext!("Invalid input/output redirection")
            }
            TokenizerError::invalid_pipe => {
                wgettext!("Cannot use stdin (fd 0) as pipe output")
            }
            TokenizerError::invalid_pipe_ampersand => {
                wgettext!("|& is not valid. In fish, use &| to pipe both stdout and stderr.")
            }
            TokenizerError::closing_unopened_subshell => {
                wgettext!("Unexpected ')' for unopened parenthesis")
            }
            TokenizerError::illegal_slice => {
                wgettext!("Unexpected '[' at this location")
            }
            TokenizerError::closing_unopened_brace => {
                wgettext!("Unexpected '}' for unopened brace expansion")
            }
            TokenizerError::unterminated_brace => {
                wgettext!("Unexpected end of string, incomplete parameter expansion")
            }
            TokenizerError::expected_pclose_found_bclose => {
                wgettext!("Unexpected '}' found, expecting ')'")
            }
            TokenizerError::expected_bclose_found_pclose => {
                wgettext!("Unexpected ')' found, expecting '}'")
            }
            _ => {
                panic!("Unexpected tokenizer error");
            }
        }
    }
}

impl printf_compat::args::ToArg<'static> for TokenizerError {
    fn to_arg(self) -> printf_compat::args::Arg<'static> {
        printf_compat::args::Arg::Str(self.into())
    }
}

impl Tok {
    fn new(r#type: TokenType) -> Tok {
        Tok {
            offset: 0,
            length: 0,
            error_offset_within_token: SOURCE_OFFSET_INVALID.try_into().unwrap(),
            error_length: 0,
            error: TokenizerError::none,
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
    fn get_source_ffi(self: &Tok, str: &CxxWString) -> UniquePtr<CxxWString> {
        self.get_source(str.as_wstr()).to_ffi()
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

/// The tokenizer struct.
pub struct Tokenizer {
    /// A pointer into the original string, showing where the next token begins.
    token_cursor: usize,
    /// The start of the original string.
    start: WString, // TODO Avoid copying once we drop the FFI.
    /// Whether we have additional tokens.
    has_next: bool,
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
}

impl Tokenizer {
    /// Constructor for a tokenizer. b is the string that is to be tokenized. It is not copied, and
    /// should not be freed by the caller until after the tokenizer is destroyed.
    ///
    /// \param start The string to tokenize
    /// \param flags Flags to the tokenizer. Setting TOK_ACCEPT_UNFINISHED will cause the tokenizer
    /// to accept incomplete tokens, such as a subshell without a closing parenthesis, as a valid
    /// token. Setting TOK_SHOW_COMMENTS will return comments as tokens
    pub fn new(start: &wstr, flags: TokFlags) -> Self {
        Tokenizer {
            token_cursor: 0,
            start: start.to_owned(),
            has_next: true,
            accept_unfinished: flags & TOK_ACCEPT_UNFINISHED,
            show_comments: flags & TOK_SHOW_COMMENTS,
            show_blank_lines: flags & TOK_SHOW_BLANK_LINES,
            continue_after_error: flags & TOK_CONTINUE_AFTER_ERROR,
            continue_line_after_comment: false,
        }
    }
}

fn new_tokenizer(start: wcharz_t, flags: u8) -> Box<Tokenizer> {
    Box::new(Tokenizer::new(start.into(), TokFlags(flags)))
}

impl Iterator for Tokenizer {
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
            self.token_cursor = comment_end(&self.start, self.token_cursor);
            let comment_len = self.token_cursor - comment_start;

            // If we are going to continue after the comment, skip any trailing newline.
            if self.start.as_char_slice().get(self.token_cursor) == Some(&'\n')
                && self.continue_line_after_comment
            {
                self.token_cursor += 1;
            }

            // Maybe return the comment.
            if self.show_comments {
                let mut result = Tok::new(TokenType::comment);
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
        match this_char {
            '\0'=> {
                self.has_next = false;
                None
            }
            '\r'|  // carriage-return
            '\n'|  // newline
            ';'=> {
                let mut result = Tok::new(TokenType::end);
                result.offset = start_pos as u32;
                result.length = 1;
                self.token_cursor+=1;
                // Hack: when we get a newline, swallow as many as we can. This compresses multiple
                // subsequent newlines into a single one.
                if !self.show_blank_lines {
                    while self.token_cursor < self.start.len() {
                        let c = self.start.char_at(self.token_cursor);
                        if c != '\n' && c != '\r' && c != ' ' && c != '\t' {
                            break
                        }
                        self.token_cursor+=1;
                    }
                }
                Some(result)
            }
            '&'=> {
                if next_char == Some('&') {
                    // && is and.
                    let mut result = Tok::new(TokenType::andand);
                    result.offset = start_pos as u32;
                    result.length = 2;
                    self.token_cursor += 2;
                    Some(result)
                } else if next_char == Some('>') || next_char == Some('|') {
                    // &> and &| redirect both stdout and stderr.
                    let redir = PipeOrRedir::try_from(buff).
                        expect("Should always succeed to parse a &> or &| redirection");
                    let mut result = Tok::new(redir.token_type());
                    result.offset = start_pos as u32;
                    result.length = redir.consumed as u32;
                    self.token_cursor += redir.consumed;
                    Some(result)
                } else {
                    let mut result = Tok::new(TokenType::background);
                    result.offset = start_pos as u32;
                    result.length = 1;
                    self.token_cursor+=1;
                    Some(result)
                }
            }
            '|'=> {
                if next_char == Some('|') {
                    // || is or.
                        let mut result=Tok::new(TokenType::oror);
                    result.offset = start_pos as u32;
                    result.length = 2;
                    self.token_cursor += 2;
                    Some(result)
                } else if next_char == Some('&') {
                    // |& is a bashism; in fish it's &|.
                    Some(self.call_error(TokenizerError::invalid_pipe_ampersand,
                                            self.token_cursor, self.token_cursor, Some(2), 2))
                } else {
                    let pipe = PipeOrRedir::try_from(buff).
                        expect("Should always succeed to parse a | pipe");
                    let mut result = Tok::new(pipe.token_type());
                    result.offset = start_pos as u32;
                    result.length = pipe.consumed as u32;
                    self.token_cursor += pipe.consumed;
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
                            Some(self.call_error(TokenizerError::invalid_redirect, self.token_cursor,
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
                    Err(()) => Some(self.call_error(TokenizerError::invalid_redirect, self.token_cursor,
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
                                Some(self.call_error(TokenizerError::invalid_pipe, error_location,
                                                        error_location, Some(redir_or_pipe.consumed),
                                                        redir_or_pipe.consumed))
                            }
                            else {
                                let mut result = Tok::new(redir_or_pipe.token_type());
                                result.offset = start_pos as u32;
                                result.length = redir_or_pipe.consumed as u32;
                                self.token_cursor += redir_or_pipe.consumed;
                                Some(result)
                            }
                        }
                        None => {
                            // Not a redirection or pipe, so just a string.
                            Some(self.read_string())
                    }
                }
            }
        }
    }
}
impl Tokenizer {
    fn next_ffi(&mut self) -> UniquePtr<Tok> {
        match self.next() {
            Some(tok) => UniquePtr::new(tok),
            None => UniquePtr::null(),
        }
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

impl Tokenizer {
    /// Returns the text of a token, as a string.
    pub fn text_of(&self, tok: &Tok) -> &wstr {
        tok.get_source(&self.start)
    }
    fn text_of_ffi(&self, tok: &Tok) -> UniquePtr<CxxWString> {
        self.text_of(tok).to_ffi()
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
        assert!(
            error_type != TokenizerError::none,
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
            type_: TokenType::error,
        }
    }
}

impl Tokenizer {
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
            this: &mut Tokenizer,
            quoted_cmdsubs: &mut Vec<usize>,
            paran_offsets: &mut Vec<usize>,
            quote: char,
        ) -> Result<(), usize> {
            if let Some(end) = quote_end(&this.start, this.token_cursor, quote) {
                if this.start.char_at(end) == '$' {
                    quoted_cmdsubs.push(paran_offsets.len());
                }
                this.token_cursor = end;
                Ok(())
            } else {
                let error_loc = this.token_cursor;
                this.token_cursor = this.start.len();
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
                self.token_cursor = comment_end(&self.start, self.token_cursor) - 1;
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
                        TokenizerError::expected_bclose_found_pclose,
                        self.token_cursor,
                        self.token_cursor,
                        Some(1),
                        1,
                    );
                }
                if paran_offsets.is_empty() {
                    return self.call_error(
                        TokenizerError::closing_unopened_subshell,
                        self.token_cursor,
                        self.token_cursor,
                        Some(1),
                        1,
                    );
                }
                paran_offsets.pop();
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
                        process_opening_quote(self, &mut quoted_cmdsubs, &mut paran_offsets, '"')
                    {
                        if !self.accept_unfinished {
                            return self.call_error(
                                TokenizerError::unterminated_quote,
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
                        TokenizerError::expected_pclose_found_bclose,
                        self.token_cursor,
                        self.token_cursor,
                        Some(1),
                        1,
                    );
                }
                if brace_offsets.is_empty() {
                    return self.call_error(
                        TokenizerError::closing_unopened_brace,
                        self.token_cursor,
                        self.start.len(),
                        None,
                        0,
                    );
                }
                brace_offsets.pop();
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
                    process_opening_quote(self, &mut quoted_cmdsubs, &mut paran_offsets, c)
                {
                    if !self.accept_unfinished {
                        return self.call_error(
                            TokenizerError::unterminated_quote,
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
                    TokenizerError::unterminated_escape,
                    buff_start,
                    self.token_cursor - 1,
                    None,
                    1,
                );
            } else if mode & TOK_MODE_ARRAY_BRACKETS {
                return self.call_error(
                    TokenizerError::unterminated_slice,
                    buff_start,
                    slice_offset,
                    None,
                    1,
                );
            } else if mode & TOK_MODE_SUBSHELL {
                assert!(!paran_offsets.is_empty());
                let offset_of_open_paran = *paran_offsets.last().unwrap();

                return self.call_error(
                    TokenizerError::unterminated_subshell,
                    buff_start,
                    offset_of_open_paran,
                    None,
                    1,
                );
            } else if mode & TOK_MODE_CURLY_BRACES {
                assert!(!brace_offsets.is_empty());
                let offset_of_open_brace = *brace_offsets.last().unwrap();

                return self.call_error(
                    TokenizerError::unterminated_brace,
                    buff_start,
                    offset_of_open_brace,
                    None,
                    1,
                );
            } else {
                panic!("Unknown non-regular-text mode");
            }
        }

        let mut result = Tok::new(TokenType::string);
        result.set_offset(buff_start);
        result.set_length(self.token_cursor - buff_start);
        result
    }
}

pub fn quote_end(s: &wstr, mut pos: usize, quote: char) -> Option<usize> {
    loop {
        pos += 1;

        if pos == s.len() {
            return None;
        }

        let c = s.char_at(pos);
        if c == '\\' {
            pos += 1;
            if pos == s.len() {
                return None;
            }
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
fn tok_is_string_character(c: char, next: Option<char>) -> bool {
    match c {
        // Unconditional separators.
        '\0' | ' ' | '\n' | '|' | '\t' | ';' | '\r' | '<' | '>' => false,
        '&' => {
            if feature_test(FeatureFlag::ampersand_nobg_in_token) {
                // Unlike in other shells, '&' is not special if followed by a string character.
                next.map(|nc| tok_is_string_character(nc, None))
                    .unwrap_or(false)
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
    ('a'..='z').contains(&c) || ('A'..='Z').contains(&c)
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

fn is_token_delimiter_ffi(c: wchar_t, next: SharedPtr<wchar_t>) -> bool {
    is_token_delimiter(
        c.try_into().unwrap(),
        next.as_ref().map(|c| (*c).try_into().unwrap()),
    )
}

/// \return the_ffi first token from the string, skipping variable assignments like A=B.
pub fn tok_command(str: &wstr) -> WString {
    let mut t = Tokenizer::new(str, TokFlags(0));
    while let Some(token) = t.next() {
        if token.type_ != TokenType::string {
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
fn tok_command_ffi(str: &CxxWString) -> UniquePtr<CxxWString> {
    tok_command(str.as_wstr()).to_ffi()
}

impl TryFrom<&wstr> for PipeOrRedir {
    type Error = ();

    /// Examples of supported syntaxes.
    ///    Note we are only responsible for parsing the redirection part, not 'cmd' or 'file'.
    ///
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
            assert!(buff.char_at(*cursor) == c, "Failed to consume char");
            *cursor += 1;
        };

        let c = buff.char_at(cursor);
        let mut result = PipeOrRedir {
            fd: -1,
            is_pipe: false,
            mode: RedirectionMode::overwrite,
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
                assert!(
                    buff.char_at(cursor) != '|',
                    "|| passed as redirection, this should have been handled as 'or' by the caller"
                );
                result.fd = STDOUT_FILENO;
                result.is_pipe = true;
            }
            '>' => {
                consume(&mut cursor, '>');
                if try_consume(&mut cursor, '>') {
                    result.mode = RedirectionMode::append;
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
                    result.mode = RedirectionMode::fd;
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
                    if result.mode != RedirectionMode::append {
                        result.mode = RedirectionMode::overwrite;
                    }
                    // Note 'echo abc >>? file' is valid: it means append and noclobber.
                    // But here "noclobber" means the file must not exist, so appending
                    // can be ignored.
                    if try_consume(&mut cursor, '?') {
                        result.mode = RedirectionMode::noclob;
                    }
                }
            }
            '<' => {
                consume(&mut cursor, '<');
                if try_consume(&mut cursor, '&') {
                    result.mode = RedirectionMode::fd;
                } else {
                    result.mode = RedirectionMode::input;
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
                    result.mode = RedirectionMode::overwrite;
                    if try_consume(&mut cursor, '>') {
                        result.mode = RedirectionMode::append; // like &>>
                    }
                    if try_consume(&mut cursor, '?') {
                        result.mode = RedirectionMode::noclob; // like &>? or &>>?
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

fn pipe_or_redir_from_string(buff: wcharz_t) -> UniquePtr<PipeOrRedir> {
    match PipeOrRedir::try_from(Into::<&wstr>::into(buff)) {
        Ok(p) => UniquePtr::new(p),
        Err(()) => UniquePtr::null(),
    }
}

impl PipeOrRedir {
    /// \return the oflags (as in open(2)) for this redirection.
    pub fn oflags(&self) -> c_int {
        self.mode.oflags().unwrap_or(-1)
    }

    // \return if we are "valid". Here "valid" means only that the source fd did not overflow.
    // For example 99999999999> is invalid.
    pub fn is_valid(&self) -> bool {
        self.fd >= 0
    }

    // \return the token type for this redirection.
    pub fn token_type(&self) -> TokenType {
        if self.is_pipe {
            TokenType::pipe
        } else {
            TokenType::redirect
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

fn new_move_word_state_machine(syl: MoveWordStyle) -> Box<MoveWordStateMachine> {
    Box::new(MoveWordStateMachine::new(syl))
}

impl MoveWordStateMachine {
    pub fn new(style: MoveWordStyle) -> Self {
        MoveWordStateMachine { state: 0, style }
    }

    pub fn consume_char(&mut self, c: char) -> bool {
        match self.style {
            MoveWordStyle::move_word_style_punctuation => self.consume_char_punctuation(c),
            MoveWordStyle::move_word_style_path_components => self.consume_char_path_components(c),
            MoveWordStyle::move_word_style_whitespace => self.consume_char_whitespace(c),
            _ => panic!(),
        }
    }
    pub fn consume_char_ffi(&mut self, c: wchar_t) -> bool {
        self.consume_char(c.try_into().unwrap())
    }

    pub fn reset(&mut self) {
        self.state = 0;
    }

    fn consume_char_punctuation(&mut self, c: char) -> bool {
        const S_ALWAYS_ONE: u8 = 0;
        const S_REST: u8 = 1;
        const S_WHITESPACE_REST: u8 = 2;
        const S_WHITESPACE: u8 = 3;
        const S_ALPHANUMERIC: u8 = 4;
        const S_END: u8 = 5;

        let mut consumed = false;
        while self.state != S_END && !consumed {
            match self.state {
                S_ALWAYS_ONE => {
                    // Always consume the first character.
                    consumed = true;
                    if c.is_whitespace() {
                        self.state = S_WHITESPACE;
                    } else if c.is_alphanumeric() {
                        self.state = S_ALPHANUMERIC;
                    } else {
                        // Don't allow switching type (ws->nonws) after non-whitespace and
                        // non-alphanumeric.
                        self.state = S_REST;
                    }
                }
                S_REST => {
                    if c.is_whitespace() {
                        // Consume only trailing whitespace.
                        self.state = S_WHITESPACE_REST;
                    } else if c.is_alphanumeric() {
                        // Consume only alnums.
                        self.state = S_ALPHANUMERIC;
                    } else {
                        consumed = false;
                        self.state = S_END;
                    }
                }
                S_WHITESPACE_REST | S_WHITESPACE => {
                    // "whitespace" consumes whitespace and switches to alnums,
                    // "whitespace_rest" only consumes whitespace.
                    if c.is_whitespace() {
                        // Consumed whitespace.
                        consumed = true;
                    } else {
                        self.state = if self.state == S_WHITESPACE {
                            S_ALPHANUMERIC
                        } else {
                            S_END
                        };
                    }
                }
                S_ALPHANUMERIC => {
                    if c.is_alphanumeric() {
                        consumed = true; // consumed alphanumeric
                    } else {
                        self.state = S_END;
                    }
                }
                _ => {}
            }
        }
        consumed
    }

    fn consume_char_path_components(&mut self, c: char) -> bool {
        const S_INITIAL_PUNCTUATION: u8 = 0;
        const S_WHITESPACE: u8 = 1;
        const S_SEPARATOR: u8 = 2;
        const S_SLASH: u8 = 3;
        const S_PATH_COMPONENT_CHARACTERS: u8 = 4;
        const S_INITIAL_SEPARATOR: u8 = 5;
        const S_END: u8 = 6;

        let mut consumed = false;
        while self.state != S_END && !consumed {
            match self.state {
                S_INITIAL_PUNCTUATION => {
                    if !is_path_component_character(c) && !c.is_whitespace() {
                        self.state = S_INITIAL_SEPARATOR;
                    } else {
                        if !is_path_component_character(c) {
                            consumed = true;
                        }
                        self.state = S_WHITESPACE;
                    }
                }
                S_WHITESPACE => {
                    if c.is_whitespace() {
                        consumed = true; // consumed whitespace
                    } else if c == '/' || is_path_component_character(c) {
                        self.state = S_SLASH; // path component
                    } else {
                        self.state = S_SEPARATOR; // path separator
                    }
                }
                S_SEPARATOR => {
                    if !c.is_whitespace() && !is_path_component_character(c) {
                        consumed = true; // consumed separator
                    } else {
                        self.state = S_END;
                    }
                }
                S_SLASH => {
                    if c == '/' {
                        consumed = true; // consumed slash
                    } else {
                        self.state = S_PATH_COMPONENT_CHARACTERS;
                    }
                }
                S_PATH_COMPONENT_CHARACTERS => {
                    if is_path_component_character(c) {
                        consumed = true; // consumed string character except slash
                    } else {
                        self.state = S_END;
                    }
                }
                S_INITIAL_SEPARATOR => {
                    if is_path_component_character(c) {
                        consumed = true;
                        self.state = S_PATH_COMPONENT_CHARACTERS;
                    } else if c.is_whitespace() {
                        self.state = S_END;
                    } else {
                        consumed = true;
                    }
                }
                _ => {}
            }
        }
        consumed
    }

    fn consume_char_whitespace(&mut self, c: char) -> bool {
        // Consume a "word" of printable characters plus any leading whitespace.
        const S_ALWAYS_ONE: u8 = 0;
        const S_BLANK: u8 = 1;
        const S_GRAPH: u8 = 2;
        const S_END: u8 = 3;

        let mut consumed = false;
        while self.state != S_END && !consumed {
            match self.state {
                S_ALWAYS_ONE => {
                    consumed = true; // always consume the first character
                                     // If it's not whitespace, only consume those from here.
                    if !c.is_whitespace() {
                        self.state = S_GRAPH;
                    } else {
                        // If it's whitespace, keep consuming whitespace until the graphs.
                        self.state = S_BLANK;
                    }
                }
                S_BLANK => {
                    if c.is_whitespace() {
                        consumed = true; // consumed whitespace
                    } else {
                        self.state = S_GRAPH;
                    }
                }
                S_GRAPH => {
                    if !c.is_whitespace() {
                        consumed = true; // consumed printable non-space
                    } else {
                        self.state = S_END;
                    }
                }
                _ => {}
            }
        }
        consumed
    }
}

fn is_path_component_character(c: char) -> bool {
    tok_is_string_character(c, None) && !L!("/={,}'\":@").as_char_slice().contains(&c)
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

fn variable_assignment_equals_pos_ffi(txt: &CxxWString) -> SharedPtr<usize> {
    match variable_assignment_equals_pos(txt.as_wstr()) {
        Some(p) => SharedPtr::new(p),
        None => SharedPtr::null(),
    }
}
