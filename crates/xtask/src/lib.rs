use std::process::Command;

pub trait CommandExt {
    fn run_or_panic(&mut self);
}

impl CommandExt for Command {
    fn run_or_panic(&mut self) {
        match self.status() {
            Ok(exit_status) => {
                if !exit_status.success() {
                    panic!("Command did not run successfully: {:?}", self.get_program())
                }
            }
            Err(err) => {
                panic!("Failed to run command: {err}");
            }
        }
    }
}
