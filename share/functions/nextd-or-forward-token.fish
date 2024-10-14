function nextd-or-forward-token --description "If commandline is empty, run nextd; else move one argument to the right"
    if test "$(commandline; printf .)" = \n.
        nextd
        commandline -f repaint
        return
    end
    commandline -f forward-token
end
