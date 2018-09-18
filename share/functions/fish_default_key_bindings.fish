function fish_default_key_bindings -d "Default (Emacs-like) key bindings for fish"
    if contains -- -h $argv
        or contains -- --help $argv
        echo "Sorry but this function doesn't support -h or --help"
        return 1
    end

    if not set -q argv[1]
        bind --erase --all --default # clear earlier bindings, if any
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
    bind --default $argv "" self-insert
    or exit # protect against invalid $argv

    bind --default $argv \n execute
    bind --default $argv \r execute

    bind --default $argv \ck kill-line

    bind --default $argv \eOC forward-char
    bind --default $argv \eOD backward-char
    bind --default $argv \e\[C forward-char
    bind --default $argv \e\[D backward-char
    bind --default $argv -k right forward-char
    bind --default $argv -k left backward-char

    bind --default $argv -k dc delete-char
    bind --default $argv -k backspace backward-delete-char
    bind --default $argv \x7f backward-delete-char

    # for PuTTY
    # https://github.com/fish-shell/fish-shell/issues/180
    bind --default $argv \e\[1~ beginning-of-line
    bind --default $argv \e\[3~ delete-char
    bind --default $argv \e\[4~ end-of-line

    # OS X SnowLeopard doesn't have these keys. Don't show an annoying error message.
    bind --default $argv -k home beginning-of-line 2>/dev/null
    bind --default $argv -k end end-of-line 2>/dev/null
    bind --default $argv \e\[3\;2~ backward-delete-char # Mavericks Terminal.app shift-ctrl-delete

    bind --default $argv \ca beginning-of-line
    bind --default $argv \ce end-of-line
    bind --default $argv \ch backward-delete-char
    bind --default $argv \cp up-or-search
    bind --default $argv \cn down-or-search
    bind --default $argv \cf forward-char
    bind --default $argv \cb backward-char
    bind --default $argv \ct transpose-chars
    bind --default $argv \et transpose-words
    bind --default $argv \eu upcase-word

    # This clashes with __fish_list_current_token
    # bind --default $argv \el downcase-word
    bind --default $argv \ec capitalize-word
    # One of these is alt+backspace.
    bind --default $argv \e\x7f backward-kill-word
    bind --default $argv \e\b backward-kill-word
    bind --default $argv \eb backward-word
    bind --default $argv \ef forward-word
    bind --default $argv \e\[1\;5C forward-word
    bind --default $argv \e\[1\;5D backward-word
    bind --default $argv \e\< beginning-of-buffer
    bind --default $argv \e\> end-of-buffer

    bind --default $argv \ed kill-word

    # Ignore some known-bad control sequences
    # https://github.com/fish-shell/fish-shell/issues/1917
    bind --default $argv \e\[I 'begin;end'
    bind --default $argv \e\[O 'begin;end'

    # term-specific special bindings
    switch "$TERM"
        case 'rxvt*'
            bind --default $argv \e\[8~ end-of-line
            bind --default $argv \eOc forward-word
            bind --default $argv \eOd backward-word
        case 'xterm-256color'
            # Microsoft's conemu uses xterm-256color plus
            # the following to tell a console to paste:
            bind --default $argv \e\x20ep fish_clipboard_paste
    end
end
