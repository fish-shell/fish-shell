use std::{
    collections::{HashMap, HashSet},
    fmt::Write,
    fs::File,
    io::Read,
    path::Path,
    process::Command,
};

use fish_build_helper::workspace_root;

use fluent::{FluentBundle, FluentResource};
use unic_langid::LanguageIdentifier;

fn main() {
    let unique_ids = extract_fluent_ids();

    let ftl_dir = workspace_root().join("localization").join("fluent");
    // These must have translations for every message.
    let required_langs = [fish_fluent::FALLBACK_LANGUAGE];
    let fluent_resources = parse_ftl_files(&ftl_dir);
    check_for_extra_ids(&fluent_resources, &unique_ids);
    check_for_unsorted_ids(&fluent_resources, &unique_ids);
    let fluent_bundles = resources_to_bundles(fluent_resources);
    check_required_langs(&fluent_bundles, &required_langs, &unique_ids);
}

fn resources_to_bundles(
    resources: HashMap<String, FluentResource>,
) -> HashMap<String, FluentBundle<FluentResource>> {
    let mut bundles = HashMap::new();
    for (language, resource) in resources {
        let langid: LanguageIdentifier = language
            .parse()
            .map_err(|e| format!("Failed to parse language identifier {language}: {e}"))
            .unwrap();
        let mut bundle = FluentBundle::new(vec![langid]);
        bundle
            .add_resource(resource)
            .map_err(|e| {
                format!("Failed to add FTL resources to the bundle for language {language}: {e:?}")
            })
            .unwrap();
        bundles.insert(language, bundle);
    }
    bundles
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
    HashSet::from_iter(id_file_content.lines().map(|line| line.to_string()))
}

fn parse_ftl_files(ftl_dir: &Path) -> HashMap<String, FluentResource> {
    let mut bundles = HashMap::new();
    for dir_entry in ftl_dir.read_dir().unwrap() {
        let dir_entry = dir_entry.unwrap();
        let file_name = dir_entry.file_name().into_string().unwrap();
        let Some(language) = file_name.strip_suffix(".ftl") else {
            continue;
        };
        let file_content = std::fs::read_to_string(dir_entry.path()).unwrap();
        match FluentResource::try_new(file_content) {
            Ok(resource) => {
                bundles.insert(language.to_owned(), resource);
            }
            Err((_resource, errors)) => {
                let mut error_string = format!("Errors parsing FTL file for {language}:\n");
                for error in errors {
                    let _ = writeln!(error_string, "{error}");
                }
                panic!("{error_string}");
            }
        }
    }
    bundles
}

fn show_id_errors_per_language(header: &str, language_to_ids: HashMap<&str, Vec<&str>>) {
    let mut error_message = String::new();
    error_message.push_str(header);
    error_message.push_str(":\n\n");
    for (language, ids) in language_to_ids {
        error_message.push_str("For language ");
        error_message.push_str(language);
        error_message.push_str(":\n");
        for id in ids {
            error_message.push_str(id);
            error_message.push('\n');
        }
    }
    panic!("{error_message}");
}

fn check_required_langs(
    ftl_data: &HashMap<String, FluentBundle<FluentResource>>,
    required_langs: &[&str],
    required_ids: &HashSet<String>,
) {
    let mut language_to_missing_ids = HashMap::new();
    for &language in required_langs {
        let Some(bundle) = ftl_data.get(language) else {
            panic!("Expected FTL file for language {language} but did not find it.");
        };
        let mut missing_ids_for_language = vec![];
        for id in required_ids {
            if !bundle.has_message(id) {
                missing_ids_for_language.push(id.as_str());
            }
        }
        if !missing_ids_for_language.is_empty() {
            // Show missing IDs in alphabetical order
            missing_ids_for_language.sort_unstable();
            language_to_missing_ids.insert(language, missing_ids_for_language);
        }
    }
    if !language_to_missing_ids.is_empty() {
        show_id_errors_per_language("Missing IDs", language_to_missing_ids);
    }
}

fn check_for_extra_ids(ftl_data: &HashMap<String, FluentResource>, valid_ids: &HashSet<String>) {
    let mut language_to_unexpected_ids = HashMap::new();
    for (language, resource) in ftl_data {
        let mut unexpected_ids_for_language = vec![];
        for entry in resource.entries() {
            if let fluent_syntax::ast::Entry::Message(message) = entry {
                let id = message.id.name;
                if !valid_ids.contains(id) {
                    unexpected_ids_for_language.push(id);
                }
            }
        }
        if !unexpected_ids_for_language.is_empty() {
            language_to_unexpected_ids.insert(language.as_str(), unexpected_ids_for_language);
        }
    }
    if !language_to_unexpected_ids.is_empty() {
        show_id_errors_per_language("Unexpected IDs found", language_to_unexpected_ids);
    }
}

/// Call this after establishing that no invalid IDs appear.
fn check_for_unsorted_ids(
    fluent_resources: &HashMap<String, FluentResource>,
    valid_ids: &HashSet<String>,
) {
    let mut sorted_ids = valid_ids.iter().collect::<Vec<_>>();
    sorted_ids.sort();
    let sorted_ids = sorted_ids;
    for (language, resource) in fluent_resources {
        let mut sorted_id_index = 0;
        for entry in resource.entries() {
            if let fluent_syntax::ast::Entry::Message(message) = entry {
                let id = message.id.name;
                while *(sorted_ids[sorted_id_index].as_str()) < *id
                    && sorted_id_index < sorted_ids.len() - 1
                {
                    sorted_id_index += 1;
                }
                if *sorted_ids[sorted_id_index] != *id {
                    let mut error_string = String::from("Expected ID order:\n\n");
                    for fluent_id in sorted_ids {
                        let _ = writeln!(error_string, "{fluent_id}");
                    }
                    let _ = writeln!(
                        error_string,
                        "\nFTL file for language {language} is not sorted properly. ID '{id}' appears out of order. See the full expected ID order above."
                    );
                    panic!("{error_string}");
                }
            }
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn check() {
        main();
    }
}
