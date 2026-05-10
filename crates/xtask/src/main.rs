use anyhow::{Context, Result};
use clap::{CommandFactory, Parser, Subcommand};
use clap_complete::CompleteEnv;
use fish_build_helper::as_os_strs;
use std::{path::PathBuf, process::Command};
use xtask::{CommandExt, cargo, format::FormatArgs, gettext::GettextArgs, shellcheck::shellcheck};

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
    /// Format files or check if they are correctly formatted.
    Format(FormatArgs),
    /// Work on the gettext PO files.
    Gettext(GettextArgs),
    /// Build HTML docs
    HtmlDocs {
        /// Path to a fish_indent executable. If none is specified, fish_indent will be built.
        #[arg(long)]
        fish_indent: Option<PathBuf>,
    },
    /// Build man pages
    ManPages,
    /// Run ShellCheck on non-fish shell scripts
    #[command(name = "shellcheck")]
    ShellCheck,
}

/// Only used to enable completion generation.
/// [`clap_complete`] is not built to account for the situation we have here, where the CLI does not
/// correspond to a top-level shell command.
/// We work around this here by pretending that we are building a CLI for the `cargo` command, which
/// only has the single subcommand `xtask`.
/// These completions can then be combined with the regular cargo completions.
#[derive(Parser)]
#[command(name = "cargo")]
struct FakeCargoWrapperForCompletion {
    #[command(subcommand)]
    xtask: FakeCliForCompletion,
}

#[derive(Subcommand)]
enum FakeCliForCompletion {
    /// Run fish's xtasks
    #[command(subcommand)]
    Xtask(Task),
}

fn main() -> Result<()> {
    CompleteEnv::with_factory(FakeCargoWrapperForCompletion::command).complete();

    let cli = Cli::parse();
    match cli.task {
        Task::Check => run_checks(),
        Task::Format(format_args) => xtask::format::format(format_args),
        Task::Gettext(gettext_args) => xtask::gettext::gettext(gettext_args),
        Task::HtmlDocs { fish_indent } => build_html_docs(fish_indent),
        Task::ManPages => cargo(["build", "--package", "fish-build-man-pages"]),
        Task::ShellCheck => shellcheck(),
    }
}

fn run_checks() -> Result<()> {
    let repo_root_dir = fish_build_helper::workspace_root();
    let check_script = repo_root_dir.join("build_tools").join("check.sh");
    Command::new(check_script).run()
}

fn build_html_docs(fish_indent: Option<PathBuf>) -> Result<()> {
    let fish_indent_path = match fish_indent {
        Some(path) => path,
        None => {
            // Build fish_indent if no existing one is specified.
            cargo([
                "build",
                "--bin",
                "fish_indent",
                "--profile",
                "dev",
                "--no-default-features",
            ])?;
            fish_build_helper::fish_build_dir()
                .join("debug")
                .join("fish_indent")
        }
    };
    // Set path so `sphinx-build` can find `fish_indent`.
    // Create tempdir to store symlink to fish_indent.
    // This is done to avoid adding other binaries to the PATH.
    let tempdir = fish_tempfile::new_dir().context("Failed to create tempdir")?;
    std::os::unix::fs::symlink(
        std::fs::canonicalize(&fish_indent_path).with_context(|| {
            format!("Failed to canonicalize path to `fish_indent`: {fish_indent_path:?}")
        })?,
        tempdir.path().join("fish_indent"),
    )
    .context("Failed to create symlink for fish_indent")?;
    let mut new_path = tempdir.path().as_os_str().to_owned();
    if let Some(current_path) = std::env::var_os("PATH") {
        new_path.push(":");
        new_path.push(current_path);
    }
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
        .run()
}
