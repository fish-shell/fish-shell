//! Constants used in the programmatic representation of fish code.

use crate::ffi::{fish_wcswidth, fish_wcwidth, wcharz_t};
use crate::tokenizer::variable_assignment_equals_pos;
use crate::wchar::{wstr, WString, L};
use crate::wchar_ffi::{wcharz, AsWstr, WCharFromFFI, WCharToFFI};
use crate::wutil::{sprintf, wgettext_fmt};
use bitflags::bitflags;
use cxx::{type_id, ExternType};
use cxx::{CxxWString, UniquePtr};
use widestring_suffix::widestrs;

pub type SourceOffset = u32;

pub const SOURCE_OFFSET_INVALID: usize = SourceOffset::MAX as _;
pub const SOURCE_LOCATION_UNKNOWN: usize = usize::MAX;

bitflags! {
    pub struct ParseTreeFlags: u8 {
        /// attempt to build a "parse tree" no matter what. this may result in a 'forest' of
        /// disconnected trees. this is intended to be used by syntax highlighting.
        const CONTINUE_AFTER_ERROR = 1 << 0;
        /// include comment tokens.
        const INCLUDE_COMMENTS = 1 << 1;
        /// indicate that the tokenizer should accept incomplete tokens */
        const ACCEPT_INCOMPLETE_TOKENS = 1 << 2;
        /// indicate that the parser should not generate the terminate token, allowing an 'unfinished'
        /// tree where some nodes may have no productions.
        const LEAVE_UNTERMINATED = 1 << 3;
        /// indicate that the parser should generate job_list entries for blank lines.
        const SHOW_BLANK_LINES = 1 << 4;
        /// indicate that extra semis should be generated.
        const SHOW_EXTRA_SEMIS = 1 << 5;
    }
}

bitflags! {
    #[derive(Default)]
    pub struct ParserTestErrorBits: u8 {
        const ERROR = 1;
        const INCOMPLETE = 2;
    }
}

#[cxx::bridge]
mod parse_constants_ffi {
    extern "C++" {
        include!("wutil.h");
        type wcharz_t = super::wcharz_t;
    }

    /// A range of source code.
    #[derive(PartialEq, Eq, Clone, Copy, Debug)]
    pub struct SourceRange {
        start: u32,
        length: u32,
    }

    extern "Rust" {
        #[cxx_name = "end"]
        fn end_ffi(self: &SourceRange) -> u32;
        #[cxx_name = "contains_inclusive"]
        fn contains_inclusive_ffi(self: &SourceRange, loc: u32) -> bool;
    }

    /// IMPORTANT: If the following enum table is modified you must also update token_type_description below.
    /// TODO above comment can be removed when we drop the FFI and get real enums.
    #[derive(Clone, Copy, Debug)]
    pub enum ParseTokenType {
        invalid = 1,

        // Terminal types.
        string,
        pipe,
        redirection,
        background,
        andand,
        oror,
        end,
        // Special terminal type that means no more tokens forthcoming.
        terminate,
        // Very special terminal types that don't appear in the production list.
        error,
        tokenizer_error,
        comment,
    }

    #[repr(u8)]
    #[derive(Clone, Copy, Debug)]
    pub enum ParseKeyword {
        // 'none' is not a keyword, it is a sentinel indicating nothing.
        none,

        kw_and,
        kw_begin,
        kw_builtin,
        kw_case,
        kw_command,
        kw_else,
        kw_end,
        kw_exclam,
        kw_exec,
        kw_for,
        kw_function,
        kw_if,
        kw_in,
        kw_not,
        kw_or,
        kw_switch,
        kw_time,
        kw_while,
    }

    extern "Rust" {
        fn token_type_description(token_type: ParseTokenType) -> wcharz_t;
        fn keyword_description(keyword: ParseKeyword) -> wcharz_t;
        fn keyword_from_string(s: wcharz_t) -> ParseKeyword;
    }

    // Statement decorations like 'command' or 'exec'.
    #[derive(Eq, PartialEq)]
    pub enum StatementDecoration {
        none,
        command,
        builtin,
        exec,
    }

    // Parse error code list.
    pub enum ParseErrorCode {
        none,

        // Matching values from enum parser_error.
        syntax,
        cmdsubst,

        generic, // unclassified error types

        // Tokenizer errors.
        tokenizer_unterminated_quote,
        tokenizer_unterminated_subshell,
        tokenizer_unterminated_slice,
        tokenizer_unterminated_escape,
        tokenizer_other,

