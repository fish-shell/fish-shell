# This is meant to be bound to something like \cC.
function __fish_cancel_commandline
    set -l cmd (commandline)
    if test -n "$cmd"
        commandline -C 1000000
        # TODO: Switch from using tput and standout mode to `set_color` when themes have been
        # augmented to include support for background colors or has support for standout/reverse
        # mode.
        #
        # Set reverse fg/bg color mode, output ^C, restore normal mode, clear to EOL (to erase any
        # autosuggestion).
        if command -sq tput
            echo -ns (set_color -r) "^C" (set_color normal) (tput el; or tput ce)
        else
            echo -n "^C"
        end
        for i in (seq (commandline -L))
            echo ""
        end
        commandline ""
        commandline -f repaint
    end
end
