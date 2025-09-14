use super::*;

pub(super) type ParseResult<T> = Result<T, MissingEndError>;

pub(super) trait Parse: Sized {
    fn parse(pop: &mut Populator<'_>) -> ParseResult<Self>;
}

impl<T: CheckParse + Parse> Parse for Option<T> {
    fn parse(pop: &mut Populator<'_>) -> ParseResult<Self> {
        if T::can_be_parsed(pop) {
            Some(pop.parse::<T>())
        } else {
            None
        }
        .transpose()
    }
}

impl<T: CheckParse + Parse + ListElement> Parse for Box<[T]> {
    fn parse(pop: &mut Populator<'_>) -> ParseResult<Self> {
        pop.parse_list(false)
    }
}

macro_rules! check_unsource {
    ($pop:expr) => {
        if $pop.unsource_leaves() {
            return Ok(Self { range: None });
        }
    };
}

macro_rules! from_range {
    ($range:expr) => {
        Ok(Self {
            range: Some($range),
        })
    };
}

impl Parse for VariableAssignment {
    fn parse(pop: &mut Populator<'_>) -> ParseResult<Self> {
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
    fn parse(pop: &mut Populator<'_>) -> ParseResult<Self> {
        check_unsource!(pop);
        from_range!(pop.consume_token_type(ParseTokenType::string))
    }
}

impl Parse for BlockStatementHeader {
    fn parse(pop: &mut Populator<'_>) -> ParseResult<Self> {
        match pop.peek_token(0).keyword {
            ParseKeyword::For => pop.parse().map(BlockStatementHeader::For),
            ParseKeyword::While => pop.parse().map(BlockStatementHeader::While),
            ParseKeyword::Function => pop.parse().map(BlockStatementHeader::Function),
            ParseKeyword::Begin => pop.parse().map(BlockStatementHeader::Begin),
            _ => internal_error!(
                pop,
                BlockStatementHeader_parse,
                "should not have descended into block_header"
            ),
        }
    }
}

impl Parse for ArgumentOrRedirection {
    fn parse(pop: &mut Populator<'_>) -> ParseResult<Self> {
        if let Some(arg) = pop.parse::<Option<Argument>>()? {
            Ok(ArgumentOrRedirection::Argument(arg))
        } else if let Some(redir) = pop.parse::<Option<Redirection>>()? {
            Ok(ArgumentOrRedirection::Redirection(Box::new(redir)))
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
    fn parse(pop: &mut Populator<'_>) -> ParseResult<Self> {
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
    fn parse(pop: &mut Populator<'_>) -> ParseResult<Self> {
        pop.parse_statement()
    }
}