        unbalancing_end,          // end outside of block
        unbalancing_else,         // else outside of if
        unbalancing_case,         // case outside of switch
        bare_variable_assignment, // a=b without command
        andor_in_pipeline,        // "and" or "or" after a pipe
    }

    struct parse_error_t {
        text: UniquePtr<CxxWString>,
        code: ParseErrorCode,
        source_start: usize,
        source_length: usize,
    }

    extern "Rust" {
        type ParseError;
        fn code(self: &ParseError) -> ParseErrorCode;
        fn source_start(self: &ParseError) -> usize;
        fn text(self: &ParseError) -> UniquePtr<CxxWString>;

        #[cxx_name = "describe"]
        fn describe_ffi(
            self: &ParseError,
            src: &CxxWString,
            is_interactive: bool,
        ) -> UniquePtr<CxxWString>;
        #[cxx_name = "describe_with_prefix"]
        fn describe_with_prefix_ffi(
            self: &ParseError,
            src: &CxxWString,
            prefix: &CxxWString,
            is_interactive: bool,
            skip_caret: bool,
        ) -> UniquePtr<CxxWString>;

        fn describe_with_prefix(
            self: &parse_error_t,
            src: &CxxWString,
            prefix: &CxxWString,
            is_interactive: bool,
            skip_caret: bool,
        ) -> UniquePtr<CxxWString>;

        type ParseErrorListFfi;
        fn new_parse_error_list() -> Box<ParseErrorListFfi>;
        #[cxx_name = "offset_source_start"]
        fn offset_source_start_ffi(self: &mut ParseErrorListFfi, amt: usize);
        fn size(self: &ParseErrorListFfi) -> usize;
        fn at(self: &ParseErrorListFfi, offset: usize) -> *const ParseError;
        fn empty(self: &ParseErrorListFfi) -> bool;
        fn push_back(self: &mut ParseErrorListFfi, error: &parse_error_t);
        fn append(self: &mut ParseErrorListFfi, other: *mut ParseErrorListFfi);
        fn erase(self: &mut ParseErrorListFfi, index: usize);
        fn clear(self: &mut ParseErrorListFfi);
    }

    extern "Rust" {
        #[cxx_name = "token_type_user_presentable_description"]
        fn token_type_user_presentable_description_ffi(
            type_: ParseTokenType,
            keyword: ParseKeyword,
        ) -> UniquePtr<CxxWString>;
    }

    // The location of a pipeline.
    pub enum PipelinePosition {
        none,       // not part of a pipeline
        first,      // first command in a pipeline
        subsequent, // second or further command in a pipeline
    }
}

pub use parse_constants_ffi::{
    parse_error_t, ParseErrorCode, ParseKeyword, ParseTokenType, PipelinePosition, SourceRange,
    StatementDecoration,
};

impl SourceRange {
    pub fn new(start: usize, length: usize) -> Self {
        SourceRange {
            start: start.try_into().unwrap(),
            length: length.try_into().unwrap(),
        }
    }
    pub fn start(&self) -> usize {
        self.start.try_into().unwrap()
    }
    pub fn length(&self) -> usize {
        self.length.try_into().unwrap()
    }
    pub fn end(&self) -> usize {
        self.start
            .checked_add(self.length)
            .expect("Overflow")
            .try_into()
            .unwrap()
    }
    fn end_ffi(&self) -> u32 {
        self.start.checked_add(self.length).expect("Overflow")
    }

    // \return true if a location is in this range, including one-past-the-end.
    pub fn contains_inclusive(&self, loc: usize) -> bool {
        self.start() <= loc && loc - self.start() <= self.length()
    }
    fn contains_inclusive_ffi(&self, loc: u32) -> bool {
        self.start <= loc && loc - self.start <= self.length
    }
}

impl Default for ParseTokenType {
    fn default() -> Self {
        ParseTokenType::invalid
    }
}

impl From<ParseTokenType> for &'static wstr {
    #[widestrs]
    fn from(token_type: ParseTokenType) -> Self {
        match token_type {
            ParseTokenType::comment => "ParseTokenType::comment"L,
            ParseTokenType::error => "ParseTokenType::error"L,
            ParseTokenType::tokenizer_error => "ParseTokenType::tokenizer_error"L,
            ParseTokenType::background => "ParseTokenType::background"L,
            ParseTokenType::end => "ParseTokenType::end"L,
            ParseTokenType::pipe => "ParseTokenType::pipe"L,
            ParseTokenType::redirection => "ParseTokenType::redirection"L,
            ParseTokenType::string => "ParseTokenType::string"L,
            ParseTokenType::andand => "ParseTokenType::andand"L,
            ParseTokenType::oror => "ParseTokenType::oror"L,
            ParseTokenType::terminate => "ParseTokenType::terminate"L,
            ParseTokenType::invalid => "ParseTokenType::invalid"L,
            _ => "unknown token type"L,
        }
    }
}

