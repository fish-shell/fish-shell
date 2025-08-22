use clap::{Parser, Subcommand};
use fish_build_helper::as_os_strs;
use std::{path::PathBuf, process::Command};
use xtask::{CommandExt, cargo};

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
    /// Build HTML docs
    HtmlDocs {
        /// Path to a fish_indent executable. If none is specified, fish_indent will be built.
        #[arg(long)]
        fish_indent: Option<PathBuf>,
    },
    /// Build man pages
    ManPages,
}

fn main() {
    let cli = Cli::parse();
    match cli.task {
        Task::Check => run_checks(),
        Task::HtmlDocs { fish_indent } => build_html_docs(fish_indent),
        Task::ManPages => cargo(["build", "--package", "fish-build-man-pages"]),
    }
}

fn run_checks() {
    let repo_root_dir = fish_build_helper::workspace_root();
    let check_script = repo_root_dir.join("build_tools").join("check.sh");
    Command::new(check_script).run_or_panic();
}

fn build_html_docs(fish_indent: Option<PathBuf>) {
    let fish_indent_path = fish_indent.unwrap_or_else(|| {
        // Build fish_indent if no existing one is specified.
        cargo([
            "build",
            "--bin",
            "fish_indent",
            "--profile",
            "dev",
            "--no-default-features",
        ]);
        fish_build_helper::fish_build_dir()
            .join("debug")
            .join("fish_indent")
    });
    // Set path so `sphinx-build` can find `fish_indent`.
    // Create tempdir to store symlink to fish_indent.
    // This is done to avoid adding other binaries to the PATH.
    let tempdir = fish_tempfile::new_dir().unwrap();
    std::os::unix::fs::symlink(
        std::fs::canonicalize(fish_indent_path).unwrap(),
        tempdir.path().join("fish_indent"),
    )
    .unwrap();
    let new_path = format!(
        "{}:{}",
        tempdir.path().to_str().unwrap(),
        fish_build_helper::env_var("PATH").unwrap()
    );
    let doc_src_dir = fish_build_helper::workspace_root().join("doc_src");
    let doctrees_dir = fish_build_helper::fish_doc_dir().join(".doctrees-html");
    let html_dir = fish_build_helper::fish_doc_dir().join("html");
    let args = as_os_strs![
        "-j",
        "auto",
        "-q",
        "-b",
        "html",
        "-c",
        &doc_src_dir,
        "-d",
        &doctrees_dir,
        &doc_src_dir,
        &html_dir,
    ];
    Command::new(option_env!("FISH_SPHINX").unwrap_or("sphinx-build"))
        .env("PATH", new_path)
        .args(args)
        .run_or_panic();
}
