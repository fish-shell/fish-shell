function fish_default_key_bindings -d "emacs-like key binds"
    set -l v1 __fish_bind_filter v1
    set -l v2 __fish_bind_filter v2
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

    $v2 bind --preset $argv [c-k] kill-line
    $v1 bind --preset $argv \ck kill-line

    bind --preset $argv [right] forward-char
    bind --preset $argv [left] backward-char
    $v1 bind --preset $argv \eOC forward-char
    $v1 bind --preset $argv \eOD backward-char
    $v1 bind --preset $argv \e\[C forward-char
    $v1 bind --preset $argv \e\[D backward-char
    $v1 bind --preset $argv -k right forward-char
    $v1 bind --preset $argv -k left backward-char

    bind --preset $argv [del] delete-char
    bind --preset $argv [backspace] backward-delete-char
    bind --preset $argv [s-backspace] backward-delete-char
    $v1 bind --preset $argv -k dc delete-char
    $v1 bind --preset $argv -k backspace backward-delete-char
    $v1 bind --preset $argv \x7f backward-delete-char

    # for PuTTY
    # https://github.com/fish-shell/fish-shell/issues/180
    $v1 bind --preset $argv \e\[1~ beginning-of-line
    $v1 bind --preset $argv \e\[3~ delete-char
    $v1 bind --preset $argv \e\[4~ end-of-line

    bind --preset $argv [home] beginning-of-line
    $v1 bind --preset $argv -k home beginning-of-line
    bind --preset $argv [end] end-of-line
    $v1 bind --preset $argv -k end end-of-line

    $v2 bind --preset $argv [c-a] beginning-of-line
    $v1 bind --preset $argv \ca beginning-of-line
    $v2 bind --preset $argv [c-e] end-of-line
    $v1 bind --preset $argv \ce end-of-line
    $v2 bind --preset $argv [c-h] backward-delete-char
    $v1 bind --preset $argv \ch backward-delete-char
    $v2 bind --preset $argv [c-p] up-or-search
    $v1 bind --preset $argv \cp up-or-search
    $v2 bind --preset $argv [c-n] down-or-search
    $v1 bind --preset $argv \cn down-or-search
    $v2 bind --preset $argv [c-f] forward-char
    $v1 bind --preset $argv \cf forward-char
    $v2 bind --preset $argv [c-b] backward-char
    $v1 bind --preset $argv \cb backward-char
    $v2 bind --preset $argv [c-t] transpose-chars
    $v1 bind --preset $argv \ct transpose-chars
    $v2 bind --preset $argv [c-g] cancel
    $v1 bind --preset $argv \cg cancel
    # v1 breaks unless we add this alias.
    bind --preset $argv [c-/] undo
    $v2 bind --preset $argv [c-_] undo # xterm idiosyncracy
    $v1 bind --preset $argv \c_ undo
    $v2 bind --preset $argv [c-z] undo
    $v1 bind --preset $argv \cz undo
    $v2 bind --preset $argv [a-/] redo
    $v1 bind --preset $argv \e/ redo
    $v2 bind --preset $argv [a-t] transpose-words
    $v1 bind --preset $argv \et transpose-words
    $v2 bind --preset $argv [a-u] upcase-word
    $v1 bind --preset $argv \eu upcase-word

    $v2 bind --preset $argv [a-c] capitalize-word
    $v1 bind --preset $argv \ec capitalize-word
    $v2 bind --preset $argv [a-backspace] backward-kill-word
    # One of these is alt+backspace.
    $v1 bind --preset $argv \e\x7f backward-kill-word
    $v1 bind --preset $argv \e\b backward-kill-word
    $v2 bind --preset $argv [a-b] backward-word
    $v2 bind --preset $argv [a-f] forward-word
    if not test "$TERM_PROGRAM" = Apple_Terminal
        $v1 bind --preset $argv \eb backward-word
        $v1 bind --preset $argv \ef forward-word
    else
        # Terminal.app sends \eb for alt+left, \ef for alt+right.
        # Yeah.
        $v1 bind --preset $argv \eb prevd-or-backward-word
        $v1 bind --preset $argv \ef nextd-or-forward-word
    end

    $v2 bind --preset $argv [a-\<] beginning-of-buffer
    $v1 bind --preset $argv \e\< beginning-of-buffer
    $v2 bind --preset $argv [a-\>] end-of-buffer
    $v1 bind --preset $argv \e\> end-of-buffer

    $v2 bind --preset $argv [a-d] kill-word
    $v1 bind --preset $argv \ed kill-word

    $v2 bind --preset $argv [c-r] history-pager
    $v1 bind --preset $argv \cr history-pager

    # term-specific special bindings
    switch "$TERM"
        case st-256color
            # suckless and bash/zsh/fish have a different approach to how the terminal should be configured;
            # the major effect is that several keys do not work as intended.
            # This is a workaround, there will be additions in he future.
            $v1 bind --preset $argv \e\[P delete-char
            $v1 bind --preset $argv \e\[Z up-line
        case 'rxvt*'
            $v1 bind --preset $argv \e\[8~ end-of-line
            $v1 bind --preset $argv \eOc forward-word
            $v1 bind --preset $argv \eOd backward-word
        case xterm-256color
            # Microsoft's conemu uses xterm-256color plus
            # the following to tell a console to paste:
            $v1 bind --preset $argv \e\x20ep fish_clipboard_paste
    end

    set -e -g fish_cursor_selection_mode
end
