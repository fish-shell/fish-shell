//! Programmatic representation of fish code.

use std::pin::Pin;
use std::rc::Rc;

use crate::ast::Ast;
use crate::parse_constants::{
    token_type_user_presentable_description, ParseErrorCode, ParseErrorList, ParseErrorListFfi,
    ParseKeyword, ParseTokenType, ParseTreeFlags, SourceOffset, SourceRange, SOURCE_OFFSET_INVALID,
};
use crate::tokenizer::TokenizerError;
use crate::wchar::{wstr, WString, L};
use crate::wchar_ffi::{WCharFromFFI, WCharToFFI};
use crate::wutil::sprintf;
use cxx::{CxxWString, UniquePtr};

/// A struct representing the token type that we use internally.
#[derive(Clone, Copy)]
pub struct ParseToken {
    /// The type of the token as represented by the parser
    pub typ: ParseTokenType,
    /// Any keyword represented by this token
    pub keyword: ParseKeyword,
    /// Hackish: whether the source contains a dash prefix
    pub has_dash_prefix: bool,
    /// Hackish: whether the source looks like '-h' or '--help'
    pub is_help_argument: bool,
    /// Hackish: if TOK_END, whether the source is a newline.
    pub is_newline: bool,
    // Hackish: whether this token is a string like FOO=bar
    pub may_be_variable_assignment: bool,
    /// If this is a tokenizer error, that error.
    pub tok_error: TokenizerError,
    source_start: SourceOffset,
    source_length: SourceOffset,
}

impl ParseToken {
    pub fn new(typ: ParseTokenType) -> Self {
        ParseToken {
            typ,
            keyword: ParseKeyword::none,
            has_dash_prefix: false,
            is_help_argument: false,
            is_newline: false,
            may_be_variable_assignment: false,
            tok_error: TokenizerError::none,
            source_start: SOURCE_OFFSET_INVALID.try_into().unwrap(),
            source_length: 0,
        }
    }
    pub fn set_source_start(&mut self, value: usize) {
        self.source_start = value.try_into().unwrap();
    }
    pub fn source_start(&self) -> usize {
        self.source_start.try_into().unwrap()
    }
    pub fn set_source_length(&mut self, value: usize) {
        self.source_length = value.try_into().unwrap();
    }
    pub fn source_length(&self) -> usize {
        self.source_length.try_into().unwrap()
    }
    /// \return the source range.
    /// Note the start may be invalid.
    pub fn range(&self) -> SourceRange {
        SourceRange::new(self.source_start(), self.source_length())
    }
    /// \return whether we are a string with the dash prefix set.
    pub fn is_dash_prefix_string(&self) -> bool {
        self.typ == ParseTokenType::string && self.has_dash_prefix
    }
    /// Returns a string description of the given parse token.
    pub fn describe(&self) -> WString {
        let mut result = Into::<&'static wstr>::into(self.typ).to_owned();
        if self.keyword != ParseKeyword::none {
            result += &sprintf!(L!(" <%ls>"), Into::<&'static wstr>::into(self.keyword))[..]
        }
        result
    }
    pub fn user_presentable_description(&self) -> WString {
        token_type_user_presentable_description(self.typ, self.keyword)
    }
}

impl From<TokenizerError> for ParseErrorCode {
    fn from(err: TokenizerError) -> Self {
        match err {
            TokenizerError::none => ParseErrorCode::none,
            TokenizerError::unterminated_quote => ParseErrorCode::tokenizer_unterminated_quote,
            TokenizerError::unterminated_subshell => {
                ParseErrorCode::tokenizer_unterminated_subshell
            }
            TokenizerError::unterminated_slice => ParseErrorCode::tokenizer_unterminated_slice,
            TokenizerError::unterminated_escape => ParseErrorCode::tokenizer_unterminated_escape,
            _ => ParseErrorCode::tokenizer_other,
        }
    }
}

/// A type wrapping up a parse tree and the original source behind it.
pub struct ParsedSource {
    pub src: WString,
    src_ffi: UniquePtr<CxxWString>,
    pub ast: Ast,
}

impl ParsedSource {
    fn new(src: WString, ast: Ast) -> Self {
        let src_ffi = src.to_ffi();
        ParsedSource { src, src_ffi, ast }
    }
}

pub type ParsedSourceRef = Rc<ParsedSource>;

/// Return a shared pointer to ParsedSource, or null on failure.
/// If parse_flag_continue_after_error is not set, this will return null on any error.
pub fn parse_source(
    src: WString,
    flags: ParseTreeFlags,
    errors: Option<&mut ParseErrorList>,
) -> Option<ParsedSourceRef> {
    let ast = Ast::parse(&src, flags, errors);
    if ast.errored() && !flags.contains(ParseTreeFlags::CONTINUE_AFTER_ERROR) {
        None
    } else {
        Some(Rc::new(ParsedSource::new(src, ast)))
    }
}

struct ParsedSourceRefFFI(pub Option<ParsedSourceRef>);

#[cxx::bridge]
mod parse_tree_ffi {
    extern "C++" {
        include!("ast.h");
        pub type Ast = crate::ast::Ast;
        pub type ParseErrorListFfi = crate::parse_constants::ParseErrorListFfi;
    }
    extern "Rust" {
        type ParsedSourceRefFFI;
        fn empty_parsed_source_ref() -> Box<ParsedSourceRefFFI>;
        fn has_value(&self) -> bool;
        fn new_parsed_source_ref(src: &CxxWString, ast: Pin<&mut Ast>) -> Box<ParsedSourceRefFFI>;
        #[cxx_name = "parse_source"]
        fn parse_source_ffi(
            src: &CxxWString,
            flags: u8,
            errors: *mut ParseErrorListFfi,
        ) -> Box<ParsedSourceRefFFI>;
        fn clone(self: &ParsedSourceRefFFI) -> Box<ParsedSourceRefFFI>;
        fn src(self: &ParsedSourceRefFFI) -> &CxxWString;
        fn ast(self: &ParsedSourceRefFFI) -> &Ast;
    }
}

impl ParsedSourceRefFFI {
    fn has_value(&self) -> bool {
        self.0.is_some()
    }
}
fn empty_parsed_source_ref() -> Box<ParsedSourceRefFFI> {
    Box::new(ParsedSourceRefFFI(None))
}
fn new_parsed_source_ref(src: &CxxWString, ast: Pin<&mut Ast>) -> Box<ParsedSourceRefFFI> {
    let mut stolen_ast = Ast::default();
    std::mem::swap(&mut stolen_ast, ast.get_mut());
    Box::new(ParsedSourceRefFFI(Some(Rc::new(ParsedSource::new(
        src.from_ffi(),
        stolen_ast,
    )))))
}
fn parse_source_ffi(
    src: &CxxWString,
    flags: u8,
    errors: *mut ParseErrorListFfi,
) -> Box<ParsedSourceRefFFI> {
    Box::new(ParsedSourceRefFFI(parse_source(
        src.from_ffi(),
        ParseTreeFlags::from_bits(flags).unwrap(),
        if errors.is_null() {
            None
        } else {
            Some(unsafe { &mut (*errors).0 })
        },
    )))
}
impl ParsedSourceRefFFI {
    fn clone(&self) -> Box<ParsedSourceRefFFI> {
        Box::new(ParsedSourceRefFFI(self.0.clone()))
    }
    fn src(&self) -> &CxxWString {
        &self.0.as_ref().unwrap().src_ffi
    }
    fn ast(&self) -> &Ast {
        &self.0.as_ref().unwrap().ast
    }
}
