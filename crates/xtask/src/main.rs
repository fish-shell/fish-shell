use std::{ffi::OsStr, process::Command};

fn run_or_panic(command: &mut Command) {
    match command.status() {
        Ok(exit_status) => {
            if !exit_status.success() {
                panic!(
                    "Command did not run successfully: {:?}",
                    command.get_program()
                )
            }
        }
        Err(err) => {
            panic!("Failed to run command: {err}");
        }
    }
}

fn cargo<I, S>(cargo_args: I)
where
    I: IntoIterator<Item = S>,
    S: AsRef<OsStr>,
{
    run_or_panic(Command::new(env!("CARGO")).args(cargo_args));
}

fn main() {
    // Args passed to xtasks, not including the binary name which gets set automatically.
    let args = std::env::args().skip(1).collect::<Vec<_>>();
    if args.is_empty() {
        panic!("Called xtask without arguments. Doing nothing.");
    }

    let version = fish_version::get_version();
    std::env::set_var("FISH_BUILD_VERSION", version);

    let command = &args[0];
    let command_args = &args[1..];
    match command.as_str() {
        "cargo" | "c" => cargo(command_args),
        "check" => run_checks(command_args),
        "html-docs" => build_html_docs(command_args),
        "version" => cargo(["run", "--package", "fish-version"]),
        other => {
            panic!("Unknown xtask: {other}");
        }
    }
}

fn run_checks(args: &[String]) {
    if !args.is_empty() {
        panic!("Args passed to `check` when none were expected: {args:?}");
    }
    let repo_root_dir = fish_build_helper::get_repo_root();
    let check_script = repo_root_dir.join("build_tools").join("check.sh");
    run_or_panic(&mut Command::new(check_script));
}

fn build_html_docs(args: &[String]) {
    if !args.is_empty() {
        panic!("Args passed to `html-docs` when none were expected: {args:?}");
    }

    cargo([
        "build",
        "--bin",
        "fish_indent",
        "--profile",
        "dev",
        "--no-default-features",
    ]);

    let repo_root_dir = fish_build_helper::get_repo_root();
    let target_dir = fish_build_helper::get_target_dir();
    // Our binaries should be in `debug`.
    let binary_dir = target_dir.join("debug");
    // Set path so `sphinx-build` can find `fish_indent`.
    std::env::set_var(
        "PATH",
        format!("{}:{}", binary_dir.to_str().unwrap(), env!("PATH")),
    );
    let doc_src_dir = repo_root_dir.join("doc_src");
    let html_dir = target_dir.join("fish-html");
    std::fs::create_dir_all(&html_dir).unwrap();
    run_or_panic(Command::new("sphinx-build").args([
        "-j",
        "auto",
        "-q",
        "-b",
        "html",
        "-c",
        doc_src_dir.to_str().unwrap(),
        doc_src_dir.to_str().unwrap(),
        html_dir.to_str().unwrap(),
    ]))
}
