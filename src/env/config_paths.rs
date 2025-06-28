use super::ConfigPaths;
use crate::{common::get_executable_path, FLOG, FLOGF};
use once_cell::sync::Lazy;
use std::path::PathBuf;

const DOC_DIR: &str = env!("DOCDIR");
const DATA_DIR: &str = env!("DATADIR");
const DATA_DIR_SUBDIR: &str = env!("DATADIR_SUBDIR");
const SYSCONF_DIR: &str = env!("SYSCONFDIR");
const LOCALE_DIR: &str = env!("LOCALEDIR");
const BIN_DIR: &str = env!("BINDIR");

pub static CONFIG_PATHS: Lazy<ConfigPaths> = Lazy::new(|| {
    // Read the current executable and follow all symlinks to it.
    // OpenBSD has issues with `std::env::current_exe`, see gh-9086 and
    // https://github.com/rust-lang/rust/issues/60560
    let argv0 = PathBuf::from(std::env::args().next().unwrap());
    let argv0 = if argv0.exists() {
        argv0
    } else {
        std::env::current_exe().unwrap_or(argv0)
    };
    let argv0 = argv0.canonicalize().unwrap_or(argv0);
    let mut paths = ConfigPaths::default();
    let mut done = false;
    let exec_path = get_executable_path(&argv0);
    if let Ok(exec_path) = exec_path.canonicalize() {
        FLOG!(
            config,
            format!("exec_path: {:?}, argv[0]: {:?}", exec_path, &argv0)
        );
        // TODO: we should determine program_name from argv0 somewhere in this file

        // Detect if we're running right out of the CMAKE build directory
        if exec_path.starts_with(env!("FISH_BUILD_DIR")) {
            let manifest_dir = PathBuf::from(env!("CARGO_MANIFEST_DIR"));
            FLOG!(
                config,
                "Running out of target directory, using paths relative to CARGO_MANIFEST_DIR:\n",
                manifest_dir.display()
            );
            done = true;
            paths = ConfigPaths {
                data: Some(manifest_dir.join("share")),
                sysconf: manifest_dir.join("etc"),
                doc: manifest_dir.join("user_doc/html"),
                bin: Some(exec_path.parent().unwrap().to_owned()),
                locale: Some(manifest_dir.join("share/locale")),
            }
        }

        if !done {
            // The next check is that we are in a relocatable directory tree
            if exec_path.ends_with("bin/fish") {
                let base_path = exec_path.parent().unwrap().parent().unwrap();
                #[cfg(feature = "embed-data")]
                let data_dir = base_path.join("share/fish/install");
                #[cfg(not(feature = "embed-data"))]
                let data_dir = base_path.join("share/fish");
                paths = ConfigPaths {
                    // One obvious path is ~/.local (with fish in ~/.local/bin/).
                    // If we picked ~/.local/share/fish as our data path,
                    // we would install there and erase history.
                    // So let's isolate us a bit more.
                    data: Some(data_dir.clone()),
                    sysconf: base_path.join("etc/fish"),
                    doc: base_path.join("share/doc/fish"),
                    bin: Some(base_path.join("bin")),
                    locale: Some(data_dir.join("locale")),
                }
            } else if exec_path.ends_with("fish") {
                FLOG!(
                    config,
                    "'fish' not in a 'bin/', trying paths relative to source tree"
                );
                let base_path = exec_path.parent().unwrap();
                #[cfg(feature = "embed-data")]
                let data_dir = base_path.join("share/install");
                #[cfg(not(feature = "embed-data"))]
                let data_dir = base_path.join("share");
                paths = ConfigPaths {
                    data: Some(data_dir.clone()),
                    sysconf: base_path.join("etc"),
                    doc: base_path.join("user_doc/html"),
                    bin: Some(base_path.to_path_buf()),
                    locale: Some(data_dir.join("locale")),
                }
            }

            if paths.data.clone().is_some_and(|x| x.exists()) && paths.sysconf.exists() {
                // The docs dir may not exist; in that case fall back to the compiled in path.
                if !paths.doc.exists() {
                    paths.doc = PathBuf::from(DOC_DIR);
                }
                done = true;
            }
        }
    }

    if !done {
        // Fall back to what got compiled in.
        let data = if cfg!(feature = "embed-data") {
            None
        } else {
            Some(PathBuf::from(DATA_DIR).join(DATA_DIR_SUBDIR))
        };
        let bin = if cfg!(feature = "embed-data") {
            exec_path.parent().map(|x| x.to_path_buf())
        } else {
            Some(PathBuf::from(BIN_DIR))
        };
        let locale = if cfg!(feature = "embed-data") {
            None
        } else {
            Some(PathBuf::from(LOCALE_DIR))
        };

        FLOG!(config, "Using compiled in paths:");
        paths = ConfigPaths {
            data,
            sysconf: PathBuf::from(SYSCONF_DIR).join("fish"),
            doc: DOC_DIR.into(),
            bin,
            locale,
        }
    }

    FLOGF!(
        config,
        "determine_config_directory_paths() results:\npaths.data: %ls\npaths.sysconf: \
        %ls\npaths.doc: %ls\npaths.bin: %ls\npaths.locale: %ls",
        paths
            .data
            .clone()
            .map(|x| x.display().to_string())
            .unwrap_or("|not found|".to_string()),
        paths.sysconf.display().to_string(),
        paths.doc.display().to_string(),
        paths
            .bin
            .clone()
            .map(|x| x.display().to_string())
            .unwrap_or("|not found|".to_string()),
        paths
            .locale
            .clone()
            .map(|x| x.display().to_string())
            .unwrap_or("|not found|".to_string()),
    );

    paths
});
