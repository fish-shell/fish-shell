function fish_update_completions --description "Update man-page based completions"
    set -l options 'h/help'
    argparse -n open --min-args=0 $options -- $argv
    or return

    if set -q _flag_help
        __fish_print_help fish_update_completions
        return 0
    end

    # Clean up old paths
    set -l update_args -B $__fish_data_dir/tools/create_manpage_completions.py --manpath --cleanup-in '~/.config/fish/completions' --cleanup-in '~/.config/fish/generated_completions' --progress
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
