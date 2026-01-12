fn main() {
    rsconf::declare_cfg("cygwin", fish_build_helper::target_os_is_cygwin());
}
