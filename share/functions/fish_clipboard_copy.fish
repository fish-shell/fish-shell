function fish_clipboard_copy
    # Copy the current selection, or the entire commandline if that is empty.
    set -l cmdline (commandline --current-selection | string collect)
    test -n "$cmdline"; or set cmdline (commandline | string collect)
    if type -q pbcopy
        printf '%s' $cmdline | pbcopy
    else if set -q WAYLAND_DISPLAY; and type -q wl-copy
        printf '%s' $cmdline | wl-copy
    else if set -q DISPLAY; and type -q xsel
        printf '%s' $cmdline | xsel --clipboard
    else if set -q DISPLAY; and type -q xclip
        printf '%s' $cmdline | xclip -selection clipboard
    else if type -q clip.exe
        printf '%s' $cmdline | clip.exe
    end
end
