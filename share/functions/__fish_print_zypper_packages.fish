function __fish_print_zypper_packages
    type -q -f zypper || return 1

    set -l xdg_cache_home (__fish_make_cache_dir)
    or return

    # Use libzypp cache file if available
    if test -f /var/cache/zypp/solv/@System/solv.idx
        awk '!/application:|srcpackage:|product:|pattern:|patch:/ {print $1'\tPackage'}' /var/cache/zypp/solv/*/solv.idx
        return 0
    end

    # If the cache is less than five minutes old, we do not recalculate it

    set -l cache_file $xdg_cache_home/.zypper-cache.$USER
    if test -f $cache_file
        cat $cache_file
        set -l age (math (date +%s) - (stat -c '%Y' $cache_file))
        set -l max_age 300
        if test $age -lt $max_age
            return 0
        end
    end

    # Remove package version information from output and pipe into cache file
    zypper --quiet --non-interactive search --type=package | tail -n +4 | sed -r 's/^. \| ((\w|[-_.])+).*/\1\t'Package'/g' >$cache_file &
    return 0
end
