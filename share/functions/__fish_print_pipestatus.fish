function __fish_print_pipestatus --description "Print pipestatus for prompt"
    set -l last_status
    if set -q __fish_last_status
        set last_status $__fish_last_status
    else
        set last_status $argv[-1] # default to $pipestatus[-1]
    end
    set -l left_brace $argv[1]
    set -l right_brace $argv[2]
    set -l separator $argv[3]

    # Colors default to normal
    set -q argv[4]
    and set -l brace_sep_color $argv[4]
    or set -l brace_sep_color normal
    set -q argv[5]
    and set -l status_color $argv[5]
    or set -l status_color normal

    set -e argv[1 2 3 4 5]

    # Only print status codes if the job failed.
    # SIGPIPE (141 = 128 + 13) is usually not a failure, see #6375.
    if not contains $last_status 0 141
        set -l sep $brace_sep_color$separator$status_color
        set -l last_pipestatus_string (__fish_status_to_signal $argv | string join "$sep")
        set -l last_status_string ""
        if test $last_status -ne $argv[-1]
            set last_status_string " "$status_color$last_status
        end
        printf "%s" $brace_sep_color $left_brace \
            $status_color $last_pipestatus_string  \
            $brace_sep_color $right_brace $last_status_string (set_color normal)
    end
end
