use crate::common::{BUILD_DIR, get_program_name};
use crate::{flog, flogf};
use fish_build_helper::workspace_root;
use std::ffi::OsStr;
use std::os::unix::ffi::OsStrExt;
use std::path::{Path, PathBuf};
use std::sync::OnceLock;

/// A struct of configuration directories, determined in main() that fish will optionally pass to
/// env_init.
pub struct ConfigPaths {
    pub sysconf: PathBuf,      // e.g., /usr/local/etc
    pub bin: Option<PathBuf>,  // e.g., /usr/local/bin
    pub data: Option<PathBuf>, // e.g., /usr/local/share
    pub man: Option<PathBuf>,  // e.g., /usr/local/share/fish/man
    pub doc: Option<PathBuf>,  // e.g., /usr/local/share/doc/fish
}

const SYSCONF_DIR: &str = env!("SYSCONFDIR");

impl ConfigPaths {
    pub fn new() -> Self {
        FISH_PATH.get_or_init(compute_fish_path);
        let exec_path = get_fish_path();
        flog!(
            config,
            match exec_path {
                FishPath::Absolute(path) => format!("executable path: {}", path.display()),
                FishPath::LookUpInPath => format!("executable path: {}", get_program_name()),
            }
        );
        let paths = Self::from_exec_path(exec_path);
        flogf!(
            config,
            "paths.sysconf: %s",
            paths.sysconf.display().to_string()
        );
        macro_rules! log_optional_path {
            ($field:ident) => {
                flogf!(
                    config,
                    "paths.%s: %s",
                    stringify!($field),
                    paths
                        .$field
                        .as_ref()
                        .map(|x| x.display().to_string())
                        .unwrap_or("|not found|".to_string()),
                );
            };
        }
        log_optional_path!(bin);
        log_optional_path!(data);
        log_optional_path!(man);
        log_optional_path!(doc);
        paths
    }

    fn from_exec_path(unresolved_exec_path: &'static FishPath) -> Self {
        let default_layout = |exec_path_parent: Option<&Path>| {
            let data = option_env!("DATADIR").map(|p| PathBuf::from(p).join("fish"));
            Self {
                sysconf: PathBuf::from(SYSCONF_DIR).join("fish"),
                bin: option_env!("BINDIR")
                    .map(PathBuf::from)
                    // N.B. the argument may be non-canonical here.
                    .or_else(|| exec_path_parent.map(|p| p.to_owned())),
                data: data.clone(),
                man: data.map(|data| data.join("man")),
                doc: option_env!("DOCDIR").map(PathBuf::from),
            }
        };

        let exec_path = {
            use FishPath::*;
            match unresolved_exec_path {
                Absolute(p) => {
                    let Ok(exec_path) = p.canonicalize() else {
                        flog!(
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
                    flog!(
                        config,
                        "No absolute executable path available. Using default paths",
                    );
                    return default_layout(None);
                }
            }
        };

        let Some(exec_path_parent) = exec_path.parent() else {
            flog!(
                config,
                "Executable path reported to be the root directory?! Using default paths.",
            );
            return default_layout(None);
        };

        let workspace_root = workspace_root();
        // TODO(MSRV>=1.88): feature(let_chains)
        if cfg!(using_cmake) && exec_path_parent.ends_with("bin") && {
            let prefix = exec_path_parent.parent().unwrap();
            let data = prefix.join("share/fish");
            let sysconf = prefix.join("etc/fish");
            data.exists() && sysconf.exists()
            // Installations with prefix set to exactly the workspace root are not supported;
            // those will behave like non-installed builds inside the workspace.
            // Installing somewhere else inside the workspace is fine.
            && prefix != workspace_root
        } {
            flog!(config, "Running from relocatable tree");
            let prefix = exec_path_parent.parent().unwrap();
            let data = prefix.join("share/fish");
            Self {
                sysconf: prefix.join("etc/fish"),
                bin: Some(exec_path_parent.to_owned()),
                data: Some(data.clone()),
                man: Some(data.join("man")),
                doc: Some(prefix.join("share/doc/fish")),
            }
        } else if exec_path.starts_with(BUILD_DIR) {
            flog!(
                config,
                format!(
                    "Running out of build directory, using paths relative to $CARGO_MANIFEST_DIR ({})",
                    workspace_root.display()
                ),
            );
            let doc_join = |dir| {
                cfg!(using_cmake)
                    .then_some(Path::new(BUILD_DIR))
                    .map(|path| path.join("cargo").join("fish-docs").join(dir))
            };
            // If we're in Cargo's target directory or in CMake's build directory, use the source files.
            Self {
                sysconf: workspace_root.join("etc"),
                bin: Some(exec_path_parent.to_owned()),
                data: Some(workspace_root.join("share")),
                man: doc_join("man"),
                doc: doc_join("html"),
            }
        } else {
            flog!(
                config,
                "Not in a relocatable tree or build directory, using default paths"
            );
            default_layout(Some(exec_path_parent))
        }
    }
}

pub enum FishPath {
    Absolute(PathBuf),
    LookUpInPath,
}

static FISH_PATH: OnceLock<FishPath> = OnceLock::new();

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
