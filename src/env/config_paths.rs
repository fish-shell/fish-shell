use crate::common::wcs2string;
use crate::wchar::prelude::*;
use crate::{common::get_executable_path, FLOG, FLOGF};
#[cfg(not(feature = "embed-data"))]
use fish_build_helper::workspace_root;
use std::ffi::OsString;
use std::os::unix::ffi::OsStringExt;
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
}

const SYSCONF_DIR: &str = env!("SYSCONFDIR");
#[cfg(not(feature = "embed-data"))]
const DOC_DIR: &str = env!("DOCDIR");

impl ConfigPaths {
    pub fn new(argv0: &wstr) -> Self {
        let argv0 = PathBuf::from(OsString::from_vec(wcs2string(argv0)));
        let exec_path = get_executable_path(argv0);
        FLOG!(config, format!("executable path: {}", exec_path.display()));
        let paths = Self::from_exec_path(exec_path);
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
        paths
    }

    #[cfg(not(feature = "embed-data"))]
    fn static_paths() -> Self {
        Self {
            sysconf: PathBuf::from(SYSCONF_DIR).join("fish"),
            bin: Some(PathBuf::from(env!("BINDIR"))),
            data: PathBuf::from(env!("DATADIR")).join("fish"),
            doc: DOC_DIR.into(),
        }
    }

    #[cfg(feature = "embed-data")]
    fn from_exec_path(exec_path: PathBuf) -> Self {
        FLOG!(config, "embed-data feature is active, ignoring data paths");
        Self {
            sysconf: PathBuf::from(SYSCONF_DIR).join("fish"),
            bin: exec_path.parent().map(|x| x.to_path_buf()),
        }
    }

    #[cfg(not(feature = "embed-data"))]
    fn from_exec_path(unresolved_exec_path: PathBuf) -> Self {
        use std::path::Path;

        let invalid_exec_path = |exec_path: &Path| {
            FLOG!(
                config,
                format!(
                    "Invalid executable path '{}', using compiled-in paths",
                    exec_path.display()
                )
            );
            Self::static_paths()
        };
        let Ok(exec_path) = unresolved_exec_path.canonicalize() else {
            return invalid_exec_path(&unresolved_exec_path);
        };
        let Some(exec_path_parent) = exec_path.parent() else {
            return invalid_exec_path(&exec_path);
        };

        let workspace_root = workspace_root();

        // The next check is that we are in a relocatable directory tree
        if exec_path_parent.ends_with("bin") {
            let base_path = exec_path_parent.parent().unwrap();
            let data = base_path.join("share/fish");
            let sysconf = base_path.join("etc/fish");
            if base_path != workspace_root // Install to repo root is not supported.
                && data.exists() && sysconf.exists()
            {
                FLOG!(config, "Running from relocatable tree");
                let doc = base_path.join("share/doc/fish");
                return Self {
                    sysconf,
                    bin: Some(exec_path_parent.to_owned()),
                    data,
                    // The docs dir may not exist; in that case fall back to the compiled in path.
                    doc: if doc.exists() {
                        doc
                    } else {
                        PathBuf::from(DOC_DIR)
                    },
                };
            }
        }

        // If we're in Cargo's target directory or in CMake's build directory, use the source files.
        if exec_path.starts_with(env!("FISH_BUILD_DIR")) {
            FLOG!(
                config,
                format!(
                    "Running out of build directory, using paths relative to $CARGO_MANIFEST_DIR ({})",
                    workspace_root.display()
                ),
            );
            return Self {
                sysconf: workspace_root.join("etc"),
                bin: Some(exec_path_parent.to_owned()),
                data: workspace_root.join("share"),
                doc: workspace_root.join("user_doc/html"),
            };
        }

        FLOG!(
            config,
            "Unexpected directory layout, using compiled-in paths"
        );
        Self::static_paths()
    }
}
