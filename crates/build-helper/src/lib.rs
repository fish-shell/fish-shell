use std::{borrow::Cow, env, os::unix::ffi::OsStrExt, path::Path};

pub fn env_var(name: &str) -> Option<String> {
    let err = match env::var(name) {
        Ok(p) => return Some(p),
        Err(err) => err,
    };
    use env::VarError::*;
    match err {
        NotPresent => None,
        NotUnicode(os_string) => {
            panic!(
                "Environment variable {name} is not valid Unicode: {:?}",
                os_string.as_bytes()
            )
        }
    }
}

pub fn workspace_root() -> &'static Path {
    let manifest_dir = Path::new(env!("CARGO_MANIFEST_DIR"));
    manifest_dir.ancestors().nth(2).unwrap()
}

fn cargo_target_dir() -> Cow<'static, Path> {
    option_env!("CARGO_TARGET_DIR")
        .map(|d| Cow::Borrowed(Path::new(d)))
        .unwrap_or(Cow::Owned(workspace_root().join("target")))
}

pub fn fish_build_dir() -> Cow<'static, Path> {
    option_env!("FISH_CMAKE_BINARY_DIR")
        .map(|d| Cow::Borrowed(Path::new(d)))
        .unwrap_or(cargo_target_dir())
}

// TODO Move this to rsconf
pub fn rebuild_if_path_changed<P: AsRef<Path>>(path: P) {
    rsconf::rebuild_if_path_changed(path.as_ref().to_str().unwrap());
}

// TODO Move this to rsconf
pub fn rebuild_if_paths_changed<P: AsRef<Path>, I: IntoIterator<Item = P>>(paths: I) {
    for path in paths {
        rsconf::rebuild_if_path_changed(path.as_ref().to_str().unwrap());
    }
}

pub fn rebuild_if_embedded_path_changed<P: AsRef<Path>>(path: P) {
    // Not necessary in debug builds, where rust-embed reads from the filesystem.
    if cfg!(any(not(debug_assertions), windows)) {
        rebuild_if_path_changed(path);
    }
}

// Target OS for compiling our crates, as opposed to the build script.
pub fn target_os() -> String {
    env_var("CARGO_CFG_TARGET_OS").unwrap()
}

pub fn target_os_is_apple() -> bool {
    matches!(target_os().as_str(), "ios" | "macos")
}

/// Detect if we're being compiled for a BSD-derived OS, allowing targeting code conditionally with
/// `#[cfg(bsd)]`.
///
/// Rust offers fine-grained conditional compilation per-os for the popular operating systems, but
/// doesn't necessarily include less-popular forks nor does it group them into families more
/// specific than "windows" vs "unix" so we can conditionally compile code for BSD systems.
pub fn target_os_is_bsd() -> bool {
    let target_os = target_os();
    let is_bsd = target_os.ends_with("bsd") || target_os == "dragonfly";
    if matches!(
        target_os.as_str(),
        "dragonfly" | "freebsd" | "netbsd" | "openbsd"
    ) {
        assert!(is_bsd, "Target incorrectly detected as not BSD!");
    }
    is_bsd
}

pub fn target_os_is_cygwin() -> bool {
    target_os() == "cygwin"
}
