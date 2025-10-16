/*!
 * This defines the fish abstract syntax tree.
 * The fish ast is a tree data structure. The nodes of the tree
 * are divided into three categories:
 *
 * - leaf nodes refer to a range of source, and have no child nodes.
 * - branch nodes have ONLY child nodes, and no other fields.
 * - list nodes contain a list of some other node type (branch or leaf).
 *
 * Most clients will be interested in visiting the nodes of an ast.
 */
use crate::common::{UnescapeStringStyle, unescape_string};
use crate::flog::{FLOG, FLOGF};
use crate::parse_constants::{
    ERROR_BAD_COMMAND_ASSIGN_ERR_MSG, INVALID_PIPELINE_CMD_ERR_MSG, ParseError, ParseErrorCode,
    ParseErrorList, ParseKeyword, ParseTokenType, ParseTreeFlags, SOURCE_OFFSET_INVALID,
    SourceRange, StatementDecoration, token_type_user_presentable_description,
};
use crate::parse_tree::ParseToken;
#[cfg(test)]
use crate::tests::prelude::*;
use crate::tokenizer::{
    TOK_ACCEPT_UNFINISHED, TOK_ARGUMENT_LIST, TOK_CONTINUE_AFTER_ERROR, TOK_SHOW_COMMENTS,
    TokFlags, TokenType, Tokenizer, TokenizerError, variable_assignment_equals_pos,
};
use crate::wchar::prelude::*;
use macro_rules_attribute::derive;
use std::borrow::Cow;
use std::convert::AsMut;
use std::ops::{ControlFlow, Deref};

/**
 * A NodeVisitor is something which can visit an AST node.
 *
 * To visit a node's fields, use the node's accept() function:
 *    let mut v = MyNodeVisitor{};
 *    node.accept(&mut v);
 */
pub trait NodeVisitor<'a> {
    fn visit(&mut self, node: &'a dyn Node);
}

/**
 * Acceptor is implemented on Nodes which can be visited by a NodeVisitor.
 *
 * It generally invokes the visitor's visit() method on each of its children.
 *
 */
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

/// A helper trait to invoke the visit() or visit_mut() method on a field.
trait VisitableField {
    fn do_visit<'a>(&'a self, visitor: &mut dyn NodeVisitor<'a>);
    fn do_visit_mut<V: NodeVisitorMut>(&mut self, visitor: &mut V) -> VisitResult;
}

impl<N: Node + NodeMut> VisitableField for N {
    fn do_visit<'a>(&'a self, visitor: &mut dyn NodeVisitor<'a>) {
        visitor.visit(self);
    }

    fn do_visit_mut<V: NodeVisitorMut>(&mut self, visitor: &mut V) -> VisitResult {
        visitor.visit_mut(self)
    }
}

impl<N: Node + NodeMut + CheckParse> VisitableField for Option<N> {
    fn do_visit<'a>(&'a self, visitor: &mut dyn NodeVisitor<'a>) {
        if let Some(node) = self {
            node.do_visit(visitor);
        }
    }

    fn do_visit_mut<V: NodeVisitorMut>(&mut self, visitor: &mut V) -> VisitResult {
        visitor.visit_optional_mut(self)
    }
}

pub struct MissingEndError {
    allowed_keywords: &'static [ParseKeyword],
    token: ParseToken,
}

pub type VisitResult = ControlFlow<MissingEndError>;

/// Similar to NodeVisitor, but for mutable nodes.
trait NodeVisitorMut {
    /// will_visit (did_visit) is called before (after) a node's fields are visited.
    fn will_visit_fields_of<N: NodeMut>(&mut self, node: &mut N);
    fn visit_mut<N: NodeMut>(&mut self, node: &mut N) -> VisitResult;
    fn did_visit_fields_of<'a, N: NodeMut>(&'a mut self, node: &'a mut N, flow: VisitResult);

    // Visit an optional field, perhaps populating it.
    fn visit_optional_mut<N: NodeMut + CheckParse>(&mut self, node: &mut Option<N>) -> VisitResult;
}

/**
 * A trait implemented on Nodes which can be visited by a NodeVisitorMut.
 *
 * It generally invokes the right visit() method on each of its children.
 */
trait AcceptorMut {
    fn accept_mut<V: NodeVisitorMut>(&mut self, visitor: &mut V);
}

impl<T: AcceptorMut> AcceptorMut for Option<T> {
    fn accept_mut<V: NodeVisitorMut>(&mut self, visitor: &mut V) {
        if let Some(node) = self {
            node.accept_mut(visitor)
        }
    }
}

/// Node is the base trait of all AST nodes.
pub trait Node: Acceptor + AsNode + std::fmt::Debug {
    /// Return the kind of this node.
    fn kind(&self) -> Kind<'_>;

