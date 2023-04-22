use crate::env::{EnvStack, Environment};
use crate::wchar::wstr;
use std::sync::atomic::AtomicUsize;

// Limit `read` to 100 MiB (bytes not wide chars) by default. This can be overridden by the
// fish_read_limit variable.
const DEFAULT_READ_BYTE_LIMIT: usize = 100 * 1024 * 1024;
pub static READ_BYTE_LIMIT: AtomicUsize = AtomicUsize::new(DEFAULT_READ_BYTE_LIMIT);

pub fn env_dispatch_init(vars: &dyn Environment) {
    todo!()
}

/// React to modifying the given variable.
pub fn env_dispatch_var_change(key: &wstr, vars: &mut EnvStack) {
    todo!()
}

pub fn get_use_posix_spawn() -> bool {
    todo!()
}
