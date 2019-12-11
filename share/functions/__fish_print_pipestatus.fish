function __fish_print_pipestatus --description "Print pipestatus for prompt"
    set -l left_brace $argv[1]
    set -l right_brace $argv[2]
    set -l separator $argv[3]
    set -l brace_sep_color $argv[4]
    set -l status_color $argv[5]
    set -e argv[1 2 3 4 5]

    # only output status codes if some process in the pipe failed
    # SIGPIPE (141 = 128 + 13) is usually not a failure, see #6375.
    if string match -qvr '^(0|141)$' $argv
        set -l sep (set_color normal){$brace_sep_color}{$separator}(set_color normal){$status_color}
        set -l last_pipestatus_string (string join "$sep" (__fish_pipestatus_with_signal $argv))
        printf "%s%s%s%s%s%s%s%s%s%s" (set_color normal )$brace_sep_color $left_brace \
            (set_color normal) $status_color $last_pipestatus_string (set_color normal) \
            $brace_sep_color $right_brace (set_color normal)
    end
end
