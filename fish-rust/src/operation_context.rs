use crate::common::CancelChecker;
use crate::env::{EnvNull, EnvStack, EnvStackRef, Environment, EnvironmentRef, EnvironmentRefFFI};
use crate::parser::{Parser, ParserRef};
use crate::proc::JobGroupRef;
use once_cell::sync::Lazy;
use std::rc::Rc;
use std::sync::{Arc, RwLock, RwLockReadGuard, RwLockWriteGuard};

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
    pub parser: Option<ParserRef>,

    // The set of variables.
    // todo! This should be an Arc<dyn Environment> but I'm not sure how to convert from
    // Arc<EnvStackRef>.
    pub vars: EnvStackRef,

    // The limit in the number of expansions which should be produced.
    pub expansion_limit: usize,

    /// The job group of the parental job.
    /// This is used only when expanding command substitutions. If this is set, any jobs created
    /// by the command substitutions should use this tree.
    pub job_group: Option<JobGroupRef>,

    // A function which may be used to poll for cancellation.
    pub cancel_checker: &'a CancelChecker,
}

// todo!
// static nullenv: Lazy<Arc<EnvNull>> = Lazy::new(|| Arc::new(EnvNull {}));
static nullenv: Lazy<EnvStackRef> = Lazy::new(|| Arc::new(EnvStack::new()));

impl<'a> OperationContext<'a> {
    // \return an "empty" context which contains no variables, no parser, and never cancels.
    pub fn empty() -> OperationContext<'static> {
        OperationContext::new(nullenv.clone(), EXPANSION_LIMIT_DEFAULT)
    }

    // \return an operation context that contains only global variables, no parser, and never
    // cancels.
    pub fn globals() -> OperationContext<'static> {
        OperationContext::new(EnvStack::globals().clone(), EXPANSION_LIMIT_DEFAULT)
    }

    /// Construct from a full set of properties.
    pub fn foreground(
        parser: ParserRef,
        cancel_checker: &'a CancelChecker,
        expansion_limit: usize,
    ) -> OperationContext<'a> {
        let vars = Arc::clone(&parser.variables);
        OperationContext {
            parser: Some(parser),
            vars,
            expansion_limit,
            job_group: None,
            cancel_checker,
        }
    }

    /// Construct from vars alone.
    pub fn new(vars: EnvStackRef, expansion_limit: usize) -> OperationContext<'a> {
        OperationContext {
            parser: None,
            vars,
            expansion_limit,
            job_group: None,
            cancel_checker: &no_cancel,
        }
    }

    pub fn has_parser(&self) -> bool {
        self.parser.is_some()
    }
    pub fn parser(&self) -> &Parser {
        self.parser.as_ref().unwrap()
    }
    // Invoke the cancel checker. \return if we should cancel.
    pub fn check_cancel(&self) -> bool {
        (self.cancel_checker)()
    }
}

#[cxx::bridge]
mod operation_context_ffi {
    extern "C++" {
        include!("env.h");
        #[cxx_name = "EnvironmentRef"]
        type EnvironmentRefFFI = crate::env::EnvironmentRefFFI;
    }
    extern "Rust" {
        type OperationContext<'a>;

        fn check_cancel(&self) -> bool;
        // fn vars(&self) -> &EnvironmentRefFFI;
    }
}

unsafe impl cxx::ExternType for OperationContext<'_> {
    type Id = cxx::type_id!("OperationContext");
    type Kind = cxx::kind::Opaque;
}

impl OperationContext<'_> {
    fn vars(&self) -> &EnvironmentRefFFI {
        todo!()
        // Box::new(EnvironmentRefFFI(Arc::clone(&self.vars)))
    }
}
