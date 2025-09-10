use crate::common::wcs2string;
use crate::global_safety::AtomicRef;
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
    #[cfg(not(feature = "embed-data"))]
    pub locale: PathBuf, // e.g., /usr/local/share/locale
}

const SYSCONF_DIR: &str = env!("SYSCONFDIR");
#[cfg(not(feature = "embed-data"))]
const DOC_DIR: &str = env!("DOCDIR");

#[cfg(not(feature = "embed-data"))]
enum ConfigPathKind {
    WorkspaceRoot,
    RelocatableTree,
    StaticPathsDueToInvalidExecPath,
    StaticPathsDueToWeirdLayout,
}

pub struct ConfigPathDetection {
    #[cfg(not(feature = "embed-data"))]
    kind: ConfigPathKind,
    pub paths: ConfigPaths,
    exec_path: PathBuf,
}

impl ConfigPaths {
    #[cfg(not(feature = "embed-data"))]
    fn static_paths() -> Self {
        Self {
            sysconf: PathBuf::from(SYSCONF_DIR).join("fish"),
            bin: Some(PathBuf::from(env!("BINDIR"))),
            data: PathBuf::from(env!("DATADIR")).join("fish"),
            doc: DOC_DIR.into(),
            locale: PathBuf::from(env!("LOCALEDIR")),
        }
    }
}

const EMPTY_LOCALE_DIR: &[u8] = &[];
pub static LOCALE_DIR: AtomicRef<[u8]> = AtomicRef::new(&EMPTY_LOCALE_DIR);

/// NOTE: This is called during early startup, do not log anything.
pub fn init_locale_dir(argv0: &wstr) -> ConfigPathDetection {
    #[allow(clippy::let_and_return)] // for old clippy
    let detection = ConfigPathDetection::new(argv0);
    #[cfg(not(feature = "embed-data"))]
    {
        use std::os::unix::ffi::OsStrExt;
        let bytes = detection.paths.locale.as_os_str().as_bytes();
        assert!(!bytes.is_empty());
        let bytes: Box<[u8]> = Box::from(bytes);
        let bytes: Box<&'static [u8]> = Box::new(Box::leak(bytes));
        LOCALE_DIR.store(Box::leak(bytes));
    }
    detection
}

impl ConfigPathDetection {
    fn new(argv0: &wstr) -> Self {
        let argv0 = PathBuf::from(OsString::from_vec(wcs2string(argv0)));
        let exec_path = get_executable_path(argv0);
        Self::from_exec_path(exec_path)
    }

    #[cfg(feature = "embed-data")]
    fn from_exec_path(exec_path: PathBuf) -> Self {
        Self {
            paths: ConfigPaths {
                sysconf: PathBuf::from(SYSCONF_DIR).join("fish"),
                bin: exec_path.parent().map(|x| x.to_path_buf()),
            },
            exec_path,
        }
    }

    #[cfg(not(feature = "embed-data"))]
    fn from_exec_path(unresolved_exec_path: PathBuf) -> Self {
        use ConfigPathKind::*;
        let invalid_exec_path = |exec_path| Self {
            kind: StaticPathsDueToInvalidExecPath,
            paths: ConfigPaths::static_paths(),
            exec_path,
        };
        let Ok(exec_path) = unresolved_exec_path.canonicalize() else {
            return invalid_exec_path(unresolved_exec_path);
        };
        let Some(exec_path_parent) = exec_path.parent() else {
            return invalid_exec_path(exec_path);
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
                let doc = base_path.join("share/doc/fish");
                return Self {
                    kind: RelocatableTree,
                    paths: ConfigPaths {
                        sysconf,
                        bin: Some(exec_path_parent.to_owned()),
                        data,
                        // The docs dir may not exist; in that case fall back to the compiled in path.
                        doc: if doc.exists() {
                            doc
                        } else {
                            PathBuf::from(DOC_DIR)
                        },
                        locale: base_path.join("share/locale"),
                    },
                    exec_path,
                };
            }
        }

        // If we're in Cargo's target directory or in CMake's build directory, use the source files.
        if exec_path.starts_with(env!("FISH_BUILD_DIR")) {
            return Self {
                kind: WorkspaceRoot,
                paths: ConfigPaths {
                    sysconf: workspace_root.join("etc"),
                    bin: Some(exec_path_parent.to_owned()),
                    data: workspace_root.join("share"),
                    doc: workspace_root.join("user_doc/html"),
                    locale: workspace_root.join("share/locale"),
                },
                exec_path,
            };
        }

        Self {
            kind: StaticPathsDueToWeirdLayout,
            paths: ConfigPaths::static_paths(),
            exec_path,
        }
    }

    pub fn log_config_paths(&self) {
        FLOG!(
            config,
            format!("executable path: {}", self.exec_path.display())
        );
        #[cfg(feature = "embed-data")]
        FLOG!(config, "embed-data feature is active, ignoring data paths");
        #[cfg(not(feature = "embed-data"))]
        use ConfigPathKind::*;
        #[cfg(not(feature = "embed-data"))]
        match &self.kind {
            WorkspaceRoot => {
                FLOG!(
                        config,
                        format!(
                            "Running out of build directory, using paths relative to $CARGO_MANIFEST_DIR ({})",
                            workspace_root().display()
                        ),
                    );
            }
            RelocatableTree => {
                FLOG!(config, "Running from relocatable tree");
            }
            StaticPathsDueToInvalidExecPath => {
                FLOG!(config, "Invalid executable path, using compiled-in paths");
            }
            StaticPathsDueToWeirdLayout => {
                FLOG!(
                    config,
                    "Unexpected directory layout, using compiled-in paths"
                );
            }
        }

        let paths = &self.paths;
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
    }
}
