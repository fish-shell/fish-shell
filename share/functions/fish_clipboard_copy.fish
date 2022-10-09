function fish_clipboard_copy
    set -l cmdline
    if isatty stdin
        # Copy the current selection, or the entire commandline if that is empty.
        # Don't use `string collect -N` here - `commandline` adds a newline.
        set cmdline (commandline --current-selection | string collect)
        test -n "$cmdline"; or set cmdline (commandline | string collect)
    else
        # Read from stdin
        while read -lz line
            set -a cmdline $line
        end
    end

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

    # Copy with OSC 52; useful if we are running in an SSH session or in
    # a container.
    if type -q base64
        if not isatty stdout
            echo "fish_clipboard_copy: stdout is not a terminal" >&2
            return 1
        end
        set -l encoded (printf %s $cmdline | base64 | string join '')
        printf '\e]52;c;%s\a' "$encoded"
    end
end
