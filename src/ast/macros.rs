/// Implement the node trait.
macro_rules! Node {
    ($name:ident) => {
        impl Node for $name {
            fn kind(&self) -> Kind<'_> {
                Kind::$name(self)
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
            fn unsourced() -> Self {
                Self::default()
            }
            fn range(&self) -> Option<SourceRange> {
                self.range
            }
        }
        impl Acceptor for $name {
            #[allow(unused_variables)]
            fn accept<'a>(&'a self, visitor: &mut dyn NodeVisitor<'a>) {}
        }
    };

    ( $(#[$_m:meta])* $_v:vis struct $name:ident $_:tt $(;)? ) => {
        Leaf!($name);
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
    };
}

/// Implement the acceptor trait for the given branch node.
macro_rules! Parse {
    (
        $(#[$_m:meta])*
        $_v:vis struct $name:ident {
            $(
                $(#[$_fm:meta])*
                $_fv:vis $field_name:ident : $_ft:ty
            ),* $(,)?
        }
    ) => {
        impl Parse for $name {
            fn parse(pop: &mut Populator<'_>) -> Self {
                let mut node = Self::default();
                pop.will_visit_fields_of(&node);

                $(
                    node.$field_name = match pop.try_parse() {
                        Ok(v) => v,
                        Err(e) => {
                            pop.did_visit_fields_of(&node, Some(e));
                            return node;
                        }
                    };
                )*

                pop.did_visit_fields_of(&node, None);
                node
            }
        }
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
        }
        impl Keyword for $name {
            fn new(range: SourceRange, keyword: ParseKeyword) -> Self {
                Self {
                    range: Some(range),
                    keyword,
                }
            }
            fn keyword(&self) -> ParseKeyword {
                self.keyword
            }
            fn allowed_keywords() -> &'static [ParseKeyword] {
                &[$(ParseKeyword::$allowed),*]
            }
            fn as_leaf(&self) -> &dyn Leaf {
                self
            }
        }
        impl TryParse for $name {
            fn try_parse(pop: &mut Populator) -> ParseResult<Self> {
                pop.parse_keyword()
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
        }
        impl Token for $name {
            fn new(range: SourceRange, token_type: ParseTokenType) -> Self {
                Self {
                    range: Some(range),
                    parse_token_type: token_type,
                }
            }
            fn token_type(&self) -> ParseTokenType {
                self.parse_token_type
            }
            fn allowed_tokens() -> &'static [ParseTokenType] {
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
        impl Parse for $name {
            fn parse(pop: &mut Populator<'_>) -> Self {
                pop.parse_token()
            }
        }
        impl $name {
            const ALLOWED_TOKENS: &'static [ParseTokenType] = &[$(ParseTokenType::$allowed),*];
        }
    }
}

macro_rules! define_list_nodes {
    (
        // due to macro hygiene, the caller needs to provide their own
        // variable name for the populator if they intend to access it
        // just specify it up here, and all the trait impls are free to use it if they need
        let $pop:ident;

        $(
            $contents:ty => $name:ident {
                chomp_newlines: $newlines:expr,
                chomp_semis: $semis:expr,
                $(stop_unwind: $unwind:expr,)?
            }
        ),*$(,)?
    ) => {$(
        pub type $name = Box<[$contents]>;

        #[allow(unused_variables)]
        impl ListElement for $contents {
            fn list_kind(list: &[Self]) -> Kind<'_> {
                Kind::$name(list)
            }

            fn chomps_newlines(pop: &Populator<'_>) -> bool {
                let $pop = pop;
                $newlines
            }

            fn chomps_semis(pop: &Populator<'_>) -> bool {
                let $pop = pop;
                $semis
            }

            $(fn stops_unwind(pop: &Populator<'_>) -> bool {
                let $pop = pop;
                $unwind
            })?
        }
    )*}
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

/// This indicates a bug in fish code.
macro_rules! internal_error {
    (
        $self:ident,
        $func:ident,
        $fmt:expr
        $(, $args:expr)*
        $(,)?
    ) => {{
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
    }};
}
