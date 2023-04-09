use crate::wchar::{wstr, L};

pub const FISH_BIND_MODE_VAR: &wstr = L!("fish_bind_mode");
pub const DEFAULT_BIND_MODE: &wstr = L!("default");

/// Set up arrays used by readch to detect escape sequences for special keys and perform related
/// initializations for our input subsystem.
pub fn init_input() {
    todo!()
}
