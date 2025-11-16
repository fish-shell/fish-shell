# localization: skip(private)
function __fish_cached --description "Cache the command output for a given amount of time"

    argparse --min-args 1 --max-args 1 --stop-nonopt 't/max-age=!_validate_int --min 0' 'k/cache-key=' -- $argv
    or return

    if set -q _flag_cache_key
        set -f cache_key $_flag_cache_key
    else
        set -f cache_key (string trim "$argv" | string split ' ')[1]
    end
    if not string match -q --regex '^[\w+-]+$' -- "$cache_key"
        return 1
    end

    if set -q _flag_max_age
        set -f max_age $_flag_max_age
    else
        set -f max_age -1
    end
    set -l cache_dir (__fish_make_cache_dir)
    or return
    set -l cache_file (path normalize $cache_dir/$cache_key)
    set -l cache_age (path mtime --relative $cache_file)

    set -l populate_cache /bin/sh -c "
        {
            $argv
        } >$cache_file || rm $cache_file 2>/dev/null
    "

    if not test -f $cache_file
        __fish_cache_put $cache_file
        $populate_cache &

        if test -n "$last_pid"
            # wait for at most 1 second if supported
            command --search waitpid &>/dev/null
            and waitpid --exited --timeout 1 $last_pid
            and test -f $cache_file
            and cat $cache_file
        end
    else
        cat $cache_file

        if test $cache_age -gt $max_age
            __fish_cache_put $cache_file
            $populate_cache &
        end
    end
end
