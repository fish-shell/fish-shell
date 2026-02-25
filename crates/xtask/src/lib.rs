use std::{ffi::OsStr, process::Command};

macro_rules! fail {
    ($($arg:tt)+) => {{
        eprintln!($($arg)+);
        std::process::exit(1);
    }}
}

pub mod format;

pub trait CommandExt {
    fn run_or_fail(&mut self);
}

impl CommandExt for Command {
    fn run_or_fail(&mut self) {
        match self.status() {
            Ok(exit_status) => {
                if !exit_status.success() {
                    fail!("Command did not run successfully: {:?}", self.get_program())
                }
            }
            Err(err) => {
                fail!("Failed to run command: {err}")
            }
        }
    }
}

pub fn cargo<I, S>(cargo_args: I)
where
    I: IntoIterator<Item = S>,
    S: AsRef<OsStr>,
{
    Command::new(env!("CARGO")).args(cargo_args).run_or_fail();
}
