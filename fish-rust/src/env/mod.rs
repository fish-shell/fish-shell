mod env_ffi;
pub mod environment;
mod environment_impl;
pub mod var;

pub use env_ffi::EnvStackSetResult;
pub use environment::*;
pub use var::*;
