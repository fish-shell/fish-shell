function fish_update_completions --description "Update man-page based completions"
    # Don't write .pyc files, use the manpath, clean up old completions
    # display progress.
    set -l update_args -B $__fish_data_dir/tools/create_manpage_completions.py --manpath --cleanup-in ~/.config/fish/completions --cleanup-in ~/.config/fish/generated_completions --progress $argv
    if command -qs python3
        python3 $update_args
    else if command -qs python2
        python2 $update_args
    else if command -qs python
        python $update_args
    else
        printf "%s\n" (_ "python executable not found")
        return 1
    end
end
