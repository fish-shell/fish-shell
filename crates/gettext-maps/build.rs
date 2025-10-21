use std::{
    ffi::OsStr,
    path::{Path, PathBuf},
    process::Command,
};

use fish_build_helper::env_var;

fn main() {
    let cache_dir =
        PathBuf::from(fish_build_helper::fish_build_dir()).join("fish-localization-map-cache");
    embed_localizations(&cache_dir);

    fish_build_helper::rebuild_if_path_changed(fish_build_helper::workspace_root().join("po"));
}

fn embed_localizations(cache_dir: &Path) {
    use fish_gettext_mo_file_parser::parse_mo_file;
    use std::{
        fs::File,
        io::{BufWriter, Write},
    };

    let po_dir = fish_build_helper::workspace_root().join("po");

    // Ensure that the directory is created, because clippy cannot compile the code if the
    // directory does not exist.
    std::fs::create_dir_all(cache_dir).unwrap();

    let localization_map_path =
        Path::new(&env_var("OUT_DIR").unwrap()).join("localization_maps.rs");
    let mut localization_map_file = BufWriter::new(File::create(&localization_map_path).unwrap());

    // This will become a map which maps from language identifiers to maps containing localizations
    // for the respective language.
    let mut catalogs = phf_codegen::Map::new();

    match Command::new("msgfmt").arg("-h").output() {
        Err(e) if e.kind() == std::io::ErrorKind::NotFound => {
            rsconf::warn!(
                "Cannot find msgfmt to build gettext message catalogs. Localization will not work."
            );
            rsconf::warn!(
                "If you install it now you need to trigger a rebuild to get localization support."
            );
            rsconf::warn!(
                "One way to achieve that is running `touch po` followed by the build command."
            );
        }
        Err(e) => {
            panic!("Error when trying to run `msgfmt -h`: {e:?}");
        }
        Ok(output) => {
            let has_check_format =
                String::from_utf8_lossy(&output.stdout).contains("--check-format");
            for dir_entry_result in po_dir.read_dir().unwrap() {
                let dir_entry = dir_entry_result.unwrap();
                let po_file_path = dir_entry.path();
                if po_file_path.extension() != Some(OsStr::new("po")) {
                    continue;
                }
                let lang = po_file_path
                    .file_stem()
                    .expect("All entries in the po directory must be regular files.");
                let language = lang.to_str().unwrap().to_owned();

                // Each language gets its own static map for the mapping from message in the source code to
                // the localized version.
                let map_name = format!("LANG_MAP_{language}");

                let cached_map_path = cache_dir.join(lang);

                // Include the file containing the map for this language in the main generated file.
                writeln!(
                    &mut localization_map_file,
                    "include!(\"{}\");",
                    cached_map_path.display()
                )
                .unwrap();
                // Map from the language identifier to the map containing the localizations for this
                // language.
                catalogs.entry(language, format!("&{map_name}"));

                if let Ok(metadata) = std::fs::metadata(&cached_map_path) {
                    // Cached map file exists, but might be outdated.
                    let cached_map_mtime = metadata.modified().unwrap();
                    let po_mtime = dir_entry.metadata().unwrap().modified().unwrap();
                    if cached_map_mtime > po_mtime {
                        // Cached map file is considered up-to-date.
                        continue;
                    };
                }

                // Generate the map file.

                // Try to create new MO data and load it into `mo_data`.
                let output = {
                    let mut cmd = &mut Command::new("msgfmt");
                    if has_check_format {
                        cmd = cmd.arg("--check-format");
                    }
                    cmd.arg("--output-file=-")
                        .arg(&po_file_path)
                        .output()
                        .unwrap()
                };
                if !output.status.success() {
                    panic!(
                        "msgfmt failed:\n{}",
                        String::from_utf8(output.stderr).unwrap()
                    );
                }
                let mo_data = output.stdout;

                // Extract map from MO data.
                let language_localizations = parse_mo_file(&mo_data).unwrap();

                // This file will contain the localization map for the current language.
                let mut cached_map_file = File::create(&cached_map_path).unwrap();
                let mut single_language_localization_map = phf_codegen::Map::new();

                // The values will be written into the source code as is, meaning escape sequences and
                // double quotes in the data will be interpreted by the Rust compiler, which is undesirable.
                // Converting them to raw strings prevents this. (As long as no input data contains `"###`.)
                fn to_raw_str(s: &str) -> String {
                    assert!(!s.contains("\"###"));
                    format!("r###\"{s}\"###")
                }
                for (msgid, msgstr) in language_localizations {
                    single_language_localization_map.entry(
                        String::from_utf8(msgid.into()).unwrap(),
                        to_raw_str(&String::from_utf8(msgstr.into()).unwrap()),
                    );
                }
                writeln!(&mut cached_map_file, "#[allow(non_upper_case_globals)]").unwrap();
                write!(
                    &mut cached_map_file,
                    "static {}: phf::Map<&'static str, &'static str> = {}",
                    &map_name,
                    single_language_localization_map.build()
                )
                .unwrap();
                writeln!(&mut cached_map_file, ";").unwrap();
            }
        }
    }

    write!(
        &mut localization_map_file,
        "pub static CATALOGS: phf::Map<&str, &phf::Map<&str, &str>> = {}",
        catalogs.build()
    )
    .unwrap();
    writeln!(&mut localization_map_file, ";").unwrap();
}
