# This is meant to be bound to something like \cC.
function __fish_cancel_commandline
    set -l cmd (commandline)
    if test -n "$cmd"
        commandline -C 1000000
        if set -q fish_color_cancel
            echo -ns (set_color $fish_color_cancel) "^C" (set_color normal)
        else
            echo -ns "^C"
        end
        if command -sq tput
            # Clear to EOL (to erase any autosuggestion).
            echo -n (tput el; or tput ce)
        end
        for i in (seq (commandline -L))
            echo ""
        end
        commandline ""
        commandline -f repaint
    end
end
