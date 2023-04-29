use crate::common::CancelChecker;
use crate::env::{EnvNull, EnvStack, Environment, EnvironmentRef};
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

pub enum Context {
    Foreground(ParserRef),
    Background(EnvironmentRef),
}

/// A operation_context_t is a simple property bag which wraps up data needed for highlighting,
/// expansion, completion, and more.
pub struct OperationContext<'a> {
    pub context: Context,

    // The limit in the number of expansions which should be produced.
    pub expansion_limit: usize,

    /// The job group of the parental job.
    /// This is used only when expanding command substitutions. If this is set, any jobs created
    /// by the command substitutions should use this tree.
    pub job_group: Option<JobGroupRef>,

    // A function which may be used to poll for cancellation.
    pub cancel_checker: &'a CancelChecker,
}

static nullenv: Lazy<Arc<EnvNull>> = Lazy::new(|| Arc::new(EnvNull {}));

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
        OperationContext {
            context: Context::Foreground(parser),
            expansion_limit,
            job_group: None,
            cancel_checker,
        }
    }

    /// Construct from vars alone.
    pub fn new(vars: EnvironmentRef, expansion_limit: usize) -> OperationContext<'a> {
        OperationContext {
            context: Context::Background(vars),
            expansion_limit,
            job_group: None,
            cancel_checker: &no_cancel,
        }
    }

    pub fn with_vars<T>(&self, cb: impl FnOnce(&dyn Environment) -> T) -> T {
        match &self.context {
            Context::Foreground(parser) => {
                let parser = parser.read().unwrap();
                cb(parser.vars())
            }
            Context::Background(vars) => cb(&**vars),
        }
    }
    pub fn has_parser(&self) -> bool {
        matches!(self.context, Context::Foreground(_))
    }
    pub fn maybe_parser(&self) -> Option<RwLockReadGuard<'_, Parser>> {
        match &self.context {
            Context::Foreground(parser) => Some(parser.read().unwrap()),
            _ => None,
        }
    }
    pub fn maybe_parser_mut(&self) -> Option<RwLockWriteGuard<'_, Parser>> {
        match &self.context {
            Context::Foreground(parser) => Some(parser.write().unwrap()),
            _ => None,
        }
    }
    pub fn parser(&self) -> RwLockReadGuard<'_, Parser> {
        match &self.context {
            Context::Foreground(parser) => parser.read().unwrap(),
            _ => panic!(),
        }
    }
    pub fn parser_mut(&self) -> RwLockWriteGuard<'_, Parser> {
        match &self.context {
            Context::Foreground(parser) => parser.write().unwrap(),
            _ => panic!(),
        }
    }

    // Invoke the cancel checker. \return if we should cancel.
    pub fn check_cancel(&self) -> bool {
        (self.cancel_checker)()
    }
}

#[cxx::bridge]
mod operation_context_ffi {
    extern "Rust" {
        type OperationContext<'a>;
    }
}

unsafe impl cxx::ExternType for OperationContext<'_> {
    type Id = cxx::type_id!("OperationContext");
    type Kind = cxx::kind::Opaque;
}
