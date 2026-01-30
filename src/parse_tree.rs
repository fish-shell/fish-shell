//! Programmatic representation of fish code.

use std::ops::Deref;
use std::pin::Pin;
use std::ptr::NonNull;
use std::sync::Arc;

use crate::ast::{self, Ast, JobList, Node};
use crate::common::{assert_send, assert_sync};
use crate::parse_constants::{
    ParseErrorCode, ParseErrorList, ParseKeyword, ParseTokenType, ParseTreeFlags,
    SOURCE_OFFSET_INVALID, SourceOffset, SourceRange, token_type_user_presentable_description,
};
use crate::prelude::*;
use crate::tokenizer::TokenizerError;
use fish_wcstringutil::count_newlines;

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
            keyword: ParseKeyword::None,
            has_dash_prefix: false,
            is_help_argument: false,
            is_newline: false,
            may_be_variable_assignment: false,
            tok_error: TokenizerError::None,
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
        self.typ == ParseTokenType::String && self.has_dash_prefix
    }
    /// Returns a string description of the given parse token.
    pub fn describe(&self) -> WString {
        let mut result = self.typ.to_wstr().to_owned();
        if self.keyword != ParseKeyword::None {
            sprintf!(=> &mut result, " <%s>", self.keyword.to_wstr());
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
            TokenizerError::None => ParseErrorCode::None,
            TokenizerError::UnterminatedQuote => ParseErrorCode::TokenizerUnterminatedQuote,
            TokenizerError::UnterminatedSubshell => ParseErrorCode::TokenizerUnterminatedSubshell,
            TokenizerError::UnterminatedSlice => ParseErrorCode::TokenizerUnterminatedSlice,
            TokenizerError::UnterminatedEscape => ParseErrorCode::TokenizerUnterminatedEscape,
            // To-do: maybe also unbalancing brace?
            _ => ParseErrorCode::TokenizerOther,
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

/// Cache for efficient line number computation when processing multiple nodes.
/// Caches the last source's offset and newline count.
#[derive(Default)]
pub struct SourceLineCache {
    /// Pointer to last source used
    last_source: Option<NonNull<ParsedSource>>,
    /// Exclusive offset of the number of newlines counted
    offset: usize,
    /// Count of newlines up to offset
    count: usize,
}

impl ParsedSource {
    /// Compute the 1-based line number for a given offset, using and updating the cache.
    pub fn lineno_for_offset(&self, offset: usize, cache: &mut SourceLineCache) -> u32 {
        // TODO(MSRV>=1.89): feature(non_null_from_ref)
        let self_ptr = unsafe { NonNull::new_unchecked(std::ptr::from_ref(self).cast_mut()) };

        // If source changed, reset cache.
        if cache.last_source != Some(self_ptr) {
            cache.last_source = Some(self_ptr);
            cache.offset = 0;
            cache.count = 0;
        }

        #[allow(clippy::comparison_chain)] // TODO(MSRV>=1.90) old clippy
        if offset > cache.offset {
            cache.count += count_newlines(&self.src[cache.offset..offset]);
        } else if offset < cache.offset {
            cache.count -= count_newlines(&self.src[offset..cache.offset]);
        }
        cache.offset = offset;

        cache.count as u32 + 1
    }
}

impl ParsedSource {
    pub fn new(src: WString, ast: Ast) -> Self {
        ParsedSource { src, ast }
    }

    // Return the top NodeRef for the parse tree, which is of type JobList.
    pub fn top_job_list(self: &Arc<Self>) -> NodeRef<JobList> {
        NodeRef::new(Arc::clone(self), self.ast.top())
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

    // Given a NodeRef, map to a child of the Node.
    // The caller provides a closure to return a reference to the child node
    // from the parent.
    pub fn child_ref<ChildType: Node>(
        &self,
        func: impl FnOnce(&NodeType) -> &ChildType,
    ) -> NodeRef<ChildType> {
        NodeRef {
            parsed_source: self.parsed_source.clone(),
            node: func(self),
        }
    }

    // Return the source offset of this node, if any.
    pub fn source_offset(&self) -> Option<usize> {
        self.try_source_range().map(|r| r.start())
    }

    // Return the source, as a string.
    pub fn source_str(&self) -> &wstr {
        &self.parsed_source.src
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

    /// Return the 1-based line number of this node, or None if unsourced.
    pub fn lineno(&self) -> Option<std::num::NonZeroU32> {
        self.lineno_with_cache(&mut SourceLineCache::default())
    }

    /// Return the 1-based line number of this node using a cache, or None if unsourced.
    pub fn lineno_with_cache(&self, cache: &mut SourceLineCache) -> Option<std::num::NonZeroU32> {
        let range = self.try_source_range()?;
        let lineno = self.parsed_source.lineno_for_offset(range.start(), cache);
        Some(std::num::NonZeroU32::new(lineno).unwrap())
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
    let ast = ast::parse(&src, flags, errors);
    if ast.errored() && !flags.continue_after_error {
        None
    } else {
        Some(Arc::new(ParsedSource::new(src, ast)))
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_lineno_for_offset() {
        let src = L!("a\nb\nc\nd").to_owned();
        let ps = ParsedSource::new(src, ast::parse(L!(""), ParseTreeFlags::default(), None));
        let mut cache = SourceLineCache::default();

        // Forward progression
        assert_eq!(ps.lineno_for_offset(0, &mut cache), 1);
        assert_eq!(ps.lineno_for_offset(4, &mut cache), 3);
        assert_eq!(ps.lineno_for_offset(6, &mut cache), 4);

        // Backward progression
        assert_eq!(ps.lineno_for_offset(2, &mut cache), 2);
        assert_eq!(ps.lineno_for_offset(0, &mut cache), 1);
    }
}
