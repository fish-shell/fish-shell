function __fish_print_port_packages
    type -q -f port || return 1

    # port needs caching, as it tends to be slow
    set -l xdg_cache_home (__fish_make_cache_dir)
    or return

    set -l cache_file $xdg_cache_home/port
    if test -f $cache_file
        cat $cache_file
        set -l age (path mtime -R -- $cache_file)
        set -l max_age 250
        if test $age -lt $max_age
            return
        end
    end

    # Remove trailing whitespace and pipe into cache file
    printf "all\ncurrent\nactive\ninactive\ninstalled\nuninstalled\noutdated" >$cache_file
    port echo all | awk '{$1=$1};1' >>$cache_file &
    cat $cache_file
    return 0
end
