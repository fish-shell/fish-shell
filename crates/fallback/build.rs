use fish_build_helper::detect_cygwin;

fn main() {
    rsconf::declare_cfg("cygwin", detect_cygwin())
}
