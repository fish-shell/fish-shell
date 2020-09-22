function __fish_print_opkg_packages
    type -q -f opkg || return 1

    argparse i/installed -- $argv
    or return 1

    if set -q _flag_installed
        opkg list-installed 2>/dev/null | sed -r 's/^([a-zA-Z0-9\-]+) - ([a-zA-Z0-9\-]+)/\1\t\2/g'
        return 0
    else
        opkg list 2>/dev/null | sed -r 's/^([a-zA-Z0-9\-]+) - ([a-zA-Z0-9\-]+)/\1\t\2/g'
        return 0
    end
end
