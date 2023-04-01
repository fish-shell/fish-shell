function __ghoti_setup_cancel_text -v ghoti_color_cancel -v ghoti_color_normal
    set -g __ghoti_cancel_text "^C"
    if set -q ghoti_color_cancel
        set __ghoti_cancel_text (echo -sn (set_color $ghoti_color_cancel) $__ghoti_cancel_text (set_color normal))
    end
    if command -sq tput
        # Clear to EOL (to erase any autosuggestions)
        set __ghoti_cancel_text (echo -sn $__ghoti_cancel_text (tput el; or tput ce))
    end
end
__ghoti_setup_cancel_text

# This is meant to be bound to something like \cC.
function __ghoti_cancel_commandline
    set -l cmd (commandline)
    if test -n "$cmd"
        echo -sn $__ghoti_cancel_text
        # `commandline -L` prints the line the cursor is on (starting from the prompt), so move the cursor
        # "to the end" then call `commandline -L` to get the total number of lines typed in at the prompt.
        commandline -C 10000000
        printf (string repeat -n (commandline -L) "\n")
        commandline ""
        emit ghoti_cancel
    end

    # cancel: Close the pager if it's open (#4298)
    # repaint: Repaint even if we haven't cancelled anything so the prompt refreshes
    # and the terminal scrolls to it.
    commandline -f cancel -f repaint
end
