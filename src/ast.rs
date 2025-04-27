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
use crate::common::{unescape_string, UnescapeStringStyle};
use crate::flog::{FLOG, FLOGF};
use crate::parse_constants::{
    token_type_user_presentable_description, ParseError, ParseErrorCode, ParseErrorList,
    ParseKeyword, ParseTokenType, ParseTreeFlags, SourceRange, StatementDecoration,
    ERROR_BAD_COMMAND_ASSIGN_ERR_MSG, INVALID_PIPELINE_CMD_ERR_MSG, SOURCE_OFFSET_INVALID,
};
use crate::parse_tree::ParseToken;
#[cfg(test)]
use crate::tests::prelude::*;
use crate::tokenizer::{
    variable_assignment_equals_pos, TokFlags, TokenType, Tokenizer, TokenizerError,
    TOK_ACCEPT_UNFINISHED, TOK_ARGUMENT_LIST, TOK_CONTINUE_AFTER_ERROR, TOK_SHOW_COMMENTS,
};
use crate::wchar::prelude::*;
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

pub struct MissingEndError {
    allowed_keywords: &'static [ParseKeyword],
    token: ParseToken,
}

pub type VisitResult = ControlFlow<MissingEndError>;

trait NodeVisitorMut {
    /// will_visit (did_visit) is called before (after) a node's fields are visited.
    fn will_visit_fields_of(&mut self, node: &mut dyn NodeMut);
    fn visit_mut(&mut self, node: &mut dyn NodeMut) -> VisitResult;
    fn did_visit_fields_of<'a>(&'a mut self, node: &'a dyn NodeMut, flow: VisitResult);

    fn visit_argument_or_redirection(&mut self, _node: &mut ArgumentOrRedirection) -> VisitResult;
    fn visit_block_statement_header(
        &mut self,
        _node: &mut BlockStatementHeaderVariant,
    ) -> VisitResult;
    fn visit_statement(&mut self, _node: &mut StatementVariant) -> VisitResult;

    fn visit_decorated_statement_decorator(
        &mut self,
        _node: &mut Option<DecoratedStatementDecorator>,
    );
    fn visit_job_conjunction_decorator(&mut self, _node: &mut Option<JobConjunctionDecorator>);
    fn visit_else_clause(&mut self, _node: &mut Option<ElseClause>);
    fn visit_semi_nl(&mut self, _node: &mut Option<SemiNl>);
    fn visit_time(&mut self, _node: &mut Option<KeywordTime>);
    fn visit_token_background(&mut self, _node: &mut Option<TokenBackground>);
}

trait AcceptorMut {
    fn accept_mut(&mut self, visitor: &mut dyn NodeVisitorMut);
}

impl<T: AcceptorMut> AcceptorMut for Option<T> {
    fn accept_mut(&mut self, visitor: &mut dyn NodeVisitorMut) {
        if let Some(node) = self {
            node.accept_mut(visitor)
        }
    }
}

/// Node is the base trait of all AST nodes.
pub trait Node: Acceptor + ConcreteNode + AsNode + std::fmt::Debug {
    /// The type of this node.
    fn typ(&self) -> Type;

    /// The category of this node.
    fn category(&self) -> Category {
        self.typ().category()
    }

    /// Return a helpful string description of this node.
    fn describe(&self) -> WString {
        let mut res = ast_type_to_string(self.typ()).to_owned();
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
        self.try_source_range().unwrap_or(SourceRange::new(0, 0))
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
    //      their types are equal: a Node cannot have a Node of the same type at the same address
    //      (unless it's a ZST, which does not apply here).
    //
    // Note this is performance-sensitive.

    // Different base pointers => not the same.
    let lptr = lhs as *const dyn Node as *const ();
    let rptr = rhs as *const dyn Node as *const ();
    if !std::ptr::eq(lptr, rptr) {
        return false;
    }

    // Same base pointer and same vtable => same object.
    if std::ptr::eq(lhs, rhs) {
        return true;
    }

    // Same base pointer, but different vtables.
    // Compare the types.
    lhs.typ() == rhs.typ()
}

/// NodeMut is a mutable node.
trait NodeMut: Node + AcceptorMut + ConcreteNode {}
impl<T> NodeMut for T where T: Node + AcceptorMut + ConcreteNode {}

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

pub trait NodeSubTraits {
    // Cast to any sub-trait.
    fn as_leaf(&self) -> Option<&dyn Leaf> {
        None
    }
    fn as_keyword(&self) -> Option<&dyn Keyword> {
        None
    }
    fn as_token(&self) -> Option<&dyn Token> {
        None
    }
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

pub trait ConcreteNode: NodeSubTraits {
    fn kind(&self) -> Kind;
    fn kind_mut(&mut self) -> KindMut;

    // Cast to any node type.
    fn as_redirection(&self) -> Option<&Redirection> {
        match self.kind() {
            Kind::Redirection(node) => Some(node),
            _ => None,
        }
    }

