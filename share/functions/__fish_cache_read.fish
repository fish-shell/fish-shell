function __fish_cache_read -a cache_file max_age
    if not test -f $cache_file
        return 1
    end
    set -l age (path mtime --relative $cache_file)
    if test -n "$max_age" && test $age -gt $max_age
        return 1
    end
    cat $cache_file
end
