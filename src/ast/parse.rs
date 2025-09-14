use super::*;

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

impl<T: CheckParse + Parse + ListElement> Parse for Box<[T]> {
    fn parse(pop: &mut Populator<'_>) -> Self {
        pop.parse_list(false)
    }
}

macro_rules! check_unsource {
    ($pop:expr) => {
        if $pop.unsource_leaves() {
            return Self { range: None };
        }
    };
}

macro_rules! from_range {
    ($range:expr) => {
        Self {
            range: Some($range),
        }
    };
}

impl Parse for VariableAssignment {
    fn parse(pop: &mut Populator<'_>) -> Self {
        check_unsource!(pop);
        if !pop.peek_token(0).may_be_variable_assignment {
            internal_error!(
                pop,
                VariableAssignment_parse,
                "Should not have created VariableAssignment from this token"
            );
        }
        from_range!(pop.consume_token_type(ParseTokenType::string))
    }
}

impl Parse for Argument {
    fn parse(pop: &mut Populator<'_>) -> Self {
        check_unsource!(pop);
        from_range!(pop.consume_token_type(ParseTokenType::string))
    }
}

impl Parse for BlockStatementHeader {
    fn parse(pop: &mut Populator<'_>) -> Self {
        match pop.peek_token(0).keyword {
            ParseKeyword::For => BlockStatementHeader::For(pop.parse()),
            ParseKeyword::While => BlockStatementHeader::While(pop.parse()),
            ParseKeyword::Function => BlockStatementHeader::Function(pop.parse()),
            ParseKeyword::Begin => BlockStatementHeader::Begin(pop.parse()),
            _ => internal_error!(
                pop,
                BlockStatementHeader_parse,
                "should not have descended into block_header"
            ),
        }
    }
}

impl Parse for ArgumentOrRedirection {
    fn parse(pop: &mut Populator<'_>) -> Self {
        if let Some(arg) = pop.parse_option::<Argument>() {
            ArgumentOrRedirection::Argument(arg)
        } else if let Some(redir) = pop.parse_option::<Redirection>() {
            ArgumentOrRedirection::Redirection(Box::new(redir))
        } else {
            internal_error!(
                pop,
                ArgumentOrRedirection_parse,
                "Unable to parse argument or redirection"
            );
        }
    }
}

impl Parse for MaybeNewlines {
    fn parse(pop: &mut Populator<'_>) -> Self {
        check_unsource!(pop);
        let mut range = SourceRange::new(0, 0);
        // TODO: it would be nice to have the start offset be the current position in the token
        // stream, even if there are no newlines.
        while pop.peek_token(0).is_newline {
            let r = pop.consume_token_type(ParseTokenType::end);
            if range.length == 0 {
                range = r;
            } else {
                range.length = r.start + r.length - range.start
            }
        }
        from_range!(range)
    }
}

impl Parse for Statement {
    fn parse(pop: &mut Populator<'_>) -> Self {
        pop.parse_statement()
    }
}
