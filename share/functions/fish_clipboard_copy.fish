function fish_clipboard_copy
    if type -q pbcopy
        commandline | pbcopy
    else if type -q xsel
        # Silence error so no error message shows up
        # if e.g. X isn't running.
        commandline | xsel --clipboard 2>/dev/null
    else if type -q xclip
        commandline | xclip -selection clipboard 2>/dev/null
    end
end