    /// Return the kind of this node, as a mutable reference.
    fn kind_mut(&mut self) -> KindMut<'_>;

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
            sprintf!(=> &mut res, " '%s'", token_type);
        } else if let Some(n) = self.as_keyword() {
            let keyword = n.keyword().to_wstr();
            sprintf!(=> &mut res, " '%s'", keyword);
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

// Convert to the dynamic Node type.
pub trait AsNode {
    fn as_node(&self) -> &dyn Node;
}

impl<T: Node + Sized> AsNode for T {
    fn as_node(&self) -> &dyn Node {
        self
    }
}

/// Return true if two nodes are the same object.
#[inline(always)]
pub fn is_same_node(lhs: &dyn Node, rhs: &dyn Node) -> bool {
    // There are two subtleties here:
    //
    // 1. Two &dyn Node may reference the same underlying object
    //    with different vtables - for example if one is a &dyn Node
    //    and the other is a &dyn NodeMut.
    //
    // 2. Two &dyn Node may reference different underlying objects
    //    with the same base pointer - for example if a Node is a
    //    the first field in another Node.
    //
    // Therefore we make the following assumptions, which seem sound enough:
    //   1. If two nodes have different base pointers, they reference different objects
    //      (though NOT true in C++).
    //   2. If two nodes have the same base pointer and same vtable, they reference
    //      the same object.
    //   3. If two nodes have the same base pointer but different vtables, they are the same iff
    //      their kind discriminats are equal: a Node cannot have a Node of the same type at the
    //      same address (unless it's a ZST, which does not apply here).
    //
    // Note this is performance-sensitive.

    // Different base pointers => not the same.
    let lptr = lhs as *const dyn Node as *const ();
    let rptr = rhs as *const dyn Node as *const ();
    if !std::ptr::eq(lptr, rptr) {
        return false;
    }

    // Same base pointer and same vtable => same object.
    #[allow(renamed_and_removed_lints)]
    #[allow(clippy::vtable_address_comparisons)] // for old clippy
    if std::ptr::eq(lhs, rhs) {
        return true;
    }

    // Same base pointer, but different vtables.
    // Compare the discriminants of their kinds.
    std::mem::discriminant(&lhs.kind()) == std::mem::discriminant(&rhs.kind())
}

/// NodeMut is a mutable node.
trait NodeMut: Node + AcceptorMut {}
impl<T> NodeMut for T where T: Node + AcceptorMut {}

/// The different kinds of nodes. Note that Token and Keyword have different subtypes.
#[derive(Copy, Clone)]
pub enum Kind<'a> {
    Redirection(&'a Redirection),
    Token(&'a dyn Token),
    Keyword(&'a dyn Keyword),
    VariableAssignment(&'a VariableAssignment),
    VariableAssignmentList(&'a VariableAssignmentList),
    ArgumentOrRedirection(&'a ArgumentOrRedirection),
    ArgumentOrRedirectionList(&'a ArgumentOrRedirectionList),
    Statement(&'a Statement),
    JobPipeline(&'a JobPipeline),
    JobConjunction(&'a JobConjunction),
    BlockStatementHeader(&'a BlockStatementHeader),
    ForHeader(&'a ForHeader),
    WhileHeader(&'a WhileHeader),
    FunctionHeader(&'a FunctionHeader),
    BeginHeader(&'a BeginHeader),
    BlockStatement(&'a BlockStatement),
    BraceStatement(&'a BraceStatement),
    IfClause(&'a IfClause),
    ElseifClause(&'a ElseifClause),
    ElseifClauseList(&'a ElseifClauseList),
    ElseClause(&'a ElseClause),
    IfStatement(&'a IfStatement),
    CaseItem(&'a CaseItem),
    SwitchStatement(&'a SwitchStatement),
    DecoratedStatement(&'a DecoratedStatement),
    NotStatement(&'a NotStatement),
    JobContinuation(&'a JobContinuation),
    JobContinuationList(&'a JobContinuationList),
    JobConjunctionContinuation(&'a JobConjunctionContinuation),
    AndorJob(&'a AndorJob),
    AndorJobList(&'a AndorJobList),
    FreestandingArgumentList(&'a FreestandingArgumentList),
    JobConjunctionContinuationList(&'a JobConjunctionContinuationList),
    MaybeNewlines(&'a MaybeNewlines),
    CaseItemList(&'a CaseItemList),
    Argument(&'a Argument),
    ArgumentList(&'a ArgumentList),
    JobList(&'a JobList),
}

pub enum KindMut<'a> {
    Redirection(&'a mut Redirection),
    Token(&'a mut dyn Token),
    Keyword(&'a mut dyn Keyword),
    VariableAssignment(&'a mut VariableAssignment),
    VariableAssignmentList(&'a mut VariableAssignmentList),
    ArgumentOrRedirection(&'a mut ArgumentOrRedirection),
    ArgumentOrRedirectionList(&'a mut ArgumentOrRedirectionList),
    Statement(&'a mut Statement),
    JobPipeline(&'a mut JobPipeline),
    JobConjunction(&'a mut JobConjunction),
    BlockStatementHeader(&'a mut BlockStatementHeader),
    ForHeader(&'a mut ForHeader),
    WhileHeader(&'a mut WhileHeader),
    FunctionHeader(&'a mut FunctionHeader),
    BeginHeader(&'a mut BeginHeader),
    BlockStatement(&'a mut BlockStatement),
    BraceStatement(&'a mut BraceStatement),
    IfClause(&'a mut IfClause),
    ElseifClause(&'a mut ElseifClause),
    ElseifClauseList(&'a mut ElseifClauseList),
    ElseClause(&'a mut ElseClause),
    IfStatement(&'a mut IfStatement),
    CaseItem(&'a mut CaseItem),
    SwitchStatement(&'a mut SwitchStatement),
    DecoratedStatement(&'a mut DecoratedStatement),
    NotStatement(&'a mut NotStatement),
    JobContinuation(&'a mut JobContinuation),
    JobContinuationList(&'a mut JobContinuationList),
    JobConjunctionContinuation(&'a mut JobConjunctionContinuation),
    AndorJob(&'a mut AndorJob),
    AndorJobList(&'a mut AndorJobList),
    FreestandingArgumentList(&'a mut FreestandingArgumentList),
    JobConjunctionContinuationList(&'a mut JobConjunctionContinuationList),
    MaybeNewlines(&'a mut MaybeNewlines),
    CaseItemList(&'a mut CaseItemList),
    Argument(&'a mut Argument),
    ArgumentList(&'a mut ArgumentList),
    JobList(&'a mut JobList),
}

// Support casting to this type.
pub trait Castable {
    fn cast(node: &dyn Node) -> Option<&Self>;
}

impl<'a> dyn Node + 'a {
    /// Cast to a concrete node type.
    pub fn cast<T: Castable>(&self) -> Option<&T> {
        T::cast(self)
    }
}

/// Trait for all "leaf" nodes: nodes with no ast children.
pub trait Leaf: Node {
    /// Returns none if this node is "unsourced." This happens if for whatever reason we are
    /// unable to parse the node, either because we had a parse error and recovered, or because
    /// we accepted incomplete and the token stream was exhausted.
    fn range(&self) -> Option<SourceRange>;
    fn range_mut(&mut self) -> &mut Option<SourceRange>;
    fn has_source(&self) -> bool {
        self.range().is_some()
    }
}

// A token node is a node which contains a token, which must be one of a fixed set.
pub trait Token: Leaf {
    /// The token type which was parsed.
    fn token_type(&self) -> ParseTokenType;
    fn token_type_mut(&mut self) -> &mut ParseTokenType;
    fn allowed_tokens(&self) -> &'static [ParseTokenType];
    /// Return whether a token type is allowed in this token_t, i.e. is a member of our Toks list.
    fn allows_token(&self, token_type: ParseTokenType) -> bool {
        self.allowed_tokens().contains(&token_type)
    }
    fn as_leaf(&self) -> &dyn Leaf;
}

/// A keyword node is a node which contains a keyword, which must be one of a fixed set.
pub trait Keyword: Leaf {
    fn keyword(&self) -> ParseKeyword;
    fn keyword_mut(&mut self) -> &mut ParseKeyword;
    fn allowed_keywords(&self) -> &'static [ParseKeyword];
    fn allows_keyword(&self, kw: ParseKeyword) -> bool {
        self.allowed_keywords().contains(&kw)
    }
    fn as_leaf(&self) -> &dyn Leaf;
}

/// This is for optional values and for lists.
trait CheckParse: Default {
    /// A true return means we should descend into the production, false means stop.
    fn can_be_parsed(pop: &mut Populator<'_>) -> bool;
}

/// Implement the node trait.
macro_rules! Node {
    ($name:ident) => {
        impl Node for $name {
            fn kind(&self) -> Kind<'_> {
                Kind::$name(self)
            }
            fn kind_mut(&mut self) -> KindMut<'_> {
                KindMut::$name(self)
            }
        }

        impl Castable for $name {
            // Try casting a Node to this type.
            fn cast(node: &dyn Node) -> Option<&Self> {
                match node.kind() {
                    Kind::$name(res) => Some(res),
                    _ => None,
                }
            }
        }
    };

    ( $(#[$_m:meta])* $_v:vis struct $name:ident $_:tt $(;)? ) => {
        Node!($name);
    };

    ( $(#[$_m:meta])* $_v:vis enum $name:ident $_:tt ) => {
        Node!($name);
    };
}

/// Implement the leaf trait.
macro_rules! Leaf {
    ($name:ident) => {
        impl Leaf for $name {
            fn range(&self) -> Option<SourceRange> {
                self.range
            }
            fn range_mut(&mut self) -> &mut Option<SourceRange> {
                &mut self.range
            }
        }
        impl Acceptor for $name {
            #[allow(unused_variables)]
            fn accept<'a>(&'a self, visitor: &mut dyn NodeVisitor<'a>) {}
        }
        impl AcceptorMut for $name {
            #[allow(unused_variables)]
            fn accept_mut<V: NodeVisitorMut>(&mut self, visitor: &mut V) {
                visitor.will_visit_fields_of(self);
                visitor.did_visit_fields_of(self, VisitResult::Continue(()));
            }
        }
    };

    ( $(#[$_m:meta])* $_v:vis struct $name:ident $_:tt $(;)? ) => {
        Leaf!($name);
    };
}

/// Define a node that implements the keyword trait.
macro_rules! define_keyword_node {
    ( $name:ident, $($allowed:ident),* $(,)? ) => {
        #[derive(Default, Debug, Leaf!)]
        pub struct $name {
            range: Option<SourceRange>,
            keyword: ParseKeyword,
        }
        impl Node for $name {
            fn kind(&self) -> Kind<'_> {
                Kind::Keyword(self)
            }
            fn kind_mut(&mut self) -> KindMut<'_> {
                KindMut::Keyword(self)
            }
        }
        impl Keyword for $name {
            fn keyword(&self) -> ParseKeyword {
                self.keyword
            }
            fn keyword_mut(&mut self) -> &mut ParseKeyword {
                &mut self.keyword
            }
            fn allowed_keywords(&self) -> &'static [ParseKeyword] {
                &[$(ParseKeyword::$allowed),*]
            }
            fn as_leaf(&self) -> &dyn Leaf {
                self
            }
        }
    }
}

/// Define a node that implements the token trait.
macro_rules! define_token_node {
    ( $name:ident, $($allowed:ident),* $(,)? ) => {
        #[derive(Default, Debug, Leaf!)]
        pub struct $name {
            range: Option<SourceRange>,
            parse_token_type: ParseTokenType,
        }
        impl Node for $name {
            fn kind(&self) -> Kind<'_> {
                Kind::Token(self)
            }
            fn kind_mut(&mut self) -> KindMut<'_> {
                KindMut::Token(self)
            }
        }
        impl Token for $name {
            fn token_type(&self) -> ParseTokenType {
                self.parse_token_type
            }
            fn token_type_mut(&mut self) -> &mut ParseTokenType {
                &mut self.parse_token_type
            }
            fn allowed_tokens(&self) -> &'static [ParseTokenType] {
                Self::ALLOWED_TOKENS
            }
            fn as_leaf(&self) -> &dyn Leaf {
                self
            }
        }
        impl CheckParse for $name {
            fn can_be_parsed(pop: &mut Populator<'_>) -> bool {
                let typ = pop.peek_type(0);
                Self::ALLOWED_TOKENS.contains(&typ)
            }
        }
        impl $name {
            const ALLOWED_TOKENS: &'static [ParseTokenType] = &[$(ParseTokenType::$allowed),*];
        }
    }
}

/// Define a list node.
macro_rules! define_list_node {
    (
        $name:ident,
        $contents:ident
    ) => {
        #[derive(Default, Debug, Node!)]
        pub struct $name(Box<[$contents]>);

        impl Deref for $name {
            type Target = Box<[$contents]>;
            fn deref(&self) -> &Self::Target {
                &self.0
            }
        }

        impl<'a> IntoIterator for &'a $name {
            type Item = &'a $contents;
            type IntoIter = std::slice::Iter<'a, $contents>;

            fn into_iter(self) -> Self::IntoIter {
                self.0.iter()
            }
        }

        impl AsMut<Box<[$contents]>> for $name {
            fn as_mut(&mut self) -> &mut Box<[$contents]> {
                &mut self.0
            }
        }

        impl Acceptor for $name {
            fn accept<'a>(&'a self, visitor: &mut dyn NodeVisitor<'a>) {
                self.iter().for_each(|item| visitor.visit(item));
            }
        }

        impl AcceptorMut for $name {
            fn accept_mut<V: NodeVisitorMut>(&mut self, visitor: &mut V) {
                visitor.will_visit_fields_of(self);
                let flow = self
                    .0
                    .iter_mut()
                    .try_for_each(|item| visitor.visit_mut(item));
                visitor.did_visit_fields_of(self, flow);
            }
        }
    };
}

/// Implement the acceptor trait for the given branch node.
macro_rules! Acceptor {
    (
        $(#[$_m:meta])*
        $_v:vis struct $name:ident {
            $(
                $(#[$_fm:meta])*
                $_fv:vis $field_name:ident : $_ft:ty
            ),* $(,)?
        }
    ) => {
        impl Acceptor for $name {
            #[allow(unused_variables)]
            fn accept<'a>(&'a self, visitor: &mut dyn NodeVisitor<'a>){
                $(
                    self.$field_name.do_visit(visitor);
                )*
            }
        }
        impl AcceptorMut for $name {
            #[allow(unused_variables)]
            fn accept_mut<V: NodeVisitorMut>(&mut self, visitor: &mut V) {
                visitor.will_visit_fields_of(self);
                let flow = loop {
                    $(
                        let result = self.$field_name.do_visit_mut(visitor);
                        if result.is_break() {
                            break result;
                        }
                    )*
                    break VisitResult::Continue(());
                };
                visitor.did_visit_fields_of(self, flow);
            }
        }
    };
}

/// A redirection has an operator like > or 2>, and a target like /dev/null or &1.
/// Note that pipes are not redirections.
#[derive(Default, Debug, Node!, Acceptor!)]
pub struct Redirection {
    pub oper: TokenRedirection,
    pub target: String_,
}

impl CheckParse for Redirection {
    fn can_be_parsed(pop: &mut Populator<'_>) -> bool {
        pop.peek_type(0) == ParseTokenType::redirection
    }
}

define_list_node!(VariableAssignmentList, VariableAssignment);

#[derive(Debug, Node!)]
pub enum ArgumentOrRedirection {
    Argument(Argument),
    Redirection(Box<Redirection>), // Boxed because it's bigger
}

impl Default for ArgumentOrRedirection {
    fn default() -> Self {
        Self::Argument(Argument::default())
    }
}

impl Acceptor for ArgumentOrRedirection {
    fn accept<'a>(&'a self, visitor: &mut dyn NodeVisitor<'a>) {
        match self {
            Self::Argument(child) => visitor.visit(child),
            Self::Redirection(child) => visitor.visit(&**child),
        };
    }
}
impl AcceptorMut for ArgumentOrRedirection {
    fn accept_mut<V: NodeVisitorMut>(&mut self, visitor: &mut V) {
        visitor.will_visit_fields_of(self);
        let flow = visitor.visit_mut(self);
        visitor.did_visit_fields_of(self, flow);
    }
}

impl ArgumentOrRedirection {
    /// Return whether this represents an argument.
    pub fn is_argument(&self) -> bool {
        matches!(self, Self::Argument(_))
    }

    /// Return whether this represents a redirection
    pub fn is_redirection(&self) -> bool {
        matches!(self, Self::Redirection(_))
    }

    /// Return this as an argument, assuming it wraps one.
    pub fn argument(&self) -> &Argument {
        match self {
            Self::Argument(arg) => arg,
            _ => panic!("Is not an argument"),
        }
    }

    /// Return this as a redirection, assuming it wraps one.
    pub fn redirection(&self) -> &Redirection {
        match self {
            Self::Redirection(redir) => redir,
            _ => panic!("Is not a redirection"),
        }
    }
}

impl CheckParse for ArgumentOrRedirection {
    fn can_be_parsed(pop: &mut Populator<'_>) -> bool {
        let typ = pop.peek_type(0);
        matches!(typ, ParseTokenType::string | ParseTokenType::redirection)
    }
}

define_list_node!(ArgumentOrRedirectionList, ArgumentOrRedirection);

/// A statement is a normal command, or an if / while / etc
#[derive(Debug, Node!)]
pub enum Statement {
    Decorated(DecoratedStatement),
    Not(Box<NotStatement>),
    Block(Box<BlockStatement>),
    Brace(Box<BraceStatement>),
    If(Box<IfStatement>),
    Switch(Box<SwitchStatement>),
}

impl Default for Statement {
    fn default() -> Self {
        Self::Decorated(DecoratedStatement::default())
    }
}

impl Statement {
    // Convenience function to get this statement as a decorated statement, if it is one.
    pub fn as_decorated_statement(&self) -> Option<&DecoratedStatement> {
        match self {
            Self::Decorated(child) => Some(child),
            _ => None,
        }
    }

    // Return the node embedded in this statement.
    fn embedded_node(&self) -> &dyn Node {
        match self {
            Self::Not(child) => &**child,
            Self::Block(child) => &**child,
            Self::Brace(child) => &**child,
            Self::If(child) => &**child,
            Self::Switch(child) => &**child,
            Self::Decorated(child) => child,
        }
    }
}

impl Acceptor for Statement {
    fn accept<'a>(&'a self, visitor: &mut dyn NodeVisitor<'a>) {
        visitor.visit(self.embedded_node());
    }
}

impl AcceptorMut for Statement {
    fn accept_mut<V: NodeVisitorMut>(&mut self, visitor: &mut V) {
        visitor.will_visit_fields_of(self);
        let flow = visitor.visit_mut(self);
        visitor.did_visit_fields_of(self, flow);
    }
}

/// A job is a non-empty list of statements, separated by pipes. (Non-empty is useful for cases
/// like if statements, where we require a command).
#[derive(Default, Debug, Node!, Acceptor!)]
pub struct JobPipeline {
    /// Maybe the time keyword.
    pub time: Option<KeywordTime>,
    /// A (possibly empty) list of variable assignments.
    pub variables: VariableAssignmentList,
    /// The statement.
    pub statement: Statement,
    /// Piped remainder.
    pub continuation: JobContinuationList,
    /// Maybe backgrounded.
    pub bg: Option<TokenBackground>,
}

/// A job_conjunction is a job followed by a && or || continuations.
#[derive(Default, Debug, Node!, Acceptor!)]
pub struct JobConjunction {
    /// The job conjunction decorator.
    pub decorator: Option<JobConjunctionDecorator>,
    /// The job itself.
    pub job: JobPipeline,
    /// The rest of the job conjunction, with && or ||s.
    pub continuations: JobConjunctionContinuationList,
    /// A terminating semicolon or newline.  This is marked optional because it may not be
    /// present, for example the command `echo foo` may not have a terminating newline. It will
    /// only fail to be present if we ran out of tokens.
    pub semi_nl: Option<SemiNl>,
}

impl CheckParse for JobConjunction {
    fn can_be_parsed(pop: &mut Populator<'_>) -> bool {
        let token = pop.peek_token(0);
        // These keywords end a job list.
        token.typ == ParseTokenType::left_brace
            || (token.typ == ParseTokenType::string
                && !matches!(
                    token.keyword,
                    ParseKeyword::Case | ParseKeyword::End | ParseKeyword::Else
                ))
    }
}

#[derive(Default, Debug, Node!, Acceptor!)]
pub struct ForHeader {
    /// 'for'
    pub kw_for: KeywordFor,
    /// var_name
    pub var_name: String_,
    /// 'in'
    pub kw_in: KeywordIn,
    /// list of arguments
    pub args: ArgumentList,
    /// newline or semicolon
    pub semi_nl: SemiNl,
}

#[derive(Default, Debug, Node!, Acceptor!)]
pub struct WhileHeader {
    /// 'while'
    pub kw_while: KeywordWhile,
    pub condition: JobConjunction,
    pub andor_tail: AndorJobList,
}

#[derive(Default, Debug, Node!, Acceptor!)]
pub struct FunctionHeader {
    pub kw_function: KeywordFunction,
    /// functions require at least one argument.
    pub first_arg: Argument,
    pub args: ArgumentList,
    pub semi_nl: SemiNl,
}

#[derive(Default, Debug, Node!, Acceptor!)]
pub struct BeginHeader {
    pub kw_begin: KeywordBegin,
    /// Note that 'begin' does NOT require a semi or nl afterwards.
    /// This is valid: begin echo hi; end
    pub semi_nl: Option<SemiNl>,
}

#[derive(Default, Debug, Node!, Acceptor!)]
pub struct BlockStatement {
    /// A header like for, while, etc.
    pub header: BlockStatementHeader,
    /// List of jobs in this block.
    pub jobs: JobList,
    /// The 'end' node.
    pub end: KeywordEnd,
    /// Arguments and redirections associated with the block.
    pub args_or_redirs: ArgumentOrRedirectionList,
}

#[derive(Default, Debug, Node!, Acceptor!)]
pub struct BraceStatement {
    /// The opening brace, in command position.
    pub left_brace: TokenLeftBrace,
    /// List of jobs in this block.
    pub jobs: JobList,
    /// The closing brace.
    pub right_brace: TokenRightBrace,
    /// Arguments and redirections associated with the block.
    pub args_or_redirs: ArgumentOrRedirectionList,
}

#[derive(Default, Debug, Node!, Acceptor!)]
pub struct IfClause {
    /// The 'if' keyword.
    pub kw_if: KeywordIf,
    /// The 'if' condition.
    pub condition: JobConjunction,
    /// 'and/or' tail.
    pub andor_tail: AndorJobList,
    /// The body to execute if the condition is true.
    pub body: JobList,
}

#[derive(Default, Debug, Node!, Acceptor!)]
pub struct ElseifClause {
    /// The 'else' keyword.
    pub kw_else: KeywordElse,
    /// The 'if' clause following it.
    pub if_clause: IfClause,
}
impl CheckParse for ElseifClause {
    fn can_be_parsed(pop: &mut Populator<'_>) -> bool {
        pop.peek_token(0).keyword == ParseKeyword::Else
            && pop.peek_token(1).keyword == ParseKeyword::If
    }
}

define_list_node!(ElseifClauseList, ElseifClause);

#[derive(Default, Debug, Node!, Acceptor!)]
pub struct ElseClause {
    /// else ; body
    pub kw_else: KeywordElse,
    pub semi_nl: Option<SemiNl>,
    pub body: JobList,
}
impl CheckParse for ElseClause {
    fn can_be_parsed(pop: &mut Populator<'_>) -> bool {
        pop.peek_token(0).keyword == ParseKeyword::Else
    }
}

#[derive(Default, Debug, Node!, Acceptor!)]
pub struct IfStatement {
    /// if part
    pub if_clause: IfClause,
    /// else if list
    pub elseif_clauses: ElseifClauseList,
    /// else part
    pub else_clause: Option<ElseClause>,
    /// literal end
    pub end: KeywordEnd,
    /// block args / redirs
    pub args_or_redirs: ArgumentOrRedirectionList,
}

#[derive(Default, Debug, Node!, Acceptor!)]
pub struct CaseItem {
    /// case \<arguments\> ; body
    pub kw_case: KeywordCase,
    pub arguments: ArgumentList,
    pub semi_nl: SemiNl,
    pub body: JobList,
}
impl CheckParse for CaseItem {
    fn can_be_parsed(pop: &mut Populator<'_>) -> bool {
        pop.peek_token(0).keyword == ParseKeyword::Case
    }
}

#[derive(Default, Debug, Node!, Acceptor!)]
pub struct SwitchStatement {
    /// switch \<argument\> ; body ; end args_redirs
    pub kw_switch: KeywordSwitch,
    pub argument: Argument,
    pub semi_nl: SemiNl,
    pub cases: CaseItemList,
    pub end: KeywordEnd,
    pub args_or_redirs: ArgumentOrRedirectionList,
}

/// A decorated_statement is a command with a list of arguments_or_redirections, possibly with
/// "builtin" or "command" or "exec"
#[derive(Default, Debug, Node!, Acceptor!)]
pub struct DecoratedStatement {
    /// An optional decoration (command, builtin, exec, etc).
    pub opt_decoration: Option<DecoratedStatementDecorator>,
    /// Command to run.
    pub command: String_,
    /// Args and redirs
    pub args_or_redirs: ArgumentOrRedirectionList,
}

/// A not statement like `not true` or `! true`
#[derive(Default, Debug, Node!, Acceptor!)]
pub struct NotStatement {
    /// Keyword, either not or exclam.
    pub kw: KeywordNot,
    pub time: Option<KeywordTime>,
    pub variables: VariableAssignmentList,
    pub contents: Statement,
}

#[derive(Default, Debug, Node!, Acceptor!)]
pub struct JobContinuation {
    pub pipe: TokenPipe,
    pub newlines: MaybeNewlines,
    pub variables: VariableAssignmentList,
    pub statement: Statement,
}
impl CheckParse for JobContinuation {
    fn can_be_parsed(pop: &mut Populator<'_>) -> bool {
        pop.peek_type(0) == ParseTokenType::pipe
    }
}

define_list_node!(JobContinuationList, JobContinuation);

#[derive(Default, Debug, Node!, Acceptor!)]
pub struct JobConjunctionContinuation {
    /// The && or || token.
    pub conjunction: TokenConjunction,
    pub newlines: MaybeNewlines,
    /// The job itself.
    pub job: JobPipeline,
}
impl CheckParse for JobConjunctionContinuation {
    fn can_be_parsed(pop: &mut Populator<'_>) -> bool {
        let typ = pop.peek_type(0);
        matches!(typ, ParseTokenType::andand | ParseTokenType::oror)
    }
}

/// An andor_job just wraps a job, but requires that the job have an 'and' or 'or' job_decorator.
/// Note this is only used for andor_job_list; jobs that are not part of an andor_job_list are not
/// instances of this.
#[derive(Default, Debug, Node!, Acceptor!)]
pub struct AndorJob {
    pub job: JobConjunction,
}
impl CheckParse for AndorJob {
    fn can_be_parsed(pop: &mut Populator<'_>) -> bool {
        let keyword = pop.peek_token(0).keyword;
        if !matches!(keyword, ParseKeyword::And | ParseKeyword::Or) {
            return false;
        }
        // Check that the argument to and/or is a string that's not help. Otherwise
        // it's either 'and --help' or a naked 'and', and not part of this list.
        let next_token = pop.peek_token(1);
        matches!(
            next_token.typ,
            ParseTokenType::string | ParseTokenType::left_brace
        ) && !next_token.is_help_argument
    }
}

define_list_node!(AndorJobList, AndorJob);

/// A freestanding_argument_list is equivalent to a normal argument list, except it may contain
/// TOK_END (newlines, and even semicolons, for historical reasons).
/// In practice the tok_ends are ignored by fish code so we do not bother to store them.
#[derive(Default, Debug, Node!, Acceptor!)]
pub struct FreestandingArgumentList {
    pub arguments: ArgumentList,
}

define_list_node!(JobConjunctionContinuationList, JobConjunctionContinuation);

define_list_node!(ArgumentList, Argument);

// For historical reasons, a job list is a list of job *conjunctions*. This should be fixed.
define_list_node!(JobList, JobConjunction);

define_list_node!(CaseItemList, CaseItem);

/// A variable_assignment contains a source range like FOO=bar.
#[derive(Default, Debug, Node!, Leaf!)]
pub struct VariableAssignment {
    range: Option<SourceRange>,
}
impl CheckParse for VariableAssignment {
    fn can_be_parsed(pop: &mut Populator<'_>) -> bool {
        // Do we have a variable assignment at all?
        if !pop.peek_token(0).may_be_variable_assignment {
            return false;
        }
        // What is the token after it?
        match pop.peek_type(1) {
            // We have `a= cmd` and should treat it as a variable assignment.
            ParseTokenType::string | ParseTokenType::left_brace => true,
            // We have `a=` which is OK if we are allowing incomplete, an error otherwise.
            ParseTokenType::terminate => pop.allow_incomplete(),
            // We have e.g. `a= >` which is an error.
            // Note that we do not produce an error here. Instead we return false
            // so this the token will be seen by allocate_populate_statement.
            _ => false,
        }
    }
}

/// Zero or more newlines.
#[derive(Default, Debug, Node!, Leaf!)]
pub struct MaybeNewlines {
    range: Option<SourceRange>,
}

/// An argument is just a node whose source range determines its contents.
/// This is a separate type because it is sometimes useful to find all arguments.
#[derive(Default, Debug, Node!, Leaf!)]
pub struct Argument {
    range: Option<SourceRange>,
}
impl CheckParse for Argument {
    fn can_be_parsed(pop: &mut Populator<'_>) -> bool {
        pop.peek_type(0) == ParseTokenType::string
    }
}

define_token_node!(SemiNl, end);
define_token_node!(String_, string);
define_token_node!(TokenBackground, background);
define_token_node!(TokenConjunction, andand, oror);
define_token_node!(TokenPipe, pipe);
define_token_node!(TokenLeftBrace, left_brace);
define_token_node!(TokenRightBrace, right_brace);
define_token_node!(TokenRedirection, redirection);

define_keyword_node!(DecoratedStatementDecorator, Command, Builtin, Exec);
define_keyword_node!(JobConjunctionDecorator, And, Or);
define_keyword_node!(KeywordBegin, Begin);
define_keyword_node!(KeywordCase, Case);
define_keyword_node!(KeywordElse, Else);
define_keyword_node!(KeywordEnd, End);
define_keyword_node!(KeywordFor, For);
define_keyword_node!(KeywordFunction, Function);
define_keyword_node!(KeywordIf, If);
define_keyword_node!(KeywordIn, In);
define_keyword_node!(KeywordNot, Not, Exclam);
define_keyword_node!(KeywordSwitch, Switch);
define_keyword_node!(KeywordTime, Time);
define_keyword_node!(KeywordWhile, While);

impl CheckParse for JobConjunctionDecorator {
    fn can_be_parsed(pop: &mut Populator<'_>) -> bool {
        // This is for a job conjunction like `and stuff`
        // But if it's `and --help` then we treat it as an ordinary command.
        let keyword = pop.peek_token(0).keyword;
        if !matches!(keyword, ParseKeyword::And | ParseKeyword::Or) {
            return false;
        }
        !pop.peek_token(1).is_help_argument
    }
}

impl CheckParse for DecoratedStatementDecorator {
    fn can_be_parsed(pop: &mut Populator<'_>) -> bool {
        // Here the keyword is 'command' or 'builtin' or 'exec'.
        // `command stuff` executes a command called stuff.
        // `command -n` passes the -n argument to the 'command' builtin.
        // `command` by itself is a command.
        let keyword = pop.peek_token(0).keyword;
        if !matches!(
            keyword,
            ParseKeyword::Command | ParseKeyword::Builtin | ParseKeyword::Exec
        ) {
            return false;
        }
        let next_token = pop.peek_token(1);
        next_token.typ == ParseTokenType::string && !next_token.is_dash_prefix_string()
    }
}

impl CheckParse for KeywordTime {
    fn can_be_parsed(pop: &mut Populator<'_>) -> bool {
        // Time keyword is only the time builtin if the next argument doesn't have a dash.
        let keyword = pop.peek_token(0).keyword;
        if !matches!(keyword, ParseKeyword::Time) {
            return false;
        }
        !pop.peek_token(1).is_dash_prefix_string()
    }
}

impl DecoratedStatement {
    /// Return the decoration for this statement.
    pub fn decoration(&self) -> StatementDecoration {
        let Some(decorator) = &self.opt_decoration else {
            return StatementDecoration::none;
        };
        let decorator: &dyn Keyword = decorator;
        match decorator.keyword() {
            ParseKeyword::Command => StatementDecoration::command,
            ParseKeyword::Builtin => StatementDecoration::builtin,
            ParseKeyword::Exec => StatementDecoration::exec,
            _ => panic!("Unexpected keyword in statement decoration"),
        }
    }
}

#[derive(Debug, Node!)]
pub enum BlockStatementHeader {
    Begin(BeginHeader),
    For(ForHeader),
    While(WhileHeader),
    Function(FunctionHeader),
}

impl Default for BlockStatementHeader {
    fn default() -> Self {
        Self::Begin(BeginHeader::default())
    }
}

impl BlockStatementHeader {
    pub fn embedded_node(&self) -> &dyn Node {
        match self {
            Self::Begin(child) => child,
            Self::For(child) => child,
            Self::While(child) => child,
            Self::Function(child) => child,
        }
    }
}

impl Acceptor for BlockStatementHeader {
    fn accept<'a>(&'a self, visitor: &mut dyn NodeVisitor<'a>) {
        visitor.visit(self.embedded_node());
    }
}
impl AcceptorMut for BlockStatementHeader {
    fn accept_mut<V: NodeVisitorMut>(&mut self, visitor: &mut V) {
        visitor.will_visit_fields_of(self);
        let flow = visitor.visit_mut(self);
        visitor.did_visit_fields_of(self, flow);
    }
}

/// Return a string literal name for an ast type.
pub fn ast_kind_to_string(k: Kind<'_>) -> &'static wstr {
    match k {
        Kind::Token(_) => L!("token"),
        Kind::Keyword(_) => L!("keyword"),
        Kind::Redirection(_) => L!("redirection"),
        Kind::VariableAssignment(_) => L!("variable_assignment"),
        Kind::VariableAssignmentList(_) => L!("variable_assignment_list"),
        Kind::ArgumentOrRedirection(_) => L!("argument_or_redirection"),
        Kind::ArgumentOrRedirectionList(_) => L!("argument_or_redirection_list"),
        Kind::Statement(_) => L!("statement"),
        Kind::JobPipeline(_) => L!("job_pipeline"),
        Kind::JobConjunction(_) => L!("job_conjunction"),
        Kind::BlockStatementHeader(_) => L!("block_statement_header"),
        Kind::ForHeader(_) => L!("for_header"),
        Kind::WhileHeader(_) => L!("while_header"),
        Kind::FunctionHeader(_) => L!("function_header"),
        Kind::BeginHeader(_) => L!("begin_header"),
        Kind::BlockStatement(_) => L!("block_statement"),
        Kind::BraceStatement(_) => L!("brace_statement"),
        Kind::IfClause(_) => L!("if_clause"),
        Kind::ElseifClause(_) => L!("elseif_clause"),
        Kind::ElseifClauseList(_) => L!("elseif_clause_list"),
        Kind::ElseClause(_) => L!("else_clause"),
        Kind::IfStatement(_) => L!("if_statement"),
        Kind::CaseItem(_) => L!("case_item"),
        Kind::SwitchStatement(_) => L!("switch_statement"),
        Kind::DecoratedStatement(_) => L!("decorated_statement"),
        Kind::NotStatement(_) => L!("not_statement"),
        Kind::JobContinuation(_) => L!("job_continuation"),
        Kind::JobContinuationList(_) => L!("job_continuation_list"),
        Kind::JobConjunctionContinuation(_) => L!("job_conjunction_continuation"),
        Kind::AndorJob(_) => L!("andor_job"),
        Kind::AndorJobList(_) => L!("andor_job_list"),
        Kind::FreestandingArgumentList(_) => L!("freestanding_argument_list"),
        Kind::JobConjunctionContinuationList(_) => L!("job_conjunction_continuation_list"),
        Kind::MaybeNewlines(_) => L!("maybe_newlines"),
        Kind::CaseItemList(_) => L!("case_item_list"),
        Kind::Argument(_) => L!("argument"),
        Kind::ArgumentList(_) => L!("argument_list"),
        Kind::JobList(_) => L!("job_list"),
    }
}

// An entry in the traversal stack. We retain nodes after visiting them, so that we can reconstruct the parent path
// as the list of visited nodes remaining on the stack. Once we've visited a node twice, then it's popped, as that
// means all of its children have been visited (or skipped).
enum TraversalEntry<'a> {
    NeedsVisit(&'a dyn Node),
    Visited(&'a dyn Node),
}

// A way to visit nodes iteratively.
// This is pre-order. Each node is visited before its children.
// Example:
//    let tv = Traversal::new(start);
//    while let Some(node) = tv.next() {...}
pub struct Traversal<'a> {
    stack: Vec<TraversalEntry<'a>>,
}

impl<'a> Traversal<'a> {
    // Construct starting with a node
    pub fn new(n: &'a dyn Node) -> Self {
        Self {
            stack: vec![TraversalEntry::NeedsVisit(n)],
        }
    }

    // Return an iterator over the parent nodes - those which have been visited but not yet popped.
    // Parents are returned in reverse order (immediate parent, grandparent, etc).
    // The most recently visited node is first; that is first parent node will be the node most recently
    // returned by next();
    pub fn parent_nodes(&self) -> impl Iterator<Item = &'a dyn Node> + '_ {
        self.stack.iter().rev().filter_map(|entry| match entry {
            TraversalEntry::Visited(node) => Some(*node),
            _ => None,
        })
    }

    // Return the parent node of the given node, asserting it is on the stack
    // (i.e. we are processing it or one of its children, transitively).
    // Note this does NOT return parents of nodes that have not yet been yielded by the iterator;
    // for example `traversal.parent(&current.child)` will panic because `current.child` has not been
    // yielded yet.
    pub fn parent(&self, node: &dyn Node) -> &'a dyn Node {
        let mut iter = self.parent_nodes();
        while let Some(n) = iter.next() {
            if is_same_node(node, n) {
                return iter.next().expect("Node is root and has no parent");
            }
        }
        panic!(
            "Node {:?} has either been popped off of the stack or not yet visited. Cannot find parent.",
            node.describe()
        );
    }

    // Skip the children of the last visited node, which must be passed
    // as a sanity check. This node must be the last visited node on the stack.
    // For convenience, also remove the (visited) node itself.
    pub fn skip_children(&mut self, node: &dyn Node) {
        for idx in (0..self.stack.len()).rev() {
            if let TraversalEntry::Visited(n) = self.stack[idx] {
                assert!(
                    is_same_node(node, n),
                    "Passed node is not the last visited node"
                );
                self.stack.truncate(idx);
                return;
            }
        }
        panic!("Passed node is not on the stack");
    }
}

impl<'a> Iterator for Traversal<'a> {
    type Item = &'a dyn Node;
    // Return the next node.
    fn next(&mut self) -> Option<&'a dyn Node> {
        let node = loop {
            match self.stack.pop()? {
                TraversalEntry::NeedsVisit(n) => {
                    // Leave a marker for the node we just visited.
                    self.stack.push(TraversalEntry::Visited(n));
                    break n;
                }
                TraversalEntry::Visited(_) => {}
            }
        };
        // Append this node's children to our stack, and then reverse the order of the children
        // so that the last item on the stack is the first child.
        let before = self.stack.len();
        node.accept(self);
        self.stack[before..].reverse();
        Some(node)
    }
}

impl<'a, 'v: 'a> NodeVisitor<'v> for Traversal<'a> {
    // Record that a child of a node needs to be visited.
    fn visit(&mut self, node: &'a dyn Node) {
        self.stack.push(TraversalEntry::NeedsVisit(node));
    }
}

pub type SourceRangeList = Vec<SourceRange>;

/// Extra source ranges.
/// These are only generated if the corresponding flags are set.
#[derive(Default)]
pub struct Extras {
    /// Set of comments, sorted by offset.
    pub comments: SourceRangeList,

    /// Set of semicolons, sorted by offset.
    pub semis: SourceRangeList,

    /// Set of error ranges, sorted by offset.
    pub errors: SourceRangeList,
}

/// Parse a job list.
pub fn parse(src: &wstr, flags: ParseTreeFlags, out_errors: Option<&mut ParseErrorList>) -> Ast {
    let mut pops = Populator::new(
        src, flags, false, /* not freestanding_arguments */
        out_errors,
    );
    let mut list = JobList::default();
    pops.populate_list(&mut list, true);
    finalize_parse(pops, list)
}

/// Parse a FreestandingArgumentList.
pub fn parse_argument_list(
    src: &wstr,
    flags: ParseTreeFlags,
    out_errors: Option<&mut ParseErrorList>,
) -> Ast<FreestandingArgumentList> {
    let mut pops = Populator::new(
        src, flags, true, /* freestanding_arguments */
        out_errors,
    );
    let mut list = FreestandingArgumentList::default();
    pops.populate_list(&mut list.arguments, true);
    finalize_parse(pops, list)
}

// Given that we have populated some top node, add all the extras that we want and produce an Ast.
fn finalize_parse<N: Node>(mut pops: Populator<'_>, top: N) -> Ast<N> {
    // Chomp trailing extras, etc.
    pops.chomp_extras(top.kind());

    let any_error = pops.any_error;
    let extras = Extras {
        comments: pops.tokens.comment_ranges,
        semis: pops.semis,
        errors: pops.errors,
    };

    Ast {
        top,
        any_error,
        extras,
    }
}

/// The ast type itself.
pub struct Ast<N: Node = JobList> {
    // The top node.
    top: N,
    /// Whether any errors were encountered during parsing.
    any_error: bool,
    /// Extra fields.
    pub extras: Extras,
}

impl<N: Node> Ast<N> {
    /// Return a traversal, allowing iteration over the nodes.
    pub fn walk(&'_ self) -> Traversal<'_> {
        Traversal::new(&self.top)
    }
    /// Return the top node. This has the type requested in the 'parse' method.
    pub fn top(&self) -> &N {
        &self.top
    }
    /// Return whether any errors were encountered during parsing.
    pub fn errored(&self) -> bool {
        self.any_error
    }

    /// Return a textual representation of the tree.
    /// Pass the original source as `orig`.
    pub fn dump(&self, orig: &wstr) -> WString {
        let mut result = WString::new();

        let mut traversal = self.walk();
        while let Some(node) = traversal.next() {
            let depth = traversal.parent_nodes().count() - 1;
            // dot-| padding
            result += &str::repeat("! ", depth)[..];

            if let Kind::Argument(n) = node.kind() {
                result += "argument";
                if let Some(argsrc) = n.try_source(orig) {
                    sprintf!(=> &mut result, ": '%s'", argsrc);
                }
            } else if let Some(n) = node.as_keyword() {
                sprintf!(=> &mut result, "keyword: %s", n.keyword().to_wstr());
            } else if let Some(n) = node.as_token() {
                let desc = match n.token_type() {
                    ParseTokenType::string => {
                        let mut desc = WString::from_str("string");
                        if let Some(strsource) = n.try_source(orig) {
                            sprintf!(=> &mut desc, ": '%s'", strsource);
                        }
                        desc
                    }
                    ParseTokenType::redirection => {
                        let mut desc = WString::from_str("redirection");
                        if let Some(strsource) = n.try_source(orig) {
                            sprintf!(=> &mut desc, ": '%s'", strsource);
                        }
                        desc
                    }
                    ParseTokenType::end => WString::from_str("<;>"),
                    ParseTokenType::invalid => {
                        // This may occur with errors, e.g. we expected to see a string but saw a
                        // redirection.
                        WString::from_str("<error>")
                    }
                    _ => {
                        token_type_user_presentable_description(n.token_type(), ParseKeyword::None)
                    }
                };
                result += &desc[..];
            } else {
                result += &node.describe()[..];
            }
            result += "\n";
        }
        result
    }
}

struct SourceRangeVisitor {
    /// Total range we have encountered.
    total: SourceRange,
    /// Whether any node was found to be unsourced.
    any_unsourced: bool,
}

impl<'a> NodeVisitor<'a> for SourceRangeVisitor {
    fn visit(&mut self, node: &'a dyn Node) {
        // Recurse for non-leaves.
        let Some(leaf) = node.as_leaf() else {
            node.accept(self);
            return;
        };
        if let Some(range) = leaf.range() {
            if range.length == 0 {
                // pass
            } else if self.total.length == 0 {
                self.total = range;
            } else {
                self.total = self.total.combine(range);
            }
        } else {
            self.any_unsourced = true;
        }
    }
}

/// A token stream generates a sequence of parser tokens, permitting arbitrary lookahead.
struct TokenStream<'a> {
    // We implement a queue with a simple circular buffer.
    // Note that peek() returns an address, so we must not move elements which are peek'd.
    // This prevents using vector (which may reallocate).
    // Deque would work but is too heavyweight for just 2 items.
    lookahead: [ParseToken; TokenStream::MAX_LOOKAHEAD],

    // Starting index in our lookahead.
    // The "first" token is at this index.
    start: usize,

    // Number of items in our lookahead.
    count: usize,

    // A reference to the original source.
    src: &'a wstr,

    // The tokenizer to generate new tokens.
    tok: Tokenizer<'a>,

    /// Any comment nodes are collected here.
    /// These are only collected if parse_flag_include_comments is set.
    comment_ranges: SourceRangeList,
}

impl<'a> TokenStream<'a> {
    // The maximum number of lookahead supported.
    const MAX_LOOKAHEAD: usize = 2;

    fn new(src: &'a wstr, flags: ParseTreeFlags, freestanding_arguments: bool) -> Self {
        let mut flags = TokFlags::from(flags);
        if freestanding_arguments {
            flags |= TOK_ARGUMENT_LIST;
        }
        Self {
            lookahead: [ParseToken::new(ParseTokenType::invalid); Self::MAX_LOOKAHEAD],
            start: 0,
            count: 0,
            src,
            tok: Tokenizer::new(src, flags),
            comment_ranges: vec![],
        }
    }

    /// Return the token at the given index, without popping it. If the token stream is exhausted,
    /// it will have parse_token_type_t::terminate. idx = 0 means the next token, idx = 1 means the
    /// next-next token, and so forth.
    /// We must have that idx < kMaxLookahead.
    fn peek(&mut self, idx: usize) -> &ParseToken {
        assert!(idx < Self::MAX_LOOKAHEAD, "Trying to look too far ahead");
        while idx >= self.count {
            self.lookahead[Self::mask(self.start + self.count)] = self.next_from_tok();
            self.count += 1
        }
        &self.lookahead[Self::mask(self.start + idx)]
    }

    /// Pop the next token.
    fn pop(&mut self) -> ParseToken {
        if self.count == 0 {
            return self.next_from_tok();
        }
        let result = self.lookahead[self.start];
        self.start = Self::mask(self.start + 1);
        self.count -= 1;
        result
    }

    // Helper to mask our circular buffer.
    fn mask(idx: usize) -> usize {
        idx % Self::MAX_LOOKAHEAD
    }

    /// Return the next parse token from the tokenizer.
    /// This consumes and stores comments.
    fn next_from_tok(&mut self) -> ParseToken {
        loop {
            let res = self.advance_1();
            if res.typ == ParseTokenType::comment {
                self.comment_ranges.push(res.range());
                continue;
            }
            return res;
        }
    }

    /// Return a new parse token, advancing the tokenizer.
    /// This returns comments.
    fn advance_1(&mut self) -> ParseToken {
        let Some(token) = self.tok.next() else {
            return ParseToken::new(ParseTokenType::terminate);
        };
        // Set the type, keyword, and whether there's a dash prefix. Note that this is quite
        // sketchy, because it ignores quotes. This is the historical behavior. For example,
        // `builtin --names` lists builtins, but `builtin "--names"` attempts to run --names as a
        // command. Amazingly as of this writing (10/12/13) nobody seems to have noticed this.
        // Squint at it really hard and it even starts to look like a feature.
        let mut result = ParseToken::new(ParseTokenType::from(token.type_));
        let text = self.tok.text_of(&token);
        result.keyword = keyword_for_token(token.type_, text);
        result.has_dash_prefix = text.starts_with('-');
        result.is_help_argument = [L!("-h"), L!("--help")].contains(&text);
        result.is_newline = result.typ == ParseTokenType::end && text == "\n";
        result.may_be_variable_assignment = variable_assignment_equals_pos(text).is_some();
        result.tok_error = token.error;

        assert!(token.offset() < SOURCE_OFFSET_INVALID);
        result.set_source_start(token.offset());
        result.set_source_length(token.length());

        if token.error != TokenizerError::none {
            let subtoken_offset = token.error_offset_within_token();
            // Skip invalid tokens that have a zero length, especially if they are at EOF.
            if subtoken_offset < result.source_length() {
                result.set_source_start(result.source_start() + subtoken_offset);
                result.set_source_length(token.error_length());
            }
        }

        result
    }
}

/// This indicates a bug in fish code.
macro_rules! internal_error {
    (
        $self:ident,
        $func:ident,
        $fmt:expr
        $(, $args:expr)*
        $(,)?
    ) => {
        FLOG!(
            debug,
            concat!(
                "Internal parse error from {$func} - this indicates a bug in fish.",
                $fmt,
            )
            $(, $args)*
        );
        FLOGF!(debug, "Encountered while parsing:<<<<\n%s\n>>>", $self.tokens.src);
        panic!();
    };
}

/// Report an error based on `fmt` for the tokens' range
macro_rules! parse_error {
    (
        $self:ident,
        $token:expr,
        $code:expr,
        $fmt:expr
        $(, $args:expr)*
        $(,)?
    ) => {
        let range = $token.range();
        parse_error_range!($self, range, $code, $fmt $(, $args)*);
    }
}

/// Report an error based on `fmt` for the source range `range`.
macro_rules! parse_error_range {
    (
        $self:ident,
        $range:expr,
        $code:expr,
        $fmt:expr
        $(, $args:expr)*
        $(,)?
    ) => {
        let text = if $self.out_errors.is_some() && !$self.unwinding {
            Some(wgettext_fmt!($fmt $(, $args)*))
        } else {
            None
        };
        $self.any_error = true;

        // Ignore additional parse errors while unwinding.
        // These may come about e.g. from `true | and`.
        if !$self.unwinding {
            $self.unwinding = true;

            FLOGF!(ast_construction, "%*sparse error - begin unwinding", $self.spaces(), "");
            // TODO: can store this conditionally dependent on flags.
            if $range.start() != SOURCE_OFFSET_INVALID {
                $self.errors.push($range);
            }

            if let Some(errors) = &mut $self.out_errors {
                let mut err = ParseError::default();
                err.text = text.unwrap();
                err.code = $code;
                err.source_start = $range.start();
                err.source_length = $range.length();
                errors.push(err);
            }
        }
    }
}

struct Populator<'a> {
    /// Flags controlling parsing.
    flags: ParseTreeFlags,

    /// Set of semicolons, sorted by offset.
    semis: SourceRangeList,

    /// Set of error ranges, sorted by offset.
    errors: SourceRangeList,

    /// Stream of tokens which we consume.
    tokens: TokenStream<'a>,

    /// If set, marks that we are parsing a freestanding argument list, e.g.
    /// as used in `complete --arguments`.
    /// This affects whether argument lists can have semicolons.
    freestanding_arguments: bool,

    /// If set, we are unwinding due to error recovery.
    unwinding: bool,

    /// If set, we have encountered an error.
    any_error: bool,

    /// The number of parent links of the node we are visiting
    depth: usize,

    // If non-null, populate with errors.
    out_errors: Option<&'a mut ParseErrorList>,
}

impl<'s> NodeVisitorMut for Populator<'s> {
    fn visit_mut<N: NodeMut>(&mut self, node: &mut N) -> VisitResult {
        use KindMut as KM;
        match node.kind_mut() {
            // Leaves
            KM::Argument(node) => self.visit_argument(node),
            KM::VariableAssignment(node) => self.visit_variable_assignment(node),
            KM::JobContinuation(node) => self.visit_job_continuation(node),
            KM::Token(node) => self.visit_token(node),
            KM::Keyword(node) => return self.visit_keyword(node),

            KM::MaybeNewlines(node) => self.visit_maybe_newlines(node),

            // Branches
            KM::ArgumentOrRedirection(node) => self.visit_argument_or_redirection(node),
            KM::BlockStatementHeader(node) => self.visit_block_statement_header(node),
            KM::Statement(node) => self.visit_statement(node),
            KM::Redirection(node) => node.accept_mut(self),
            KM::JobPipeline(node) => node.accept_mut(self),
            KM::JobConjunction(node) => node.accept_mut(self),
            KM::ForHeader(node) => node.accept_mut(self),
            KM::WhileHeader(node) => node.accept_mut(self),
            KM::FunctionHeader(node) => node.accept_mut(self),
            KM::BeginHeader(node) => node.accept_mut(self),
            KM::BlockStatement(node) => node.accept_mut(self),
            KM::BraceStatement(node) => node.accept_mut(self),
            KM::IfClause(node) => node.accept_mut(self),
            KM::ElseifClause(node) => node.accept_mut(self),
            KM::ElseClause(node) => node.accept_mut(self),
            KM::IfStatement(node) => node.accept_mut(self),
            KM::CaseItem(node) => node.accept_mut(self),
            KM::SwitchStatement(node) => node.accept_mut(self),
            KM::DecoratedStatement(node) => node.accept_mut(self),
            KM::NotStatement(node) => node.accept_mut(self),
            KM::JobConjunctionContinuation(node) => node.accept_mut(self),
            KM::AndorJob(node) => node.accept_mut(self),

            // Lists
            KM::VariableAssignmentList(node) => self.populate_list(node, false),
            KM::ArgumentOrRedirectionList(node) => self.populate_list(node, false),
            KM::ElseifClauseList(node) => self.populate_list(node, false),
            KM::JobContinuationList(node) => self.populate_list(node, false),
            KM::AndorJobList(node) => self.populate_list(node, false),
            KM::JobConjunctionContinuationList(node) => self.populate_list(node, false),
            KM::CaseItemList(node) => self.populate_list(node, false),
            KM::ArgumentList(node) => self.populate_list(node, false),
            KM::JobList(node) => self.populate_list(node, false),

            // Weird top-level case, not actually a list.
            KM::FreestandingArgumentList(node) => node.accept_mut(self),
        }
        VisitResult::Continue(())
    }

    fn will_visit_fields_of<N: NodeMut>(&mut self, node: &mut N) {
        FLOGF!(
            ast_construction,
            "%*swill_visit %s",
            self.spaces(),
            "",
            node.describe()
        );
        self.depth += 1
    }

    fn did_visit_fields_of<'a, N: NodeMut>(&'a mut self, node: &'a mut N, flow: VisitResult) {
        self.depth -= 1;

        if self.unwinding {
            return;
        }
        let VisitResult::Break(error) = flow else {
            return;
        };

        let token = &error.token;
        // To-do: maybe extend this to other tokenizer errors?
        if token.typ == ParseTokenType::tokenizer_error
            && token.tok_error == TokenizerError::closing_unopened_brace
        {
            parse_error_range!(
                self,
                token.range(),
                ParseErrorCode::unbalancing_brace,
                "%s",
                <TokenizerError as Into<&wstr>>::into(token.tok_error)
            );
        }

        // We believe the node is some sort of block statement. Attempt to find a source range
        // for the block's keyword (for, if, etc) and a user-presentable description. This
        // is used to provide better error messages. Note at this point the parse tree is
        // incomplete.
        let mut cursor = node.as_node();
        let header = loop {
            match cursor.kind() {
                Kind::BlockStatement(node) => cursor = &node.header,
                Kind::BlockStatementHeader(node) => cursor = node.embedded_node(),
                Kind::ForHeader(node) => {
                    break Some((node.kw_for.range.unwrap(), L!("for loop")));
                }
                Kind::WhileHeader(node) => {
                    break Some((node.kw_while.range.unwrap(), L!("while loop")));
                }
                Kind::FunctionHeader(node) => {
                    break Some((node.kw_function.range.unwrap(), L!("function definition")));
                }
                Kind::BeginHeader(node) => {
                    break Some((node.kw_begin.range.unwrap(), L!("begin")));
                }
                Kind::IfStatement(node) => {
                    break Some((node.if_clause.kw_if.range.unwrap(), L!("if statement")));
                }
                Kind::SwitchStatement(node) => {
                    break Some((node.kw_switch.range.unwrap(), L!("switch statement")));
                }
                _ => break None,
            }
        };

        if let Some((header_kw_range, enclosing_stmt)) = header {
            let next_token = self.peek_token(0);
            if next_token.typ == ParseTokenType::string
                && matches!(
                    next_token.keyword,
                    ParseKeyword::Case | ParseKeyword::Else | ParseKeyword::End
                )
            {
                self.consume_excess_token_generating_error();
            }
            parse_error_range!(
                self,
                header_kw_range,
                ParseErrorCode::generic,
                "Missing end to balance this %s",
                enclosing_stmt
            );
        } else {
            parse_error!(
                self,
                token,
                ParseErrorCode::generic,
                "Expected %s, but found %s",
                keywords_user_presentable_description(error.allowed_keywords),
                error.token.user_presentable_description(),
            );
        }
    }

    fn visit_optional_mut<N: NodeMut + CheckParse>(&mut self, node: &mut Option<N>) -> VisitResult {
        *node = self.try_parse::<N>();
        VisitResult::Continue(())
    }
}

/// Helper to describe a list of keywords.
/// TODO: these need to be localized properly.
fn keywords_user_presentable_description(kws: &'static [ParseKeyword]) -> WString {
    assert!(!kws.is_empty(), "Should not be empty list");
    if kws.len() == 1 {
        return sprintf!("keyword '%s'", kws[0]);
    }
    let mut res = L!("keywords ").to_owned();
    for (i, kw) in kws.iter().enumerate() {
        if i != 0 {
            res += L!(" or ");
        }
        res += &sprintf!("'%s'", *kw)[..];
    }
    res
}

/// Helper to describe a list of token types.
/// TODO: these need to be localized properly.
fn token_types_user_presentable_description(types: &'static [ParseTokenType]) -> WString {
    assert!(!types.is_empty(), "Should not be empty list");
    let mut res = WString::new();
    for typ in types {
        if !res.is_empty() {
            res += L!(" or ");
        }
        res += &token_type_user_presentable_description(*typ, ParseKeyword::None)[..];
    }
    res
}

impl<'s> Populator<'s> {
    /// Construct a new Populator, parsing source with the given flags.
    /// If freestanding_arguments is true, then we are parsing a freestanding argument list
    /// as used in `complete --arguments`; this affects whether argument lists can have semicolons.
    fn new(
        src: &'s wstr,
        flags: ParseTreeFlags,
        freestanding_arguments: bool,
        out_errors: Option<&'s mut ParseErrorList>,
    ) -> Self {
        Self {
            flags,
            semis: vec![],
            errors: vec![],
            tokens: TokenStream::new(src, flags, freestanding_arguments),
            freestanding_arguments,
            unwinding: false,
            any_error: false,
            depth: 0,
            out_errors,
        }
    }

    /// Helper for FLOGF. This returns a number of spaces appropriate for a '%*c' format.
    fn spaces(&self) -> usize {
        self.depth * 2
    }

    /// Return the parser's status.
    fn status(&mut self) -> ParserStatus {
        if self.unwinding {
            ParserStatus::unwinding
        } else if self.flags.contains(ParseTreeFlags::LEAVE_UNTERMINATED)
            && self.peek_type(0) == ParseTokenType::terminate
        {
            ParserStatus::unsourcing
        } else {
            ParserStatus::ok
        }
    }

    /// Return whether any leaf nodes we visit should be marked as unsourced.
    fn unsource_leaves(&mut self) -> bool {
        matches!(
            self.status(),
            ParserStatus::unsourcing | ParserStatus::unwinding
        )
    }

    /// Return whether we permit an incomplete parse tree.
    fn allow_incomplete(&self) -> bool {
        self.flags.contains(ParseTreeFlags::LEAVE_UNTERMINATED)
    }

    /// Return whether a list kind allows arbitrary newlines in it.
    fn list_kind_chomps_newlines(&self, kind: Kind) -> bool {
        match kind {
            Kind::ArgumentList(_) | Kind::FreestandingArgumentList(_) => {
                self.freestanding_arguments
            }

            Kind::ArgumentOrRedirectionList(_) => {
                // No newlines inside arguments.
                false
            }
            Kind::VariableAssignmentList(_) => {
                // No newlines inside variable assignment lists.
                false
            }
            Kind::JobList(_) => {
                // Like echo a \n \n echo b
                true
            }
            Kind::CaseItemList(_) => {
                // Like switch foo \n \n \n case a \n end
                true
            }
            Kind::AndorJobList(_) => {
                // Like while true ; \n \n and true ; end
                true
            }
            Kind::ElseifClauseList(_) => {
                // Like if true ; \n \n else if false; end
                true
            }
            Kind::JobConjunctionContinuationList(_) => {
                // This would be like echo a && echo b \n && echo c
                // We could conceivably support this but do not now.
                false
            }
            Kind::JobContinuationList(_) => {
                // This would be like echo a \n | echo b
                // We could conceivably support this but do not now.
                false
            }
            _ => {
                internal_error!(
                    self,
                    list_kind_chomps_newlines,
                    "Type %s not handled",
                    ast_kind_to_string(kind)
                );
            }
        }
    }

    /// Return whether a list kind allows arbitrary semicolons in it.
    fn list_kind_chomps_semis(&self, kind: Kind) -> bool {
        match kind {
            Kind::ArgumentList(_) | Kind::FreestandingArgumentList(_) => {
                // Hackish. If we are producing a freestanding argument list, then it allows
                // semicolons, for hysterical raisins.
                // That is, this is OK: complete -c foo -a 'x ; y ; z'
                // But this is not: foo x ; y ; z
                self.freestanding_arguments
            }

            Kind::ArgumentOrRedirectionList(_) | Kind::VariableAssignmentList(_) => false,
            Kind::JobList(_) => {
                // Like echo a ; ;  echo b
                true
            }
            Kind::CaseItemList(_) => {
                // Like switch foo ; ; ;  case a \n end
                // This is historically allowed.
                true
            }
            Kind::AndorJobList(_) => {
                // Like while true ; ; ;  and true ; end
                true
            }
            Kind::ElseifClauseList(_) => {
                // Like if true ; ; ;  else if false; end
                false
            }
            Kind::JobConjunctionContinuationList(_) => {
                // Like echo a ; ; && echo b. Not supported.
                false
            }
            Kind::JobContinuationList(_) => {
                // This would be like echo a ; | echo b
                // Not supported.
                // We could conceivably support this but do not now.
                false
            }
            _ => {
                internal_error!(
                    self,
                    list_kind_chomps_semis,
                    "Type %s not handled",
                    ast_kind_to_string(kind)
                );
            }
        }
    }

    /// Chomp extra comments, semicolons, etc. for a given list kind.
    fn chomp_extras(&mut self, kind: Kind) {
        let chomp_semis = self.list_kind_chomps_semis(kind);
        let chomp_newlines = self.list_kind_chomps_newlines(kind);
        loop {
            let peek = self.tokens.peek(0);
            if chomp_newlines && peek.typ == ParseTokenType::end && peek.is_newline {
                // Just skip this newline, no need to save it.
                self.tokens.pop();
            } else if chomp_semis && peek.typ == ParseTokenType::end && !peek.is_newline {
                let tok = self.tokens.pop();
                // Perhaps save this extra semi.
                if self.flags.contains(ParseTreeFlags::SHOW_EXTRA_SEMIS) {
                    self.semis.push(tok.range());
                }
            } else {
                break;
            }
        }
    }

    /// Return whether a list kind should recover from errors.
    /// That is, whether we should stop unwinding when we encounter this type.
    fn list_kind_stops_unwind(&self, kind: Kind) -> bool {
        matches!(kind, Kind::JobList(_))
            && self.flags.contains(ParseTreeFlags::CONTINUE_AFTER_ERROR)
    }

    /// Return a reference to a non-comment token at index `idx`.
    fn peek_token(&mut self, idx: usize) -> &ParseToken {
        self.tokens.peek(idx)
    }

    /// Return the type of a non-comment token.
    fn peek_type(&mut self, idx: usize) -> ParseTokenType {
        self.peek_token(idx).typ
    }

    /// Consume the next token, chomping any comments.
    /// It is an error to call this unless we know there is a non-terminate token available.
    /// Return the token.
    fn consume_any_token(&mut self) -> ParseToken {
        let tok = self.tokens.pop();
        assert!(
            tok.typ != ParseTokenType::comment,
            "Should not be a comment"
        );
        assert!(
            tok.typ != ParseTokenType::terminate,
            "Cannot consume terminate token, caller should check status first"
        );
        tok
    }

    /// Consume the next token which is expected to be of the given type.
    fn consume_token_type(&mut self, typ: ParseTokenType) -> SourceRange {
        assert!(
            typ != ParseTokenType::terminate,
            "Should not attempt to consume terminate token"
        );
        let tok = self.consume_any_token();
        if tok.typ != typ {
            parse_error!(
                self,
                tok,
                ParseErrorCode::generic,
                "Expected %s, but found %s",
                token_type_user_presentable_description(typ, ParseKeyword::None),
                tok.user_presentable_description()
            );
            return SourceRange::new(0, 0);
        }
        tok.range()
    }

    /// The next token could not be parsed at the top level.
    /// For example a trailing end like `begin ; end ; end`
    /// Or an unexpected redirection like `>`
    /// Consume it and add an error.
    fn consume_excess_token_generating_error(&mut self) {
        let tok = self.consume_any_token();

        // In the rare case that we are parsing a freestanding argument list and not a job list,
        // generate a generic error.
        // TODO: this is a crummy message if we get a tokenizer error, for example:
        //   complete -c foo -a "'abc"
        if self.freestanding_arguments {
            parse_error!(
                self,
                tok,
                ParseErrorCode::generic,
                "Expected %s, but found %s",
                token_type_user_presentable_description(ParseTokenType::string, ParseKeyword::None),
                tok.user_presentable_description()
            );
            return;
        }

        match tok.typ {
            ParseTokenType::string => {
                // There are three keywords which end a job list.
                match tok.keyword {
                    ParseKeyword::Case => {
                        parse_error!(
                            self,
                            tok,
                            ParseErrorCode::unbalancing_case,
                            "'case' builtin not inside of switch block"
                        );
                    }
                    ParseKeyword::End => {
                        parse_error!(
                            self,
                            tok,
                            ParseErrorCode::unbalancing_end,
                            "'end' outside of a block"
                        );
                    }
                    ParseKeyword::Else => {
                        parse_error!(
                            self,
                            tok,
                            ParseErrorCode::unbalancing_else,
                            "'else' builtin not inside of if block"
                        );
                    }
                    _ => {
                        internal_error!(
                            self,
                            consume_excess_token_generating_error,
                            "Token %s should not have prevented parsing a job list",
                            tok.user_presentable_description()
                        );
                    }
                }
            }
            ParseTokenType::redirection if self.peek_type(0) == ParseTokenType::string => {
                let next = self.tokens.pop();
                parse_error_range!(
                    self,
                    next.range().combine(tok.range()),
                    ParseErrorCode::generic,
                    "Expected a string, but found a redirection"
                );
            }
            ParseTokenType::pipe
            | ParseTokenType::redirection
            | ParseTokenType::right_brace
            | ParseTokenType::background
            | ParseTokenType::andand
            | ParseTokenType::oror => {
                parse_error!(
                    self,
                    tok,
                    ParseErrorCode::generic,
                    "Expected a string, but found %s",
                    tok.user_presentable_description()
                );
            }
            ParseTokenType::tokenizer_error => {
                parse_error!(
                    self,
                    tok,
                    ParseErrorCode::from(tok.tok_error),
                    "%s",
                    tok.tok_error
                );
            }
            ParseTokenType::end => {
                internal_error!(
                    self,
                    consume_excess_token_generating_error,
                    "End token should never be excess"
                );
            }
            ParseTokenType::terminate => {
                internal_error!(
                    self,
                    consume_excess_token_generating_error,
                    "Terminate token should never be excess"
                );
            }
            _ => {
                internal_error!(
                    self,
                    consume_excess_token_generating_error,
                    "Unexpected excess token type: %s",
                    tok.user_presentable_description()
                );
            }
        }
    }

    /// Given that we are a list of type ListNodeType, whose contents type is ContentsNode,
    /// populate as many elements as we can.
    /// If exhaust_stream is set, then keep going until we get parse_token_type_t::terminate.
    fn populate_list<Contents, List>(&mut self, list: &mut List, exhaust_stream: bool)
    where
        Contents: NodeMut + CheckParse + Default,
        List: Node + Deref<Target = Box<[Contents]>> + AsMut<Box<[Contents]>>,
    {
        assert!(list.is_empty(), "List is not initially empty");

        // Do not attempt to parse a list if we are unwinding.
        if self.unwinding {
            assert!(
                !exhaust_stream,
                "exhaust_stream should only be set at top level, and so we should not be unwinding"
            );
            // Mark in the list that it was unwound.
            FLOGF!(
                ast_construction,
                "%*sunwinding %s",
                self.spaces(),
                "",
                ast_kind_to_string(list.kind())
            );
            assert!(list.is_empty(), "Should be an empty list");
            return;
        }

        // We're going to populate a vector with our nodes.
        let mut contents = vec![];

        loop {
            // If we are unwinding, then either we recover or we break the loop, dependent on the
            // loop type.
            if self.unwinding {
                if !self.list_kind_stops_unwind(list.kind()) {
                    break;
                }
                // We are going to stop unwinding.
                // Rather hackish. Just chomp until we get to a string or end node.
                loop {
                    let typ = self.peek_type(0);
                    if matches!(
                        typ,
                        ParseTokenType::string | ParseTokenType::terminate | ParseTokenType::end
                    ) {
                        break;
                    }
                    let tok = self.tokens.pop();
                    self.errors.push(tok.range());
                    FLOGF!(
                        ast_construction,
                        "%*schomping range %u-%u",
                        self.spaces(),
                        "",
                        tok.source_start(),
                        tok.source_length()
                    );
                }
                FLOGF!(ast_construction, "%*sdone unwinding", self.spaces(), "");
                self.unwinding = false;
            }

            // Chomp semis and newlines.
            self.chomp_extras(list.kind());

            // Now try parsing a node.
            if let Some(node) = self.try_parse::<Contents>() {
                // #7201: Minimize reallocations of contents vector
                // Empirically, 99.97% of cases are 16 elements or fewer,
                // with 75% being empty, so this works out best.
                if contents.is_empty() {
                    contents.reserve(16);
                }
                contents.push(node);
            } else if exhaust_stream && self.peek_type(0) != ParseTokenType::terminate {
                // We aren't allowed to stop. Produce an error and keep going.
                self.consume_excess_token_generating_error()
            } else {
                // We either stop once we can't parse any more of this contents node, or we
                // exhausted the stream as requested.
                break;
            }
        }

        // Populate our list from our contents.
        if !contents.is_empty() {
            assert!(
                contents.len() <= u32::MAX.try_into().unwrap(),
                "Contents size out of bounds"
            );
            assert!(list.is_empty(), "List should still be empty");
            *list.as_mut() = contents.into_boxed_slice();
        }

        FLOGF!(
            ast_construction,
            "%*s%s size: %u",
            self.spaces(),
            "",
            ast_kind_to_string(list.kind()),
            list.len()
        );
    }

    /// Allocate and populate a statement.
    fn allocate_populate_statement(&mut self) -> Statement {
        // In case we get a parse error, we still need to return something non-null. Use a
        // decorated statement; all of its leaf nodes will end up unsourced.
        fn got_error(slf: &mut Populator<'_>) -> Statement {
            assert!(slf.unwinding, "Should have produced an error");
            new_decorated_statement(slf)
        }

        fn new_decorated_statement(slf: &mut Populator<'_>) -> Statement {
            let embedded = slf.allocate_visit::<DecoratedStatement>();
            if !slf.unwinding && slf.peek_token(0).typ == ParseTokenType::left_brace {
                parse_error!(
                    slf,
                    slf.peek_token(0),
                    ParseErrorCode::generic,
                    "Expected %s, but found %s",
                    token_type_user_presentable_description(
                        ParseTokenType::end,
                        ParseKeyword::None
                    ),
                    slf.peek_token(0).user_presentable_description()
                );
            }
            Statement::Decorated(embedded)
        }

        if self.peek_token(0).typ == ParseTokenType::terminate && self.allow_incomplete() {
            // This may happen if we just have a 'time' prefix.
            // Construct a decorated statement, which will be unsourced.
            self.allocate_visit::<DecoratedStatement>();
        } else if self.peek_token(0).typ == ParseTokenType::left_brace {
            let embedded = self.allocate_boxed_visit::<BraceStatement>();
            return Statement::Brace(embedded);
        } else if self.peek_token(0).typ != ParseTokenType::string {
            // We may be unwinding already; do not produce another error.
            // For example in `true | and`.
            parse_error!(
                self,
                self.peek_token(0),
                ParseErrorCode::generic,
                "Expected a command, but found %s",
                self.peek_token(0).user_presentable_description()
            );
            return got_error(self);
        } else if self.peek_token(0).may_be_variable_assignment {
            // Here we have a variable assignment which we chose to not parse as a variable
            // assignment because there was no string after it.
            // Ensure we consume the token, so we don't get back here again at the same place.
            let token = &self.consume_any_token();
            let text = &self.tokens.src
                [token.source_start()..token.source_start() + token.source_length()];
            let equals_pos = variable_assignment_equals_pos(text).unwrap();
            let variable = &text[..equals_pos];
            let value = &text[equals_pos + 1..];
            parse_error!(
                self,
                token,
                ParseErrorCode::bare_variable_assignment,
                ERROR_BAD_COMMAND_ASSIGN_ERR_MSG,
                variable,
                value
            );
            return got_error(self);
        }

        // In some cases a block starter is a decorated statement instead, mostly if invoked with "--help".
        // The logic here is subtle:
        //
        // If we are 'begin', it's only really a block if it has no arguments.
        // If we are 'function' or another block starter, then we are a non-block if we are invoked with -h or --help
        // If we are anything else, we require an argument, so do the same thing if the subsequent
        // token is a statement terminator.
        if self.peek_token(0).typ == ParseTokenType::string {
            // If we are one of these, then look for specifically help arguments. Otherwise, if the next token
            // looks like an option (starts with a dash), then parse it as a decorated statement.
            let help_only_kws = [
                ParseKeyword::Begin,
                ParseKeyword::Function,
                ParseKeyword::If,
                ParseKeyword::Switch,
                ParseKeyword::While,
            ];
            if if help_only_kws.contains(&self.peek_token(0).keyword) {
                self.peek_token(1).is_help_argument
            } else {
                self.peek_token(1).is_dash_prefix_string()
            } {
                return new_decorated_statement(self);
            }

            // Likewise if the next token doesn't look like an argument at all. This corresponds to
            // e.g. a "naked if".
            let naked_invocation_invokes_help =
                ![ParseKeyword::Begin, ParseKeyword::End].contains(&self.peek_token(0).keyword);
            if naked_invocation_invokes_help && self.peek_token(1).typ == ParseTokenType::terminate
            {
                return new_decorated_statement(self);
            }
        }

        match self.peek_token(0).keyword {
            ParseKeyword::Not | ParseKeyword::Exclam => {
                let embedded = self.allocate_boxed_visit::<NotStatement>();
                Statement::Not(embedded)
            }
            ParseKeyword::For
            | ParseKeyword::While
            | ParseKeyword::Function
            | ParseKeyword::Begin => {
                let embedded = self.allocate_boxed_visit::<BlockStatement>();
                Statement::Block(embedded)
            }
            ParseKeyword::If => {
                let embedded = self.allocate_boxed_visit::<IfStatement>();
                Statement::If(embedded)
            }
            ParseKeyword::Switch => {
                let embedded = self.allocate_boxed_visit::<SwitchStatement>();
                Statement::Switch(embedded)
            }
            ParseKeyword::End => {
                // 'end' is forbidden as a command.
                // For example, `if end` or `while end` will produce this error.
                // We still have to descend into the decorated statement because
                // we can't leave our pointer as null.
                parse_error!(
                    self,
                    self.peek_token(0),
                    ParseErrorCode::generic,
                    "Expected a command, but found %s",
                    self.peek_token(0).user_presentable_description()
                );
                return got_error(self);
            }
            _ => new_decorated_statement(self),
        }
    }

    /// Allocate and populate a block statement header.
    /// This must never return null.
    fn allocate_populate_block_header(&mut self) -> BlockStatementHeader {
        match self.peek_token(0).keyword {
            ParseKeyword::For => {
                let embedded = self.allocate_visit::<ForHeader>();
                BlockStatementHeader::For(embedded)
            }
            ParseKeyword::While => {
                let embedded = self.allocate_visit::<WhileHeader>();
                BlockStatementHeader::While(embedded)
            }
            ParseKeyword::Function => {
                let embedded = self.allocate_visit::<FunctionHeader>();
                BlockStatementHeader::Function(embedded)
            }
            ParseKeyword::Begin => {
                let embedded = self.allocate_visit::<BeginHeader>();
                BlockStatementHeader::Begin(embedded)
            }
            _ => {
                internal_error!(
                    self,
                    allocate_populate_block_header,
                    "should not have descended into block_header"
                );
            }
        }
    }

    fn try_parse<T: NodeMut + Default + CheckParse>(&mut self) -> Option<T> {
        if !T::can_be_parsed(self) {
            return None;
        }
        Some(self.allocate_visit())
    }

    // Given a node type, allocate it, invoking its default constructor,
    // and then visit it as a field.
    // Return the resulting Node.
    fn allocate_visit<T: NodeMut + Default>(&mut self) -> T {
        let mut result = T::default();
        let _ = self.visit_mut(&mut result);
        result
    }

    // Like allocate_visit, but returns the value as a Box.
    fn allocate_boxed_visit<T: NodeMut + Default>(&mut self) -> Box<T> {
        let mut result = Box::<T>::default();
        let _ = self.visit_mut(&mut *result);
        result
    }

    fn visit_argument_or_redirection(&mut self, node: &mut ArgumentOrRedirection) {
        if let Some(arg) = self.try_parse::<Argument>() {
            *node = ArgumentOrRedirection::Argument(arg);
        } else if let Some(redir) = self.try_parse::<Redirection>() {
            *node = ArgumentOrRedirection::Redirection(Box::new(redir));
        } else {
            internal_error!(
                self,
                visit_argument_or_redirection,
                "Unable to parse argument or redirection"
            );
        }
    }
    fn visit_block_statement_header(&mut self, node: &mut BlockStatementHeader) {
        *node = self.allocate_populate_block_header();
    }
    fn visit_statement(&mut self, node: &mut Statement) {
        *node = self.allocate_populate_statement();
    }

    fn visit_argument(&mut self, arg: &mut Argument) {
        if self.unsource_leaves() {
            arg.range = None;
            return;
        }
        arg.range = Some(self.consume_token_type(ParseTokenType::string));
    }

    fn visit_variable_assignment(&mut self, varas: &mut VariableAssignment) {
        if self.unsource_leaves() {
            varas.range = None;
            return;
        }
        if !self.peek_token(0).may_be_variable_assignment {
            internal_error!(
                self,
                visit_variable_assignment,
                "Should not have created variable_assignment_t from this token"
            );
        }
        varas.range = Some(self.consume_token_type(ParseTokenType::string));
    }

    fn visit_job_continuation(&mut self, node: &mut JobContinuation) {
        // Special error handling to catch 'and' and 'or' in pipelines, like `true | and false`.
        let kw = self.peek_token(1).keyword;
        if matches!(kw, ParseKeyword::And | ParseKeyword::Or) {
            parse_error!(
                self,
                self.peek_token(1),
                ParseErrorCode::andor_in_pipeline,
                INVALID_PIPELINE_CMD_ERR_MSG,
                kw
            );
        }
        node.accept_mut(self);
    }

    // Overload for token fields.
    fn visit_token(&mut self, token: &mut dyn Token) {
        if self.unsource_leaves() {
            *token.range_mut() = None;
            return;
        }

        if !token.allows_token(self.peek_token(0).typ) {
            if self.flags.contains(ParseTreeFlags::LEAVE_UNTERMINATED)
                && [
                    TokenizerError::unterminated_quote,
                    TokenizerError::unterminated_subshell,
                ]
                .contains(&self.peek_token(0).tok_error)
            {
                return;
            }

            parse_error!(
                self,
                self.peek_token(0),
                ParseErrorCode::generic,
                "Expected %s, but found %s",
                token_types_user_presentable_description(token.allowed_tokens()),
                self.peek_token(0).user_presentable_description()
            );
            *token.range_mut() = None;
            return;
        }
        let tok = self.consume_any_token();
        *token.token_type_mut() = tok.typ;
        *token.range_mut() = Some(tok.range());
    }

    // Overload for keyword fields.
    fn visit_keyword(&mut self, keyword: &mut dyn Keyword) -> VisitResult {
        if self.unsource_leaves() {
            *keyword.range_mut() = None;
            return VisitResult::Continue(());
        }

        if !keyword.allows_keyword(self.peek_token(0).keyword) {
            *keyword.range_mut() = None;

            if self.flags.contains(ParseTreeFlags::LEAVE_UNTERMINATED)
                && [
                    TokenizerError::unterminated_quote,
                    TokenizerError::unterminated_subshell,
                ]
                .contains(&self.peek_token(0).tok_error)
            {
                return VisitResult::Continue(());
            }

            // Special error reporting for keyword_t<kw_end>.
            let allowed_keywords = keyword.allowed_keywords();
            if keyword.allowed_keywords() == [ParseKeyword::End] {
                return VisitResult::Break(MissingEndError {
                    allowed_keywords,
                    token: *self.peek_token(0),
                });
            } else {
                parse_error!(
                    self,
                    self.peek_token(0),
                    ParseErrorCode::generic,
                    "Expected %s, but found %s",
                    keywords_user_presentable_description(allowed_keywords),
                    self.peek_token(0).user_presentable_description(),
                );
                return VisitResult::Continue(());
            }
        }
        let tok = self.consume_any_token();
        *keyword.keyword_mut() = tok.keyword;
        *keyword.range_mut() = Some(tok.range());
        VisitResult::Continue(())
    }

    fn visit_maybe_newlines(&mut self, nls: &mut MaybeNewlines) {
        if self.unsource_leaves() {
            nls.range = None;
            return;
        }
        let mut range = SourceRange::new(0, 0);
        // TODO: it would be nice to have the start offset be the current position in the token
        // stream, even if there are no newlines.
        while self.peek_token(0).is_newline {
            let r = self.consume_token_type(ParseTokenType::end);
            if range.length == 0 {
                range = r;
            } else {
                range.length = r.start + r.length - range.start
            }
        }
        nls.range = Some(range);
    }
}

/// The status of our parser.
enum ParserStatus {
    /// Parsing is going just fine, thanks for asking.
    ok,

    /// We have exhausted the token stream, but the caller was OK with an incomplete parse tree.
    /// All further leaf nodes should have the unsourced flag set.
    unsourcing,

    /// We encountered an parse error and are "unwinding."
    /// Do not consume any tokens until we get back to a list type which stops unwinding.
    unwinding,
}

/// Return tokenizer flags corresponding to parse tree flags.
impl From<ParseTreeFlags> for TokFlags {
    fn from(flags: ParseTreeFlags) -> Self {
        let mut tok_flags = TokFlags(0);
        // Note we do not need to respect parse_flag_show_blank_lines, no clients are interested
        // in them.
        if flags.contains(ParseTreeFlags::INCLUDE_COMMENTS) {
            tok_flags |= TOK_SHOW_COMMENTS;
        }
        if flags.contains(ParseTreeFlags::ACCEPT_INCOMPLETE_TOKENS) {
            tok_flags |= TOK_ACCEPT_UNFINISHED;
        }
        if flags.contains(ParseTreeFlags::CONTINUE_AFTER_ERROR) {
            tok_flags |= TOK_CONTINUE_AFTER_ERROR
        }
        tok_flags
    }
}

/// Convert from Tokenizer's token type to a parse_token_t type.
impl From<TokenType> for ParseTokenType {
    fn from(token_type: TokenType) -> Self {
        match token_type {
            TokenType::string => ParseTokenType::string,
            TokenType::pipe => ParseTokenType::pipe,
            TokenType::andand => ParseTokenType::andand,
            TokenType::oror => ParseTokenType::oror,
            TokenType::end => ParseTokenType::end,
            TokenType::background => ParseTokenType::background,
            TokenType::left_brace => ParseTokenType::left_brace,
            TokenType::right_brace => ParseTokenType::right_brace,
            TokenType::redirect => ParseTokenType::redirection,
            TokenType::error => ParseTokenType::tokenizer_error,
            TokenType::comment => ParseTokenType::comment,
        }
    }
}

fn is_keyword_char(c: char) -> bool {
    ('a'..='z').contains(&c)
        || ('A'..='Z').contains(&c)
        || ('0'..='9').contains(&c)
        || c == '\''
        || c == '"'
        || c == '\\'
        || c == '\n'
        || c == '!'
}

/// Given a token, returns unescaped keyword, or the empty string.
pub(crate) fn unescape_keyword(tok: TokenType, token: &wstr) -> Cow<'_, wstr> {
    /* Only strings can be keywords */
    if tok != TokenType::string {
        return Cow::Borrowed(L!(""));
    }

    // If token is clean (which most are), we can compare it directly. Otherwise we have to expand
    // it. We only expand quotes, and we don't want to do expensive expansions like tilde
    // expansions. So we do our own "cleanliness" check; if we find a character not in our allowed
    // set we know it's not a keyword, and if we never find a quote we don't have to expand! Note
    // that this lowercase set could be shrunk to be just the characters that are in keywords.
    let mut needs_expand = false;
    for c in token.chars() {
        if !is_keyword_char(c) {
            return Cow::Borrowed(L!(""));
        }
        // If we encounter a quote, we need expansion.
        needs_expand = needs_expand || c == '"' || c == '\'' || c == '\\'
    }

    // Expand if necessary.
    if !needs_expand {
        return Cow::Borrowed(token);
    }

    Cow::Owned(unescape_string(token, UnescapeStringStyle::default()).unwrap_or_default())
}

/// Given a token, returns the keyword it matches, or ParseKeyword::None.
fn keyword_for_token(tok: TokenType, token: &wstr) -> ParseKeyword {
    ParseKeyword::from(&unescape_keyword(tok, token)[..])
}

#[test]
#[serial]
fn test_ast_parse() {
    let _cleanup = test_init();
    let src = L!("echo");
    let ast = parse(src, ParseTreeFlags::empty(), None);
    assert!(!ast.any_error);
}
