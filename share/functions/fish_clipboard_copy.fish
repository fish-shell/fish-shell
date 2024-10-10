function fish_clipboard_copy
    set -l cmdline
    if isatty stdin
        # Copy the current selection, or the entire commandline if that is empty.
        # Don't use `string collect -N` here - `commandline` adds a newline.
        set cmdline (commandline --current-selection | __fish_indent --only-indent | string collect)
        test -n "$cmdline"; or set cmdline (commandline | __fish_indent --only-indent | string collect)
    else
        # Read from stdin
        while read -lz line
            set -a cmdline $line
        end
    end

    if type -q pbcopy
        printf '%s' $cmdline | pbcopy
    else if set -q WAYLAND_DISPLAY; and type -q wl-copy
        printf '%s' $cmdline | wl-copy &
        disown
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
        # tmux requires user configuration to interpret OSC 52 on stdout.
        # Luckily we can still make this work for the common case by bypassing
        # tmux and writing to its underlying terminal.
        if set -q TMUX
            set -l tmux_tty (tmux display-message -p '#{client_tty}')
            or return 1
            # The terminal might not be writable if we switched user.
            if test -w $tmux_tty
                printf '\e]52;c;%s\a' "$encoded" >$tmux_tty
            end
        end
    end
end
