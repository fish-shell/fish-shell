use crate::common::CancelChecker;
use crate::env::{EnvDyn, EnvDynFFI};
use crate::env::{EnvNull, EnvStack, EnvStackRef, EnvStackRefFFI, Environment, EnvironmentRef};
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
    pub vars: Arc<dyn Environment>,

    // vars_ffi: EnvStackRefFFI,

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
        OperationContext::background(nullenv.clone(), EXPANSION_LIMIT_DEFAULT)
    }

    // \return an operation context that contains only global variables, no parser, and never
    // cancels.
    pub fn globals() -> OperationContext<'static> {
        OperationContext::background(EnvStack::globals().clone(), EXPANSION_LIMIT_DEFAULT)
    }

    /// Construct from a full set of properties.
    pub fn foreground(
        parser: ParserRef,
        cancel_checker: &'a CancelChecker,
        expansion_limit: usize,
    ) -> OperationContext<'a> {
        let vars = Arc::clone(&parser.variables);
        // let vars_ffi = EnvStackRefFFI(Arc::clone(&vars));
        OperationContext {
            parser: Some(parser),
            vars,
            // vars_ffi: todo!(),
            expansion_limit,
            job_group: None,
            cancel_checker,
        }
    }

    /// Construct from vars alone.
    pub fn background(vars: Arc<dyn Environment>, expansion_limit: usize) -> OperationContext<'a> {
        // let vars_ffi = EnvStackRefFFI(Arc::clone(&vars));
        OperationContext {
            parser: None,
            vars,
            // vars_ffi: todo!(),
            expansion_limit,
            job_group: None,
            cancel_checker: &no_cancel,
        }
    }

    pub fn background_with_cancel_checker(
        vars: &EnvStackRef,
        cancel_checker: &'a CancelChecker,
        expansion_limit: usize,
    ) -> OperationContext<'a> {
        let vars = Arc::clone(&vars);
        OperationContext {
            parser: None,
            vars,
            expansion_limit,
            job_group: None,
            cancel_checker,
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

/// \return an operation context for a background operation..
/// Crucially the operation context itself does not contain a parser.
/// It is the caller's responsibility to ensure the environment lives as long as the result.
pub fn get_bg_context(env: &EnvDyn, generation_count: u32) -> OperationContext<'static> {
    todo!()
    // const std::shared_ptr<environment_t> &env,
    //                                       uint32_t generation_count) {
    // cancel_checker_t cancel_checker = [generation_count] {
    //     // Cancel if the generation count changed.
    //     return generation_count != read_generation_count();
    // };
    // return operation_context_t{nullptr, *env, std::move(cancel_checker), kExpansionLimitBackground};
}

#[cxx::bridge]
mod operation_context_ffi {
    extern "C++" {
        include!("env_fwd.h");
        include!("parser.h");
        #[cxx_name = "EnvStackRef"]
        type EnvStackRefFFI = crate::env::EnvStackRefFFI;
        #[cxx_name = "EnvDyn"]
        type EnvDynFFI = crate::env::EnvDynFFI;
        type Parser = crate::parser::Parser;
    }
    extern "Rust" {
        type OperationContext<'a>;

        fn empty_operation_context() -> Box<OperationContext<'static>>;
        fn check_cancel(&self) -> bool;
        unsafe fn vars<'a, 'b>(&'b self) -> &'b EnvStackRefFFI;

        #[cxx_name = "get_bg_context"]
        fn get_bg_context_ffi(
            env: &EnvDynFFI,
            generation_count: u32,
        ) -> Box<OperationContext<'static>>;
        #[cxx_name = "parser_context"]
        fn parser_context_ffi(parser: &Parser) -> Box<OperationContext<'static>>;
    }
}

fn get_bg_context_ffi(env: &EnvDynFFI, generation_count: u32) -> Box<OperationContext<'static>> {
    Box::new(get_bg_context(&env.0, generation_count))
}

fn parser_context_ffi(parser: &Parser) -> Box<OperationContext<'static>> {
    Box::new(parser.context())
}

unsafe impl cxx::ExternType for OperationContext<'_> {
    type Id = cxx::type_id!("OperationContext");
    type Kind = cxx::kind::Opaque;
}

impl OperationContext<'_> {
    fn vars<'a, 'b>(&'b self) -> &'b EnvStackRefFFI {
        todo!()
        // &self.vars_ffi
    }
}

fn empty_operation_context() -> Box<OperationContext<'static>> {
    Box::new(OperationContext::empty())
}
