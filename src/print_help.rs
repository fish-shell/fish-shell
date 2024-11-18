//! Helper for executables (not builtins) to print a help message
//! Uses the fish in PATH, not necessarily the matching fish binary

use std::ffi::{OsStr, OsString};
use std::process::Command;

const HELP_ERR: &str = "Could not show help message";

pub fn print_help(command: &str) {
    let mut cmdline = OsString::new();
    cmdline.push("__fish_print_help ");
    cmdline.push(command);

    match Command::new("fish")
        .args([OsStr::new("-c"), &cmdline])
        .spawn()
        .and_then(|mut c| c.wait())
    {
        Ok(status) if !status.success() => eprintln!("{}", HELP_ERR),
        Err(e) => eprintln!("{}: {}", HELP_ERR, e),
        _ => (),
    }
}
