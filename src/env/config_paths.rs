use crate::{common::get_executable_path, FLOG, FLOGF};
use once_cell::sync::Lazy;
use std::path::PathBuf;

/// A struct of configuration directories, determined in main() that fish will optionally pass to
/// env_init.
pub struct ConfigPaths {
    pub sysconf: PathBuf,     // e.g., /usr/local/etc
    pub bin: Option<PathBuf>, // e.g., /usr/local/bin
    #[cfg(not(feature = "embed-data"))]
    pub data: PathBuf, // e.g., /usr/local/share
    #[cfg(not(feature = "embed-data"))]
    pub doc: PathBuf, // e.g., /usr/local/share/doc/fish
    #[cfg(not(feature = "embed-data"))]
    pub locale: PathBuf, // e.g., /usr/local/share/locale
}

pub static CONFIG_PATHS: Lazy<ConfigPaths> = Lazy::new(|| {
    let argv0 = PathBuf::from(std::env::args().next().unwrap());
    let exec_path = get_executable_path(&argv0);
    FLOG!(config, format!("executable path: {}", exec_path.display()));

    let paths = ConfigPaths::new(exec_path);

    FLOGF!(
        config,
        "paths.sysconf: %ls",
        paths.sysconf.display().to_string()
    );
    FLOGF!(
        config,
        "paths.bin: %ls",
        paths
            .bin
            .clone()
            .map(|x| x.display().to_string())
            .unwrap_or("|not found|".to_string()),
    );
    #[cfg(not(feature = "embed-data"))]
    FLOGF!(config, "paths.data: %ls", paths.data.display().to_string());
    #[cfg(not(feature = "embed-data"))]
    FLOGF!(config, "paths.doc: %ls", paths.doc.display().to_string());
    #[cfg(not(feature = "embed-data"))]
    FLOGF!(
        config,
        "paths.locale: %ls",
        paths.locale.display().to_string()
    );

    paths
});

const SYSCONF_DIR: &str = env!("SYSCONFDIR");
#[cfg(not(feature = "embed-data"))]
const DOC_DIR: &str = env!("DOCDIR");

impl ConfigPaths {
    #[cfg(not(feature = "embed-data"))]
    fn static_paths() -> Self {
        // Fall back to what got compiled in.
        FLOG!(config, "Using compiled in paths:");
        Self {
            sysconf: PathBuf::from(SYSCONF_DIR).join("fish"),
            bin: Some(PathBuf::from(env!("BINDIR"))),
            data: PathBuf::from(env!("DATADIR")).join("fish"),
            doc: DOC_DIR.into(),
            locale: PathBuf::from(env!("LOCALEDIR")),
        }
    }

    #[cfg(feature = "embed-data")]
    fn new(exec_path: PathBuf) -> Self {
        FLOG!(config, "embed-data feature is active, ignoring data paths");
        ConfigPaths {
            sysconf: PathBuf::from(SYSCONF_DIR).join("fish"),
            bin: exec_path.parent().map(|x| x.to_path_buf()),
        }
    }

    #[cfg(not(feature = "embed-data"))]
    fn new(unresolved_exec_path: PathBuf) -> Self {
        let Ok(exec_path) = unresolved_exec_path.canonicalize() else {
            return Self::static_paths();
        };

        // If we're in Cargo's target directory or CMake's build directory, use the source files.
        if exec_path.starts_with(env!("FISH_BUILD_DIR")) {
            let workspace_root = fish_build_helper::workspace_root();
            FLOG!(
                config,
                format!(
                    "Running out of target directory, using paths relative to workspace root ({})",
                    workspace_root.display(),
                )
            );
            return ConfigPaths {
                sysconf: workspace_root.join("etc"),
                bin: Some(exec_path.parent().unwrap().to_owned()),
                data: workspace_root.join("share"),
                doc: workspace_root.join("user_doc/html"),
                locale: workspace_root.join("share/locale"),
            };
        }

        // The next check is that we are in a relocatable directory tree
        if exec_path.ends_with("bin/fish") {
            let base_path = exec_path.parent().unwrap().parent().unwrap();
            let data = base_path.join("share/fish");
            let sysconf = base_path.join("etc/fish");
            if data.exists() && sysconf.exists() {
                let doc = base_path.join("share/doc/fish");
                return ConfigPaths {
                    sysconf,
                    bin: Some(base_path.join("bin")),
                    data,
                    // The docs dir may not exist; in that case fall back to the compiled in path.
                    doc: if doc.exists() {
                        doc
                    } else {
                        PathBuf::from(DOC_DIR)
                    },
                    locale: base_path.join("share/locale"),
                };
            }
        }

        Self::static_paths()
    }
}
