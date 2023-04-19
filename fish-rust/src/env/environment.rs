#![allow(unused_variables)]
//! Prototypes for functions for manipulating fish script variables.

use crate::env::{EnvMode, EnvVar};
use crate::wchar::{wstr, WString};

// Limit `read` to 100 MiB (bytes not wide chars) by default. This can be overridden by the
// fish_read_limit variable.
const DEFAULT_READ_BYTE_LIMIT: usize = 100 * 1024 * 1024;
pub static mut read_byte_limit: usize = DEFAULT_READ_BYTE_LIMIT;
pub static mut curses_initialized: bool = true;

pub trait Environment {
    fn get(&self, name: &wstr) -> Option<EnvVar> {
        todo!()
    }
    fn getf(&self, name: &wstr, mode: EnvMode) -> Option<EnvVar> {
        todo!()
    }
    fn get_unless_empty(&self, name: &wstr) -> Option<EnvVar> {
        todo!()
    }
    fn getf_unless_empty(&self, name: &wstr, mode: EnvMode) -> Option<EnvVar> {
        todo!()
    }
}

pub enum EnvStackSetResult {
    ENV_OK,
}

pub struct EnvStack {}
impl Environment for EnvStack {}
impl EnvStack {
    pub fn set_one(&self, key: &wstr, mode: EnvMode, val: WString) -> EnvStackSetResult {
        todo!()
    }
}

impl EnvStack {
    pub fn globals() -> &'static dyn Environment {
        todo!()
    }
}
