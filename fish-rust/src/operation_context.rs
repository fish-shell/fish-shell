use crate::common::CancelChecker;
use crate::env::{EnvStack, Environment, NullEnvironment};
use crate::parser::Parser;
use crate::proc::JobGroupRef;
use std::rc::Rc;
use std::sync::RwLock;

/// A common helper which always returns false.
pub fn no_cancel() -> bool {
    false
}

// Default limits for expansion.
/// The default maximum number of items from expansion.
pub const EXPANSION_LIMIT_DEFAULT: usize = 512 * 1024;
/// A smaller limit for background operations like syntax highlighting.
pub const EXPANSION_LIMIT_BACKGROUND: usize = 512;

/// A operation_context_t is a simple property bag which wraps up data needed for highlighting,
/// expansion, completion, and more.
pub struct OperationContext<'a> {
    // The parser, if this is a foreground operation. If this is a background operation, this may be
    // nullptr.
    pub parser: Option<Rc<RwLock<Parser>>>,

    // The set of variables. It is the creator's responsibility to ensure this lives as log as the
    // context itself.
    pub vars: &'a dyn Environment,

    // The limit in the number of expansions which should be produced.
    pub expansion_limit: usize,

    /// The job group of the parental job.
    /// This is used only when expanding command substitutions. If this is set, any jobs crated by
    /// the command substitutions should use this tree.
    pub job_group: Option<JobGroupRef>,

    // A function which may be used to poll for cancellation.
    pub cancel_checker: &'a CancelChecker,
}

static nullenv: NullEnvironment = NullEnvironment {};

impl<'a> OperationContext<'a> {
    // Invoke the cancel checker. \return if we should cancel.
    pub fn check_cancel(&self) -> bool {
        (self.cancel_checker)()
    }

    // \return an "empty" context which contains no variables, no parser, and never cancels.
    pub fn empty() -> OperationContext<'static> {
        OperationContext::new(&nullenv, EXPANSION_LIMIT_DEFAULT)
    }

    // \return an operation context that contains only global variables, no parser, and never
    // cancels.
    pub fn globals() -> OperationContext<'static> {
        OperationContext::new(EnvStack::globals(), EXPANSION_LIMIT_DEFAULT)
    }

    /// Construct from a full set of properties.
    pub fn with_cancel_checker(
        parser: Rc<RwLock<Parser>>,
        vars: &'a dyn Environment,
        cancel_checker: &'a CancelChecker,
        expansion_limit: usize,
    ) -> OperationContext<'a> {
        OperationContext {
            parser: Some(parser),
            vars,
            expansion_limit,
            job_group: None,
            cancel_checker,
        }
    }

    /// Construct from vars alone.
    pub fn new(vars: &'a dyn Environment, expansion_limit: usize) -> OperationContext<'a> {
        OperationContext {
            parser: None,
            vars,
            expansion_limit,
            job_group: None,
            cancel_checker: &no_cancel,
        }
    }
}
