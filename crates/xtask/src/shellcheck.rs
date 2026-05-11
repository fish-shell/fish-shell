use anyhow::{Context, Result};
use fish_build_helper::workspace_root;
use ignore::Walk;
use pcre2::bytes::Regex;
use std::{
    fs::File,
    io::{BufRead, BufReader},
    path::{Path, PathBuf},
    process::Command,
    sync::LazyLock,
};

use crate::CommandExt;

pub fn shellcheck() -> Result<()> {
    Command::new("shellcheck")
        .args(files_to_check()?)
        .current_dir(workspace_root())
        .run()
}

fn is_shell_script<P: AsRef<Path>>(path: P) -> Result<bool> {
    let file = File::open(&path).with_context(|| format!("Failed to open {:?}", path.as_ref()))?;
    let mut first_line = String::new();
    let Ok(_) = BufReader::new(file).read_line(&mut first_line) else {
        return Ok(false);
    };
    static SHEBANG_REGEX: LazyLock<Regex> = LazyLock::new(|| Regex::new("^#!.*[^i]sh").unwrap());
    Ok(SHEBANG_REGEX
        .is_match(first_line.trim().as_bytes())
        .unwrap())
}

fn files_to_check() -> Result<Vec<PathBuf>> {
    let mut files = vec![];
    for dir_entry in Walk::new(workspace_root()) {
        let dir_entry = dir_entry.context("Error traversing workspace")?;
        if !dir_entry
            .file_type()
            .with_context(|| format!("Failed to determine file type of {dir_entry:?}"))?
            .is_file()
        {
            continue;
        }
        let path = dir_entry.into_path();
        if !is_shell_script(&path)? {
            continue;
        }
        files.push(path);
    }
    Ok(files)
}
