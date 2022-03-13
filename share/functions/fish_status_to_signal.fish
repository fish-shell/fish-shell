function fish_status_to_signal --description "Convert exit code to signal name"
    __fish_make_completion_signals # Make sure signals are cached
    for arg in $argv
        if test $arg -gt 128
            # Important: each element of $__kill_signals is a string like "10 USR1"
            if set -l signal_names (string replace -r --filter '^'(math $arg-128)' ' SIG $__kill_signals)
                # Some signals have multiple mnemonics. Pick the first one.
                echo $signal_names[1]
            else
                echo $arg
            end
        else
            echo $arg
        end
    end
    return 0
end
