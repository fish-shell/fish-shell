function prevd-or-backward-word
    if test -z (commandline)
        prevd
    else
        commandline -f backward-word
    end
end

