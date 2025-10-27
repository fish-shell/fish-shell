use crate::common::{BUILD_DIR, get_program_name};
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

/// The kind of directory structure we assume.
/// Each variants is passed the executable path's parent, if available.
enum DirectoryLayout {
    /// In a relocatable tree.
    RelocatableTree(PathBuf),
    /// In the build directory.
    BuildDirectory(PathBuf),
    /// None of the above, or something went wrong.
    /// Arg may be non-canonicalized here.
    DefaultLayout(Option<PathBuf>),
}

fn resolve_exec_path(unresolved_exec_path: &'static FishPath) -> DirectoryLayout {
    use DirectoryLayout::*;

    let default_layout =
        |exec_path_parent: Option<&Path>| DefaultLayout(exec_path_parent.map(|p| p.to_owned()));

    let exec_path = {
        use FishPath::*;
        match unresolved_exec_path {
            Absolute(p) => {
                let Ok(exec_path) = p.canonicalize() else {
                    FLOG!(
                        config,
                        format!(
                            "Failed to canonicalize executable path '{}'. Using default paths",
                            p.display()
                        )
                    );
                    return default_layout(p.parent());
                };
                exec_path
            }
            LookUpInPath => {
                FLOG!(
                    config,
                    "No absolute executable path available. Using default paths",
                );
                return default_layout(None);
            }
        }
    };

    let Some(exec_path_parent) = exec_path.parent() else {
        FLOG!(
            config,
            "Executable path reported to be the root directory?! Using default paths.",
        );
        return default_layout(None);
    };

    let workspace_root = workspace_root();
    if exec_path_parent.ends_with("bin") && {
        let prefix = exec_path_parent.parent().unwrap();
        let data = prefix.join("share/fish");
        let sysconf = prefix.join("etc/fish");
        // Installations with prefix set to exactly the workspace root are not supported;
        // those will behave like non-installed builds inside the workspace.
        // Installing somewhere else inside the workspace is fine.
        prefix != workspace_root && data.exists() && sysconf.exists()
    } {
        FLOG!(config, "Running from relocatable tree");
        RelocatableTree(exec_path_parent.to_owned())
    } else if exec_path.starts_with(BUILD_DIR) {
        FLOG!(
            config,
            format!(
                "Running out of build directory, using paths relative to $CARGO_MANIFEST_DIR ({})",
                workspace_root.display()
            ),
        );
        // If we're in Cargo's target directory or in CMake's build directory, use the source files.
        BuildDirectory(exec_path_parent.to_owned())
    } else {
        FLOG!(
            config,
            "Not in a relocatable tree or build directory, using default paths"
        );
        default_layout(Some(exec_path_parent))
    }
}

impl ConfigPaths {
    pub fn new() -> Self {
        FISH_PATH.get_or_init(compute_fish_path);
        let exec_path = get_fish_path();
        FLOG!(
            config,
            match exec_path {
                FishPath::Absolute(path) => format!("executable path: {}", path.display()),
                FishPath::LookUpInPath => format!("executable path: {}", get_program_name()),
            }
        );
        let paths = Self::from_layout(resolve_exec_path(exec_path));
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

    fn from_layout(exec_path: DirectoryLayout) -> Self {
        let workspace_root = workspace_root();

        #[cfg(feature = "embed-data")]
        FLOG!(config, "embed-data feature is active, ignoring data paths");

        use DirectoryLayout::*;
        match exec_path {
            RelocatableTree(exec_path_parent) => {
                let prefix = exec_path_parent.parent().unwrap();
                Self {
                    sysconf: prefix.join("etc/fish"),
                    bin: Some(exec_path_parent.to_owned()),
                    #[cfg(not(feature = "embed-data"))]
                    data: prefix.join("share/fish"),
                    // The docs dir may not exist; in that case fall back to the compiled in path.
                    #[cfg(not(feature = "embed-data"))]
                    doc: {
                        let doc = prefix.join("share/doc/fish");
                        if doc.exists() {
                            doc
                        } else {
                            PathBuf::from(DOC_DIR)
                        }
                    },
                }
            }
            BuildDirectory(exec_path_parent) => Self {
                sysconf: workspace_root.join("etc"),
                bin: Some(exec_path_parent.to_owned()),
                #[cfg(not(feature = "embed-data"))]
                data: workspace_root.join("share"),
                #[cfg(not(feature = "embed-data"))]
                doc: workspace_root.join("user_doc/html"),
            },
            #[allow(unused_variables)]
            DefaultLayout(exec_path_parent) => Self {
                sysconf: PathBuf::from(SYSCONF_DIR).join("fish"),
                #[cfg(feature = "embed-data")]
                bin: exec_path_parent,
                #[cfg(not(feature = "embed-data"))]
                bin: Some(PathBuf::from(env!("BINDIR"))),
                #[cfg(not(feature = "embed-data"))]
                data: PathBuf::from(env!("DATADIR")).join("fish"),
                #[cfg(not(feature = "embed-data"))]
                doc: DOC_DIR.into(),
            },
        }
    }
}

pub enum FishPath {
    Absolute(PathBuf),
    LookUpInPath,
}

static FISH_PATH: OnceCell<FishPath> = OnceCell::new();

/// Get the absolute path to the fish executable itself
pub fn get_fish_path() -> &'static FishPath {
    FISH_PATH.get().unwrap()
}

fn compute_fish_path() -> FishPath {
    use FishPath::*;
    let Ok(mut path) = std::env::current_exe() else {
        return LookUpInPath;
    };

    assert!(path.is_absolute());

    // When /proc/self/exe points to a file that was deleted (or overwritten on update!)
    // then linux adds a " (deleted)" suffix.
    // If that's not a valid path, let's remove that awkward suffix.
    // TODO(MSRV>=1.88) use if-let-chain
    if !path.exists() {
        if let (Some(filename), Some(parent)) = (path.file_name(), path.parent()) {
            if let Some(corrected_filename) = filename
                .as_bytes()
                .strip_suffix(b" (deleted)")
                .map(OsStr::from_bytes)
            {
                path = parent.join(corrected_filename);
            }
        }
    }

    Absolute(path)
}
