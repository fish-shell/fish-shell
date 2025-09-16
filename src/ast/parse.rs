use super::*;

macro_rules! check_unsource {
    ($pop:expr) => {
        if $pop.unsource_leaves() {
            return Self::default();
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
