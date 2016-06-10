
function fish_default_key_bindings -d "Default (Emacs-like) key bindings for fish"
    if not set -q argv[1]
        if test "$fish_key_bindings" != "fish_default_key_bindings"
            # Allow the user to set the variable universally
            set -q fish_key_bindings
            or set -g fish_key_bindings
            set fish_key_bindings fish_default_key_bindings # This triggers the handler, which calls us again and ensures the user_key_bindings are executed
            return
        end
        # Clear earlier bindings, if any
        bind --erase --all
    end

    # This is the default binding, i.e. the one used if no other binding matches
    bind $argv "" self-insert

    bind $argv \n execute
    bind $argv \r execute

    bind $argv \ck kill-line
    bind $argv \cy fish_clipboard_copy
    bind $argv \cv fish_clipboard_paste
    bind $argv \t complete

    bind $argv \e\n "commandline -i \n"
    bind $argv \e\r "commandline -i \n"

    bind $argv \e\[A up-or-search
    bind $argv \e\[B down-or-search
    bind $argv -k down down-or-search
    bind $argv -k up up-or-search

    # Some linux VTs output these (why?)
    bind $argv \eOA up-or-search
    bind $argv \eOB down-or-search
    bind $argv \eOC forward-char
    bind $argv \eOD backward-char

    bind $argv \e\[C forward-char
    bind $argv \e\[D backward-char
    bind $argv -k right forward-char
    bind $argv -k left backward-char

    bind $argv -k dc delete-char
    bind $argv -k backspace backward-delete-char
    bind $argv \x7f backward-delete-char

    bind $argv \e\[H beginning-of-line
    bind $argv \e\[F end-of-line

    # for PuTTY
    # https://github.com/fish-shell/fish-shell/issues/180
    bind $argv \e\[1~ beginning-of-line
    bind $argv \e\[3~ delete-char
    bind $argv \e\[4~ end-of-line

    # OS X SnowLeopard doesn't have these keys. Don't show an annoying error message.
    bind $argv -k home beginning-of-line 2>/dev/null
    bind $argv -k end end-of-line 2>/dev/null
    bind $argv \e\[3\;2~ backward-delete-char # Mavericks Terminal.app shift-delete

    bind $argv \e\eOC nextd-or-forward-word
    bind $argv \e\eOD prevd-or-backward-word
    bind $argv \e\e\[C nextd-or-forward-word
    bind $argv \e\e\[D prevd-or-backward-word
    bind $argv \eO3C nextd-or-forward-word
    bind $argv \eO3D prevd-or-backward-word
    bind $argv \e\[3C nextd-or-forward-word
    bind $argv \e\[3D prevd-or-backward-word
    bind $argv \e\[1\;3C nextd-or-forward-word
    bind $argv \e\[1\;3D prevd-or-backward-word

    bind $argv \e\eOA history-token-search-backward
    bind $argv \e\eOB history-token-search-forward
    bind $argv \e\e\[A history-token-search-backward
    bind $argv \e\e\[B history-token-search-forward
    bind $argv \eO3A history-token-search-backward
    bind $argv \eO3B history-token-search-forward
    bind $argv \e\[3A history-token-search-backward
    bind $argv \e\[3B history-token-search-forward
    bind $argv \e\[1\;3A history-token-search-backward
    bind $argv \e\[1\;3B history-token-search-forward

    bind $argv \ca beginning-of-line
    bind $argv \ce end-of-line
    bind $argv \ey yank-pop
    bind $argv \ch backward-delete-char
    bind $argv \cp up-or-search
    bind $argv \cn down-or-search
    bind $argv \cf forward-char
    bind $argv \cb backward-char
    bind $argv \ct transpose-chars
    bind $argv \et transpose-words
    bind $argv \eu upcase-word

    # This clashes with __fish_list_current_token
    # bind $argv \el downcase-word
    bind $argv \ec capitalize-word
    bind $argv \e\x7f backward-kill-word
    bind $argv \eb backward-word
    bind $argv \ef forward-word
    bind $argv \e\[1\;5C forward-word
    bind $argv \e\[1\;5D backward-word
    bind $argv \e\[1\;9A history-token-search-backward # iTerm2
    bind $argv \e\[1\;9B history-token-search-forward # iTerm2
    bind $argv \e\[1\;9C nextd-or-forward-word #iTerm2
    bind $argv \e\[1\;9D prevd-or-backward-word #iTerm2
    # Bash compatibility
    # https://github.com/fish-shell/fish-shell/issues/89
    bind $argv \e. history-token-search-backward
    bind $argv -k ppage beginning-of-history
    bind $argv -k npage end-of-history
    bind $argv \e\< beginning-of-buffer
    bind $argv \e\> end-of-buffer

    bind $argv \el __fish_list_current_token
    bind $argv \ew 'set tok (commandline -pt); if test $tok[1]; echo; whatis $tok[1]; commandline -f repaint; end'
    bind $argv \cl 'clear; commandline -f repaint'
    bind $argv \cc __fish_cancel_commandline
    bind $argv \cu backward-kill-line
    bind $argv \cw backward-kill-path-component
    bind $argv \ed 'set -l cmd (commandline); if test -z "$cmd"; echo; dirh; commandline -f repaint; else; commandline -f kill-word; end'
    bind $argv \cd delete-or-exit

    bind \ed forward-kill-word
    bind \ed kill-word

    # Allow reading manpages by pressing F1 (many GUI applications) or Alt+h (like in zsh)
    bind $argv -k f1 __fish_man_page
    bind $argv \eh __fish_man_page

    # This will make sure the output of the current command is paged using the default pager when you press Meta-p
    # If none is set, less will be used
    bind $argv \ep '__fish_paginate'

    # shift-tab does a tab complete followed by a search
    bind $argv --key btab complete-and-search

    # escape cancels stuff	
    bind \e cancel

    # Ignore some known-bad control sequences
    # https://github.com/fish-shell/fish-shell/issues/1917
    bind \e\[I 'begin;end'
    bind \e\[O 'begin;end'

    # term-specific special bindings
    switch "$TERM"
        case 'rxvt*'
            bind $argv \e\[8~ end-of-line
            bind $argv \eOc forward-word
            bind $argv \eOd backward-word
    end

    # Make it easy to turn an unexecuted command into a comment in the shell history. Also,
    # remove the commenting chars so the command can be further edited then executed.
    bind \e\# __fish_toggle_comment_commandline
end
