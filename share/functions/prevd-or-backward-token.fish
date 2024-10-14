function prevd-or-backward-token --description "If commandline is empty, run prevd; else move one argument to the left"
    if test "$(commandline; printf .)" = \n.
        prevd
        commandline -f repaint
        return
    end
    commandline -f backward-token
end
