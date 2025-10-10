use std::{
    collections::HashSet, fmt::Write as _, fs::File, io::Read as _, path::Path, process::Command,
};

use fish_build_helper::default_ftl_file;

use fluent::FluentResource;

fn main() {
    let source_ids = extract_fluent_ids();
    let fallback_file = default_ftl_file();
    let file_content = std::fs::read_to_string(&fallback_file).unwrap();
    let fallback_resource = FluentResource::try_new(file_content).unwrap_or_else(|(_, errors)| {
        panic!(
            "Failed to parse {:?} as Fluent resource:\n{errors:?}",
            fallback_file
        )
    });
    let fallback_resource_ids: HashSet<_> = fallback_resource
        .entries()
        .filter_map(|entry| {
            if let fluent_syntax::ast::Entry::Message(message) = entry {
                Some(message.id.name.to_owned())
            } else {
                None
            }
        })
        .collect();
    if source_ids != fallback_resource_ids {
        let mut error_string = String::new();
        let mut missing_in_file = source_ids.difference(&fallback_resource_ids).peekable();
        if missing_in_file.peek().is_some() {
            writeln!(
                error_string,
                "These message IDs are missing from {fallback_file:?}:"
            )
            .unwrap();
            for id in missing_in_file {
                error_string.push_str(id);
                error_string.push('\n');
            }
            error_string.push('\n');
        }
        let mut extra_in_file = fallback_resource_ids.difference(&source_ids).peekable();
        if extra_in_file.peek().is_some() {
            writeln!(
                error_string,
                "These message IDs are present in {fallback_file:?} but shouldn't be:"
            )
            .unwrap();
            for id in extra_in_file {
                error_string.push_str(id);
                error_string.push('\n');
            }
        }
        panic!("{error_string}")
    }
}

fn concat_files_in_dir<P: AsRef<Path>>(dir: P) -> std::io::Result<String> {
    let mut concatenated_content = String::new();
    for dir_entry in std::fs::read_dir(dir)? {
        let dir_entry = dir_entry?;
        if dir_entry.file_type()?.is_file() {
            File::open(dir_entry.path())?.read_to_string(&mut concatenated_content)?;
        }
    }
    Ok(concatenated_content)
}

fn extract_fluent_ids() -> HashSet<String> {
    let id_file_content = match std::env::var_os("FISH_FLUENT_ID_DIR") {
        Some(dir) => concat_files_in_dir(dir).unwrap(),
        None => {
            let temp_dir = fish_tempfile::new_dir().unwrap();
            Command::new(env!("CARGO"))
                .args([
                    "check",
                    "--features=fluent-extract",
                    "--workspace",
                    "--all-targets",
                ])
                .env("FISH_FLUENT_ID_DIR", temp_dir.path().as_os_str())
                .status()
                .map_err(|e| format!("Failed to extract Fluent IDs: {e}"))
                .unwrap();
            concat_files_in_dir(temp_dir.path()).unwrap()
        }
    };
    HashSet::from_iter(id_file_content.lines().map(|line| line.to_owned()))
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn check() {
        main();
    }
}