fn token_type_description(token_type: ParseTokenType) -> wcharz_t {
    let s: &'static wstr = token_type.into();
    wcharz!(s)
}

impl Default for ParseKeyword {
    fn default() -> Self {
        ParseKeyword::none
    }
}

impl From<ParseKeyword> for &'static wstr {
    #[widestrs]
    fn from(keyword: ParseKeyword) -> Self {
        match keyword {
            ParseKeyword::kw_exclam => "!"L,
            ParseKeyword::kw_and => "and"L,
            ParseKeyword::kw_begin => "begin"L,
            ParseKeyword::kw_builtin => "builtin"L,
            ParseKeyword::kw_case => "case"L,
            ParseKeyword::kw_command => "command"L,
            ParseKeyword::kw_else => "else"L,
            ParseKeyword::kw_end => "end"L,
            ParseKeyword::kw_exec => "exec"L,
            ParseKeyword::kw_for => "for"L,
            ParseKeyword::kw_function => "function"L,
            ParseKeyword::kw_if => "if"L,
            ParseKeyword::kw_in => "in"L,
            ParseKeyword::kw_not => "not"L,
            ParseKeyword::kw_or => "or"L,
            ParseKeyword::kw_switch => "switch"L,
            ParseKeyword::kw_time => "time"L,
            ParseKeyword::kw_while => "while"L,
            _ => "unknown_keyword"L,
        }
    }
}

impl printf_compat::args::ToArg<'static> for ParseKeyword {
    fn to_arg(self) -> printf_compat::args::Arg<'static> {
        printf_compat::args::Arg::Str(self.into())
    }
}

fn keyword_description(keyword: ParseKeyword) -> wcharz_t {
    let s: &'static wstr = keyword.into();
    wcharz!(s)
}

impl From<&wstr> for ParseKeyword {
    #[widestrs]
    fn from(s: &wstr) -> Self {
        match s {
            _ if s == "!"L => ParseKeyword::kw_exclam,
            _ if s == "and"L => ParseKeyword::kw_and,
            _ if s == "begin"L => ParseKeyword::kw_begin,
            _ if s == "builtin"L => ParseKeyword::kw_builtin,
            _ if s == "case"L => ParseKeyword::kw_case,
            _ if s == "command"L => ParseKeyword::kw_command,
            _ if s == "else"L => ParseKeyword::kw_else,
            _ if s == "end"L => ParseKeyword::kw_end,
            _ if s == "exec"L => ParseKeyword::kw_exec,
            _ if s == "for"L => ParseKeyword::kw_for,
            _ if s == "function"L => ParseKeyword::kw_function,
            _ if s == "if"L => ParseKeyword::kw_if,
            _ if s == "in"L => ParseKeyword::kw_in,
            _ if s == "not"L => ParseKeyword::kw_not,
            _ if s == "or"L => ParseKeyword::kw_or,
            _ if s == "switch"L => ParseKeyword::kw_switch,
            _ if s == "time"L => ParseKeyword::kw_time,
            _ if s == "while"L => ParseKeyword::kw_while,
            _ => ParseKeyword::none,
        }
    }
}

fn keyword_from_string<'a>(s: impl Into<&'a wstr>) -> ParseKeyword {
    let s: &wstr = s.into();
    ParseKeyword::from(s)
}

impl Default for ParseErrorCode {
    fn default() -> Self {
        ParseErrorCode::none
    }
}

#[derive(Clone, Default)]
pub struct ParseError {
    /// Text of the error.
    pub text: WString,
    /// Code for the error.
    pub code: ParseErrorCode,
    /// Offset and length of the token in the source code that triggered this error.
    pub source_start: usize,
    pub source_length: usize,
}

impl ParseError {
    /// Return a string describing the error, suitable for presentation to the user. If
    /// is_interactive is true, the offending line with a caret is printed as well.
    pub fn describe(self: &ParseError, src: &wstr, is_interactive: bool) -> WString {
        self.describe_with_prefix(src, L!(""), is_interactive, false)
    }

