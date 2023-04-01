function __ghoti_print_packages
    # This is `__ghoti_print_packages`. It prints packages,
    # from the first package manager it finds.
    # That's a pretty bad idea, which is why this is broken up,
    # and only available for legacy reasons.
    __ghoti_print_apt_packages $argv
    and return

    __ghoti_print_pkg_packages $argv
    and return

    __ghoti_print_pkg_add_packages $argv
    and return

    __ghoti_print_pacman_packages $argv
    and return

    __ghoti_print_rpm_packages $argv
    and return

    __ghoti_print_eopkg_packages $argv
    and return

    __ghoti_print_portage_packages $argv
    and return

    __ghoti_print_port_packages $argv
    and return

    __ghoti_print_xbps_packages $argv
    and return
end
