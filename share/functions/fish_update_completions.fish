function fish_update_completions --description "Update man-page based completions"
    # Don't write .pyc files, use the manpath, clean up old completions
    # display progress.
    set -l update_args -B $__fish_data_dir/tools/create_manpage_completions.py --manpath --cleanup-in $__fish_user_data_dir/generated_completions --cleanup-in $__fish_cache_dir/generated_completions --progress $argv
    if set -l python (__fish_anypython)
        $python $update_args
    else
        printf "%s\n" (_ "python executable not found") >&2
        return 1
    end
end