    /// Return a string describing the error, suitable for presentation to the user, with the given
    /// prefix. If skip_caret is false, the offending line with a caret is printed as well.
    pub fn describe_with_prefix(
        self: &ParseError,
        src: &wstr,
        prefix: &wstr,
        is_interactive: bool,
        skip_caret: bool,
    ) -> WString {
        let mut result = prefix.to_owned();
        // Some errors don't have their message passed in, so we construct them here.
        // This affects e.g. `eval "a=(foo)"`
        match self.code {
            ParseErrorCode::andor_in_pipeline => {
                let context = wstr::from_char_slice(
                    &src.as_char_slice()[self.source_start..self.source_start + self.source_length],
                );
                result += wstr::from_char_slice(
                    wgettext_fmt!(INVALID_PIPELINE_CMD_ERR_MSG, context).as_char_slice(),
                );
            }
            ParseErrorCode::bare_variable_assignment => {
                let context = wstr::from_char_slice(
                    &src.as_char_slice()[self.source_start..self.source_start + self.source_length],
                );
                let assignment_src = context;
                #[allow(clippy::explicit_auto_deref)]
                let equals_pos = variable_assignment_equals_pos(assignment_src).unwrap();
                let variable = &assignment_src[..equals_pos];
                let value = &assignment_src[equals_pos + 1..];
                result += wstr::from_char_slice(
                    wgettext_fmt!(ERROR_BAD_COMMAND_ASSIGN_ERR_MSG, variable, value)
                        .as_char_slice(),
                );
            }
            _ => {
                if skip_caret && self.text.is_empty() {
                    return L!("").to_owned();
                }
                result += wstr::from_char_slice(self.text.as_char_slice());
            }
        }

        let mut start = self.source_start;
        let mut len = self.source_length;
        if start >= src.len() {
            // If we are past the source, we clamp it to the end.
            start = src.len() - 1;
            len = 0;
        }

        if start + len > src.len() {
            len = src.len() - self.source_start;
        }

        if skip_caret {
            return result;
        }

        // Locate the beginning of this line of source.
        let mut line_start = 0;

        // Look for a newline prior to source_start. If we don't find one, start at the beginning of
        // the string; otherwise start one past the newline. Note that source_start may itself point
        // at a newline; we want to find the newline before it.
        if start > 0 {
            let prefix = &src.as_char_slice()[..start];
            let newline_left_of_start = prefix.iter().rev().position(|c| *c == '\n');
            if let Some(left_of_start) = newline_left_of_start {
                line_start = start - left_of_start;
            }
        }
        // Look for the newline after the source range. If the source range itself includes a
        // newline, that's the one we want, so start just before the end of the range.
        let last_char_in_range = if len == 0 { start } else { start + len - 1 };
        let line_end = src.as_char_slice()[last_char_in_range..]
            .iter()
            .position(|c| *c == '\n')
            .map(|pos| pos + last_char_in_range)
            .unwrap_or(src.len());

        assert!(line_end >= line_start);
        assert!(start >= line_start);

        // Don't include the caret and line if we're interactive and this is the first line, because
        // then it's obvious.
        let interactive_skip_caret = is_interactive && start == 0;
        if interactive_skip_caret {
            return result;
        }

        // Append the line of text.
        if !result.is_empty() {
            result += "\n";
        }
        result += wstr::from_char_slice(&src.as_char_slice()[line_start..line_end]);

        // Append the caret line. The input source may include tabs; for that reason we
        // construct a "caret line" that has tabs in corresponding positions.
        let mut caret_space_line = WString::new();
        caret_space_line.reserve(start - line_start);
        for i in line_start..start {
            let wc = src.as_char_slice()[i];
            if wc == '\t' {
                caret_space_line += "\t";
            } else if wc == '\n' {
                // It's possible that the start points at a newline itself. In that case,
                // pretend it's a space. We only expect this to be at the end of the string.
                caret_space_line += " ";
            } else {
                let width = fish_wcwidth(wc.into()).0;
                if width > 0 {
                    caret_space_line += " ".repeat(width as usize).as_str();
                }
            }
        }
        result += "\n";
        result += wstr::from_char_slice(caret_space_line.as_char_slice());
        result += "^";
        if len > 1 {
            // Add a squiggle under the error location.
            // We do it like this
            //               ^~~^
            // With a "^" under the start and end, and squiggles in-between.
            let width = fish_wcswidth(unsafe { src.as_ptr().add(start) }, len).0;
            if width >= 2 {
                // Subtract one for each of the carets - this is important in case
                // the starting char has a width of > 1.
                result += "~".repeat(width as usize - 2).as_str();
                result += "^";
            }
        }
        result
    }
}