    fn as_variable_assignment(&self) -> Option<&VariableAssignment> {
        match self.kind() {
            Kind::VariableAssignment(node) => Some(node),
            _ => None,
        }
    }
    fn as_variable_assignment_list(&self) -> Option<&VariableAssignmentList> {
        match self.kind() {
            Kind::VariableAssignmentList(node) => Some(node),
            _ => None,
        }
    }
    fn as_argument_or_redirection(&self) -> Option<&ArgumentOrRedirection> {
        match self.kind() {
            Kind::ArgumentOrRedirection(node) => Some(node),
            _ => None,
        }
    }
    fn as_argument_or_redirection_list(&self) -> Option<&ArgumentOrRedirectionList> {
        match self.kind() {
            Kind::ArgumentOrRedirectionList(node) => Some(node),
            _ => None,
        }
    }
    fn as_statement(&self) -> Option<&Statement> {
        match self.kind() {
            Kind::Statement(node) => Some(node),
            _ => None,
        }
    }
    fn as_job_pipeline(&self) -> Option<&JobPipeline> {
        match self.kind() {
            Kind::JobPipeline(node) => Some(node),
            _ => None,
        }
    }
    fn as_job_conjunction(&self) -> Option<&JobConjunction> {
        match self.kind() {
            Kind::JobConjunction(node) => Some(node),
            _ => None,
        }
    }
    fn as_for_header(&self) -> Option<&ForHeader> {
        match self.kind() {
            Kind::ForHeader(node) => Some(node),
            _ => None,
        }
    }
    fn as_while_header(&self) -> Option<&WhileHeader> {
        match self.kind() {
            Kind::WhileHeader(node) => Some(node),
            _ => None,
        }
    }
    fn as_function_header(&self) -> Option<&FunctionHeader> {
        match self.kind() {
            Kind::FunctionHeader(node) => Some(node),
            _ => None,
        }
    }
    fn as_begin_header(&self) -> Option<&BeginHeader> {
        match self.kind() {
            Kind::BeginHeader(node) => Some(node),
            _ => None,
        }
    }
    fn as_block_statement(&self) -> Option<&BlockStatement> {
        match self.kind() {
            Kind::BlockStatement(node) => Some(node),
            _ => None,
        }
    }
    fn as_brace_statement(&self) -> Option<&BraceStatement> {
        match self.kind() {
            Kind::BraceStatement(node) => Some(node),
            _ => None,
        }
    }
    fn as_if_clause(&self) -> Option<&IfClause> {
        match self.kind() {
            Kind::IfClause(node) => Some(node),
            _ => None,
        }
    }
    fn as_elseif_clause(&self) -> Option<&ElseifClause> {
        match self.kind() {
            Kind::ElseifClause(node) => Some(node),
            _ => None,
        }
    }
    fn as_elseif_clause_list(&self) -> Option<&ElseifClauseList> {
        match self.kind() {
            Kind::ElseifClauseList(node) => Some(node),
            _ => None,
        }
    }
    fn as_else_clause(&self) -> Option<&ElseClause> {
        match self.kind() {
            Kind::ElseClause(node) => Some(node),
            _ => None,
        }
    }
    fn as_if_statement(&self) -> Option<&IfStatement> {
        match self.kind() {
            Kind::IfStatement(node) => Some(node),
            _ => None,
        }
    }
    fn as_case_item(&self) -> Option<&CaseItem> {
        match self.kind() {
            Kind::CaseItem(node) => Some(node),
            _ => None,
        }
    }
    fn as_switch_statement(&self) -> Option<&SwitchStatement> {
        match self.kind() {
            Kind::SwitchStatement(node) => Some(node),
            _ => None,
        }
    }
    fn as_decorated_statement(&self) -> Option<&DecoratedStatement> {
        match self.kind() {
            Kind::DecoratedStatement(node) => Some(node),
            _ => None,
        }
    }
    fn as_not_statement(&self) -> Option<&NotStatement> {
        match self.kind() {
            Kind::NotStatement(node) => Some(node),
            _ => None,
        }
    }
    fn as_job_continuation(&self) -> Option<&JobContinuation> {
        match self.kind() {
            Kind::JobContinuation(node) => Some(node),
            _ => None,
        }
    }
    fn as_job_continuation_list(&self) -> Option<&JobContinuationList> {
        match self.kind() {
            Kind::JobContinuationList(node) => Some(node),
            _ => None,
        }
    }
    fn as_job_conjunction_continuation(&self) -> Option<&JobConjunctionContinuation> {
        match self.kind() {
            Kind::JobConjunctionContinuation(node) => Some(node),
            _ => None,
        }
    }
    fn as_andor_job(&self) -> Option<&AndorJob> {
        match self.kind() {
            Kind::AndorJob(node) => Some(node),
            _ => None,
        }
    }
    fn as_andor_job_list(&self) -> Option<&AndorJobList> {
        match self.kind() {
            Kind::AndorJobList(node) => Some(node),
            _ => None,
        }
    }
    fn as_freestanding_argument_list(&self) -> Option<&FreestandingArgumentList> {
        match self.kind() {
            Kind::FreestandingArgumentList(node) => Some(node),
            _ => None,
        }
    }
    fn as_job_conjunction_continuation_list(&self) -> Option<&JobConjunctionContinuationList> {
        match self.kind() {
            Kind::JobConjunctionContinuationList(node) => Some(node),
            _ => None,
        }
    }
    fn as_maybe_newlines(&self) -> Option<&MaybeNewlines> {
        match self.kind() {
            Kind::MaybeNewlines(node) => Some(node),
            _ => None,
        }
    }
    fn as_case_item_list(&self) -> Option<&CaseItemList> {
        match self.kind() {
            Kind::CaseItemList(node) => Some(node),
            _ => None,
        }
    }
    fn as_argument(&self) -> Option<&Argument> {
        match self.kind() {
            Kind::Argument(node) => Some(node),
            _ => None,
        }
    }
    fn as_argument_list(&self) -> Option<&ArgumentList> {
        match self.kind() {
            Kind::ArgumentList(node) => Some(node),
            _ => None,
        }
    }
    fn as_job_list(&self) -> Option<&JobList> {
        match self.kind() {
            Kind::JobList(node) => Some(node),
            _ => None,
        }
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
}

/// A keyword node is a node which contains a keyword, which must be one of a fixed set.
pub trait Keyword: Leaf {
    fn keyword(&self) -> ParseKeyword;
    fn keyword_mut(&mut self) -> &mut ParseKeyword;
    fn allowed_keywords(&self) -> &'static [ParseKeyword];
    fn allows_keyword(&self, kw: ParseKeyword) -> bool {
        self.allowed_keywords().contains(&kw)
    }
}

/// This is for optional values and for lists.
trait CheckParse {
    /// A true return means we should descend into the production, false means stop.
    fn can_be_parsed(pop: &mut Populator<'_>) -> bool;
}

/// Implement the node trait.
macro_rules! implement_node {
    (
        $name:ident,
        $type:ident $(,)?
    ) => {
        impl ConcreteNode for $name {
            fn kind(&self) -> Kind {
                Kind::$name(self)
            }
            fn kind_mut(&mut self) -> KindMut {
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

        impl Node for $name {
            fn typ(&self) -> Type {
                Type::$type
            }
        }
    };
}

/// Implement the leaf trait.
macro_rules! implement_leaf {
    ( $name:ident ) => {
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
            fn accept_mut(&mut self, visitor: &mut dyn NodeVisitorMut) {
                visitor.will_visit_fields_of(self);
                visitor.did_visit_fields_of(self, VisitResult::Continue(()));
            }
        }
    };
}

/// Define a node that implements the keyword trait.
macro_rules! define_keyword_node {
    ( $name:ident, $($allowed:ident),* $(,)? ) => {
        #[derive(Default, Debug)]
        pub struct $name {
            range: Option<SourceRange>,
            keyword: ParseKeyword,
        }

        implement_leaf!($name);
        impl ConcreteNode for $name {
            fn kind(&self) -> Kind {
                Kind::Keyword(self)
            }
            fn kind_mut(&mut self) -> KindMut {
                KindMut::Keyword(self)
            }
        }
        impl NodeSubTraits for $name {
            fn as_leaf(&self) -> Option<&dyn Leaf> {
                Some(self)
            }
            fn as_keyword(&self) -> Option<&dyn Keyword> {
                Some(self)
            }
        }
        impl Node for $name {
            fn typ(&self) -> Type {
                Type::keyword_base
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
        }
    }
}

/// Define a node that implements the token trait.
macro_rules! define_token_node {
    ( $name:ident, $($allowed:ident),* $(,)? ) => {
        #[derive(Default, Debug)]
        pub struct $name {
            range: Option<SourceRange>,
            parse_token_type: ParseTokenType,
        }
        impl ConcreteNode for $name {
            fn kind(&self) -> Kind {
                Kind::Token(self)
            }
            fn kind_mut(&mut self) -> KindMut {
                KindMut::Token(self)
            }
        }
        impl Node for $name {
            fn typ(&self) -> Type {
                Type::token_base
            }
        }
        implement_leaf!($name);
        impl NodeSubTraits for $name {
            fn as_leaf(&self) -> Option<&dyn Leaf> {
                Some(self)
            }
            fn as_token(&self) -> Option<&dyn Token> {
                Some(self)
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
        $type:tt,
        $contents:ident
    ) => {
        #[derive(Default, Debug)]
        pub struct $name(Box<[$contents]>);

        implement_node!($name, $type);

        impl NodeSubTraits for $name {}

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
            fn accept_mut(&mut self, visitor: &mut dyn NodeVisitorMut) {
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
macro_rules! implement_acceptor_for_branch {
    (
        $name:ident
        $(, ($field_name:ident: $field_type:tt) )*
        $(,)?
    ) => {
        impl Acceptor for $name {
            #[allow(unused_variables)]
            fn accept<'a>(&'a self, visitor: &mut dyn NodeVisitor<'a>){
                let _ = visitor_accept_field!(
                    Self,
                    accept,
                    visit,
                    self,
                    visitor,
                    ( $( $field_name: $field_type, )* ) );
            }
        }
        impl AcceptorMut for $name {
            #[allow(unused_variables)]
            fn accept_mut(&mut self, visitor: &mut dyn NodeVisitorMut) {
                visitor.will_visit_fields_of(self);
                let flow = visitor_accept_field!(
                                Self,
                                accept_mut,
                                visit_mut,
                                self,
                                visitor,
                                ( $( $field_name: $field_type, )* ));
                visitor.did_visit_fields_of(self, flow);
            }
        }
    }
}

/// Visit the given fields in order, returning whether the visitation succeeded.
macro_rules! visitor_accept_field {
    (
        $Self:ident,
        $accept:ident,
        $visit:ident,
        $self:ident,
        $visitor:ident,
        $fields:tt
    ) => {
        loop {
            visitor_accept_field_impl!($visit, $self, $visitor, $fields);
            break VisitResult::Continue(());
        }
    };
}

/// Visit the given fields in order, breaking if a visitation fails.
macro_rules! visitor_accept_field_impl {
    // Base case: no fields left to visit.
    (
        $visit:ident,
        $self:ident,
        $visitor:ident,
        ()
    ) => {};
    // Visit the first or last field and then the rest.
    (
        $visit:ident,
        $self:ident,
        $visitor:ident,
        (
            $field_name:ident: $field_type:tt,
            $( $field_names:ident: $field_types:tt, )*
        )
    ) => {
        visit_1_field!($visit, ($self.$field_name), $field_type, $visitor);
        visitor_accept_field_impl!(
            $visit, $self, $visitor,
            ( $( $field_names: $field_types, )* ));
    }
}

/// Visit the given field, breaking on failure.
macro_rules! visit_1_field {
    (
        visit,
        $field:expr,
        $field_type:tt,
        $visitor:ident
    ) => {
        visit_1_field_impl!(visit, $field, $field_type, $visitor);
    };
    (
        visit_mut,
        $field:expr,
        $field_type:tt,
        $visitor:ident
    ) => {
        let result = visit_1_field_impl!(visit_mut, $field, $field_type, $visitor);
        if result.is_break() {
            break result;
        }
    };
}

/// Visit the given field.
macro_rules! visit_1_field_impl {
    (
        $visit:ident,
        $field:expr,
        (variant<$field_type:ident>),
        $visitor:ident
    ) => {
        visit_variant_field!($visit, $field_type, $field, $visitor)
    };
    (
        $visit:ident,
        $field:expr,
        (Option<$field_type:ident>),
        $visitor:ident
    ) => {
        visit_optional_field!($visit, $field_type, $field, $visitor)
    };
    (
        $visit:ident,
        $field:expr,
        $field_type:tt,
        $visitor:ident
    ) => {
        $visitor.$visit(apply_borrow!($visit, $field))
    };
}

macro_rules! apply_borrow {
    ( visit, $expr:expr ) => {
        &$expr
    };
    ( visit_mut, $expr:expr ) => {
        &mut $expr
    };
}

macro_rules! visit_variant_field {
    (
        visit,
        $field_type:ident,
        $field:expr,
        $visitor:ident
    ) => {
        $visitor.visit($field.embedded_node().as_node())
    };
    (
        visit_mut,
        $field_type:ident,
        $field:expr,
        $visitor:ident
    ) => {
        visit_variant_field_mut!($field_type, $visitor, $field)
    };
}

macro_rules! visit_variant_field_mut {
    (BlockStatementHeaderVariant, $visitor:ident, $field:expr) => {
        $visitor.visit_block_statement_header(&mut $field)
    };
    (StatementVariant, $visitor:ident, $field:expr) => {
        $visitor.visit_statement(&mut $field)
    };
}

macro_rules! visit_optional_field {
    (
        visit,
        $field_type:ident,
        $field:expr,
        $visitor:ident
    ) => {
        match &$field {
            Some(value) => $visitor.visit(&*value),
            None => visit_result!(visit),
        }
    };
    (
        visit_mut,
        $field_type:ident,
        $field:expr,
        $visitor:ident
    ) => {{
        visit_optional_field_mut!($field_type, $field, $visitor);
        VisitResult::Continue(())
    }};
}

macro_rules! visit_optional_field_mut {
    (DecoratedStatementDecorator, $field:expr, $visitor:ident) => {
        $visitor.visit_decorated_statement_decorator(&mut $field);
    };
    (JobConjunctionDecorator, $field:expr, $visitor:ident) => {
        $visitor.visit_job_conjunction_decorator(&mut $field);
    };
    (ElseClause, $field:expr, $visitor:ident) => {
        $visitor.visit_else_clause(&mut $field);
    };
    (SemiNl, $field:expr, $visitor:ident) => {
        $visitor.visit_semi_nl(&mut $field);
    };
    (KeywordTime, $field:expr, $visitor:ident) => {
        $visitor.visit_time(&mut $field);
    };
    (TokenBackground, $field:expr, $visitor:ident) => {
        $visitor.visit_token_background(&mut $field);
    };
}

macro_rules! visit_result {
    ( visit) => {
        ()
    };
    ( visit_mut ) => {
        VisitResult::Continue(())
    };
}

/// A redirection has an operator like > or 2>, and a target like /dev/null or &1.
/// Note that pipes are not redirections.
#[derive(Default, Debug)]
pub struct Redirection {
    pub oper: TokenRedirection,
    pub target: String_,
}
implement_node!(Redirection, redirection);
impl NodeSubTraits for Redirection {}
implement_acceptor_for_branch!(Redirection, (oper: TokenRedirection), (target: String_));

impl CheckParse for Redirection {
    fn can_be_parsed(pop: &mut Populator<'_>) -> bool {
        pop.peek_type(0) == ParseTokenType::redirection
    }
}

define_list_node!(
    VariableAssignmentList,
    variable_assignment_list,
    VariableAssignment
);

#[derive(Debug)]
pub enum ArgumentOrRedirection {
    Argument(Argument),
    Redirection(Redirection),
}
impl NodeSubTraits for ArgumentOrRedirection {}

impl Default for ArgumentOrRedirection {
    fn default() -> Self {
        Self::Argument(Argument::default())
    }
}

impl Acceptor for ArgumentOrRedirection {
    fn accept<'a>(&'a self, visitor: &mut dyn NodeVisitor<'a>) {
        match self {
            Self::Argument(child) => visitor.visit(child),
            Self::Redirection(child) => visitor.visit(child),
        };
    }
}
impl AcceptorMut for ArgumentOrRedirection {
    fn accept_mut(&mut self, visitor: &mut dyn NodeVisitorMut) {
        visitor.will_visit_fields_of(self);
        let flow = visitor.visit_argument_or_redirection(self);
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

implement_node!(ArgumentOrRedirection, argument_or_redirection);

impl CheckParse for ArgumentOrRedirection {
    fn can_be_parsed(pop: &mut Populator<'_>) -> bool {
        let typ = pop.peek_type(0);
        matches!(typ, ParseTokenType::string | ParseTokenType::redirection)
    }
}

define_list_node!(
    ArgumentOrRedirectionList,
    argument_or_redirection_list,
    ArgumentOrRedirection
);

/// A statement is a normal command, or an if / while / etc
#[derive(Default, Debug)]
pub struct Statement {
    pub contents: StatementVariant,
}
implement_node!(Statement, statement);
impl NodeSubTraits for Statement {}
implement_acceptor_for_branch!(Statement, (contents: (variant<StatementVariant>)));

/// A job is a non-empty list of statements, separated by pipes. (Non-empty is useful for cases
/// like if statements, where we require a command).
#[derive(Default, Debug)]
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
implement_node!(JobPipeline, job_pipeline);
impl NodeSubTraits for JobPipeline {}
implement_acceptor_for_branch!(
    JobPipeline,
    (time: (Option<KeywordTime>)),
    (variables: (VariableAssignmentList)),
    (statement: (Statement)),
    (continuation: (JobContinuationList)),
    (bg: (Option<TokenBackground>)),
);

/// A job_conjunction is a job followed by a && or || continuations.
#[derive(Default, Debug)]
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
implement_node!(JobConjunction, job_conjunction);
impl NodeSubTraits for JobConjunction {}
implement_acceptor_for_branch!(
    JobConjunction,
    (decorator: (Option<JobConjunctionDecorator>)),
    (job: (JobPipeline)),
    (continuations: (JobConjunctionContinuationList)),
    (semi_nl: (Option<SemiNl>)),
);

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

#[derive(Default, Debug)]
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
implement_node!(ForHeader, for_header);
impl NodeSubTraits for ForHeader {}
implement_acceptor_for_branch!(
    ForHeader,
    (kw_for: (KeywordFor)),
    (var_name: (String_)),
    (kw_in: (KeywordIn)),
    (args: (ArgumentList)),
    (semi_nl: (SemiNl)),
);

#[derive(Default, Debug)]
pub struct WhileHeader {
    /// 'while'
    pub kw_while: KeywordWhile,
    pub condition: JobConjunction,
    pub andor_tail: AndorJobList,
}
implement_node!(WhileHeader, while_header);
impl NodeSubTraits for WhileHeader {}
implement_acceptor_for_branch!(
    WhileHeader,
    (kw_while: (KeywordWhile)),
    (condition: (JobConjunction)),
    (andor_tail: (AndorJobList)),
);

#[derive(Default, Debug)]
pub struct FunctionHeader {
    pub kw_function: KeywordFunction,
    /// functions require at least one argument.
    pub first_arg: Argument,
    pub args: ArgumentList,
    pub semi_nl: SemiNl,
}
implement_node!(FunctionHeader, function_header);
impl NodeSubTraits for FunctionHeader {}
implement_acceptor_for_branch!(
    FunctionHeader,
    (kw_function: (KeywordFunction)),
    (first_arg: (Argument)),
    (args: (ArgumentList)),
    (semi_nl: (SemiNl)),
);

#[derive(Default, Debug)]
pub struct BeginHeader {
    pub kw_begin: KeywordBegin,
    /// Note that 'begin' does NOT require a semi or nl afterwards.
    /// This is valid: begin echo hi; end
    pub semi_nl: Option<SemiNl>,
}
implement_node!(BeginHeader, begin_header);
impl NodeSubTraits for BeginHeader {}
implement_acceptor_for_branch!(
    BeginHeader,
    (kw_begin: (KeywordBegin)),
    (semi_nl: (Option<SemiNl>))
);

#[derive(Default, Debug)]
pub struct BlockStatement {
    /// A header like for, while, etc.
    pub header: BlockStatementHeaderVariant,
    /// List of jobs in this block.
    pub jobs: JobList,
    /// The 'end' node.
    pub end: KeywordEnd,
    /// Arguments and redirections associated with the block.
    pub args_or_redirs: ArgumentOrRedirectionList,
}
implement_node!(BlockStatement, block_statement);
impl NodeSubTraits for BlockStatement {}
implement_acceptor_for_branch!(
    BlockStatement,
    (header: (variant<BlockStatementHeaderVariant>)),
    (jobs: (JobList)),
    (end: (KeywordEnd)),
    (args_or_redirs: (ArgumentOrRedirectionList)),
);

#[derive(Default, Debug)]
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
implement_node!(BraceStatement, brace_statement);
impl NodeSubTraits for BraceStatement {}
implement_acceptor_for_branch!(
    BraceStatement,
    (left_brace: (TokenLeftBrace)),
    (jobs: (JobList)),
    (right_brace: (TokenRightBrace)),
    (args_or_redirs: (ArgumentOrRedirectionList)),
);

#[derive(Default, Debug)]
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
implement_node!(IfClause, if_clause);
impl NodeSubTraits for IfClause {}
implement_acceptor_for_branch!(
    IfClause,
    (kw_if: (KeywordIf)),
    (condition: (JobConjunction)),
    (andor_tail: (AndorJobList)),
    (body: (JobList)),
);

#[derive(Default, Debug)]
pub struct ElseifClause {
    /// The 'else' keyword.
    pub kw_else: KeywordElse,
    /// The 'if' clause following it.
    pub if_clause: IfClause,
}
implement_node!(ElseifClause, elseif_clause);
impl NodeSubTraits for ElseifClause {}
implement_acceptor_for_branch!(
    ElseifClause,
    (kw_else: (KeywordElse)),
    (if_clause: (IfClause)),
);
impl CheckParse for ElseifClause {
    fn can_be_parsed(pop: &mut Populator<'_>) -> bool {
        pop.peek_token(0).keyword == ParseKeyword::Else
            && pop.peek_token(1).keyword == ParseKeyword::If
    }
}

define_list_node!(ElseifClauseList, elseif_clause_list, ElseifClause);

#[derive(Default, Debug)]
pub struct ElseClause {
    /// else ; body
    pub kw_else: KeywordElse,
    pub semi_nl: Option<SemiNl>,
    pub body: JobList,
}
implement_node!(ElseClause, else_clause);
impl NodeSubTraits for ElseClause {}
implement_acceptor_for_branch!(
    ElseClause,
    (kw_else: (KeywordElse)),
    (semi_nl: (Option<SemiNl>)),
    (body: (JobList)),
);
impl CheckParse for ElseClause {
    fn can_be_parsed(pop: &mut Populator<'_>) -> bool {
        pop.peek_token(0).keyword == ParseKeyword::Else
    }
}

#[derive(Default, Debug)]
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
implement_node!(IfStatement, if_statement);
impl NodeSubTraits for IfStatement {}
implement_acceptor_for_branch!(
    IfStatement,
    (if_clause: (IfClause)),
    (elseif_clauses: (ElseifClauseList)),
    (else_clause: (Option<ElseClause>)),
    (end: (KeywordEnd)),
    (args_or_redirs: (ArgumentOrRedirectionList)),
);

#[derive(Default, Debug)]
pub struct CaseItem {
    /// case \<arguments\> ; body
    pub kw_case: KeywordCase,
    pub arguments: ArgumentList,
    pub semi_nl: SemiNl,
    pub body: JobList,
}
implement_node!(CaseItem, case_item);
impl NodeSubTraits for CaseItem {}
implement_acceptor_for_branch!(
    CaseItem,
    (kw_case: (KeywordCase)),
    (arguments: (ArgumentList)),
    (semi_nl: (SemiNl)),
    (body: (JobList)),
);
impl CheckParse for CaseItem {
    fn can_be_parsed(pop: &mut Populator<'_>) -> bool {
        pop.peek_token(0).keyword == ParseKeyword::Case
    }
}

#[derive(Default, Debug)]
pub struct SwitchStatement {
    /// switch \<argument\> ; body ; end args_redirs
    pub kw_switch: KeywordSwitch,
    pub argument: Argument,
    pub semi_nl: SemiNl,
    pub cases: CaseItemList,
    pub end: KeywordEnd,
    pub args_or_redirs: ArgumentOrRedirectionList,
}
implement_node!(SwitchStatement, switch_statement);
impl NodeSubTraits for SwitchStatement {}
implement_acceptor_for_branch!(
    SwitchStatement,
    (kw_switch: (KeywordSwitch)),
    (argument: (Argument)),
    (semi_nl: (SemiNl)),
    (cases: (CaseItemList)),
    (end: (KeywordEnd)),
    (args_or_redirs: (ArgumentOrRedirectionList)),
);

/// A decorated_statement is a command with a list of arguments_or_redirections, possibly with
/// "builtin" or "command" or "exec"
#[derive(Default, Debug)]
pub struct DecoratedStatement {
    /// An optional decoration (command, builtin, exec, etc).
    pub opt_decoration: Option<DecoratedStatementDecorator>,
    /// Command to run.
    pub command: String_,
    /// Args and redirs
    pub args_or_redirs: ArgumentOrRedirectionList,
}
implement_node!(DecoratedStatement, decorated_statement);
impl NodeSubTraits for DecoratedStatement {}
implement_acceptor_for_branch!(
    DecoratedStatement,
    (opt_decoration: (Option<DecoratedStatementDecorator>)),
    (command: (String_)),
    (args_or_redirs: (ArgumentOrRedirectionList)),
);

/// A not statement like `not true` or `! true`
#[derive(Default, Debug)]
pub struct NotStatement {
    /// Keyword, either not or exclam.
    pub kw: KeywordNot,
    pub time: Option<KeywordTime>,
    pub variables: VariableAssignmentList,
    pub contents: Statement,
}
implement_node!(NotStatement, not_statement);
impl NodeSubTraits for NotStatement {}
implement_acceptor_for_branch!(
    NotStatement,
    (kw: (KeywordNot)),
    (time: (Option<KeywordTime>)),
    (variables: (VariableAssignmentList)),
    (contents: (Statement)),
);

#[derive(Default, Debug)]
pub struct JobContinuation {
    pub pipe: TokenPipe,
    pub newlines: MaybeNewlines,
    pub variables: VariableAssignmentList,
    pub statement: Statement,
}
implement_node!(JobContinuation, job_continuation);
impl NodeSubTraits for JobContinuation {}
implement_acceptor_for_branch!(
    JobContinuation,
    (pipe: (TokenPipe)),
    (newlines: (MaybeNewlines)),
    (variables: (VariableAssignmentList)),
    (statement: (Statement)),
);
impl CheckParse for JobContinuation {
    fn can_be_parsed(pop: &mut Populator<'_>) -> bool {
        pop.peek_type(0) == ParseTokenType::pipe
    }
}

define_list_node!(JobContinuationList, job_continuation_list, JobContinuation);

#[derive(Default, Debug)]
pub struct JobConjunctionContinuation {
    /// The && or || token.
    pub conjunction: TokenConjunction,
    pub newlines: MaybeNewlines,
    /// The job itself.
    pub job: JobPipeline,
}
implement_node!(JobConjunctionContinuation, job_conjunction_continuation);
impl NodeSubTraits for JobConjunctionContinuation {}
implement_acceptor_for_branch!(
    JobConjunctionContinuation,
    (conjunction: (TokenConjunction)),
    (newlines: (MaybeNewlines)),
    (job: (JobPipeline)),
);
impl CheckParse for JobConjunctionContinuation {
    fn can_be_parsed(pop: &mut Populator<'_>) -> bool {
        let typ = pop.peek_type(0);
        matches!(typ, ParseTokenType::andand | ParseTokenType::oror)
    }
}

/// An andor_job just wraps a job, but requires that the job have an 'and' or 'or' job_decorator.
/// Note this is only used for andor_job_list; jobs that are not part of an andor_job_list are not
/// instances of this.
#[derive(Default, Debug)]
pub struct AndorJob {
    pub job: JobConjunction,
}
implement_node!(AndorJob, andor_job);
impl NodeSubTraits for AndorJob {}
implement_acceptor_for_branch!(AndorJob, (job: (JobConjunction)));
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

define_list_node!(AndorJobList, andor_job_list, AndorJob);

/// A freestanding_argument_list is equivalent to a normal argument list, except it may contain
/// TOK_END (newlines, and even semicolons, for historical reasons).
/// In practice the tok_ends are ignored by fish code so we do not bother to store them.
#[derive(Default, Debug)]
pub struct FreestandingArgumentList {
    pub arguments: ArgumentList,
}
implement_node!(FreestandingArgumentList, freestanding_argument_list);
impl NodeSubTraits for FreestandingArgumentList {}
implement_acceptor_for_branch!(FreestandingArgumentList, (arguments: (ArgumentList)));

define_list_node!(
    JobConjunctionContinuationList,
    job_conjunction_continuation_list,
    JobConjunctionContinuation
);

define_list_node!(ArgumentList, argument_list, Argument);

// For historical reasons, a job list is a list of job *conjunctions*. This should be fixed.
define_list_node!(JobList, job_list, JobConjunction);

define_list_node!(CaseItemList, case_item_list, CaseItem);

/// A variable_assignment contains a source range like FOO=bar.
#[derive(Default, Debug)]
pub struct VariableAssignment {
    range: Option<SourceRange>,
}
implement_node!(VariableAssignment, variable_assignment);
impl NodeSubTraits for VariableAssignment {
    fn as_leaf(&self) -> Option<&dyn Leaf> {
        Some(self)
    }
}
implement_leaf!(VariableAssignment);
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
            // so this the token will be seen by allocate_populate_statement_contents.
            _ => false,
        }
    }
}

/// Zero or more newlines.
#[derive(Default, Debug)]
pub struct MaybeNewlines {
    range: Option<SourceRange>,
}
impl NodeSubTraits for MaybeNewlines {
    fn as_leaf(&self) -> Option<&dyn Leaf> {
        Some(self)
    }
}
implement_node!(MaybeNewlines, maybe_newlines);
implement_leaf!(MaybeNewlines);

/// An argument is just a node whose source range determines its contents.
/// This is a separate type because it is sometimes useful to find all arguments.
#[derive(Default, Debug)]
pub struct Argument {
    range: Option<SourceRange>,
}
implement_node!(Argument, argument);
impl NodeSubTraits for Argument {
    fn as_leaf(&self) -> Option<&dyn Leaf> {
        Some(self)
    }
}
implement_leaf!(Argument);
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

#[derive(Debug)]
pub enum BlockStatementHeaderVariant {
    None,
    ForHeader(ForHeader),
    WhileHeader(WhileHeader),
    FunctionHeader(FunctionHeader),
    BeginHeader(BeginHeader),
}

impl Default for BlockStatementHeaderVariant {
    fn default() -> Self {
        BlockStatementHeaderVariant::None
    }
}

impl Acceptor for BlockStatementHeaderVariant {
    fn accept<'a>(&'a self, visitor: &mut dyn NodeVisitor<'a>) {
        match self {
            BlockStatementHeaderVariant::None => panic!("cannot visit null block header"),
            BlockStatementHeaderVariant::ForHeader(node) => node.accept(visitor),
            BlockStatementHeaderVariant::WhileHeader(node) => node.accept(visitor),
            BlockStatementHeaderVariant::FunctionHeader(node) => node.accept(visitor),
            BlockStatementHeaderVariant::BeginHeader(node) => node.accept(visitor),
        }
    }
}
impl AcceptorMut for BlockStatementHeaderVariant {
    fn accept_mut(&mut self, visitor: &mut dyn NodeVisitorMut) {
        match self {
            BlockStatementHeaderVariant::None => panic!("cannot visit null block header"),
            BlockStatementHeaderVariant::ForHeader(node) => node.accept_mut(visitor),
            BlockStatementHeaderVariant::WhileHeader(node) => node.accept_mut(visitor),
            BlockStatementHeaderVariant::FunctionHeader(node) => node.accept_mut(visitor),
            BlockStatementHeaderVariant::BeginHeader(node) => node.accept_mut(visitor),
        }
    }
}

impl BlockStatementHeaderVariant {
    pub fn typ(&self) -> Type {
        self.embedded_node().typ()
    }
    pub fn try_source_range(&self) -> Option<SourceRange> {
        self.embedded_node().try_source_range()
    }

    pub fn as_for_header(&self) -> Option<&ForHeader> {
        match self {
            BlockStatementHeaderVariant::ForHeader(node) => Some(node),
            _ => None,
        }
    }
    pub fn as_while_header(&self) -> Option<&WhileHeader> {
        match self {
            BlockStatementHeaderVariant::WhileHeader(node) => Some(node),
            _ => None,
        }
    }
    pub fn as_function_header(&self) -> Option<&FunctionHeader> {
        match self {
            BlockStatementHeaderVariant::FunctionHeader(node) => Some(node),
            _ => None,
        }
    }
    pub fn as_begin_header(&self) -> Option<&BeginHeader> {
        match self {
            BlockStatementHeaderVariant::BeginHeader(node) => Some(node),
            _ => None,
        }
    }

    fn embedded_node(&self) -> &dyn NodeMut {
        match self {
            BlockStatementHeaderVariant::None => panic!("cannot visit null block header"),
            BlockStatementHeaderVariant::ForHeader(node) => node,
            BlockStatementHeaderVariant::WhileHeader(node) => node,
            BlockStatementHeaderVariant::FunctionHeader(node) => node,
            BlockStatementHeaderVariant::BeginHeader(node) => node,
        }
    }
}

#[derive(Debug)]
pub enum StatementVariant {
    None,
    NotStatement(Box<NotStatement>),
    BlockStatement(Box<BlockStatement>),
    BraceStatement(Box<BraceStatement>),
    IfStatement(Box<IfStatement>),
    SwitchStatement(Box<SwitchStatement>),
    DecoratedStatement(DecoratedStatement),
}

impl Default for StatementVariant {
    fn default() -> Self {
        StatementVariant::None
    }
}

impl Acceptor for StatementVariant {
    fn accept<'a>(&'a self, visitor: &mut dyn NodeVisitor<'a>) {
        match self {
            StatementVariant::None => panic!("cannot visit null statement"),
            StatementVariant::NotStatement(node) => node.accept(visitor),
            StatementVariant::BlockStatement(node) => node.accept(visitor),
            StatementVariant::BraceStatement(node) => node.accept(visitor),
            StatementVariant::IfStatement(node) => node.accept(visitor),
            StatementVariant::SwitchStatement(node) => node.accept(visitor),
            StatementVariant::DecoratedStatement(node) => node.accept(visitor),
        }
    }
}
impl AcceptorMut for StatementVariant {
    fn accept_mut(&mut self, visitor: &mut dyn NodeVisitorMut) {
        match self {
            StatementVariant::None => panic!("cannot visit null statement"),
            StatementVariant::NotStatement(node) => node.accept_mut(visitor),
            StatementVariant::BlockStatement(node) => node.accept_mut(visitor),
            StatementVariant::BraceStatement(node) => node.accept_mut(visitor),
            StatementVariant::IfStatement(node) => node.accept_mut(visitor),
            StatementVariant::SwitchStatement(node) => node.accept_mut(visitor),
            StatementVariant::DecoratedStatement(node) => node.accept_mut(visitor),
        }
    }
}

impl StatementVariant {
    pub fn typ(&self) -> Type {
        self.embedded_node().typ()
    }
    pub fn try_source_range(&self) -> Option<SourceRange> {
        self.embedded_node().try_source_range()
    }

    pub fn as_not_statement(&self) -> Option<&NotStatement> {
        match self {
            StatementVariant::NotStatement(node) => Some(node),
            _ => None,
        }
    }
    pub fn as_block_statement(&self) -> Option<&BlockStatement> {
        match self {
            StatementVariant::BlockStatement(node) => Some(node),
            _ => None,
        }
    }
    pub fn as_brace_statement(&self) -> Option<&BraceStatement> {
        match self {
            StatementVariant::BraceStatement(node) => Some(node),
            _ => None,
        }
    }
    pub fn as_if_statement(&self) -> Option<&IfStatement> {
        match self {
            StatementVariant::IfStatement(node) => Some(node),
            _ => None,
        }
    }
    pub fn as_switch_statement(&self) -> Option<&SwitchStatement> {
        match self {
            StatementVariant::SwitchStatement(node) => Some(node),
            _ => None,
        }
    }
    pub fn as_decorated_statement(&self) -> Option<&DecoratedStatement> {
        match self {
            StatementVariant::DecoratedStatement(node) => Some(node),
            _ => None,
        }
    }

    fn embedded_node(&self) -> &dyn NodeMut {
        match self {
            StatementVariant::None => panic!("cannot visit null statement"),
            StatementVariant::NotStatement(node) => &**node,
            StatementVariant::BlockStatement(node) => &**node,
            StatementVariant::BraceStatement(node) => &**node,
            StatementVariant::IfStatement(node) => &**node,
            StatementVariant::SwitchStatement(node) => &**node,
            StatementVariant::DecoratedStatement(node) => node,
        }
    }
}

/// Return a string literal name for an ast type.
pub fn ast_type_to_string(t: Type) -> &'static wstr {
    match t {
        Type::token_base => L!("token_base"),
        Type::keyword_base => L!("keyword_base"),
        Type::redirection => L!("redirection"),
        Type::variable_assignment => L!("variable_assignment"),
        Type::variable_assignment_list => L!("variable_assignment_list"),
        Type::argument_or_redirection => L!("argument_or_redirection"),
        Type::argument_or_redirection_list => L!("argument_or_redirection_list"),
        Type::statement => L!("statement"),
        Type::job_pipeline => L!("job_pipeline"),
        Type::job_conjunction => L!("job_conjunction"),
        Type::for_header => L!("for_header"),
        Type::while_header => L!("while_header"),
        Type::function_header => L!("function_header"),
        Type::begin_header => L!("begin_header"),
        Type::block_statement => L!("block_statement"),
        Type::brace_statement => L!("brace_statement"),
        Type::if_clause => L!("if_clause"),
        Type::elseif_clause => L!("elseif_clause"),
        Type::elseif_clause_list => L!("elseif_clause_list"),
        Type::else_clause => L!("else_clause"),
        Type::if_statement => L!("if_statement"),
        Type::case_item => L!("case_item"),
        Type::switch_statement => L!("switch_statement"),
        Type::decorated_statement => L!("decorated_statement"),
        Type::not_statement => L!("not_statement"),
        Type::job_continuation => L!("job_continuation"),
        Type::job_continuation_list => L!("job_continuation_list"),
        Type::job_conjunction_continuation => L!("job_conjunction_continuation"),
        Type::andor_job => L!("andor_job"),
        Type::andor_job_list => L!("andor_job_list"),
        Type::freestanding_argument_list => L!("freestanding_argument_list"),
        Type::token_conjunction => L!("token_conjunction"),
        Type::job_conjunction_continuation_list => L!("job_conjunction_continuation_list"),
        Type::maybe_newlines => L!("maybe_newlines"),
        Type::token_pipe => L!("token_pipe"),
        Type::case_item_list => L!("case_item_list"),
        Type::argument => L!("argument"),
        Type::argument_list => L!("argument_list"),
        Type::job_list => L!("job_list"),
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
        panic!("Node {:?} has either been popped off of the stack or not yet visited. Cannot find parent.", node.describe());
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

/// The ast type itself.
pub struct Ast {
    // The top node.
    // Its type depends on what was requested to parse.
    top: Box<dyn Node>,
    /// Whether any errors were encountered during parsing.
    any_error: bool,
    /// Extra fields.
    pub extras: Extras,
}

impl Ast {
    /// Construct an ast by parsing `src` as a job list.
    /// The ast attempts to produce `type` as the result.
    /// `type` may only be JobList or FreestandingArgumentList.
    pub fn parse(
        src: &wstr,
        flags: ParseTreeFlags,
        out_errors: Option<&mut ParseErrorList>,
    ) -> Self {
        parse_from_top(src, flags, out_errors, Type::job_list)
    }
    /// Like parse(), but constructs a freestanding_argument_list.
    pub fn parse_argument_list(
        src: &wstr,
        flags: ParseTreeFlags,
        out_errors: Option<&mut ParseErrorList>,
    ) -> Self {
        parse_from_top(src, flags, out_errors, Type::freestanding_argument_list)
    }
    /// Return a traversal, allowing iteration over the nodes.
    pub fn walk(&'_ self) -> Traversal<'_> {
        Traversal::new(self.top.as_node())
    }
    /// Return the top node. This has the type requested in the 'parse' method.
    pub fn top(&self) -> &dyn Node {
        &*self.top
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

            if let Some(n) = node.as_argument() {
                result += "argument";
                if let Some(argsrc) = n.try_source(orig) {
                    sprintf!(=> &mut result, ": '%ls'", argsrc);
                }
            } else if let Some(n) = node.as_keyword() {
                sprintf!(=> &mut result, "keyword: %ls", n.keyword().to_wstr());
            } else if let Some(n) = node.as_token() {
                let desc = match n.token_type() {
                    ParseTokenType::string => {
                        let mut desc = WString::from_str("string");
                        if let Some(strsource) = n.try_source(orig) {
                            sprintf!(=> &mut desc, ": '%ls'", strsource);
                        }
                        desc
                    }
                    ParseTokenType::redirection => {
                        let mut desc = WString::from_str("redirection");
                        if let Some(strsource) = n.try_source(orig) {
                            sprintf!(=> &mut desc, ": '%ls'", strsource);
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
        match node.category() {
            Category::leaf => match node.as_leaf().unwrap().range() {
                None => self.any_unsourced = true,
                // Union with our range.
                Some(range) if range.length > 0 => {
                    if self.total.length == 0 {
                        self.total = range;
                    } else {
                        let end =
                            (self.total.start + self.total.length).max(range.start + range.length);
                        self.total.start = self.total.start.min(range.start);
                        self.total.length = end - self.total.start;
                    }
                }
                _ => (),
            },
            _ => {
                // Other node types recurse.
                node.accept(self);
            }
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
            Some(wgettext_maybe_fmt!($fmt $(, $args)*))
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

    /** The type which we are attempting to parse, typically job_list but may be
    freestanding_argument_list. */
    top_type: Type,

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
    fn visit_mut(&mut self, node: &mut dyn NodeMut) -> VisitResult {
        use KindMut as KM;
        match node.kind_mut() {
            // Leaves
            KM::Argument(node) => self.visit_argument(node),
            KM::VariableAssignment(node) => self.visit_variable_assignment(node),
            KM::JobContinuation(node) => self.visit_job_continuation(node),
            KM::Token(node) => self.visit_token(node),
            KM::Keyword(node) => {
                return self.visit_keyword(node);
            }
            KM::MaybeNewlines(node) => self.visit_maybe_newlines(node),

            // Branches
            KM::Redirection(node) => node.accept_mut(self),
            KM::ArgumentOrRedirection(node) => node.accept_mut(self),
            KM::Statement(node) => node.accept_mut(self),
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

    fn will_visit_fields_of(&mut self, node: &mut dyn NodeMut) {
        FLOGF!(
            ast_construction,
            "%*swill_visit %ls",
            self.spaces(),
            "",
            node.describe()
        );
        self.depth += 1
    }

    fn did_visit_fields_of<'a>(&'a mut self, node: &'a dyn NodeMut, flow: VisitResult) {
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
        // incomplete; in particular parent nodes are not set.
        let mut cursor = node;
        let header = loop {
            match cursor.typ() {
                Type::block_statement => {
                    cursor = cursor.as_block_statement().unwrap().header.embedded_node();
                }
                Type::for_header => {
                    let n = cursor.as_for_header().unwrap();
                    break Some((n.kw_for.range.unwrap(), L!("for loop")));
                }
                Type::while_header => {
                    let n = cursor.as_while_header().unwrap();
                    break Some((n.kw_while.range.unwrap(), L!("while loop")));
                }
                Type::function_header => {
                    let n = cursor.as_function_header().unwrap();
                    break Some((n.kw_function.range.unwrap(), L!("function definition")));
                }
                Type::begin_header => {
                    let n = cursor.as_begin_header().unwrap();
                    break Some((n.kw_begin.range.unwrap(), L!("begin")));
                }
                Type::if_statement => {
                    let n = cursor.as_if_statement().unwrap();
                    break Some((n.if_clause.kw_if.range.unwrap(), L!("if statement")));
                }
                Type::switch_statement => {
                    let n = cursor.as_switch_statement().unwrap();
                    break Some((n.kw_switch.range.unwrap(), L!("switch statement")));
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
                "Missing end to balance this %ls",
                enclosing_stmt
            );
        } else {
            parse_error!(
                self,
                token,
                ParseErrorCode::generic,
                "Expected %ls, but found %ls",
                keywords_user_presentable_description(error.allowed_keywords),
                error.token.user_presentable_description(),
            );
        }
    }

    fn visit_argument_or_redirection(&mut self, node: &mut ArgumentOrRedirection) -> VisitResult {
        if let Some(arg) = self.try_parse::<Argument>() {
            *node = ArgumentOrRedirection::Argument(arg);
        } else if let Some(redir) = self.try_parse::<Redirection>() {
            *node = ArgumentOrRedirection::Redirection(redir);
        } else {
            internal_error!(
                self,
                visit_argument_or_redirection,
                "Unable to parse argument or redirection"
            );
        }
        VisitResult::Continue(())
    }
    fn visit_block_statement_header(
        &mut self,
        node: &mut BlockStatementHeaderVariant,
    ) -> VisitResult {
        *node = self.allocate_populate_block_header();
        VisitResult::Continue(())
    }
    fn visit_statement(&mut self, node: &mut StatementVariant) -> VisitResult {
        *node = self.allocate_populate_statement_contents();
        VisitResult::Continue(())
    }

    fn visit_decorated_statement_decorator(
        &mut self,
        node: &mut Option<DecoratedStatementDecorator>,
    ) {
        *node = self.try_parse::<DecoratedStatementDecorator>();
    }
    fn visit_job_conjunction_decorator(&mut self, node: &mut Option<JobConjunctionDecorator>) {
        *node = self.try_parse::<JobConjunctionDecorator>();
    }
    fn visit_else_clause(&mut self, node: &mut Option<ElseClause>) {
        *node = self.try_parse::<ElseClause>();
    }
    fn visit_semi_nl(&mut self, node: &mut Option<SemiNl>) {
        *node = self.try_parse::<SemiNl>();
    }
    fn visit_time(&mut self, node: &mut Option<KeywordTime>) {
        *node = self.try_parse::<KeywordTime>();
    }
    fn visit_token_background(&mut self, node: &mut Option<TokenBackground>) {
        *node = self.try_parse::<TokenBackground>();
    }
}

/// Helper to describe a list of keywords.
/// TODO: these need to be localized properly.
fn keywords_user_presentable_description(kws: &'static [ParseKeyword]) -> WString {
    assert!(!kws.is_empty(), "Should not be empty list");
    if kws.len() == 1 {
        return sprintf!("keyword '%ls'", kws[0]);
    }
    let mut res = L!("keywords ").to_owned();
    for (i, kw) in kws.iter().enumerate() {
        if i != 0 {
            res += L!(" or ");
        }
        res += &sprintf!("'%ls'", *kw)[..];
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
    /// Construct from a source, flags, top type, and out_errors, which may be null.
    fn new(
        src: &'s wstr,
        flags: ParseTreeFlags,
        top_type: Type,
        out_errors: Option<&'s mut ParseErrorList>,
    ) -> Self {
        Self {
            flags,
            semis: vec![],
            errors: vec![],
            tokens: TokenStream::new(src, flags, top_type == Type::freestanding_argument_list),
            top_type,
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

    /// Return whether a list type `type` allows arbitrary newlines in it.
    fn list_type_chomps_newlines(&self, typ: Type) -> bool {
        match typ {
            Type::argument_list => {
                // Hackish. If we are producing a freestanding argument list, then it allows
                // semicolons, for hysterical raisins.
                self.top_type == Type::freestanding_argument_list
            }
            Type::argument_or_redirection_list => {
                // No newlines inside arguments.
                false
            }
            Type::variable_assignment_list => {
                // No newlines inside variable assignment lists.
                false
            }
            Type::job_list => {
                // Like echo a \n \n echo b
                true
            }
            Type::case_item_list => {
                // Like switch foo \n \n \n case a \n end
                true
            }
            Type::andor_job_list => {
                // Like while true ; \n \n and true ; end
                true
            }
            Type::elseif_clause_list => {
                // Like if true ; \n \n else if false; end
                true
            }
            Type::job_conjunction_continuation_list => {
                // This would be like echo a && echo b \n && echo c
                // We could conceivably support this but do not now.
                false
            }
            Type::job_continuation_list => {
                // This would be like echo a \n | echo b
                // We could conceivably support this but do not now.
                false
            }
            _ => {
                internal_error!(
                    self,
                    list_type_chomps_newlines,
                    "Type %ls not handled",
                    ast_type_to_string(typ)
                );
            }
        }
    }

    /// Return whether a list type `type` allows arbitrary semicolons in it.
    fn list_type_chomps_semis(&self, typ: Type) -> bool {
        match typ {
            Type::argument_list => {
                // Hackish. If we are producing a freestanding argument list, then it allows
                // semicolons, for hysterical raisins.
                // That is, this is OK: complete -c foo -a 'x ; y ; z'
                // But this is not: foo x ; y ; z
                self.top_type == Type::freestanding_argument_list
            }

            Type::argument_or_redirection_list | Type::variable_assignment_list => false,
            Type::job_list => {
                // Like echo a ; ;  echo b
                true
            }
            Type::case_item_list => {
                // Like switch foo ; ; ;  case a \n end
                // This is historically allowed.
                true
            }
            Type::andor_job_list => {
                // Like while true ; ; ;  and true ; end
                true
            }
            Type::elseif_clause_list => {
                // Like if true ; ; ;  else if false; end
                false
            }
            Type::job_conjunction_continuation_list => {
                // Like echo a ; ; && echo b. Not supported.
                false
            }
            Type::job_continuation_list => {
                // This would be like echo a ; | echo b
                // Not supported.
                // We could conceivably support this but do not now.
                false
            }
            _ => {
                internal_error!(
                    self,
                    list_type_chomps_semis,
                    "Type %ls not handled",
                    ast_type_to_string(typ)
                );
            }
        }
    }

    /// Chomp extra comments, semicolons, etc. for a given list type.
    fn chomp_extras(&mut self, typ: Type) {
        let chomp_semis = self.list_type_chomps_semis(typ);
        let chomp_newlines = self.list_type_chomps_newlines(typ);
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

    /// Return whether a list type should recover from errors.s
    /// That is, whether we should stop unwinding when we encounter this type.
    fn list_type_stops_unwind(&self, typ: Type) -> bool {
        typ == Type::job_list && self.flags.contains(ParseTreeFlags::CONTINUE_AFTER_ERROR)
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
                "Expected %ls, but found %ls",
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
        if self.top_type == Type::freestanding_argument_list {
            parse_error!(
                self,
                tok,
                ParseErrorCode::generic,
                "Expected %ls, but found %ls",
                token_type_user_presentable_description(ParseTokenType::string, ParseKeyword::None),
                tok.user_presentable_description()
            );
            return;
        }

        assert!(self.top_type == Type::job_list);
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
                            "Token %ls should not have prevented parsing a job list",
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
                    "Expected a string, but found %ls",
                    tok.user_presentable_description()
                );
            }
            ParseTokenType::tokenizer_error => {
                parse_error!(
                    self,
                    tok,
                    ParseErrorCode::from(tok.tok_error),
                    "%ls",
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
                    "Unexpected excess token type: %ls",
                    tok.user_presentable_description()
                );
            }
        }
    }

    /// Given that we are a list of type ListNodeType, whose contents type is ContentsNode,
    /// populate as many elements as we can.
    /// If exhaust_stream is set, then keep going until we get parse_token_type_t::terminate.
    fn populate_list<ContentsType, ListType>(&mut self, list: &mut ListType, exhaust_stream: bool)
    where
        ContentsType: NodeMut + CheckParse + Default,
        ListType: Node + AsMut<Box<[ContentsType]>>,
    {
        let typ = list.typ();
        let list = list.as_mut();
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
                "%*sunwinding %ls",
                self.spaces(),
                "",
                ast_type_to_string(typ)
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
                if !self.list_type_stops_unwind(typ) {
                    break;
                }
                // We are going to stop unwinding.
                // Rather hackish. Just chomp until we get to a string or end node.
                loop {
                    let typ = self.peek_type(0);
                    if [
                        ParseTokenType::string,
                        ParseTokenType::terminate,
                        ParseTokenType::end,
                    ]
                    .contains(&typ)
                    {
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
            self.chomp_extras(typ);

            // Now try parsing a node.
            if let Some(node) = self.try_parse::<ContentsType>() {
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
            *list = contents.into_boxed_slice();
        }

        FLOGF!(
            ast_construction,
            "%*s%ls size: %lu",
            self.spaces(),
            "",
            ast_type_to_string(typ),
            list.len()
        );
    }

    /// Allocate and populate a statement contents pointer.
    /// This must never return null.
    fn allocate_populate_statement_contents(&mut self) -> StatementVariant {
        // In case we get a parse error, we still need to return something non-null. Use a
        // decorated statement; all of its leaf nodes will end up unsourced.
        fn got_error(slf: &mut Populator<'_>) -> StatementVariant {
            assert!(slf.unwinding, "Should have produced an error");
            new_decorated_statement(slf)
        }

        fn new_decorated_statement(slf: &mut Populator<'_>) -> StatementVariant {
            let embedded = slf.allocate_visit::<DecoratedStatement>();
            if !slf.unwinding && slf.peek_token(0).typ == ParseTokenType::left_brace {
                parse_error!(
                    slf,
                    slf.peek_token(0),
                    ParseErrorCode::generic,
                    "Expected %s, but found %ls",
                    token_type_user_presentable_description(
                        ParseTokenType::end,
                        ParseKeyword::None
                    ),
                    slf.peek_token(0).user_presentable_description()
                );
            }
            StatementVariant::DecoratedStatement(embedded)
        }

        if self.peek_token(0).typ == ParseTokenType::terminate && self.allow_incomplete() {
            // This may happen if we just have a 'time' prefix.
            // Construct a decorated statement, which will be unsourced.
            self.allocate_visit::<DecoratedStatement>();
        } else if self.peek_token(0).typ == ParseTokenType::left_brace {
            let embedded = self.allocate_boxed_visit::<BraceStatement>();
            return StatementVariant::BraceStatement(embedded);
        } else if self.peek_token(0).typ != ParseTokenType::string {
            // We may be unwinding already; do not produce another error.
            // For example in `true | and`.
            parse_error!(
                self,
                self.peek_token(0),
                ParseErrorCode::generic,
                "Expected a command, but found %ls",
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
                StatementVariant::NotStatement(embedded)
            }
            ParseKeyword::For
            | ParseKeyword::While
            | ParseKeyword::Function
            | ParseKeyword::Begin => {
                let embedded = self.allocate_boxed_visit::<BlockStatement>();
                StatementVariant::BlockStatement(embedded)
            }
            ParseKeyword::If => {
                let embedded = self.allocate_boxed_visit::<IfStatement>();
                StatementVariant::IfStatement(embedded)
            }
            ParseKeyword::Switch => {
                let embedded = self.allocate_boxed_visit::<SwitchStatement>();
                StatementVariant::SwitchStatement(embedded)
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
                    "Expected a command, but found %ls",
                    self.peek_token(0).user_presentable_description()
                );
                return got_error(self);
            }
            _ => new_decorated_statement(self),
        }
    }

    /// Allocate and populate a block statement header.
    /// This must never return null.
    fn allocate_populate_block_header(&mut self) -> BlockStatementHeaderVariant {
        match self.peek_token(0).keyword {
            ParseKeyword::For => {
                let embedded = self.allocate_visit::<ForHeader>();
                BlockStatementHeaderVariant::ForHeader(embedded)
            }
            ParseKeyword::While => {
                let embedded = self.allocate_visit::<WhileHeader>();
                BlockStatementHeaderVariant::WhileHeader(embedded)
            }
            ParseKeyword::Function => {
                let embedded = self.allocate_visit::<FunctionHeader>();
                BlockStatementHeaderVariant::FunctionHeader(embedded)
            }
            ParseKeyword::Begin => {
                let embedded = self.allocate_visit::<BeginHeader>();
                BlockStatementHeaderVariant::BeginHeader(embedded)
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
        if [ParseKeyword::And, ParseKeyword::Or].contains(&self.peek_token(1).keyword) {
            parse_error!(
                self,
                self.peek_token(1),
                ParseErrorCode::andor_in_pipeline,
                INVALID_PIPELINE_CMD_ERR_MSG,
                self.peek_token(1).keyword
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
                "Expected %ls, but found %ls",
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
                    "Expected %ls, but found %ls",
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

fn parse_from_top(
    src: &wstr,
    flags: ParseTreeFlags,
    out_errors: Option<&mut ParseErrorList>,
    top_type: Type,
) -> Ast {
    let mut pops = Populator::new(src, flags, top_type, out_errors);
    let top: Box<dyn Node> = match top_type {
        Type::job_list => {
            let mut list: Box<JobList> = Box::default();
            pops.populate_list(&mut *list, true);
            list
        }
        Type::freestanding_argument_list => {
            let mut list = Box::<FreestandingArgumentList>::default();
            pops.populate_list(&mut list.arguments, true);
            list
        }
        _ => panic!("Invalid top type"),
    };

    // Chomp trailing extras, etc.
    pops.chomp_extras(Type::job_list);

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
    let ast = Ast::parse(src, ParseTreeFlags::empty(), None);
    assert!(!ast.any_error);
}

#[derive(Clone, Copy, Debug, Eq, PartialEq)]
pub enum Category {
    branch,
    leaf,
    list,
}

#[derive(Clone, Copy, Debug, Eq, PartialEq)]
pub enum Type {
    token_base,
    keyword_base,
    redirection,
    variable_assignment,
    variable_assignment_list,
    argument_or_redirection,
    argument_or_redirection_list,
    statement,
    job_pipeline,
    job_conjunction,
    for_header,
    while_header,
    function_header,
    begin_header,
    block_statement,
    brace_statement,
    if_clause,
    elseif_clause,
    elseif_clause_list,
    else_clause,
    if_statement,
    case_item,
    switch_statement,
    decorated_statement,
    not_statement,
    job_continuation,
    job_continuation_list,
    job_conjunction_continuation,
    andor_job,
    andor_job_list,
    freestanding_argument_list,
    token_conjunction,
    job_conjunction_continuation_list,
    maybe_newlines,
    token_pipe,
    case_item_list,
    argument,
    argument_list,
    job_list,
}

impl Type {
    // Return the underlying category of this type.
    pub fn category(self) -> Category {
        match self {
            Type::token_base => Category::leaf,
            Type::keyword_base => Category::leaf,
            Type::redirection => Category::branch,
            Type::variable_assignment => Category::leaf,
            Type::variable_assignment_list => Category::list,
            Type::argument_or_redirection => Category::branch,
            Type::argument_or_redirection_list => Category::list,
            Type::statement => Category::branch,
            Type::job_pipeline => Category::branch,
            Type::job_conjunction => Category::branch,
            Type::for_header => Category::branch,
            Type::while_header => Category::branch,
            Type::function_header => Category::branch,
            Type::begin_header => Category::branch,
            Type::block_statement => Category::branch,
            Type::brace_statement => Category::branch,
            Type::if_clause => Category::branch,
            Type::elseif_clause => Category::branch,
            Type::elseif_clause_list => Category::list,
            Type::else_clause => Category::branch,
            Type::if_statement => Category::branch,
            Type::case_item => Category::branch,
            Type::switch_statement => Category::branch,
            Type::decorated_statement => Category::branch,
            Type::not_statement => Category::branch,
            Type::job_continuation => Category::branch,
            Type::job_continuation_list => Category::list,
            Type::job_conjunction_continuation => Category::branch,
            Type::andor_job => Category::branch,
            Type::andor_job_list => Category::list,
            Type::freestanding_argument_list => Category::branch, // not a typo, wraps a list
            Type::token_conjunction => Category::leaf,
            Type::job_conjunction_continuation_list => Category::list,
            Type::maybe_newlines => Category::leaf,
            Type::token_pipe => Category::leaf,
            Type::case_item_list => Category::list,
            Type::argument => Category::leaf,
            Type::argument_list => Category::list,
            Type::job_list => Category::list,
        }
    }
}
