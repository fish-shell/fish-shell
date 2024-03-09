function __fish_shared_key_bindings -d "Bindings shared between emacs and vi mode"
    set -l v1 __fish_bind_filter v1
    set -l v2 __fish_bind_filter v2
    # These are some bindings that are supposed to be shared between vi mode and default mode.
    # They are supposed to be unrelated to text-editing (or movement).
    # This takes $argv so the vi-bindings can pass the mode they are valid in.

    if contains -- -h $argv
        or contains -- --help $argv
        echo "Sorry but this function doesn't support -h or --help" >&2
        return 1
    end

    $v2 bind --preset $argv [c-y] yank
    bind --preset $argv \cy yank
    or return # protect against invalid $argv
    bind --preset $argv \ey yank-pop

    # Left/Right arrow
    $v2 bind --preset $argv [right] forward-char
    $v2 bind --preset $argv [left] backward-char
    $v1 bind --preset $argv -k right forward-char
    $v1 bind --preset $argv -k left backward-char
    $v1 bind --preset $argv \e\[C forward-char
    $v1 bind --preset $argv \e\[D backward-char
    # Some terminals output these when they're in in keypad mode.
    $v1 bind --preset $argv \eOC forward-char
    $v1 bind --preset $argv \eOD backward-char

    # Ctrl-left/right - these also work in vim.
    $v2 bind --preset $argv [c-right] forward-word
    $v2 bind --preset $argv [c-left] backward-word
    $v1 bind --preset $argv \e\[1\;5C forward-word
    $v1 bind --preset $argv \e\[1\;5D backward-word

    bind --preset $argv [pageup] beginning-of-history
    bind --preset $argv [pagedown] end-of-history
    $v1 bind --preset $argv -k ppage beginning-of-history
    $v1 bind --preset $argv -k npage end-of-history

    # Interaction with the system clipboard.
    $v2 bind --preset $argv [c-x] fish_clipboard_copy
    $v2 bind --preset $argv [c-v] fish_clipboard_paste
    $v1 bind --preset $argv \cx fish_clipboard_copy
    $v1 bind --preset $argv \cv fish_clipboard_paste

    bind --preset $argv [esc] cancel
    bind --preset $argv [c-lbrack] cancel
    bind --preset $argv [tab] complete
    $v2 bind --preset $argv [c-i] complete
    $v2 bind --preset $argv [c-s] pager-toggle-search
    $v1 bind --preset $argv \cs pager-toggle-search
    # shift-tab does a tab complete followed by a search.
    $v2 bind --preset $argv [s-tab] complete-and-search
    $v1 bind --preset $argv -k btab complete-and-search
    $v2 bind --preset $argv [s-del] history-pager-delete or backward-delete-char
    $v1 bind --preset $argv -k sdc history-pager-delete or backward-delete-char

    $v2 bind --preset $argv [a-ret] "commandline -f expand-abbr; commandline -i \n"
    $v1 bind --preset $argv \e\n "commandline -f expand-abbr; commandline -i \n"
    $v1 bind --preset $argv \e\r "commandline -f expand-abbr; commandline -i \n"

    bind --preset $argv [down] down-or-search
    $v1 bind --preset $argv -k down down-or-search
    bind --preset $argv [up] up-or-search
    $v1 bind --preset $argv -k up up-or-search
    $v1 bind --preset $argv \e\[A up-or-search
    $v1 bind --preset $argv \e\[B down-or-search
    $v1 bind --preset $argv \eOA up-or-search
    $v1 bind --preset $argv \eOB down-or-search

    $v2 bind --preset $argv [s-right] forward-bigword
    $v2 bind --preset $argv [s-left] backward-bigword
    $v1 bind --preset $argv -k sright forward-bigword
    $v1 bind --preset $argv -k sleft backward-bigword

    $v2 bind --preset $argv [a-right] nextd-or-forward-word
    $v2 bind --preset $argv [a-left] prevd-or-backward-word
    $v1 bind --preset $argv \e\eOC nextd-or-forward-word
    $v1 bind --preset $argv \e\eOD prevd-or-backward-word
    $v1 bind --preset $argv \e\e\[C nextd-or-forward-word
    $v1 bind --preset $argv \e\e\[D prevd-or-backward-word
    $v1 bind --preset $argv \eO3C nextd-or-forward-word
    $v1 bind --preset $argv \eO3D prevd-or-backward-word
    $v1 bind --preset $argv \e\[3C nextd-or-forward-word
    $v1 bind --preset $argv \e\[3D prevd-or-backward-word
    $v1 bind --preset $argv \e\[1\;3C nextd-or-forward-word
    $v1 bind --preset $argv \e\[1\;3D prevd-or-backward-word
    $v1 bind --preset $argv \e\[1\;9C nextd-or-forward-word #iTerm2
    $v1 bind --preset $argv \e\[1\;9D prevd-or-backward-word #iTerm2

    $v2 bind --preset $argv [a-up] history-token-search-backward
    $v2 bind --preset $argv [a-down] history-token-search-forward
    $v1 bind --preset $argv \e\eOA history-token-search-backward
    $v1 bind --preset $argv \e\eOB history-token-search-forward
    $v1 bind --preset $argv \e\e\[A history-token-search-backward
    $v1 bind --preset $argv \e\e\[B history-token-search-forward
    $v1 bind --preset $argv \eO3A history-token-search-backward
    $v1 bind --preset $argv \eO3B history-token-search-forward
    $v1 bind --preset $argv \e\[3A history-token-search-backward
    $v1 bind --preset $argv \e\[3B history-token-search-forward
    $v1 bind --preset $argv \e\[1\;3A history-token-search-backward
    $v1 bind --preset $argv \e\[1\;3B history-token-search-forward
    $v1 bind --preset $argv \e\[1\;9A history-token-search-backward # iTerm2
    $v1 bind --preset $argv \e\[1\;9B history-token-search-forward # iTerm2
    # Bash compatibility
    # https://github.com/fish-shell/fish-shell/issues/89
    $v2 bind --preset $argv [a-.] history-token-search-backward
    $v1 bind --preset $argv \e. history-token-search-backward

    $v2 bind --preset $argv [a-l] __fish_list_current_token
    $v1 bind --preset $argv \el __fish_list_current_token
    $v2 bind --preset $argv [a-o] __fish_preview_current_file
    $v1 bind --preset $argv \eo __fish_preview_current_file
    $v2 bind --preset $argv [a-w] __fish_whatis_current_token
    $v1 bind --preset $argv \ew __fish_whatis_current_token
    $v2 bind --preset $argv [c-l] clear-screen
    $v1 bind --preset $argv \cl clear-screen
    $v2 bind --preset $argv [c-c] cancel-commandline
    $v1 bind --preset $argv \cc cancel-commandline
    $v2 bind --preset $argv [c-u] backward-kill-line
    $v1 bind --preset $argv \cu backward-kill-line
    $v2 bind --preset $argv [c-w] backward-kill-path-component
    $v1 bind --preset $argv \cw backward-kill-path-component
    bind --preset $argv [end] end-of-line
    $v1 bind --preset $argv \e\[F end-of-line
    bind --preset $argv [home] beginning-of-line
    $v1 bind --preset $argv \e\[H beginning-of-line

    $v2 bind --preset $argv [a-d] 'set -l cmd (commandline); if test -z "$cmd"; echo; dirh; commandline -f repaint; else; commandline -f kill-word; end'
    $v1 bind --preset $argv \ed 'set -l cmd (commandline); if test -z "$cmd"; echo; dirh; commandline -f repaint; else; commandline -f kill-word; end'
    $v2 bind --preset $argv [c-d] delete-or-exit
    $v1 bind --preset $argv \cd delete-or-exit

    $v2 bind --preset $argv [a-s] 'for cmd in sudo doas please; if command -q $cmd; fish_commandline_prepend $cmd; break; end; end'
    $v1 bind --preset $argv \es 'for cmd in sudo doas please; if command -q $cmd; fish_commandline_prepend $cmd; break; end; end'

    # Allow reading manpages by pressing F1 (many GUI applications) or Alt+h (like in zsh).
    bind --preset $argv [F1] __fish_man_page
    $v1 bind --preset $argv -k f1 __fish_man_page
    $v2 bind --preset $argv [a-h] __fish_man_page
    $v1 bind --preset $argv \eh __fish_man_page

    # This will make sure the output of the current command is paged using the default pager when
    # you press Meta-p.
    # If none is set, less will be used.
    $v2 bind --preset $argv [a-p] __fish_paginate
    $v1 bind --preset $argv \ep __fish_paginate

    # Make it easy to turn an unexecuted command into a comment in the shell history. Also,
    # remove the commenting chars so the command can be further edited then executed.
    $v2 bind --preset $argv [a-#] __fish_toggle_comment_commandline
    $v1 bind --preset $argv \e\# __fish_toggle_comment_commandline

    # The [meta-e] and [meta-v] keystrokes invoke an external editor on the command buffer.
    $v2 bind --preset $argv [a-e] edit_command_buffer
    $v1 bind --preset $argv \ee edit_command_buffer
    $v2 bind --preset $argv [a-v] edit_command_buffer
    $v1 bind --preset $argv \ev edit_command_buffer

    # Tmux' focus events.
    for mode in (bind --list-modes)
        # We only need the in-focus event currently (to redraw the vi-cursor).
        $v2 bind --preset -M $mode [focus_in] 'emit fish_focus_in'
        $v1 bind --preset -M $mode \e\[I 'emit fish_focus_in'
    end

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
        # TODO
        $v2 bind --preset $argv [c-space] 'test -n "$(commandline)" && commandline -i " "'
        # TODO
        bind --preset $argv -k nul 'test -n "$(commandline)" && commandline -i " "'
        # Shift-space behaves like space because it's easy to mistype.
        $v2 bind --preset $argv [s-space] 'commandline -i " "; commandline -f expand-abbr'
        $v1 bind --preset $argv \e\[32\;2u 'commandline -i " "; commandline -f expand-abbr' # CSI u escape sequence

        bind --preset $argv [ret] execute
        bind --preset $argv [c-j] execute
        bind --preset $argv [c-m] execute
        $v1 bind --preset $argv \n execute
        $v1 bind --preset $argv \r execute
        # Make Control+Return behave like Return because it's easy to mistype after accepting an autosuggestion.
        $v2 bind --preset $argv [c-ret] execute
        $v1 bind --preset $argv \e\[27\;5\;13~ execute # Sent with XTerm.vt100.formatOtherKeys: 0
        $v1 bind --preset $argv \e\[13\;5u execute # CSI u sequence, sent with XTerm.vt100.formatOtherKeys: 1
        # Same for Shift+Return
        $v2 bind --preset $argv [s-ret] execute
        $v1 bind --preset $argv \e\[27\;2\;13~ execute
        $v1 bind --preset $argv \e\[13\;2u execute
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
