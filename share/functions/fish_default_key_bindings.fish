function fish_default_key_bindings -d "emacs-like key binds"
    set -l legacy_bind bind
    if contains -- -h $argv
        or contains -- --help $argv
        echo "Sorry but this function doesn't support -h or --help"
        return 1
    end

    if not set -q argv[1]
        bind --erase --all --preset # clear earlier bindings, if any
        if test "$fish_key_bindings" != fish_default_key_bindings
            # Allow the user to set the variable universally
            set -q fish_key_bindings
            or set -g fish_key_bindings
            # This triggers the handler, which calls us again and ensures the user_key_bindings
            # are executed.
            set fish_key_bindings fish_default_key_bindings
            # unless the handler somehow doesn't exist, which would leave us without bindings.
            # this happens in no-config mode.
            functions -q __fish_reload_key_bindings
            and return
        end
    end

    # Silence warnings about unavailable keys. See #4431, 4188
    if not contains -- -s $argv
        set argv -s $argv
    end

    # These are shell-specific bindings that we share with vi mode.
    __fish_shared_key_bindings $argv
    or return # protect against invalid $argv

    bind --preset $argv ctrl-k kill-line

    bind --preset $argv right forward-char
    bind --preset $argv left backward-char
    $legacy_bind --preset $argv \eOC forward-char
    $legacy_bind --preset $argv \eOD backward-char
    $legacy_bind --preset $argv \e\[C forward-char
    $legacy_bind --preset $argv \e\[D backward-char
    $legacy_bind --preset $argv -k right forward-char
    $legacy_bind --preset $argv -k left backward-char

    bind --preset $argv delete delete-char
    bind --preset $argv backspace backward-delete-char
    bind --preset $argv shift-backspace backward-delete-char

    # for PuTTY
    # https://github.com/fish-shell/fish-shell/issues/180
    $legacy_bind --preset $argv \e\[1~ beginning-of-line
    $legacy_bind --preset $argv \e\[3~ delete-char
    $legacy_bind --preset $argv \e\[4~ end-of-line

    bind --preset $argv home beginning-of-line
    $legacy_bind --preset $argv -k home beginning-of-line
    bind --preset $argv end end-of-line
    $legacy_bind --preset $argv -k end end-of-line

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
    bind --preset $argv alt-/ redo
    bind --preset $argv alt-t transpose-words
    bind --preset $argv alt-u upcase-word

    bind --preset $argv alt-c capitalize-word
    bind --preset $argv alt-backspace backward-kill-word
    bind --preset $argv alt-b backward-word
    bind --preset $argv alt-f forward-word
    if test "$TERM_PROGRAM" = Apple_Terminal
        # Terminal.app sends \eb for alt+left, \ef for alt+right.
        # Yeah.
        $legacy_bind --preset $argv alt-b prevd-or-backward-word
        $legacy_bind --preset $argv alt-f nextd-or-forward-word
    end

    bind --preset $argv alt-\< beginning-of-buffer
    bind --preset $argv alt-\> end-of-buffer

    bind --preset $argv alt-d kill-word

    bind --preset $argv ctrl-r history-pager

    # term-specific special bindings
    switch "$TERM"
        case st-256color
            # suckless and bash/zsh/fish have a different approach to how the terminal should be configured;
            # the major effect is that several keys do not work as intended.
            # This is a workaround, there will be additions in he future.
            $legacy_bind --preset $argv \e\[P delete-char
            $legacy_bind --preset $argv \e\[Z up-line
        case 'rxvt*'
            $legacy_bind --preset $argv \e\[8~ end-of-line
            $legacy_bind --preset $argv \eOc forward-word
            $legacy_bind --preset $argv \eOd backward-word
        case xterm-256color
            # Microsoft's conemu uses xterm-256color plus
            # the following to tell a console to paste:
            $legacy_bind --preset $argv \e\x20ep fish_clipboard_paste
    end

    set -e -g fish_cursor_selection_mode
end
