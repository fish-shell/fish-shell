function fish_clipboard_copy
    # Copy the current selection, or the entire commandline if that is empty.
    set -l cmdline (commandline --current-selection)
    test -n "$cmdline"; or set cmdline (commandline)
    if type -q pbcopy
        printf '%s\n' $cmdline | pbcopy
    else if type -q xsel
        # Silence error so no error message shows up
        # if e.g. X isn't running.
        printf '%s\n' $cmdline | xsel --clipboard 2>/dev/null
    else if type -q xclip
        printf '%s\n' $cmdline | xclip -selection clipboard 2>/dev/null
    end
end
