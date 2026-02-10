fn main() {
    use fish_build_helper::{rebuild_if_embedded_path_changed, workspace_root};
    rebuild_if_embedded_path_changed(workspace_root().join("localization").join("fluent"));
}
