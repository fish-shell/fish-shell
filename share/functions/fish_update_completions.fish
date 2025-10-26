function fish_update_completions --description "Update man-page based completions"
    set -l detach false
    if test "$fish_update_completions_detach" = true
        set detach true
    end
    set -l python (__fish_anypython)
    or begin
        printf "%s\n" (_ "python executable not found") >&2
        return 1
    end

    set -l update_argv \
        $python \
        # Don't write .pyc files.
        -B \
        - \
        # Use the manpath
        --manpath \
        # Clean up old completions
        --cleanup-in $__fish_user_data_dir/generated_completions \
        --cleanup-in $__fish_cache_dir/generated_completions

    __fish_data_with_file tools/create_manpage_completions.py cat |
        if $detach
            # Run python directly in the background and swallow all output
            # Orphan the job so that it continues to run in case of an early exit (#6269)
            # Note that some distros split the manpage completion script out (#7183).
            # In that case, we silence Python's failure.
            /bin/sh -c '
                c=$(cat)
                ( printf %s "$c" | "$@" ) >/dev/null 2>&1 &
            ' -- $update_argv $argv
        else
            # Display progress
            $update_argv --progress $argv
        end
end
