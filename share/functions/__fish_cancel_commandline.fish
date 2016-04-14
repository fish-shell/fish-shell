# This is meant to be bound to something like \cC.
function __fish_cancel_commandline
    set -l cmd (commandline)
    if test -n "$cmd"
        commandline -C 1000000
        echo (set_color -b bryellow black)"^C"(set_color normal)
        for i in (seq (commandline -L))
            echo ""
        end
        commandline ""
        commandline -f repaint
    end
end