impl From<&parse_error_t> for ParseError {
    fn from(error: &parse_error_t) -> Self {
        ParseError {
            text: error.text.from_ffi(),
            code: error.code,
            source_start: error.source_start,
            source_length: error.source_length,
        }
    }
}

impl parse_error_t {
    fn describe_with_prefix(
        self: &parse_error_t,
        src: &CxxWString,
        prefix: &CxxWString,
        is_interactive: bool,
        skip_caret: bool,
    ) -> UniquePtr<CxxWString> {
        ParseError::from(self).describe_with_prefix_ffi(src, prefix, is_interactive, skip_caret)
    }
}

impl ParseError {
    fn code(&self) -> ParseErrorCode {
        self.code
    }
    fn source_start(&self) -> usize {
        self.source_start
    }
    fn text(&self) -> UniquePtr<CxxWString> {
        self.text.to_ffi()
    }

    fn describe_ffi(
        self: &ParseError,
        src: &CxxWString,
        is_interactive: bool,
    ) -> UniquePtr<CxxWString> {
        self.describe(src.as_wstr(), is_interactive).to_ffi()
    }

    fn describe_with_prefix_ffi(
        self: &ParseError,
        src: &CxxWString,
        prefix: &CxxWString,
        is_interactive: bool,
        skip_caret: bool,
    ) -> UniquePtr<CxxWString> {
        self.describe_with_prefix(src.as_wstr(), prefix.as_wstr(), is_interactive, skip_caret)
            .to_ffi()
    }
}

