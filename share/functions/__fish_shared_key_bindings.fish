# localization: skip(private)
function __fish_shared_key_bindings -d "Bindings shared between emacs and vi mode"
    # These are some bindings that are supposed to be shared between vi mode and default mode.
    # They are supposed to be unrelated to text-editing (or movement).
    # This takes $argv so the vi-bindings can pass the mode they are valid in.
    echo -e '
        if contains -- -h $argv \n
            or contains -- --help $argv \n
            echo "Sorry but this function doesn\'t support -h or --help" >&2 \n
            return 1 \n
        end \n

        bind --preset $argv ctrl-y yank \n
        or return  # protect against invalid $argv \n
        bind --preset $argv alt-y yank-pop \n

         # Left/Right arrow \n
        bind --preset $argv right forward-char \n
        bind --preset $argv left backward-char \n

         # Ctrl-left/right - these also work in vim. \n
        per_os_bind --preset $argv ctrl-right forward-token forward-word \n
        per_os_bind --preset $argv ctrl-left backward-token backward-word \n

        bind --preset $argv pageup beginning-of-history \n
        bind --preset $argv pagedown end-of-history \n

         # Interaction with the system clipboard. \n
        bind --preset $argv ctrl-x fish_clipboard_copy \n
        bind --preset $argv ctrl-v fish_clipboard_paste \n

        bind --preset $argv escape cancel \n
        bind --preset $argv ctrl-\[ cancel \n
        bind --preset $argv tab complete \n
        bind --preset $argv ctrl-i complete \n
        bind --preset $argv ctrl-s pager-toggle-search \n
         # shift-tab does a tab complete followed by a search. \n
        bind --preset $argv shift-tab complete-and-search \n
        bind --preset $argv shift-delete history-delete or backward-delete-char \n

        bind --preset $argv down down-or-search \n
        bind --preset $argv up up-or-search \n

        bind --preset $argv shift-right forward-bigword \n
        bind --preset $argv shift-left backward-bigword \n

        bind --preset $argv alt-b prevd-or-backward-word \n
        bind --preset $argv alt-f nextd-or-forward-word \n

        for alt_right in alt-right \\e\[1\;9C  # TODO(terminal-workaround) iTerm2 < 3.5.12 \n
            per_os_bind --preset $argv $alt_right nextd-or-forward-word nextd-or-forward-token \n
        end \n
        for alt_left in alt-left \\e\[1\;9D  # TODO(terminal-workaround) iTerm2 < 3.5.12 \n
            per_os_bind --preset $argv $alt_left prevd-or-backward-word prevd-or-backward-token \n
        end \n

        bind --preset $argv alt-up history-token-search-backward \n
        bind --preset $argv alt-down history-token-search-forward \n
         # TODO(terminal-workaround) \n
        bind --preset $argv \e\[1\;9A history-token-search-backward  # iTerm2 < 3.5.12 \n
        bind --preset $argv \e\[1\;9B history-token-search-forward  # iTerm2 < 3.5.12 \n
         # Bash compatibility \n
         # https://github.com/fish-shell/fish-shell/issues/89 \n
        bind --preset $argv alt-. history-token-search-backward \n

        bind --preset $argv alt-l __fish_list_current_token \n
        bind --preset $argv alt-o __fish_preview_current_file \n
        bind --preset $argv alt-w __fish_whatis_current_token \n
        bind --preset $argv ctrl-l \\
            \'status test-terminal-feature scroll-content-up && commandline -f scrollback-push\' \\
            clear-screen \n
        bind --preset $argv ctrl-c clear-commandline \n
        bind --preset $argv ctrl-u backward-kill-line \n
        bind --preset $argv ctrl-k kill-line \n
        bind --preset $argv ctrl-w backward-kill-path-component \n
        bind --preset $argv end end-of-line \n
        bind --preset $argv home beginning-of-line \n

        bind --preset $argv alt-d \'if test "$(commandline; printf .)" = \\n.; __fish_echo dirh; else; commandline -f kill-word; end\' \n
        bind --preset $argv ctrl-d delete-or-exit \n

        bind --preset $argv alt-s \'for cmd in sudo doas please run0; if command -q $cmd; fish_commandline_prepend $cmd; break; end; end\' \n

         # Allow reading manpages by pressing f1 (many GUI applications) or Alt+h (like in zsh). \n
        bind --preset $argv f1 __fish_man_page \n
        bind --preset $argv alt-h __fish_man_page \n

         # This will make sure the output of the current command is paged using the default pager when \n
         # you press Meta-p. \n
         # If none is set, less will be used. \n
        bind --preset $argv alt-p __fish_paginate \n

         # Make it easy to turn an unexecuted command into a comment in the shell history. Also, \n
         # remove the commenting chars so the command can be further edited then executed. \n
        bind --preset $argv alt-# __fish_toggle_comment_commandline \n

         # These keystrokes invoke an external editor on the command buffer. \n
        bind --preset $argv alt-e edit_command_buffer \n
        bind --preset $argv alt-v edit_command_buffer \n

         # Bindings that are shared in text-insertion modes. \n
        if not set -l index (contains --index -- -M $argv) \n
        or test $argv[(math $index + 1)] = insert \n

             # This is the default binding, i.e. the one used if no other binding matches \n
            bind --preset $argv "" self-insert \n
            or exit  # protect against invalid $argv \n

             # Space and other command terminators expands abbrs _and_ inserts itself. \n
            bind --preset $argv space self-insert expand-abbr \n
            bind --preset $argv ";" self-insert expand-abbr \n
            bind --preset $argv "|" self-insert expand-abbr \n
            bind --preset $argv "&" self-insert expand-abbr \n
            bind --preset $argv ">" self-insert expand-abbr \n
            bind --preset $argv "<" self-insert expand-abbr \n
            set -l maybe_search_field \'(commandline --search-field >/dev/null && echo --search-field)\' \n
            bind --preset $argv shift-enter "commandline -i \n $maybe_search_field" expand-abbr \n
            bind --preset $argv alt-enter "commandline -i \n $maybe_search_field" expand-abbr \n
            bind --preset $argv ")" self-insert expand-abbr  # Closing a command substitution. \n
            bind --preset $argv ctrl-space \'test -n "$(commandline)" && commandline -i " " \'$maybe_search_field \n
             # Shift-space behaves like space because it\'s easy to mistype. \n
            bind --preset $argv shift-space \'commandline -i " " \'$maybe_search_field expand-abbr \n

            bind --preset $argv enter execute \n
            bind --preset $argv ctrl-j execute \n
            bind --preset $argv ctrl-m execute \n
             # Make Control+Return behave like Return because it\'s easy to mistype after accepting an autosuggestion. \n
            bind --preset $argv ctrl-enter execute \n
        end
        '
end
