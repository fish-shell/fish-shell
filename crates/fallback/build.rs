use fish_build_helper::target_os_is_cygwin;

fn main() {
    rsconf::declare_cfg("cygwin", target_os_is_cygwin());
}
