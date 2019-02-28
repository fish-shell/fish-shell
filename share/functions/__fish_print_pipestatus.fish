function __fish_print_pipestatus --description "Print pipestatus for prompt"
    # maybe these could be in global variables similar to __fish_color_status to allow
    # users to set the variables to modify the braces/separator/color used
    set -l left_brace $argv[1]
    set -l right_brace $argv[2]
    set -l separator $argv[3]
    set -l brace_sep_color $argv[4]
    set -l status_color $argv[5]
    set -e argv[1 2 3 4 5]

    # only output $pipestatus if there was a pipe and any part of it had non-zero exit status
    if set -q argv[2] && string match -qvr '^0$' $argv
        set -l sep (set_color normal){$brace_sep_color}{$separator}(set_color normal){$status_color}
        set -l last_pipestatus_string (string join "$sep" (__fish_pipestatus_with_signal $argv))
        printf "%s%s%s%s%s%s%s%s%s" $brace_sep_color $left_brace (set_color normal) \
               $status_color $last_pipestatus_string (set_color normal) $brace_sep_color \
               $right_brace (set_color normal)
    end
end
