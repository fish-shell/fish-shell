function __fish_print_pipestatus --description "Print pipestatus for prompt"
    # take $status as optional argument to maintain compatibility
    set -l last_status
    if set last_status (string match -r -- '^\d+$' $argv[1])
        set -e argv[1]
    else
        set last_status $argv[-1] # default to $pipestatus[-1]
    end
    set -l left_brace $argv[1]
    set -l right_brace $argv[2]
    set -l separator $argv[3]
    set -l brace_sep_color $argv[4]
    set -l status_color $argv[5]
    set -e argv[1 2 3 4 5]

    # Only print status codes if the job failed.
    # SIGPIPE (141 = 128 + 13) is usually not a failure, see #6375.
    if test $last_status -ne 0 && test $last_status -ne 141
        set -l sep (set_color normal){$brace_sep_color}{$separator}(set_color normal){$status_color}
        set -l last_pipestatus_string (string join "$sep" (__fish_pipestatus_with_signal $argv))
        set -l last_status_string ""
        if test $last_status -ne $argv[-1]
            set last_status_string " "$status_color$last_status
        end
        printf "%s%s%s%s%s%s%s%s%s%s%s" (set_color normal )$brace_sep_color $left_brace \
            (set_color normal) $status_color $last_pipestatus_string (set_color normal) \
            $brace_sep_color $right_brace $last_status_string (set_color normal)
    end
end
