function __fish_cached --description "Cache the command output for a given amount of time"

    argparse --stop-nonopt 't/max_age=' 'k/cache_key=' -- $argv

    if set -q _flag_cache_key
        set cache_key $_flag_cache_key
    else
        set cache_key (string split ' ' "$argv" | head -1)
    end
    set cache_dir (__fish_make_cache_dir)
    set cache_file (path normalize $cache_dir/$cache_key)
    set cache_age (path mtime --relative $cache_file)

    if not test -f $cache_file
        or test $cache_age -gt $_flag_max_age

        eval "$argv" >$cache_file
        __fish_cache_put $cache_file
    end
    cat $cache_file
end
