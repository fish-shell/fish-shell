use crate::env::{EnvStack, Environment};
use crate::wchar::prelude::*;
use crate::wchar_ffi::WCharToFFI;

pub fn update_wait_on_escape_ms(vars: &EnvStack) {
    let fish_escape_delay_ms = vars.get_unless_empty(L!("fish_escape_delay_ms"));
    let is_empty = fish_escape_delay_ms.is_none();
    let value = fish_escape_delay_ms
        .map(|s| s.as_string().to_ffi())
        .unwrap_or(L!("").to_ffi());
    crate::ffi::update_wait_on_escape_ms_ffi(is_empty, &value);
}

pub fn update_wait_on_sequence_key_ms(vars: &EnvStack) {
    let fish_sequence_key_delay_ms = vars.get_unless_empty(L!("fish_sequence_key_delay_ms"));
    let is_empty = fish_sequence_key_delay_ms.is_none();
    let value = fish_sequence_key_delay_ms
        .map(|s| s.as_string().to_ffi())
        .unwrap_or(L!("").to_ffi());
    crate::ffi::update_wait_on_sequence_key_ms_ffi(is_empty, &value);
}
