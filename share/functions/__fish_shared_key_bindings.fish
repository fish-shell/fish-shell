function __fish_shared_key_bindings -d "Bindings shared between emacs and vi mode"
    # These are some bindings that are supposed to be shared between vi mode and default mode.
    # They are supposed to be unrelated to text-editing (or movement).
    # This takes $argv so the vi-bindings can pass the mode they are valid in.

    if contains -- -h $argv
        or contains -- --help $argv
        echo "Sorry but this function doesn't support -h or --help"
        return 1
    end

    bind $argv \cy yank
    or return # protect against invalid $argv
    bind $argv \ey yank-pop

    # Left/Right arrow
    bind $argv -k right forward-char
    bind $argv -k left backward-char
    bind $argv \e\[C forward-char
    bind $argv \e\[D backward-char
    # Some terminals output these when they're in in keypad mode.
    bind $argv \eOC forward-char
    bind $argv \eOD backward-char

    # Interaction with the system clipboard.
    bind $argv \cx fish_clipboard_copy
    bind $argv \cv fish_clipboard_paste

    bind $argv \e cancel
    bind $argv \t complete
    # shift-tab does a tab complete followed by a search.
    bind $argv --key btab complete-and-search

    bind $argv \e\n "commandline -i \n"
    bind $argv \e\r "commandline -i \n"

    bind $argv -k down down-or-search
    bind $argv -k up up-or-search
    bind $argv \e\[A up-or-search
    bind $argv \e\[B down-or-search
    bind $argv \eOA up-or-search
    bind $argv \eOB down-or-search

    # Alt-left/Alt-right
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
    bind $argv \e\[1\;9C nextd-or-forward-word #iTerm2
    bind $argv \e\[1\;9D prevd-or-backward-word #iTerm2

    # Alt-up/Alt-down
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
    bind $argv \e\[1\;9A history-token-search-backward # iTerm2
    bind $argv \e\[1\;9B history-token-search-forward # iTerm2
    # Bash compatibility
    # https://github.com/fish-shell/fish-shell/issues/89
    bind $argv \e. history-token-search-backward

    bind $argv \el __fish_list_current_token
    bind $argv \ew 'set tok (commandline -pt); if test $tok[1]; echo; whatis $tok[1]; commandline -f repaint; end'
    bind $argv \cl 'clear; commandline -f repaint'
    bind $argv \cc __fish_cancel_commandline
    bind $argv \cu backward-kill-line
    bind $argv \cw backward-kill-path-component
    bind $argv \e\[F end-of-line
    bind $argv \e\[H beginning-of-line

    bind $argv \ed 'set -l cmd (commandline); if test -z "$cmd"; echo; dirh; commandline -f repaint; else; commandline -f kill-word; end'
    bind $argv \cd delete-or-exit

    # Allow reading manpages by pressing F1 (many GUI applications) or Alt+h (like in zsh).
    bind $argv -k f1 __fish_man_page
    bind $argv \eh __fish_man_page

    # This will make sure the output of the current command is paged using the default pager when
    # you press Meta-p.
    # If none is set, less will be used.
    bind $argv \ep '__fish_paginate'

    # Make it easy to turn an unexecuted command into a comment in the shell history. Also,
    # remove the commenting chars so the command can be further edited then executed.
    bind $argv \e\# __fish_toggle_comment_commandline

    # The [meta-e] and [meta-v] keystrokes invoke an external editor on the command buffer.
    bind \ee edit_command_buffer
    bind \ev edit_command_buffer
end
