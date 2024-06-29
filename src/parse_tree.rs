//! Programmatic representation of fish code.

use std::ops::Deref;
use std::pin::Pin;
use std::sync::Arc;

use crate::ast::{Ast, Node};
use crate::common::{assert_send, assert_sync};
use crate::parse_constants::{
    token_type_user_presentable_description, ParseErrorCode, ParseErrorList, ParseKeyword,
    ParseTokenType, ParseTreeFlags, SourceOffset, SourceRange, SOURCE_OFFSET_INVALID,
};
use crate::tokenizer::TokenizerError;
use crate::wchar::prelude::*;
use crate::wcstringutil::count_newlines;

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
    /// Return the source range.
    /// Note the start may be invalid.
    pub fn range(&self) -> SourceRange {
        SourceRange::new(self.source_start(), self.source_length())
    }
    /// Return whether we are a string with the dash prefix set.
    pub fn is_dash_prefix_string(&self) -> bool {
        self.typ == ParseTokenType::string && self.has_dash_prefix
    }
    /// Returns a string description of the given parse token.
    pub fn describe(&self) -> WString {
        let mut result = self.typ.to_wstr().to_owned();
        if self.keyword != ParseKeyword::none {
            sprintf!(=> &mut result, " <%ls>", self.keyword.to_wstr())
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
    pub ast: Ast,
}

// Safety: this can be derived once the src_ffi field is removed.
unsafe impl Send for ParsedSource {}
unsafe impl Sync for ParsedSource {}

const _: () = assert_send::<ParsedSource>();
const _: () = assert_sync::<ParsedSource>();

impl ParsedSource {
    pub fn new(src: WString, ast: Ast) -> Self {
        ParsedSource { src, ast }
    }

    // Return a line counter over this source.
    pub fn line_counter<NodeType: Node>(self: &Arc<Self>) -> LineCounter<NodeType> {
        LineCounter {
            parsed_source: Pin::new(Arc::clone(self)),
            node: std::ptr::null(),
            cached_offset: 0,
            cached_count: 0,
        }
    }
}

pub type ParsedSourceRef = Arc<ParsedSource>;

/// A reference to a node within a parse tree.
pub struct NodeRef<NodeType: Node> {
    /// The parse tree containing the node.
    /// This is pinned because we hold a pointer into it.
    parsed_source: Pin<Arc<ParsedSource>>,

    /// The node itself. This points into the parsed source.
    node: *const NodeType,
}

impl<NodeType: Node> NodeRef<NodeType> {
    pub fn new(parsed_source: ParsedSourceRef, node: *const NodeType) -> Self {
        NodeRef {
            parsed_source: Pin::new(parsed_source),
            node,
        }
    }
}

impl<NodeType: Node> Clone for NodeRef<NodeType> {
    fn clone(&self) -> Self {
        NodeRef {
            parsed_source: self.parsed_source.clone(),
            node: self.node,
        }
    }
}

impl<NodeType: Node> Deref for NodeRef<NodeType> {
    type Target = NodeType;
    fn deref(&self) -> &Self::Target {
        // Safety: the node is valid for the lifetime of the source.
        unsafe { &*self.node }
    }
}

impl<NodeType: Node> NodeRef<NodeType> {
    pub fn parsed_source(&self) -> &ParsedSource {
        &self.parsed_source
    }

    pub fn parsed_source_ref(&self) -> ParsedSourceRef {
        Pin::into_inner(self.parsed_source.clone())
    }
}

// Safety: NodeRef is Send and Sync because it's just a pointer into a parse tree, which is pinned.
unsafe impl<NodeType: Node> Send for NodeRef<NodeType> {}
unsafe impl<NodeType: Node> Sync for NodeRef<NodeType> {}

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
        Some(Arc::new(ParsedSource::new(src, ast)))
    }
}

/// A type which assists in returning line numbers.
/// This is a somewhat strange type which both counts line numbers and also holds
/// a reference to a "current" node; this matches the expected usage from parse_execution.
pub struct LineCounter<NodeType: Node> {
    /// The parse tree containing the node.
    /// This is pinned because we hold a pointer into it.
    parsed_source: Pin<Arc<ParsedSource>>,

    /// The node itself. This points into the parsed source, or it may be null.
    node: *const NodeType,

    // Cached line number information: the line number of the start of the node, and the number of newlines.
    cached_offset: usize,
    cached_count: usize,
}

impl<NodeType: Node> LineCounter<NodeType> {
    // Return a line counter for empty source.
    pub fn empty() -> Self {
        let parsed_source =
            Pin::new(parse_source(WString::new(), ParseTreeFlags::default(), None).unwrap());
        LineCounter {
            parsed_source,
            node: std::ptr::null(),
            cached_offset: 0,
            cached_count: 0,
        }
    }

    // Count the number of newlines, leveraging our cache.
    pub fn line_offset_of_character_at_offset(&mut self, offset: usize) -> usize {
        let src = &self.parsed_source.src;
        assert!(offset <= src.len());

        // Easy hack to handle 0.
        if offset == 0 {
            return 0;
        }

        // We want to return the number of newlines at offsets less than the given offset.
        if offset > self.cached_offset {
            // Add one for every newline we find in the range [cached_offset, offset).
            // The codegen is substantially better when using a char slice than the char iterator.
            self.cached_count += count_newlines(&src[self.cached_offset..offset]);
        } else if offset < self.cached_offset {
            // Subtract one for every newline we find in the range [offset, cached_range.start).
            self.cached_count -= count_newlines(&src[offset..self.cached_offset]);
        }
        self.cached_offset = offset;
        self.cached_count
    }

    // Returns the 0-based line number of the node.
    pub fn line_offset_of_node(&mut self) -> Option<usize> {
        let src_offset = self.source_offset_of_node()?;
        Some(self.line_offset_of_character_at_offset(src_offset))
    }

    // Return the 0 based character offset of the node.
    pub fn source_offset_of_node(&mut self) -> Option<usize> {
        // Safety: any node is valid for the lifetime of the source.
        let node = unsafe { self.node.as_ref()? };
        let range = node.try_source_range()?;
        Some(range.start())
    }

    // Set the node. The node must belong to the parsed source.
    // Returns the original node.
    pub fn set_node<'a>(&mut self, node: Option<&'a NodeType>) -> Option<&'a NodeType> {
        let node_ptr = node.map_or(std::ptr::null(), |node| node);
        let prev = std::mem::replace(&mut self.node, node_ptr);
        unsafe { prev.as_ref() }
    }

    // Return the source.
    pub fn get_source(&self) -> &wstr {
        &self.parsed_source.src
    }
}
