function prevd-or-backward-word --key-binding
    if test -z (commandline)
        prevd
    else
        commandline -f backward-word
    end
end

