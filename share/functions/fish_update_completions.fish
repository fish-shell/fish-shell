function fish_update_completions --description "Update man-page based completions"
    set -l python (__fish_anypython)
    or begin
        printf "%s\n" (_ "python executable not found") >&2
        return 1
    end

    set -l update_args \
        # Don't write .pyc files.
        -B \
        $tools/create_manpage_completions.py \
        # Use the manpath
        --manpath \
        # Clean up old completions
        --cleanup-in $__fish_user_data_dir/generated_completions \
        --cleanup-in $__fish_cache_dir/generated_completions \
        # Display progress
        --progress \
        $argv

    __fish_data_with_file tools/create_manpage_completions.py cat |
        $python $update_args
end
