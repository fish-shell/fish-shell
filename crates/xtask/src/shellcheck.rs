use fish_build_helper::workspace_root;
use fish_thread::SingleThreadedLazyCell;
use ignore::Walk;
use pcre2::bytes::Regex;
use std::{
    fs::File,
    io::{BufRead, BufReader},
    path::{Path, PathBuf},
    process::Command,
};

pub fn shellcheck() {
    let file_paths = files_to_check();
    match Command::new("shellcheck")
        .args(file_paths)
        .current_dir(workspace_root())
        .status()
    {
        Ok(status) => {
            std::process::exit(status.code().unwrap_or(1));
        }
        Err(e) => {
            eprintln!("Failed to run shellcheck: {e}");
            std::process::exit(1);
        }
    }
}

fn is_shell_script<P: AsRef<Path>>(path: P) -> bool {
    let file = File::open(&path).unwrap();
    let mut first_line = String::new();
    let Ok(_) = BufReader::new(file).read_line(&mut first_line) else {
        return false;
    };
    static SHEBANG_REGEX: SingleThreadedLazyCell<Regex> =
        SingleThreadedLazyCell::new(|| Regex::new("^#!.*[^i]sh").unwrap());
    SHEBANG_REGEX
        .is_match(first_line.trim().as_bytes())
        .unwrap()
}

fn files_to_check() -> Vec<PathBuf> {
    Walk::new(workspace_root())
        .map(|path| path.unwrap_or_else(|e| fail!("Error traversing workspace: {e}")))
        .filter(|path| path.file_type().unwrap().is_file())
        .map(|path| path.into_path())
        .filter(|path| is_shell_script(path))
        .collect()
}
