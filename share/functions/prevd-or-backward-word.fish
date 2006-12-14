function prevd-or-backward-word
    if test -z (commandline)
        prevd
		commandline -f repaint
    else
        commandline -f backward-word
    end
end

