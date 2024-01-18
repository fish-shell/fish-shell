function __fish_print_pacman_packages
    # Caches for 5 minutes
    type -q -f pacman || return 1

    argparse i/installed -- $argv
    or return 1

    set -l xdg_cache_home (__fish_make_cache_dir)
    or return

    if not set -q _flag_installed
        set -l cache_file $xdg_cache_home/pacman
        if test -f $cache_file
            cat $cache_file
            set -l age (path mtime -R -- $cache_file)
            set -l max_age 250
            if test $age -lt $max_age
                return
            end
        end
        # prints: <package name>	Package
        pacman -Ssq | sed -e 's/$/\t'Package'/' >$cache_file &
        return 0
    else
        pacman -Q | string replace ' ' \t
        return 0
    end
end
