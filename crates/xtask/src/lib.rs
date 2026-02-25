use std::{
    ffi::OsStr,
    path::{Path, PathBuf},
    process::Command,
};

use walkdir::WalkDir;

pub mod format;

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

pub fn cargo<I, S>(cargo_args: I)
where
    I: IntoIterator<Item = S>,
    S: AsRef<OsStr>,
{
    Command::new(env!("CARGO")).args(cargo_args).run_or_panic();
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
