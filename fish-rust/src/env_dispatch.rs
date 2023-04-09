use crate::env::{EnvStack, Environment};
use crate::wchar::wstr;

pub fn env_dispatch_init(vars: &dyn Environment) {
    todo!()
}

/// React to modifying the given variable.
pub fn env_dispatch_var_change(key: &wstr, vars: &mut EnvStack) {
    todo!()
}
