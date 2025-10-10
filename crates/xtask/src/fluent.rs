use std::{collections::HashMap, io::Read, path::PathBuf};

use clap::{Args, Subcommand};
use fish_build_helper::{default_ftl_file, ftl_dir};
use fluent_ftl_tools::{
    consistency::check_all_resource_files,
    format::{FormattingMode, format_text},
    missing::find_missing_message_ids_in_files,
    rename::rename_in_all_files,
    serialize_resources_to_files,
};

use crate::files_with_extension;

#[derive(Subcommand)]
pub enum FluentCommandArgs {
    /// Check consistency of FTL files.
    Check { files: Vec<PathBuf> },
    /// Format files. Without arguments, all FTL files are formatted.
    /// If '-' is specified, input is read from stdin and output written to stdout.
    Format { files: Vec<PathBuf> },
    /// Rename an ID or variables used with an ID.
    Rename(RenameArgs),
    /// Display message IDs which could be added to the specified file(s), or all files if no
    /// arguments are given.
    ShowMissing { files: Vec<PathBuf> },
}

#[derive(Args)]
pub struct RenameArgs {
    /// The message ID.
    /// Use 'old=new', where 'old' is the old message ID as currently present in the file,
    /// and 'new' the name it should be renamed to.
    /// If the ID should not change, use 'old'.
    id: String,
    /// The variables to rename for the specified ID.
    /// Use 'old=new', where 'old' is the old variable name as currently present in the file,
    /// and 'new' the name it should be renamed to.
    vars: Vec<String>,
}

pub fn fluent(args: FluentCommandArgs) {
    match args {
        FluentCommandArgs::Check { files } => check(files),
        FluentCommandArgs::Format { files } => format(files),
        FluentCommandArgs::Rename(rename_args) => rename(rename_args),
        FluentCommandArgs::ShowMissing { files } => show_missing(files),
    }
}

fn check(mut files: Vec<PathBuf>) {
    if files.is_empty() {
        files = files_with_extension([ftl_dir()], "ftl");
    }
    if let Err(e) = check_all_resource_files(default_ftl_file(), &files) {
        fail!("{e}");
    }
}

fn format(mut files: Vec<PathBuf>) {
    if files == [PathBuf::from("-")] {
        let mut input = String::new();
        std::io::stdin().read_to_string(&mut input).unwrap();
        match format_text(&input) {
            Ok(formatted_text) => print!("{formatted_text}"),
            Err(_) => {
                print!("{input}");
                std::process::exit(1);
            }
        }
        return;
    }

    if files.is_empty() {
        files = files_with_extension([ftl_dir()], "ftl");
    }
    let errors = files
        .iter()
        .filter_map(|path| {
            fluent_ftl_tools::format::format_path(path, FormattingMode::Rewrite).err()
        })
        .collect::<Vec<String>>()
        .join("\n");
    if !errors.is_empty() {
        fail!("{errors}");
    }
}

fn rename(args: RenameArgs) {
    let old_id;
    let new_id;
    match args.id.split_once('=') {
        Some((old, new)) => {
            old_id = old.into();
            new_id = Some(new.into());
        }
        None => {
            old_id = args.id;
            new_id = None;
        }
    }
    let mut variable_update = HashMap::new();
    for arg in args.vars {
        match arg.split_once('=') {
            Some((old, new)) => {
                variable_update.insert(old.into(), new.into());
            }
            None => {
                fail!("Argument '{arg}' must use the format 'old=new'")
            }
        }
    }
    let files = files_with_extension([ftl_dir()], "ftl");
    let resources =
        rename_in_all_files(&files, &old_id, &new_id, &variable_update).unwrap_or_else(|e| {
            fail!("Failed to perform renaming:\n{e}");
        });
    serialize_resources_to_files(&resources).unwrap_or_else(|e| {
        fail!("Failed to update files:\n{e}");
    });
}

fn show_missing(mut files: Vec<PathBuf>) {
    if files.is_empty() {
        files = files_with_extension([ftl_dir()], "ftl");
    }
    match find_missing_message_ids_in_files(default_ftl_file(), &files) {
        Ok(None) => {
            println!("No missing messages.");
        }
        Ok(Some(missing_message)) => {
            println!("{missing_message}");
        }
        Err(e) => {
            fail!("Error:\n{e}");
        }
    }
}
