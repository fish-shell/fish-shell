fn main() {
    use fish_build_helper::{ftl_dir, rebuild_if_embedded_path_changed};
    rebuild_if_embedded_path_changed(ftl_dir());
}
