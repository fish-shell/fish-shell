use clap::{Parser, Subcommand};
use std::process::Command;
use xtask::CommandExt;

#[derive(Parser)]
#[command(
    name = "cargo xtask",
    about = "Wrapper for running various utilities",
    arg_required_else_help(true)
)]
struct Cli {
    #[command(subcommand)]
    task: Task,
}

#[derive(Subcommand)]
enum Task {
    /// Run various checks on the repo.
    Check,
}

fn main() {
    let cli = Cli::parse();
    match cli.task {
        Task::Check => run_checks(),
    }
}

fn run_checks() {
    let repo_root_dir = fish_build_helper::workspace_root();
    let check_script = repo_root_dir.join("build_tools").join("check.sh");
    Command::new(check_script).run_or_panic();
}
