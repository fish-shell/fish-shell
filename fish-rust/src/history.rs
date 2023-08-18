use crate::env::{EnvMode, EnvStack};
use crate::wchar::prelude::*;

/// Sets private mode on. Once in private mode, it cannot be turned off.
pub fn start_private_mode(vars: &EnvStack) {
    vars.set_one(L!("fish_history"), EnvMode::GLOBAL, "".into());
    vars.set_one(L!("fish_private_mode"), EnvMode::GLOBAL, "1".into());
}
