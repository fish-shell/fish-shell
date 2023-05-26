use crate::env::Environment;
use crate::wchar::L;

#[repr(u8)]
pub enum CursorSelectionMode {
    Exclusive = 0,
    Inclusive = 1,
}

pub fn check_autosuggestion_enabled(vars: &dyn Environment) -> bool {
    vars.get(L!("fish_autosuggestion_enabled"))
        .map(|v| v.as_string())
        .map(|v| v != L!("0"))
        .unwrap_or(true)
}
