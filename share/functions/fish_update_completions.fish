function fish_update_completions --description "Update man-page based completions"
    set -l script $__fish_data_dir/tools/create_manpage_completions.py
    set -l python (__fish_anypython)
    or begin
        printf "%s\n" (_ "python executable not found") >&2
        return 1
    end

    if not test -e "$script"
        if not status list-files tools/create_manpage_completions.py &>/dev/null
            echo "Cannot find man page completion generator. Please check your fish installation."
            return 1
        end
        set -l temp (__fish_mktemp_relative -d fish_update_completions)
        for file in create_manpage_completions.py deroff.py
            status get-file tools/$file >$temp/$file
            or return
        end
        set script $temp/create_manpage_completions.py
    end

    # Don't write .pyc files, use the manpath, clean up old completions
    # display progress.
    set -l update_args -B $script --manpath --cleanup-in $__fish_user_data_dir/generated_completions --cleanup-in $__fish_cache_dir/generated_completions --progress $argv
    $python $update_args
end
