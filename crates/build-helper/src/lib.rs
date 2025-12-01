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
