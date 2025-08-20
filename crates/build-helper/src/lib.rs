use std::{
    env,
    path::{Path, PathBuf},
};

pub fn canonicalize<P: AsRef<Path>>(path: P) -> PathBuf {
    std::fs::canonicalize(path).unwrap()
}

pub fn get_repo_root() -> PathBuf {
    let manifest_dir = PathBuf::from(env!("CARGO_MANIFEST_DIR"));
    canonicalize(manifest_dir.ancestors().nth(2).unwrap())
}

pub fn get_target_dir() -> PathBuf {
    option_env!("CARGO_TARGET_DIR")
        .map(canonicalize)
        .unwrap_or(get_repo_root().join("target"))
}

// TODO Move this to rsconf
pub fn rebuild_if_paths_changed<P: AsRef<Path>, I: IntoIterator<Item = P>>(paths: I) {
    for path in paths {
        rsconf::rebuild_if_path_changed(path.as_ref().to_str().unwrap());
    }
}
