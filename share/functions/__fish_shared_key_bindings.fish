function __fish_shared_key_bindings -d "Bindings shared between emacs and vi mode"
    # These are some bindings that are supposed to be shared between vi mode and default mode.
    # They are supposed to be unrelated to text-editing (or movement).
    # This takes $argv so the vi-bindings can pass the mode they are valid in.

    if contains -- -h $argv
        or contains -- --help $argv
        echo "Sorry but this function doesn't support -h or --help" >&2
        return 1
    end

    bind --preset $argv ctrl-y yank
    or return # protect against invalid $argv
    bind --preset $argv alt-y yank-pop

    # Left/Right arrow
    bind --preset $argv right forward-char
    bind --preset $argv left backward-char

    # Ctrl-left/right - these also work in vim.
    if test (__fish_uname) = Darwin
        bind --preset $argv ctrl-right forward-token
        bind --preset $argv ctrl-left backward-token
    else
        bind --preset $argv ctrl-right forward-word
        bind --preset $argv ctrl-left backward-word
    end

    bind --preset $argv pageup beginning-of-history
    bind --preset $argv pagedown end-of-history

    # Interaction with the system clipboard.
    bind --preset $argv ctrl-x fish_clipboard_copy
    bind --preset $argv ctrl-v fish_clipboard_paste

    bind --preset $argv escape cancel
    bind --preset $argv ctrl-\[ cancel
    bind --preset $argv tab complete
    bind --preset $argv ctrl-i complete
    bind --preset $argv ctrl-s pager-toggle-search
    # shift-tab does a tab complete followed by a search.
    bind --preset $argv shift-tab complete-and-search
    bind --preset $argv shift-delete history-delete or backward-delete-char

    bind --preset $argv down down-or-search
    bind --preset $argv up up-or-search

    bind --preset $argv shift-right forward-bigword
    bind --preset $argv shift-left backward-bigword

    bind --preset $argv alt-b prevd-or-backward-word
    bind --preset $argv alt-f nextd-or-forward-word

    set -l alt_right_aliases alt-right \e\[1\;9C # iTerm2 < 3.5.12
    set -l alt_left_aliases alt-left \e\[1\;9D # iTerm2 < 3.5.12
    if test (__fish_uname) = Darwin
        for alt_right in $alt_right_aliases
            bind --preset $argv $alt_right nextd-or-forward-word
        end
        for alt_left in $alt_left_aliases
            bind --preset $argv $alt_left prevd-or-backward-word
        end
    else
        for alt_right in $alt_right_aliases
            bind --preset $argv $alt_right nextd-or-forward-token
        end
        for alt_left in $alt_left_aliases
            bind --preset $argv $alt_left prevd-or-backward-token
        end
    end

    bind --preset $argv alt-up history-token-search-backward
    bind --preset $argv alt-down history-token-search-forward
    bind --preset $argv \e\[1\;9A history-token-search-backward # iTerm2 < 3.5.12
    bind --preset $argv \e\[1\;9B history-token-search-forward # iTerm2 < 3.5.12
    # Bash compatibility
    # https://github.com/fish-shell/fish-shell/issues/89
    bind --preset $argv alt-. history-token-search-backward

    bind --preset $argv alt-l __fish_list_current_token
    bind --preset $argv alt-o __fish_preview_current_file
    bind --preset $argv alt-w __fish_whatis_current_token
    bind --preset $argv ctrl-l scrollback-push clear-screen
    bind --preset $argv ctrl-c clear-commandline
    bind --preset $argv ctrl-u backward-kill-line
    bind --preset $argv ctrl-k kill-line
    bind --preset $argv ctrl-w backward-kill-path-component
    bind --preset $argv end end-of-line
    bind --preset $argv home beginning-of-line

    bind --preset $argv alt-d 'if test "$(commandline; printf .)" = \n.; __fish_echo dirh; else; commandline -f kill-word; end'
    bind --preset $argv ctrl-d delete-or-exit

    bind --preset $argv alt-s 'for cmd in sudo doas please; if command -q $cmd; fish_commandline_prepend $cmd; break; end; end'

    # Allow reading manpages by pressing f1 (many GUI applications) or Alt+h (like in zsh).
    bind --preset $argv f1 __fish_man_page
    bind --preset $argv alt-h __fish_man_page

    # This will make sure the output of the current command is paged using the default pager when
    # you press Meta-p.
    # If none is set, less will be used.
    bind --preset $argv alt-p __fish_paginate

    # Make it easy to turn an unexecuted command into a comment in the shell history. Also,
    # remove the commenting chars so the command can be further edited then executed.
    bind --preset $argv alt-# __fish_toggle_comment_commandline

    # These keystrokes invoke an external editor on the command buffer.
    bind --preset $argv alt-e edit_command_buffer
    bind --preset $argv alt-v edit_command_buffer

    # Bindings that are shared in text-insertion modes.
    if not set -l index (contains --index -- -M $argv)
        or test $argv[(math $index + 1)] = insert

        # This is the default binding, i.e. the one used if no other binding matches
        bind --preset $argv "" self-insert
        or exit # protect against invalid $argv

        # Space and other command terminators expands abbrs _and_ inserts itself.
        bind --preset $argv space self-insert expand-abbr
        bind --preset $argv ";" self-insert expand-abbr
        bind --preset $argv "|" self-insert expand-abbr
        bind --preset $argv "&" self-insert expand-abbr
        bind --preset $argv ">" self-insert expand-abbr
        bind --preset $argv "<" self-insert expand-abbr
        set -l maybe_search_field '(commandline --search-field >/dev/null && echo --search-field)'
        bind --preset $argv shift-enter "commandline -i \n $maybe_search_field" expand-abbr
        bind --preset $argv alt-enter "commandline -i \n $maybe_search_field" expand-abbr
        bind --preset $argv ")" self-insert expand-abbr # Closing a command substitution.
        bind --preset $argv ctrl-space 'test -n "$(commandline)" && commandline -i " " '$maybe_search_field
        # Shift-space behaves like space because it's easy to mistype.
        bind --preset $argv shift-space 'commandline -i " " '$maybe_search_field expand-abbr

        bind --preset $argv enter execute
        bind --preset $argv ctrl-j execute
        bind --preset $argv ctrl-m execute
        # Make Control+Return behave like Return because it's easy to mistype after accepting an autosuggestion.
        bind --preset $argv ctrl-enter execute
    end
end