#[widestrs]
pub fn token_type_user_presentable_description(
    type_: ParseTokenType,
    keyword: ParseKeyword,
) -> WString {
    if keyword != ParseKeyword::none {
        return sprintf!("keyword: '%ls'"L, Into::<&'static wstr>::into(keyword));
    }
    match type_ {
        ParseTokenType::string => "a string"L.to_owned(),
        ParseTokenType::pipe => "a pipe"L.to_owned(),
        ParseTokenType::redirection => "a redirection"L.to_owned(),
        ParseTokenType::background => "a '&'"L.to_owned(),
        ParseTokenType::andand => "'&&'"L.to_owned(),
        ParseTokenType::oror => "'||'"L.to_owned(),
        ParseTokenType::end => "end of the statement"L.to_owned(),
        ParseTokenType::terminate => "end of the input"L.to_owned(),
        ParseTokenType::error => "a parse error"L.to_owned(),
        ParseTokenType::tokenizer_error => "an incomplete token"L.to_owned(),
        ParseTokenType::comment => "a comment"L.to_owned(),
        _ => sprintf!("a %ls"L, Into::<&'static wstr>::into(type_)),
    }
}

fn token_type_user_presentable_description_ffi(
    type_: ParseTokenType,
    keyword: ParseKeyword,
) -> UniquePtr<CxxWString> {
    token_type_user_presentable_description(type_, keyword).to_ffi()
}

pub type ParseErrorList = Vec<ParseError>;

#[derive(Clone)]
pub struct ParseErrorListFfi(pub ParseErrorList);

unsafe impl ExternType for ParseErrorListFfi {
    type Id = type_id!("ParseErrorListFfi");
    type Kind = cxx::kind::Opaque;
}

/// Helper function to offset error positions by the given amount. This is used when determining
/// errors in a substring of a larger source buffer.
pub fn parse_error_offset_source_start(errors: &mut ParseErrorList, amt: usize) {
    if amt > 0 {
        for ref mut error in errors.iter_mut() {
            // Preserve the special meaning of -1 as 'unknown'.
            if error.source_start != SOURCE_LOCATION_UNKNOWN {
                error.source_start += amt;
            }
        }
    }
}

fn new_parse_error_list() -> Box<ParseErrorListFfi> {
    Box::new(ParseErrorListFfi(Vec::new()))
}

impl ParseErrorListFfi {
    fn offset_source_start_ffi(&mut self, amt: usize) {
        parse_error_offset_source_start(&mut self.0, amt)
    }

    fn size(&self) -> usize {
        self.0.len()
    }

    fn at(&self, offset: usize) -> *const ParseError {
        &self.0[offset]
    }

    fn empty(&self) -> bool {
        self.0.is_empty()
    }

    fn push_back(&mut self, error: &parse_error_t) {
        self.0.push(error.into())
    }

    fn append(&mut self, other: *mut ParseErrorListFfi) {
        self.0.append(&mut (unsafe { &*other }.0.clone()));
    }

    fn erase(&mut self, index: usize) {
        self.0.remove(index);
    }

    fn clear(&mut self) {
        self.0.clear()
    }
}

/// Maximum number of function calls.
pub const FISH_MAX_STACK_DEPTH: usize = 128;

/// Maximum number of nested string substitutions (in lieu of evals)
/// Reduced under TSAN: our CI test creates 500 jobs and this is very slow with TSAN.
#[cfg(feature = "FISH_TSAN_WORKAROUNDS")]
pub const FISH_MAX_EVAL_DEPTH: usize = 250;
#[cfg(not(feature = "FISH_TSAN_WORKAROUNDS"))]
pub const FISH_MAX_EVAL_DEPTH: usize = 500;

/// Error message on a function that calls itself immediately.
pub const INFINITE_FUNC_RECURSION_ERR_MSG: &str =
    "The function '%ls' calls itself immediately, which would result in an infinite loop.";

/// Error message on reaching maximum call stack depth.
pub const CALL_STACK_LIMIT_EXCEEDED_ERR_MSG: &str =
    "The call stack limit has been exceeded. Do you have an accidental infinite loop?";

/// Error message when encountering an unknown builtin name.
pub const UNKNOWN_BUILTIN_ERR_MSG: &str = "Unknown builtin '%ls'";

/// Error message when encountering a failed expansion, e.g. for the variable name in for loops.
pub const FAILED_EXPANSION_VARIABLE_NAME_ERR_MSG: &str = "Unable to expand variable name '%ls'";

/// Error message when encountering an illegal file descriptor.
pub const ILLEGAL_FD_ERR_MSG: &str = "Illegal file descriptor in redirection '%ls'";

/// Error message for wildcards with no matches.
pub const WILDCARD_ERR_MSG: &str = "No matches for wildcard '%ls'. See `help wildcards-globbing`.";

/// Error when using break outside of loop.
pub const INVALID_BREAK_ERR_MSG: &str = "'break' while not inside of loop";

/// Error when using continue outside of loop.
pub const INVALID_CONTINUE_ERR_MSG: &str = "'continue' while not inside of loop";

/// Error message when a command may not be in a pipeline.
pub const INVALID_PIPELINE_CMD_ERR_MSG: &str = "The '%ls' command can not be used in a pipeline";

// Error messages. The number is a reminder of how many format specifiers are contained.

/// Error for $^.
pub const ERROR_BAD_VAR_CHAR1: &str = "$%lc is not a valid variable in fish.";

/// Error for ${a}.
pub const ERROR_BRACKETED_VARIABLE1: &str =
    "Variables cannot be bracketed. In fish, please use {$%ls}.";

/// Error for "${a}".
pub const ERROR_BRACKETED_VARIABLE_QUOTED1: &str =
    "Variables cannot be bracketed. In fish, please use \"$%ls\".";

/// Error issued on $?.
pub const ERROR_NOT_STATUS: &str = "$? is not the exit status. In fish, please use $status.";

/// Error issued on $$.
pub const ERROR_NOT_PID: &str = "$$ is not the pid. In fish, please use $fish_pid.";

/// Error issued on $#.
pub const ERROR_NOT_ARGV_COUNT: &str = "$# is not supported. In fish, please use 'count $argv'.";

/// Error issued on $@.
pub const ERROR_NOT_ARGV_AT: &str = "$@ is not supported. In fish, please use $argv.";

/// Error issued on $*.
pub const ERROR_NOT_ARGV_STAR: &str = "$* is not supported. In fish, please use $argv.";

/// Error issued on $.
pub const ERROR_NO_VAR_NAME: &str = "Expected a variable name after this $.";

/// Error message for Posix-style assignment: foo=bar.
pub const ERROR_BAD_COMMAND_ASSIGN_ERR_MSG: &str =
    "Unsupported use of '='. In fish, please use 'set %ls %ls'.";

/// Error message for a command like `time foo &`.
pub const ERROR_TIME_BACKGROUND: &str =
    "'time' is not supported for background jobs. Consider using 'command time'.";

/// Error issued on { echo; echo }.
pub const ERROR_NO_BRACE_GROUPING: &str =
    "'{ ... }' is not supported for grouping commands. Please use 'begin; ...; end'";
