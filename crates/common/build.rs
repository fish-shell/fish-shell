use fish_build_helper::target_os_is_apple;

fn main() {
    rsconf::declare_cfg("apple", target_os_is_apple());
}
