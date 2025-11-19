function fish_default_key_bindings -d "emacs-like key binds"
    if contains -- -h $argv
        or contains -- --help $argv
        echo "Sorry but this function doesn't support -h or --help"
        return 1
    end

    if not set -q argv[1]
        bind --erase --all --preset # clear earlier bindings, if any
        if test "$fish_key_bindings" != fish_default_key_bindings
            __fish_change_key_bindings fish_default_key_bindings || return
            set fish_bind_mode default
        end
    end

    # Silence warnings about unavailable keys. See #4431, 4188
    if not contains -- -s $argv
        set argv -s $argv
    end

    # These are shell-specific bindings that we share with vi mode.
    __fish_shared_key_bindings $argv
    or return # protect against invalid $argv

    bind --preset $argv right forward-char
    bind --preset $argv left backward-char

    bind --preset $argv delete delete-char
    bind --preset $argv backspace backward-delete-char
    bind --preset $argv shift-backspace backward-delete-char

    bind --preset $argv home beginning-of-line
    bind --preset $argv end end-of-line

    bind --preset $argv ctrl-a beginning-of-line
    bind --preset $argv ctrl-e end-of-line
    bind --preset $argv ctrl-h backward-delete-char
    bind --preset $argv ctrl-p up-or-search
    bind --preset $argv ctrl-n down-or-search
    bind --preset $argv ctrl-f forward-char
    bind --preset $argv ctrl-b backward-char
    bind --preset $argv ctrl-t transpose-chars
    bind --preset $argv ctrl-g cancel
    bind --preset $argv ctrl-/ undo
    bind --preset $argv ctrl-_ undo # XTerm idiosyncracy, can get rid of this once we go full CSI u
    bind --preset $argv ctrl-z undo
    bind --preset $argv ctrl-shift-z redo
    bind --preset $argv alt-/ redo
    bind --preset $argv alt-t transpose-words
    bind --preset $argv alt-u upcase-word

    bind --preset $argv alt-c capitalize-word
    __fish_per_os_bind --preset $argv alt-backspace backward-kill-word backward-kill-token
    __fish_per_os_bind --preset $argv ctrl-alt-h backward-kill-word backward-kill-token
    __fish_per_os_bind --preset $argv ctrl-backspace backward-kill-token backward-kill-word
    __fish_per_os_bind --preset $argv alt-delete kill-word kill-token
    __fish_per_os_bind --preset $argv ctrl-delete kill-token kill-word

    bind --preset $argv alt-\< beginning-of-buffer
    bind --preset $argv alt-\> end-of-buffer

    bind --preset $argv ctrl-r history-pager

    # TODO(term-workaround)
    switch "$TERM"
        case xterm-256color
            # Microsoft's conemu uses xterm-256color plus
            # the following to tell a console to paste:
            bind --preset $argv \e\x20ep fish_clipboard_paste
    end
end
