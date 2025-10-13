function fish_update_completions --description "Update man-page based completions"
    set -l python (__fish_anypython)
    or begin
        printf "%s\n" (_ "python executable not found") >&2
        return 1
    end

    function __fish_update_completions -V python
        set -l user_args $argv[..-2]
        set -l tools $argv[-1]
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
            $user_args
        $python $update_args
    end
    __fish_data_with_directory tools \
        'create_manpage_completions\.py|deroff\.py' \
        __fish_update_completions $argv
    __fish_with_status functions --erase __fish_update_completions
end
