function __fish_print_pacman_packages
    # Caches for 5 minutes
    type -q -f pacman || return 1

    argparse i/installed -- $argv
    or return 1

    set -l xdg_cache_home (__fish_make_cache_dir)
    or return

    if not set -q _flag_installed
        set -l cache_file $xdg_cache_home/pacman
        __fish_cache_read $cache_file 250 && return
        __fish_cache_put $cache_file
        # prints: <package name>	Package
        pacman -Ssq | sed -e 's/$/\t'Package'/' >$cache_file &
        return 0
    else
        pacman -Q | string replace ' ' \t
        return 0
    end
end
