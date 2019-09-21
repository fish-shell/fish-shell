function fish_default_key_bindings -d "Default (Emacs-like) key bindings for fish"
    if contains -- -h $argv
        or contains -- --help $argv
        echo "Sorry but this function doesn't support -h or --help"
        return 1
    end

    if not set -q argv[1]
        bind --erase --all --preset # clear earlier bindings, if any
        if test "$fish_key_bindings" != "fish_default_key_bindings"
            # Allow the user to set the variable universally
            set -q fish_key_bindings
            or set -g fish_key_bindings
            # This triggers the handler, which calls us again and ensures the user_key_bindings
            # are executed.
            set fish_key_bindings fish_default_key_bindings
            return
        end
    end

    # Silence warnings about unavailable keys. See #4431, 4188
    if not contains -- -s $argv
        set argv "-s" $argv
    end

    # These are shell-specific bindings that we share with vi mode.
    __fish_shared_key_bindings $argv
    or return # protect against invalid $argv

    # This is the default binding, i.e. the one used if no other binding matches
    bind --preset $argv "" self-insert
    or exit # protect against invalid $argv

    # Space expands abbrs _and_ inserts itself.
    bind --preset $argv " " self-insert expand-abbr

    bind --preset $argv \n execute
    bind --preset $argv \r execute

    bind --preset $argv \ck kill-line

    bind --preset $argv \eOC forward-char
    bind --preset $argv \eOD backward-char
    bind --preset $argv \e\[C forward-char
    bind --preset $argv \e\[D backward-char
    bind --preset $argv -k right forward-char
    bind --preset $argv -k left backward-char

    bind --preset $argv -k dc delete-char
    bind --preset $argv -k backspace backward-delete-char
    bind --preset $argv \x7f backward-delete-char

    # for PuTTY
    # https://github.com/fish-shell/fish-shell/issues/180
    bind --preset $argv \e\[1~ beginning-of-line
    bind --preset $argv \e\[3~ delete-char
    bind --preset $argv \e\[4~ end-of-line

    # OS X SnowLeopard doesn't have these keys. Don't show an annoying error message.
    bind --preset $argv -k home beginning-of-line 2>/dev/null
    bind --preset $argv -k end end-of-line 2>/dev/null
    bind --preset $argv \e\[3\;2~ backward-delete-char # Mavericks Terminal.app shift-ctrl-delete

    bind --preset $argv \ca beginning-of-line
    bind --preset $argv \ce end-of-line
    bind --preset $argv \ch backward-delete-char
    bind --preset $argv \cp up-or-search
    bind --preset $argv \cn down-or-search
    bind --preset $argv \cf forward-char
    bind --preset $argv \cb backward-char
    bind --preset $argv \ct transpose-chars
    bind --preset $argv \et transpose-words
    bind --preset $argv \eu upcase-word

    # This clashes with __fish_list_current_token
    # bind --preset $argv \el downcase-word
    bind --preset $argv \ec capitalize-word
    # One of these is alt+backspace.
    bind --preset $argv \e\x7f backward-kill-word
    bind --preset $argv \e\b backward-kill-word
    bind --preset $argv \eb backward-word
    bind --preset $argv \ef forward-word
    bind --preset $argv \e\[1\;5C forward-word
    bind --preset $argv \e\[1\;5D backward-word
    bind --preset $argv \e\< beginning-of-buffer
    bind --preset $argv \e\> end-of-buffer

    bind --preset $argv \ed kill-word

    # term-specific special bindings
    switch "$TERM"
        case 'rxvt*'
            bind --preset $argv \e\[8~ end-of-line
            bind --preset $argv \eOc forward-word
            bind --preset $argv \eOd backward-word
        case 'xterm-256color'
            # Microsoft's conemu uses xterm-256color plus
            # the following to tell a console to paste:
            bind --preset $argv \e\x20ep fish_clipboard_paste
    end
end
