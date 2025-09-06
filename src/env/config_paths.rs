use crate::{common::get_executable_path, FLOG, FLOGF};
use once_cell::sync::Lazy;
use std::path::Path;
use std::path::PathBuf;

/// A struct of configuration directories, determined in main() that fish will optionally pass to
/// env_init.
#[derive(Default)]
pub struct ConfigPaths {
    pub data: Option<PathBuf>,   // e.g., /usr/local/share
    pub sysconf: PathBuf,        // e.g., /usr/local/etc
    pub doc: PathBuf,            // e.g., /usr/local/share/doc/fish
    pub bin: Option<PathBuf>,    // e.g., /usr/local/bin
    pub locale: Option<PathBuf>, // e.g., /usr/local/share/locale
}

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
    let exec_path = get_executable_path(&argv0);

    let paths = ConfigPaths::new(&argv0, exec_path);

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

const SYSCONF_DIR: &str = env!("SYSCONFDIR");
const DOC_DIR: &str = env!("DOCDIR");

impl ConfigPaths {
    #[cfg(feature = "embed-data")]
    fn new(_argv0: &Path, exec_path: PathBuf) -> Self {
        FLOG!(config, "embed-data feature is active, ignoring data paths");
        ConfigPaths {
            data: None,
            sysconf: PathBuf::from(SYSCONF_DIR).join("fish"),
            doc: DOC_DIR.into(),
            bin: exec_path.parent().map(|x| x.to_path_buf()),
            locale: None,
        }
    }

    #[cfg(not(feature = "embed-data"))]
    fn new(argv0: &Path, exec_path: PathBuf) -> Self {
        if let Ok(exec_path) = exec_path.canonicalize() {
            FLOG!(
                config,
                format!("exec_path: {:?}, argv[0]: {:?}", exec_path, &argv0)
            );
            // TODO: we should determine program_name from argv0 somewhere in this file

            // If we're in Cargo's target directory or CMake's build directory, use the source files.
            if exec_path.starts_with(env!("FISH_BUILD_DIR")) {
                let workspace_root = fish_build_helper::workspace_root();
                FLOG!(
                    config,
                    "Running out of target directory, using paths relative to workspace root:\n",
                    workspace_root.display()
                );
                return ConfigPaths {
                    data: Some(workspace_root.join("share")),
                    sysconf: workspace_root.join("etc"),
                    doc: workspace_root.join("user_doc/html"),
                    bin: Some(exec_path.parent().unwrap().to_owned()),
                    locale: Some(workspace_root.join("share/locale")),
                };
            }

            let mut paths = ConfigPaths::default();
            // The next check is that we are in a relocatable directory tree
            if exec_path.ends_with("bin/fish") {
                let base_path = exec_path.parent().unwrap().parent().unwrap();
                paths = ConfigPaths {
                    data: Some(base_path.join("share/fish")),
                    sysconf: base_path.join("etc/fish"),
                    doc: base_path.join("share/doc/fish"),
                    bin: Some(base_path.join("bin")),
                    locale: Some(base_path.join("share/locale")),
                }
            }

            if paths.data.clone().is_some_and(|x| x.exists()) && paths.sysconf.exists() {
                // The docs dir may not exist; in that case fall back to the compiled in path.
                if !paths.doc.exists() {
                    paths.doc = PathBuf::from(DOC_DIR);
                }
                return paths;
            }
        }

        // Fall back to what got compiled in.
        FLOG!(config, "Using compiled in paths:");
        ConfigPaths {
            data: Some(PathBuf::from(env!("DATADIR")).join("fish")),
            sysconf: PathBuf::from(SYSCONF_DIR).join("fish"),
            doc: DOC_DIR.into(),
            bin: Some(PathBuf::from(env!("BINDIR"))),
            locale: Some(PathBuf::from(env!("LOCALEDIR"))),
        }
    }
}
