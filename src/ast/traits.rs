use super::{
    ast_kind_to_string, Kind, MissingEndError, NodeVisitor, ParseKeyword, ParseTokenType,
    Populator, SourceRangeVisitor,
};
use crate::parse_constants::SourceRange;
use crate::wchar::{wstr, WString};

/// Node is the base trait of all AST nodes.
pub trait Node: Acceptor + AsNode + std::fmt::Debug {
    /// Return the kind of this node.
    fn kind(&self) -> Kind<'_>;

    /// Helper to try to cast to a keyword.
    fn as_keyword(&self) -> Option<&dyn Keyword> {
        match self.kind() {
            Kind::Keyword(n) => Some(n),
            _ => None,
        }
    }

    /// Helper to try to cast to a token.
    fn as_token(&self) -> Option<&dyn Token> {
        match self.kind() {
            Kind::Token(n) => Some(n),
            _ => None,
        }
    }

    /// Helper to try to cast to a leaf.
    /// Note this must be kept in sync with Leaf implementors.
    fn as_leaf(&self) -> Option<&dyn Leaf> {
        match self.kind() {
            Kind::Token(n) => Some(Token::as_leaf(n)),
            Kind::Keyword(n) => Some(Keyword::as_leaf(n)),
            Kind::VariableAssignment(n) => Some(n),
            Kind::MaybeNewlines(n) => Some(n),
            Kind::Argument(n) => Some(n),
            _ => None,
        }
    }

    /// Return a helpful string description of this node.
    fn describe(&self) -> WString {
        let mut res = ast_kind_to_string(self.kind()).to_owned();
        if let Some(n) = self.as_token() {
            let token_type = n.token_type().to_wstr();
            sprintf!(=> &mut res, " '%ls'", token_type);
        } else if let Some(n) = self.as_keyword() {
            let keyword = n.keyword().to_wstr();
            sprintf!(=> &mut res, " '%ls'", keyword);
        }
        res
    }

    /// Return the source range for this node, or none if unsourced.
    /// This may return none if the parse was incomplete or had an error.
    fn try_source_range(&self) -> Option<SourceRange> {
        let mut visitor = SourceRangeVisitor {
            total: SourceRange::new(0, 0),
            any_unsourced: false,
        };
        visitor.visit(self.as_node());
        if visitor.any_unsourced {
            None
        } else {
            Some(visitor.total)
        }
    }

    /// Return the source range for this node, or an empty range {0, 0} if unsourced.
    fn source_range(&self) -> SourceRange {
        self.try_source_range().unwrap_or_default()
    }

    /// Return the source code for this node, or none if unsourced.
    fn try_source<'s>(&self, orig: &'s wstr) -> Option<&'s wstr> {
        self.try_source_range().map(|r| &orig[r.start()..r.end()])
    }

    /// Return the source code for this node, or an empty string if unsourced.
    fn source<'s>(&self, orig: &'s wstr) -> &'s wstr {
        self.try_source(orig).unwrap_or_default()
    }

    /// The raw byte size of this node, excluding children.
    /// This also excludes the allocations stored in lists.
    fn self_memory_size(&self) -> usize {
        std::mem::size_of_val(self)
    }
}

/// Convert to the dynamic Node type.
pub trait AsNode {
    fn as_node(&self) -> &dyn Node;
}

impl<T: Node + Sized> AsNode for T {
    fn as_node(&self) -> &dyn Node {
        self
    }
}

pub(super) type ParseResult<T> = Result<T, MissingEndError>;

/// Only explicitly *implemented* for Keyword nodes.
/// Only explicitly *called* by the Parse impl of a branch node.
///
/// In the event of an error, a branch node's Parse impl will catch the error,
/// stop parsing the rest of the node (leaving subsequent fields default-initialized),
/// and begin an unwind (see [ParserStatus::unwinding]).
/// In effect, this short-circuits the parse, but a JobList is sometimes able
/// to "stop" the unwind and resume parsing another job.
///
/// Either way, the top-level parse operation is infallible in terms of Rust idioms,
/// with error information stored elsewhere in the [Ast] struct and some nodes being
/// unsourced after an error.
pub(super) trait TryParse: Sized {
    fn try_parse(pop: &mut Populator<'_>) -> ParseResult<Self>;
}

pub(super) trait Parse: TryParse {
    fn parse(pop: &mut Populator<'_>) -> Self;
}

impl<T: Parse> TryParse for T {
    fn try_parse(pop: &mut Populator<'_>) -> ParseResult<Self> {
        Ok(Self::parse(pop))
    }
}

/// This is for optional values and for lists.
pub(super) trait CheckParse: TryParse {
    /// A true return means we should descend into the production, false means stop.
    fn can_be_parsed(pop: &mut Populator<'_>) -> bool;
}

impl<T: CheckParse + TryParse> TryParse for Option<T> {
    fn try_parse(pop: &mut Populator<'_>) -> ParseResult<Self> {
        if T::can_be_parsed(pop) {
            Some(pop.try_parse::<T>())
        } else {
            None
        }
        .transpose()
    }
}

/// A list of nodes is parsed by calling CheckParse::can_be_parsed(pop) and T::parse(pop)
/// repeatedly, until the former returns false.
impl<T: CheckParse + Parse + ListElement> Parse for Box<[T]> {
    fn parse(pop: &mut Populator<'_>) -> Self {
        pop.parse_list(false)
    }
}

/// A node that can be parsed as an element of a homogenous list.
pub(super) trait ListElement: Sized + Node {
    fn list_kind(list: &[Self]) -> Kind<'_>;

