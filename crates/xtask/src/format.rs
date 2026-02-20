use clap::Args;
use std::{
    io::{ErrorKind, Write},
    path::{Path, PathBuf},
    process::{Command, Stdio},
};
use walkdir::WalkDir;

const GREEN: &str = "\x1b[32m";
const YELLOW: &str = "\x1b[33m";
const NORMAL: &str = "\x1b[0m";

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
            "{YELLOW}warning: No paths specified. Nothing to do. Use the \"--all\" flag to consider all eligible files.{NORMAL}"
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
                    if response != "y\n" {
                        println!("Exiting without formatting.");
                        return;
                    }
                }
            }
            Err(e) => {
                if e.kind() == ErrorKind::NotFound {
                    println!(
                        "{YELLOW}warning: Did not find git, will proceed without checking for unstaged changes.{NORMAL}"
                    )
                } else {
                    panic!("Faild to run git status:\n{e}")
                }
            }
        }
    }
    format_fish(&args);
    format_python(&args);
    format_rust(&args);
}

fn get_matching_files<P: AsRef<Path>, M: Fn(&Path) -> bool>(root: P, matcher: M) -> Vec<PathBuf> {
    let mut matching_file_paths = vec![];
    for entry in WalkDir::new(root) {
        let entry = entry.unwrap();
        if entry.file_type().is_file() && matcher(entry.path()) {
            matching_file_paths.push(entry.into_path());
        }
    }
    matching_file_paths
}
fn files_with_extension<P: AsRef<Path>, I: IntoIterator<Item = P>>(
    all_paths: I,
    extension: &str,
) -> Vec<PathBuf> {
    all_paths
        .into_iter()
        .flat_map(|p| WalkDir::new(p))
        .filter_map(|res| {
            let entry = res.unwrap();
            let path = entry.path();
            if entry.metadata().is_ok_and(|m| m.is_file())
                && entry.path().extension().is_some_and(|e| e == extension)
            {
                Some(path.to_owned())
            } else {
                None
            }
        })
        .collect::<Vec<_>>()
}

fn run_formatter(formatter: &mut Command, name: &str) {
    println!("=== Running {GREEN}{name}{NORMAL}");
    match formatter.status() {
        Ok(exit_status) => {
            if !exit_status.success() {
                panic!("{name:?}: Files are not formatted correctly.");
            }
        }
        Err(e) => {
            if e.kind() == std::io::ErrorKind::NotFound {
                eprintln!(
                    "{YELLOW}Formatter not found: {name:?}. Skipping associated files.{NORMAL}"
                );
            } else {
                panic!("Error occurred while running {name:?}:\n{e}")
            }
        }
    }
}

fn format_fish(args: &FormatArgs) {
    let mut fish_paths = files_with_extension(&args.paths, "fish");
    if args.all {
        let workspace_root = fish_build_helper::workspace_root();
        let fish_ignored_dir = workspace_root.join("tests/checks");
        let fish_matcher = |p: &Path| {
            !p.starts_with(&fish_ignored_dir)
                && p.extension().is_some_and(|extension| extension == "fish")
        };
        fish_paths.extend(get_matching_files(workspace_root, fish_matcher));
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
            rustup component add rustfmt{NORMAL}"
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
