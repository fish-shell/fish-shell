use std::{borrow::Cow, env, path::Path};

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
    // FISH_BUILD_DIR is set by CMake, if we are using it.
    option_env!("FISH_BUILD_DIR")
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
