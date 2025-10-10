use std::{
    collections::{HashMap, HashSet},
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

    let ftl_dir = workspace_root().join("fluent");
    // These must have translations for every message.
    let required_langs = ["en"];

    let ftl_data = parse_ftl_files(&ftl_dir);

    check_required_langs(&ftl_data, &required_langs, &unique_ids);
    check_for_extra_args(&ftl_data, &unique_ids);
    check_for_unsorted_args(&ftl_data, &unique_ids);
}

fn extract_fluent_ids() -> HashSet<String> {
    let mut tempfile = fish_tempfile::new().unwrap();
    Command::new("cargo")
        .args([
            "check",
            "--features=fluent-extract",
            "--workspace",
            "--all-targets",
        ])
        .env("FISH_FLUENT_ID_FILE", tempfile.get_path().as_os_str())
        .status()
        .map_err(|e| format!("Failed to extract Fluent IDs: {e}"))
        .unwrap();
    let mut id_file_content = String::new();
    tempfile
        .get_mut()
        .read_to_string(&mut id_file_content)
        .unwrap();
    HashSet::from_iter(id_file_content.lines().map(|line| line.to_string()))
}

fn parse_ftl_files(
    ftl_dir: &Path,
) -> HashMap<String, (FluentResource, FluentBundle<FluentResource>)> {
    let mut bundles = HashMap::new();
    for dir_entry in ftl_dir.read_dir().unwrap() {
        let dir_entry = dir_entry.unwrap();
        let file_name = dir_entry.file_name().into_string().unwrap();
        if file_name.ends_with(".ftl") {
            let language = file_name.strip_suffix(".ftl").unwrap();
            let langid: LanguageIdentifier = language
                .parse()
                .map_err(|e| format!("Failed to parse language identifier {language}: {e}"))
                .unwrap();

            let mut file_content = String::new();
            File::open(dir_entry.path())
                .unwrap()
                .read_to_string(&mut file_content)
                .unwrap();
            match FluentResource::try_new(file_content.clone()) {
                Ok(resource) => {
                    let mut bundle = FluentBundle::new(vec![langid]);
                    bundle
                    .add_resource(resource)
                    .map_err(|e|format!("Failed to add FTL resources to the bundle for language {language}: {e:?}"))
                    .unwrap();
                    // `FluentResource` does not implement `Clone`, so we parse twice, to enable us
                    // to have both the `FluentResource` and the `FluentBundle` available.
                    bundles.insert(
                        language.to_owned(),
                        (FluentResource::try_new(file_content).unwrap(), bundle),
                    );
                }
                Err((_resource, errors)) => {
                    let mut error_string = format!("Errors parsing FTL file for {language}:\n");
                    for error in errors {
                        error_string.push_str(&format!("{error}\n"));
                    }
                    panic!("{error_string}");
                }
            }
        }
    }
    bundles
}

fn check_required_langs(
    ftl_data: &HashMap<String, (FluentResource, FluentBundle<FluentResource>)>,
    required_langs: &[&str],
    required_ids: &HashSet<String>,
) {
    for &language in required_langs {
        let Some((_, bundle)) = ftl_data.get(language) else {
            panic!("Expected FTL file for language {language} but did not find it.");
        };
        for id in required_ids {
            if !bundle.has_message(id) {
                panic!("Bundle {language} does not have id '{id}'");
            }
        }
    }
}

fn check_for_extra_args(
    ftl_data: &HashMap<String, (FluentResource, FluentBundle<FluentResource>)>,
    valid_ids: &HashSet<String>,
) {
    for (language, (resource, _)) in ftl_data {
        for entry in resource.entries() {
            if let fluent_syntax::ast::Entry::Message(message) = entry {
                let id = message.id.name;
                if !valid_ids.contains(id) {
                    panic!("FTL file for language {language} contains an unexpected id: {id}");
                }
            }
        }
    }
}

/// Call this after establishing that no invalid IDs appear.
fn check_for_unsorted_args(
    ftl_data: &HashMap<String, (FluentResource, FluentBundle<FluentResource>)>,
    valid_ids: &HashSet<String>,
) {
    let mut sorted_ids = valid_ids.iter().collect::<Vec<_>>();
    sorted_ids.sort();
    let sorted_ids = sorted_ids;
    for (language, (resource, _)) in ftl_data {
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
                        error_string.push_str(&format!("{fluent_id}\n"));
                    }
                    error_string.push_str(&format!("\nFTL file for language {language} is not sorted properly. ID '{id}' appears out of order. See the full expected ID order above."));
                    panic!("{error_string}");
                }
            }
        }
    }
}
