function fish_clipboard_cut
    # Copy current selection to the clipboard then delete it.
    # If no selection is active, do nothing.
    set -l selection (commandline --current-selection)
    if test -z "$selection"
        return 1
    end

    if type -q pbcopy
        printf '%s' $selection | pbcopy
    else if set -q WAYLAND_DISPLAY; and type -q wl-copy
        printf '%s' $selection | wl-copy &
        disown
    else if set -q DISPLAY; and type -q xsel
        printf '%s' $selection | xsel --clipboard
    else if set -q DISPLAY; and type -q xclip
        printf '%s' $selection | xclip -selection clipboard
    else if type -q clip.exe
        printf '%s' $selection | clip.exe
    end

    commandline --function kill-selection end-selection
end
