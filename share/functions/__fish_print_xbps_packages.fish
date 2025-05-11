function __fish_print_xbps_packages
    # Caches for 5 minutes
    type -q -f xbps-query || return 1

    argparse i/installed -- $argv
    or return 1

    set -l xdg_cache_home (__fish_make_cache_dir)
    or return

    if not set -q _flag_installed
        set -l cache_file $xdg_cache_home/xbps
        __fish_cache_read $cache_file 300 && return
        __fish_cache_put $cache_file
        # prints: <package name>	Package
        xbps-query -Rs "" | sed 's/^... \([^ ]*\)-.* .*/\1/; s/$/\t'Package'/' | tee $cache_file
        return 0
    else
        xbps-query -l | sed 's/^.. \([^ ]*\)-.* .*/\1/' # TODO: actually put package versions in tab for locally installed packages
        return 0
    end
end
