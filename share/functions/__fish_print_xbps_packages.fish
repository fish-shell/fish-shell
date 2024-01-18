function __fish_print_xbps_packages
    # Caches for 5 minutes
    type -q -f xbps-query || return 1

    argparse i/installed -- $argv
    or return 1

    set -l xdg_cache_home (__fish_make_cache_dir)
    or return

    if not set -q _flag_installed
        set -l cache_file $xdg_cache_home/xbps
        if test -f $cache_file
            set -l age (path mtime -R -- $cache_file)
            set -l max_age 300
            if test $age -lt $max_age
                cat $cache_file
                return
            end
        end
        # prints: <package name>	Package
        xbps-query -Rs "" | sed 's/^... \([^ ]*\)-.* .*/\1/; s/$/\t'Package'/' | tee $cache_file
        return 0
    else
        xbps-query -l | sed 's/^.. \([^ ]*\)-.* .*/\1/' # TODO: actually put package versions in tab for locally installed packages
        return 0
    end
end