    /// Return whether a list of these nodes allows arbitrary newlines in it.
    fn chomps_newlines(pop: &Populator<'_>) -> bool;
    /// Return whether a list of these nodes allows arbitrary semicolons in it.
    fn chomps_semis(pop: &Populator<'_>) -> bool;
    /// Return whether a list of these nodes should recover from errors.
    /// That is, whether we should stop unwinding when we encounter this node's parent list.
    fn stops_unwind(_pop: &Populator<'_>) -> bool {
        false
    }
}

impl<'a, N: ListElement> Node for &'a [N] {
    fn kind(&self) -> Kind<'_> {
        N::list_kind(self)
    }
}

impl<N: ListElement> Node for Box<[N]> {
    fn kind(&self) -> Kind<'_> {
        N::list_kind(self)
    }
}

impl<'a, N: ListElement> Acceptor for &'a [N] {
    fn accept<'b>(&'b self, visitor: &mut dyn NodeVisitor<'b>) {
        self.iter().for_each(|item| visitor.visit(item));
    }
}

impl<N: ListElement> Acceptor for Box<[N]> {
    fn accept<'a>(&'a self, visitor: &mut dyn NodeVisitor<'a>) {
        self.iter().for_each(|item| visitor.visit(item));
    }
}

/// Trait for all "leaf" nodes: nodes with no ast children.
pub trait Leaf: Node {
    /// Basically Default::default(), but works around an issue with the Sized bound.
    /// Also slightly clearer about intent.
    fn unsourced() -> Self
    where
        Self: Sized;

    /// Returns none if this node is "unsourced." This happens if for whatever reason we are
    /// unable to parse the node, either because we had a parse error and recovered, or because
    /// we accepted incomplete and the token stream was exhausted.
    fn range(&self) -> Option<SourceRange>;

    fn has_source(&self) -> bool {
        self.range().is_some()
    }
}

// A token node is a node which contains a token, which must be one of a fixed set.
pub trait Token: Leaf {
    fn new(range: SourceRange, token_type: ParseTokenType) -> Self
    where
        Self: Sized;

    // The use of `&dyn Token` prevents making this an associated const.
    fn allowed_tokens() -> &'static [ParseTokenType]
    where
        Self: Sized;

    /// Return whether a token type is allowed in this token_t, i.e. is a member of our Toks list.
    fn allows_token(token_type: ParseTokenType) -> bool
    where
        Self: Sized,
    {
        Self::allowed_tokens().contains(&token_type)
    }

    /// The token type which was parsed.
    fn token_type(&self) -> ParseTokenType;
    fn as_leaf(&self) -> &dyn Leaf;
}

/// A keyword node is a node which contains a keyword, which must be one of a fixed set.
pub trait Keyword: Leaf {
    fn new(range: SourceRange, keyword: ParseKeyword) -> Self
    where
        Self: Sized;

    // The use of `&dyn Keyword` prevents making this an associated const.
    fn allowed_keywords() -> &'static [ParseKeyword]
    where
        Self: Sized;

    fn allows_keyword(kw: ParseKeyword) -> bool
    where
        Self: Sized,
    {
        Self::allowed_keywords().contains(&kw)
    }

    fn keyword(&self) -> ParseKeyword;
    fn as_leaf(&self) -> &dyn Leaf;
}

/// Acceptor is implemented on Nodes which can be visited by a NodeVisitor.
/// It generally invokes the visitor's visit() method on each of its children.
pub trait Acceptor {
    fn accept<'a>(&'a self, visitor: &mut dyn NodeVisitor<'a>);
}

impl<T: Acceptor> Acceptor for Option<T> {
    fn accept<'a>(&'a self, visitor: &mut dyn NodeVisitor<'a>) {
        if let Some(node) = self {
            node.accept(visitor)
        }
    }
}

/// A helper trait to invoke the visit() method on a field.
pub(super) trait VisitableField {
    fn do_visit<'a>(&'a self, visitor: &mut dyn NodeVisitor<'a>);
}

impl<N: Node> VisitableField for N {
    fn do_visit<'a>(&'a self, visitor: &mut dyn NodeVisitor<'a>) {
        visitor.visit(self);
    }
}

impl<N: Node> VisitableField for Option<N> {
    fn do_visit<'a>(&'a self, visitor: &mut dyn NodeVisitor<'a>) {
        if let Some(node) = self {
            node.do_visit(visitor);
        }
    }
}
