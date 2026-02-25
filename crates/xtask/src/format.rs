use anstyle::{AnsiColor, Style};
use clap::Args;
use std::{
    io::{ErrorKind, Write},
    path::{Path, PathBuf},
    process::{Command, Stdio},
};
use walkdir::WalkDir;

const GREEN: Style = AnsiColor::Green.on_default();
const YELLOW: Style = AnsiColor::Yellow.on_default();

#[derive(Args)]
pub struct FormatArgs {
    /// Consider all eligible files.
    #[arg(long)]
    all: bool,
    /// Report files which are not formatted as expected, without modifying any files.
    #[arg(long)]
    check: bool,
    /// Format files even if uncommitted changes are detected.
    #[arg(long)]
    force: bool,
    paths: Vec<PathBuf>,
}

pub fn format(args: FormatArgs) {
    if !args.all && args.paths.is_empty() {
        println!(
            "{YELLOW}warning: No paths specified. Nothing to do. Use the \"--all\" flag to consider all eligible files.{YELLOW:#}"
        );
        return;
    }
    if !args.force && !args.check {
        match Command::new("git")
            .args(["status", "--porcelain", "--short", "--untracked-files=all"])
            .output()
        {
            Ok(output) => {
                if !output.stdout.is_empty() {
                    std::io::stdout().write_all(&output.stdout).unwrap();
                    print!(
                        "You have uncommitted changes (listed above). Are you sure you want to format? (y/N): "
                    );
                    std::io::stdout().flush().unwrap();
                    let mut response = String::new();
                    std::io::stdin().read_line(&mut response).unwrap();
                    if response.trim_end() != "y" {
                        println!("Exiting without formatting.");
                        return;
                    }
                }
            }
            Err(e) => {
                if e.kind() == ErrorKind::NotFound {
                    println!(
                        "{YELLOW}warning: Did not find git, will proceed without checking for unstaged changes.{YELLOW:#}"
                    )
                } else {
                    fail!("Failed to run git status:\n{e}")
                }
            }
        }
    }
    format_fish(&args);
    format_python(&args);
    format_rust(&args);
}

fn get_matching_files<P: AsRef<Path>, I: IntoIterator<Item = P>, M: Fn(&Path) -> bool>(
    all_paths: I,
    matcher: M,
) -> Vec<PathBuf> {
    all_paths
        .into_iter()
        .flat_map(WalkDir::new)
        .filter_map(|res| {
            let entry = res.unwrap();
            let path = entry.path();
            if entry.file_type().is_file() && matcher(path) {
                Some(path.to_owned())
            } else {
                None
            }
        })
        .collect()
}
fn files_with_extension<P: AsRef<Path>, I: IntoIterator<Item = P>>(
    all_paths: I,
    extension: &str,
) -> Vec<PathBuf> {
    let matcher = |p: &Path| p.extension().is_some_and(|e| e == extension);
    get_matching_files(all_paths, matcher)
}

fn run_formatter(formatter: &mut Command, name: &str) {
    println!("=== Running {GREEN}{name}{GREEN:#}");
    match formatter.status() {
        Ok(exit_status) => {
            if !exit_status.success() {
                fail!("{name:?}: Files are not formatted correctly.");
            }
        }
        Err(e) => {
            if e.kind() == std::io::ErrorKind::NotFound {
                eprintln!(
                    "{YELLOW}Formatter not found: {name:?}. Skipping associated files.{YELLOW:#}"
                );
            } else {
                fail!("Error occurred while running {name:?}:\n{e}")
            }
        }
    }
}

fn format_fish(args: &FormatArgs) {
    let mut fish_paths = files_with_extension(&args.paths, "fish");
    if args.all {
        let workspace_root = fish_build_helper::workspace_root();
        let fish_formatting_dirs = ["benchmarks", "build_tools", "etc", "share"];
        fish_paths.extend(files_with_extension(
            fish_formatting_dirs
                .iter()
                .map(|dir_name| workspace_root.join(dir_name)),
            "fish",
        ));
    };
    if fish_paths.is_empty() {
        return;
    }
    // TODO: make `fish_indent` available as a Rust library function, to avoid needing a
    // `fish_indent` binary in `$PATH`.
    let mut formatter = Command::new("fish_indent");
    if args.check {
        formatter.arg("--check");
    } else {
        formatter.arg("-w");
    }
    formatter.arg("--");
    formatter.args(fish_paths);
    run_formatter(&mut formatter, "fish_indent");
}

fn format_python(args: &FormatArgs) {
    let mut formatter = Command::new("ruff");
    formatter.arg("format");
    if args.check {
        formatter.arg("--check");
    }
    let mut python_files = files_with_extension(&args.paths, "py");

    if args.all {
        python_files.push(fish_build_helper::workspace_root().to_owned());
    };
    if python_files.is_empty() {
        return;
    }
    formatter.args(python_files);
    run_formatter(&mut formatter, "ruff format");
}

fn format_rust(args: &FormatArgs) {
    let rustfmt_status = Command::new("cargo")
        .arg("fmt")
        .arg("--version")
        .stdout(Stdio::null())
        .status()
        .unwrap();
    if !rustfmt_status.success() {
        eprintln!(
            "{YELLOW}Please install \"rustfmt\" to format Rust, e.g. via:\n\
            rustup component add rustfmt{YELLOW:#}"
        );
        return;
    }
    if args.all {
        let mut formatter = Command::new("cargo");
        formatter.arg("fmt");
        formatter.arg("--all");
        if args.check {
            formatter.arg("--check");
        }
        run_formatter(&mut formatter, "cargo fmt");
    }
    let rust_files = files_with_extension(&args.paths, "rs");
    if !rust_files.is_empty() {
        let mut formatter = Command::new("rustfmt");
        if args.check {
            formatter.arg("--check");
            formatter.arg("--files-with-diff");
        }
        formatter.args(rust_files);
        run_formatter(&mut formatter, "rustfmt");
    }
}
