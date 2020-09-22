function __fish_print_port_packages
    type -q -f port || return 1

    # port needs caching, as it tends to be slow
    # BSD find is used for determining file age because HFS+ and APFS
    # don't save unix time, but the actual date. Also BSD stat is vastly
    # different from linux stat and converting its time format is tedious
    set -l xdg_cache_home (__fish_make_cache_dir)
    or return

    set -l cache_file $xdg_cache_home/.port-cache.$USER
    if test -e $cache_file
        # Delete if cache is older than 15 minutes
        find "$cache_file" -ctime +15m | awk '{$1=$1;print}' | xargs rm
        if test -f $cache_file
            cat $cache_file
            return
        end
    end
    # Remove trailing whitespace and pipe into cache file
    printf "all\ncurrent\nactive\ninactive\ninstalled\nuninstalled\noutdated" >$cache_file
    port echo all | awk '{$1=$1};1' >>$cache_file &
    cat $cache_file
    return 0
end
