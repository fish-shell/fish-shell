function fish_status_to_signal --description "Print signal name from argument (\$status), or just argument"
    __fish_make_completion_signals # Make sure signals are cached
    for arg in $argv
        if test $arg -gt 128
            # Important: each element of $__kill_signals is a string like "10 USR1"
            string replace -r --filter '^'(math $arg-128)' ' SIG $__kill_signals |
                read -l signal && echo $signal || echo $arg
        else
            echo $arg
        end
    end
    return 0
end
