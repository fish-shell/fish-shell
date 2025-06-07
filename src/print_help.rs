//! Helper for executables (not builtins) to print a help message
//! Uses the fish in PATH, not necessarily the matching fish binary

use crate::wchar::prelude::*;

use std::ffi::{OsStr, OsString};
use std::process::Command;

const HELP_ERR: LocalizableString = localizable_string!("Could not show help message");

pub fn print_help(command: &str) {
    let mut cmdline = OsString::new();
    cmdline.push("__fish_print_help ");
    cmdline.push(command);

    match Command::new("fish")
        .args([OsStr::new("-c"), &cmdline])
        .spawn()
        .and_then(|mut c| c.wait())
    {
        Ok(status) if !status.success() => eprintf!("%s\n", wgettext!(HELP_ERR)),
        Err(e) => eprintf!("%s: %s\n", wgettext!(HELP_ERR), e),
        _ => (),
    }
}
