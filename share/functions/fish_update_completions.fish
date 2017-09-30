function fish_update_completions --description "Update man-page based completions"
    # Clean up old paths
    set -l update_args -B $__fish_datadir/tools/create_manpage_completions.py --manpath --cleanup-in '~/.config/fish/completions' --cleanup-in '~/.config/fish/generated_completions' --progress
    if command -qs python3
        python3 $update_args
    else if command -qs python2
        python2 $update_args
    else if command -qs python
        python $update_args
    end
end
