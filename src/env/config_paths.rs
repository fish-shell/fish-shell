use crate::common::{BUILD_DIR, PROGRAM_NAME};
use crate::{FLOG, FLOGF};
use fish_build_helper::workspace_root;
use once_cell::sync::OnceCell;
use std::ffi::OsStr;
use std::os::unix::ffi::OsStrExt;
use std::path::{Path, PathBuf};

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
    pub fn new() -> Self {
        let exec_path = FISH_PATH.get_or_init(compute_fish_path);
        FLOG!(config, format!("executable path: {}", exec_path.display()));
        let paths = Self::from_exec_path(exec_path);
        FLOGF!(
            config,
            "paths.sysconf: %s",
            paths.sysconf.display().to_string()
        );
        FLOGF!(
            config,
            "paths.bin: %s",
            paths
                .bin
                .clone()
                .map(|x| x.display().to_string())
                .unwrap_or("|not found|".to_string()),
        );
        #[cfg(not(feature = "embed-data"))]
        FLOGF!(config, "paths.data: %s", paths.data.display().to_string());
        #[cfg(not(feature = "embed-data"))]
        FLOGF!(config, "paths.doc: %s", paths.doc.display().to_string());
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
    fn from_exec_path(exec_path: &Path) -> Self {
        FLOG!(config, "embed-data feature is active, ignoring data paths");
        Self {
            sysconf: if exec_path
                .canonicalize()
                .is_ok_and(|exec_path| exec_path.starts_with(BUILD_DIR))
            {
                workspace_root().join("etc")
            } else {
                PathBuf::from(SYSCONF_DIR).join("fish")
            },
            bin: exec_path.parent().map(|x| x.to_path_buf()),
        }
    }

    #[cfg(not(feature = "embed-data"))]
    fn from_exec_path(unresolved_exec_path: &Path) -> Self {
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
            return invalid_exec_path(unresolved_exec_path);
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
        if exec_path.starts_with(BUILD_DIR) {
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

static FISH_PATH: OnceCell<PathBuf> = OnceCell::new();

/// Get the absolute path to the fish executable itself
pub fn get_fish_path() -> &'static PathBuf {
    FISH_PATH.get().unwrap()
}

fn compute_fish_path() -> PathBuf {
    let Ok(path) = std::env::current_exe() else {
        assert!(PROGRAM_NAME.get().unwrap() == "fish");
        return PathBuf::from("fish");
    };
    assert!(path.is_absolute());
    if path.exists() {
        return path;
    }

    // When /proc/self/exe points to a file that was deleted (or overwritten on update!)
    // then linux adds a " (deleted)" suffix.
    // If that's not a valid path, let's remove that awkward suffix.
    if !path.as_os_str().as_bytes().ends_with(b" (deleted)") {
        return path;
    }

    if let (Some(filename), Some(parent)) = (path.file_name(), path.parent()) {
        if let Some(filename) = filename.to_str() {
            let corrected_filename = OsStr::new(filename.strip_suffix(" (deleted)").unwrap());
            return parent.join(corrected_filename);
        }
    }
    path
}
