function __fish_shared_key_bindings -d "Bindings shared between emacs and vi mode"
    # These are some bindings that are supposed to be shared between vi mode and default mode.
    # They are supposed to be unrelated to text-editing (or movement).
    # This takes $argv so the vi-bindings can pass the mode they are valid in.

    if contains -- -h $argv
        or contains -- --help $argv
        echo "Sorry but this function doesn't support -h or --help" >&2
        return 1
    end

    bind --preset $argv \cy yank
    or return # protect against invalid $argv
    bind --preset $argv \ey yank-pop

    # Left/Right arrow
    bind --preset $argv -k right forward-char
    bind --preset $argv -k left backward-char
    bind --preset $argv \e\[C forward-char
    bind --preset $argv \e\[D backward-char
    # Some terminals output these when they're in in keypad mode.
    bind --preset $argv \eOC forward-char
    bind --preset $argv \eOD backward-char

    # Ctrl-left/right - these also work in vim.
    bind --preset $argv \e\[1\;5C forward-word
    bind --preset $argv \e\[1\;5D backward-word

    bind --preset $argv -k ppage beginning-of-history
    bind --preset $argv -k npage end-of-history

    # Interaction with the system clipboard.
    bind --preset $argv \cx fish_clipboard_copy
    bind --preset $argv \cv fish_clipboard_paste

    bind --preset $argv \e cancel
    bind --preset $argv \t complete
    bind --preset $argv \cs pager-toggle-search
    # shift-tab does a tab complete followed by a search.
    bind --preset $argv --key btab complete-and-search
    bind --preset $argv -k sdc history-pager-delete or backward-delete-char # shifted delete

    bind --preset $argv \e\n "commandline -f expand-abbr; commandline -i \n"
    bind --preset $argv \e\r "commandline -f expand-abbr; commandline -i \n"

    bind --preset $argv -k down down-or-search
    bind --preset $argv -k up up-or-search
    bind --preset $argv \e\[A up-or-search
    bind --preset $argv \e\[B down-or-search
    bind --preset $argv \eOA up-or-search
    bind --preset $argv \eOB down-or-search

    bind --preset $argv -k sright forward-bigword
    bind --preset $argv -k sleft backward-bigword

    # Alt-left/Alt-right
    bind --preset $argv \e\eOC nextd-or-forward-word
    bind --preset $argv \e\eOD prevd-or-backward-word
    bind --preset $argv \e\e\[C nextd-or-forward-word
    bind --preset $argv \e\e\[D prevd-or-backward-word
    bind --preset $argv \eO3C nextd-or-forward-word
    bind --preset $argv \eO3D prevd-or-backward-word
    bind --preset $argv \e\[3C nextd-or-forward-word
    bind --preset $argv \e\[3D prevd-or-backward-word
    bind --preset $argv \e\[1\;3C nextd-or-forward-word
    bind --preset $argv \e\[1\;3D prevd-or-backward-word
    bind --preset $argv \e\[1\;9C nextd-or-forward-word #iTerm2
    bind --preset $argv \e\[1\;9D prevd-or-backward-word #iTerm2

    # Alt-up/Alt-down
    bind --preset $argv \e\eOA history-token-search-backward
    bind --preset $argv \e\eOB history-token-search-forward
    bind --preset $argv \e\e\[A history-token-search-backward
    bind --preset $argv \e\e\[B history-token-search-forward
    bind --preset $argv \eO3A history-token-search-backward
    bind --preset $argv \eO3B history-token-search-forward
    bind --preset $argv \e\[3A history-token-search-backward
    bind --preset $argv \e\[3B history-token-search-forward
    bind --preset $argv \e\[1\;3A history-token-search-backward
    bind --preset $argv \e\[1\;3B history-token-search-forward
    bind --preset $argv \e\[1\;9A history-token-search-backward # iTerm2
    bind --preset $argv \e\[1\;9B history-token-search-forward # iTerm2
    # Bash compatibility
    # https://github.com/fish-shell/fish-shell/issues/89
    bind --preset $argv \e. history-token-search-backward

    bind --preset $argv \el __fish_list_current_token
    bind --preset $argv \eo __fish_preview_current_file
    bind --preset $argv \ew __fish_whatis_current_token
    bind --preset $argv \cl clear-screen
    bind --preset $argv \cc cancel-commandline
    bind --preset $argv \cu backward-kill-line
    bind --preset $argv \cw backward-kill-path-component
    bind --preset $argv \e\[F end-of-line
    bind --preset $argv \e\[H beginning-of-line

    bind --preset $argv \ed 'set -l cmd (commandline); if test -z "$cmd"; echo; dirh; commandline -f repaint; else; commandline -f kill-word; end'
    bind --preset $argv \cd delete-or-exit

    bind --preset $argv \es 'for cmd in sudo doas please; if command -q $cmd; fish_commandline_prepend $cmd; break; end; end'

    # Allow reading manpages by pressing F1 (many GUI applications) or Alt+h (like in zsh).
    bind --preset $argv -k f1 __fish_man_page
    bind --preset $argv \eh __fish_man_page

    # This will make sure the output of the current command is paged using the default pager when
    # you press Meta-p.
    # If none is set, less will be used.
    bind --preset $argv \ep __fish_paginate

    # Make it easy to turn an unexecuted command into a comment in the shell history. Also,
    # remove the commenting chars so the command can be further edited then executed.
    bind --preset $argv \e\# __fish_toggle_comment_commandline

    # The [meta-e] and [meta-v] keystrokes invoke an external editor on the command buffer.
    bind --preset $argv \ee edit_command_buffer
    bind --preset $argv \ev edit_command_buffer

    # Tmux' focus events.
    # Exclude paste mode because that should get _everything_ literally.
    for mode in (bind --list-modes | string match -v paste)
        # We only need the in-focus event currently (to redraw the vi-cursor).
        bind --preset -M $mode \e\[I 'emit fish_focus_in'
        bind --preset -M $mode \e\[O false
        bind --preset -M $mode \e\[\?1004h false
    end

    # Support for "bracketed paste"
    # The way it works is that we acknowledge our support by printing
    # \e\[?2004h
    # then the terminal will "bracket" every paste in
    # \e\[200~ and \e\[201~
    # Every character in between those two will be part of the paste and should not cause a binding to execute (like \n executing commands).
    #
    # We enable it after every command and disable it before (in __fish_config_interactive.fish)
    #
    # Support for this seems to be ubiquitous - emacs enables it unconditionally (!) since 25.1
    # (though it only supports it since then, it seems to be the last term to gain support).
    #
    # NOTE: This is more of a "security" measure than a proper feature.
    # The better way to paste remains the `fish_clipboard_paste` function (bound to \cv by default).
    # We don't disable highlighting here, so it will be redone after every character (which can be slow),
    # and it doesn't handle "paste-stop" sequences in the paste (which the terminal needs to strip).
    #
    # See http://thejh.net/misc/website-terminal-copy-paste.

    # Bind the starting sequence in every bind mode, even user-defined ones.
    # Exclude paste mode or there'll be an additional binding after switching between emacs and vi
    for mode in (bind --list-modes | string match -v paste)
        bind --preset -M $mode -m paste \e\[200~ __fish_start_bracketed_paste
    end
    # This sequence ends paste-mode and returns to the previous mode we have saved before.
    bind --preset -M paste \e\[201~ __fish_stop_bracketed_paste
    # In paste-mode, everything self-inserts except for the sequence to get out of it
    bind --preset -M paste "" self-insert
    # Without this, a \r will overwrite the other text, rendering it invisible - which makes the exercise kinda pointless.
    bind --preset -M paste \r "commandline -i \n"

    # We usually just pass the text through as-is to facilitate pasting code,
    # but when the current token contains an unbalanced single-quote (`'`),
    # we escape all single-quotes and backslashes, effectively turning the paste
    # into one literal token, to facilitate pasting non-code (e.g. markdown or git commitishes)
    bind --preset -M paste "'" "__fish_commandline_insert_escaped \' \$__fish_paste_quoted"
    bind --preset -M paste \\ "__fish_commandline_insert_escaped \\\ \$__fish_paste_quoted"
    # Only insert spaces if we're either quoted or not at the beginning of the commandline
    # - this strips leading spaces if they would trigger histignore.
    bind --preset -M paste " " self-insert-notfirst

    # Bindings that are shared in text-insertion modes.
    if not set -l index (contains --index -- -M $argv)
        or test $argv[(math $index + 1)] = insert

        # This is the default binding, i.e. the one used if no other binding matches
        bind --preset $argv "" self-insert
        or exit # protect against invalid $argv

        # Space and other command terminators expands abbrs _and_ inserts itself.
        bind --preset $argv " " self-insert expand-abbr
        bind --preset $argv ";" self-insert expand-abbr
        bind --preset $argv "|" self-insert expand-abbr
        bind --preset $argv "&" self-insert expand-abbr
        bind --preset $argv ">" self-insert expand-abbr
        bind --preset $argv "<" self-insert expand-abbr
        # Closing a command substitution expands abbreviations
        bind --preset $argv ")" self-insert expand-abbr
        # Ctrl-space inserts space without expanding abbrs
        bind --preset $argv -k nul 'test -n "$(commandline)" && commandline -i " "'
        # Shift-space (CSI u escape sequence) behaves like space because it's easy to mistype.
        bind --preset $argv \e\[32\;2u 'commandline -i " "; commandline -f expand-abbr'


        bind --preset $argv \n execute
        bind --preset $argv \r execute
        # Make Control+Return behave like Return because it's easy to mistype after accepting an autosuggestion.
        bind --preset $argv \e\[27\;5\;13~ execute # Sent with XTerm.vt100.formatOtherKeys: 0
        bind --preset $argv \e\[13\;5u execute # CSI u sequence, sent with XTerm.vt100.formatOtherKeys: 1
        # Same for Shift+Return
        bind --preset $argv \e\[27\;2\;13~ execute
        bind --preset $argv \e\[13\;2u execute
    end
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
    string match -q 'single*' (__fish_tokenizer_state -- (commandline -ct | string collect))
    and set -g __fish_paste_quoted 1
    commandline -f begin-undo-group
end

function __fish_stop_bracketed_paste
    # Restore the last bind mode.
    set fish_bind_mode $__fish_last_bind_mode
    set -e __fish_paste_quoted
    commandline -f end-undo-group
end
