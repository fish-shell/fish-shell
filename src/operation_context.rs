use crate::common::CancelChecker;
use crate::env::EnvDyn;
use crate::env::{EnvStack, Environment};
use crate::parser::Parser;
use crate::proc::JobGroupRef;

use crate::reader::read_generation_count;

/// A common helper which always returns false.
pub fn no_cancel() -> bool {
    false
}

// Default limits for expansion.
/// The default maximum number of items from expansion.
pub const EXPANSION_LIMIT_DEFAULT: usize = 512 * 1024;
/// A smaller limit for background operations like syntax highlighting.
pub const EXPANSION_LIMIT_BACKGROUND: usize = 512;

#[allow(clippy::enum_variant_names)]
enum Vars<'a> {
    // The parser, if this is a foreground operation. If this is a background operation, this may be
    // nullptr.
    Parser(&'a Parser),
    // A set of variables.
    Vars(&'a dyn Environment),

    TestOnly(&'a Parser, &'a dyn Environment),
}

/// A operation_context_t is a simple property bag which wraps up data needed for highlighting,
/// expansion, completion, and more.
pub struct OperationContext<'a> {
    vars: Vars<'a>,

    // The limit in the number of expansions which should be produced.
    pub expansion_limit: usize,

    /// The job group of the parental job.
    /// This is used only when expanding command substitutions. If this is set, any jobs created
    /// by the command substitutions should use this tree.
    pub job_group: Option<JobGroupRef>,

    // A function which may be used to poll for cancellation.
    pub cancel_checker: CancelChecker,
}

impl<'a> OperationContext<'a> {
    pub fn vars(&self) -> &dyn Environment {
        match &self.vars {
            Vars::Parser(parser) => &*parser.variables,
            Vars::Vars(vars) => *vars,
            Vars::TestOnly(_, vars) => *vars,
        }
    }

    // Return an "empty" context which contains no variables, no parser, and never cancels.
    pub fn empty() -> OperationContext<'static> {
        use std::sync::OnceLock;
        static NULL_ENV: OnceLock<EnvStack> = OnceLock::new();
        let null_env = NULL_ENV.get_or_init(EnvStack::new);
        OperationContext::background(null_env, EXPANSION_LIMIT_DEFAULT)
    }

    // Return an operation context that contains only global variables, no parser, and never
    // cancels.
    pub fn globals() -> OperationContext<'static> {
        OperationContext::background(EnvStack::globals(), EXPANSION_LIMIT_DEFAULT)
    }

    /// Construct from a full set of properties.
    pub fn foreground(
        parser: &'a Parser,
        cancel_checker: CancelChecker,
        expansion_limit: usize,
    ) -> OperationContext<'a> {
        OperationContext {
            vars: Vars::Parser(parser),
            expansion_limit,
            job_group: None,
            cancel_checker,
        }
    }

    pub fn test_only_foreground(
        parser: &'a Parser,
        vars: &'a dyn Environment,
        cancel_checker: CancelChecker,
    ) -> OperationContext<'a> {
        OperationContext {
            vars: Vars::TestOnly(parser, vars),
            expansion_limit: EXPANSION_LIMIT_DEFAULT,
            job_group: None,
            cancel_checker,
        }
    }

    /// Construct from vars alone.
    pub fn background(vars: &'a dyn Environment, expansion_limit: usize) -> OperationContext<'a> {
        OperationContext {
            vars: Vars::Vars(vars),
            expansion_limit,
            job_group: None,
            cancel_checker: Box::new(no_cancel),
        }
    }

    pub fn background_with_cancel_checker(
        vars: &'a dyn Environment,
        cancel_checker: CancelChecker,
        expansion_limit: usize,
    ) -> OperationContext<'a> {
        OperationContext {
            vars: Vars::Vars(vars),
            expansion_limit,
            job_group: None,
            cancel_checker,
        }
    }

    pub fn has_parser(&self) -> bool {
        matches!(self.vars, Vars::Parser(_) | Vars::TestOnly(_, _))
    }
    pub fn maybe_parser(&self) -> Option<&Parser> {
        match &self.vars {
            Vars::Parser(parser) => Some(parser),
            Vars::Vars(_) => None,
            Vars::TestOnly(parser, _) => Some(parser),
        }
    }
    pub fn parser(&self) -> &Parser {
        match &self.vars {
            Vars::Parser(parser) => parser,
            Vars::Vars(_) => panic!(),
            Vars::TestOnly(parser, _) => parser,
        }
    }
    // Invoke the cancel checker. Return if we should cancel.
    pub fn check_cancel(&self) -> bool {
        (self.cancel_checker)()
    }
}

/// Return an operation context for a background operation..
/// Crucially the operation context itself does not contain a parser.
/// It is the caller's responsibility to ensure the environment lives as long as the result.
pub fn get_bg_context(env: &EnvDyn, generation_count: u32) -> OperationContext {
    let cancel_checker = move || {
        // Cancel if the generation count changed.
        generation_count != read_generation_count()
    };
    OperationContext::background_with_cancel_checker(
        env,
        Box::new(cancel_checker),
        EXPANSION_LIMIT_BACKGROUND,
    )
}
