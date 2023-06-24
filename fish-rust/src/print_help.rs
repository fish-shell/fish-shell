//! Helper for executables (not builtins) to print a help message
//! Uses the fish in PATH, not necessarily the matching fish binary

use libc::c_char;
use std::ffi::{CStr, OsStr, OsString};
use std::process::Command;

const HELP_ERR: &str = "Could not show help message";

#[cxx::bridge]
mod ffi2 {
    extern "Rust" {
        unsafe fn unsafe_print_help(command: *const c_char);
    }
}

pub fn print_help(command: &str) {
    let mut cmdline = OsString::new();
    cmdline.push("__fish_print_help ");
    cmdline.push(command);

    Command::new("fish")
        .args([OsStr::new("-c"), &cmdline])
        .spawn()
        .expect(HELP_ERR);
}

unsafe fn unsafe_print_help(command_buf: *const c_char) {
    let command_cstr: &CStr = unsafe { CStr::from_ptr(command_buf) };
    let command = command_cstr.to_str().unwrap();
    print_help(command);
}
