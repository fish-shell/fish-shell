use std::{
    ffi::OsStr,
    io::Write,
    path::{Path, PathBuf},
    process::{Command, Stdio},
};

use anyhow::{Context, Result, bail};
use walkdir::WalkDir;

pub mod format;
pub mod gettext;
pub mod shellcheck;

pub trait CommandExt {
    fn run(&mut self) -> Result<()>;
    fn run_with_stdio(&mut self, stdin: Vec<u8>) -> Result<Vec<u8>>;
}

impl CommandExt for Command {
    fn run(&mut self) -> Result<()> {
        if !self
            .status()
            .with_context(|| format!("Failed to run {:?}", self.get_program()))?
            .success()
        {
            bail!("Command did not run successfully: {:?}", self.get_program())
        }
        Ok(())
    }

    fn run_with_stdio(&mut self, stdin: Vec<u8>) -> Result<Vec<u8>> {
        let command_name = self.get_program().to_owned();
        let mut child = self
            .stdin(Stdio::piped())
            .stdout(Stdio::piped())
            .spawn()
            .with_context(|| format!("Failed to run {command_name:?}"))?;
        child
            .stdin
            .take()
            .unwrap()
            .write_all(&stdin)
            .with_context(|| format!("Failed to write to stdin of {command_name:?}"))?;
        let command_output = child
            .wait_with_output()
            .with_context(|| format!("Failed to read stdout of {command_name:?}"))?;
        if !command_output.status.success() {
            bail!("{command_name:?} failed");
        }
        Ok(command_output.stdout)
    }
}

pub const CARGO: &str = env!("CARGO");

pub fn cargo<I, S>(cargo_args: I) -> Result<()>
where
    I: IntoIterator<Item = S>,
    S: AsRef<OsStr>,
{
    Command::new(CARGO).args(cargo_args).run()
}

fn get_matching_files<P: AsRef<Path>, I: IntoIterator<Item = P>, M: Fn(&Path) -> bool>(
    all_paths: I,
    matcher: M,
) -> Result<Vec<PathBuf>> {
    let mut matching_files = vec![];
    for path in all_paths {
        for dir_entry in WalkDir::new(path.as_ref()) {
            let dir_entry = dir_entry
                .with_context(|| format!("Failed to check paths at {:?}", path.as_ref()))?;
            let path = dir_entry.path();
            if dir_entry.file_type().is_file() && matcher(path) {
                matching_files.push(path.to_owned());
            }
        }
    }
    Ok(matching_files)
}

fn files_with_extension<P: AsRef<Path>, I: IntoIterator<Item = P>>(
    all_paths: I,
    extension: &str,
) -> Result<Vec<PathBuf>> {
    let matcher = |p: &Path| p.extension().is_some_and(|e| e == extension);
    get_matching_files(all_paths, matcher)
}

fn copy_file<P: AsRef<Path>, Q: AsRef<Path>>(from: P, to: Q) -> Result<()> {
    std::fs::copy(&from, &to)
        .with_context(|| {
            format!(
                "Failed to copy from {:?} to {:?}",
                from.as_ref(),
                to.as_ref()
            )
        })
        .map(|_| ())
}
