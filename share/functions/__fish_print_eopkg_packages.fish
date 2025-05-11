function __fish_print_eopkg_packages
    type -q -f eopkg || return 1

    argparse i/installed -- $argv
    or return 1

    set -l xdg_cache_home (__fish_make_cache_dir)
    or return

    # If the cache is less than max_age, we do not recalculate it
    # Determine whether to print installed/available packages

    if set -q _flag_installed
        set -l cache_file $xdg_cache_home/eopkg-installed
        __fish_cache_read $cache_file 500 && return
        __fish_cache_put $cache_file
        # Remove package version information from output and pipe into cache file
        eopkg list-installed -N | cut -d ' ' -f 1 >$cache_file &
        return 0
    else
        set -l cache_file $xdg_cache_home/eopkg-available
        __fish_cache_read $cache_file 500 && return
        __fish_cache_put $cache_file
        # Remove package version information from output and pipe into cache file
        eopkg list-available -N | cut -d ' ' -f 1 >$cache_file &
        return 0
    end
    return 1
end
