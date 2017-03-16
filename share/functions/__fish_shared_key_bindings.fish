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

    bind $argv -k ppage beginning-of-history
    bind $argv -k npage end-of-history

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
    # ncurses > 6.0 sends a "delete scrollback" sequence along with clear.
    # This string replace removes it.
    bind $argv \cl 'echo -n (clear | string replace \e\[3J ""); commandline -f repaint'
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

    # Support for "bracketed paste"
    # The way it works is that we acknowledge our support by printing
    # \e\[?2004h
    # then the terminal will "bracket" every paste in
    # \e\[200~ and \e\[201~
    # Every character in between those two will be part of the paste and should not cause a binding to execute (like \n executing commands).
    #
    # We enable it after every command and disable it before (in __fish_config_interactive.fish)
    #
    # Support for this seems to be ubiquitous - emacs enables it unconditionally (!) since 25.1 (though it only supports it since then,
    # it seems to be the last term to gain support).
    # TODO: Should we disable this in older emacsen?
    #
    # NOTE: This is more of a "security" measure than a proper feature.
    # The better way to paste remains the `fish_clipboard_paste` function (bound to \cv by default).
    # We don't disable highlighting here, so it will be redone after every character (which can be slow),
    # and it doesn't handle "paste-stop" sequences in the paste (which the terminal needs to strip, but KDE konsole doesn't).
    #
    # See http://thejh.net/misc/website-terminal-copy-paste. The second case will not be caught in KDE konsole.
    # Bind the starting sequence in every bind mode, even user-defined ones.

    # We usually just pass the text through as-is to facilitate pasting code,
    # but when the current token contains an unbalanced single-quote (`'`),
    # we escape all single-quotes and backslashes, effectively turning the paste
    # into one literal token, to facilitate pasting non-code (e.g. markdown or git commitishes)

    # Exclude paste mode or there'll be an additional binding after switching between emacs and vi
    for mode in (bind --list-modes | string match -v paste)
        bind -M $mode -m paste \e\[200~ '__fish_start_bracketed_paste'
    end
    # This sequence ends paste-mode and returns to the previous mode we have saved before.
    bind -M paste \e\[201~ '__fish_stop_bracketed_paste'
    # In paste-mode, everything self-inserts except for the sequence to get out of it
    bind -M paste "" self-insert
    # Without this, a \r will overwrite the other text, rendering it invisible - which makes the exercise kinda pointless.
    # TODO: Test this in windows (\r\n line endings)
    bind -M paste \r "commandline -i \n"
    bind -M paste "'" "__fish_commandline_insert_escaped ' \$__fish_paste_quoted"
    bind -M paste \\ "__fish_commandline_insert_escaped \\ \$__fish_paste_quoted"
end

function __fish_commandline_insert_escaped --description 'Insert the first arg escaped if a second arg is given'
    if set -q argv[2]
        commandline -i \\$argv[1]
    else
        commandline -i $argv[1]
    end
end

function __fish_start_bracketed_paste
    # Save the last bind mode so we can restore it.
    set -g __fish_last_bind_mode $fish_bind_mode
    # If the token is currently single-quoted,
    # we escape single-quotes (and backslashes).
    __fish_commandline_is_singlequoted
    and set -g __fish_paste_quoted 1
end

function __fish_stop_bracketed_paste
    # Restore the last bind mode.
    set fish_bind_mode $__fish_last_bind_mode
    set -e __fish_paste_quoted
    commandline -f force-repaint
end
