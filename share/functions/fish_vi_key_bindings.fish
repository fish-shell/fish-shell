function fish_vi_key_bindings --description 'vi-like key bindings for fish'
    if contains -- -h $argv
        or contains -- --help $argv
        echo "Sorry but this function doesn't support -h or --help"
        return 1
    end

    # Erase all bindings if not explicitly requested otherwise to
    # allow for hybrid bindings.
    # This needs to be checked here because if we are called again
    # via the variable handler the argument will be gone.
    set -l rebind true
    if test "$argv[1]" = "--no-erase"
        set rebind false
        set -e argv[1]
    else
        bind --erase --all --preset # clear earlier bindings, if any
    end

    # Allow just calling this function to correctly set the bindings.
    # Because it's a rather discoverable name, users will execute it
    # and without this would then have subtly broken bindings.
    if test "$fish_key_bindings" != "fish_vi_key_bindings"
        and test "$rebind" = "true"
        # Allow the user to set the variable universally.
        set -q fish_key_bindings
        or set -g fish_key_bindings
        # This triggers the handler, which calls us again and ensures the user_key_bindings
        # are executed.
        set fish_key_bindings fish_vi_key_bindings
        return
    end

    set -l init_mode insert
    # These are only the special vi-style keys
    # not end/home, we share those.
    set -l eol_keys \$ g\$
    set -l bol_keys \^ 0 g\^

    if contains -- $argv[1] insert default visual
        set init_mode $argv[1]
    else if set -q argv[1]
        # We should still go on so the bindings still get set.
        echo "Unknown argument $argv" >&2
    end

    # Inherit shared key bindings.
    # Do this first so vi-bindings win over default.
    for mode in insert default visual
        __fish_shared_key_bindings -s -M $mode
    end

    bind -s --preset -M insert \r execute
    bind -s --preset -M insert \n execute

    bind -s --preset -M insert "" self-insert
    # Space expands abbrs _and_ inserts itself.
    bind -s --preset -M insert " " self-insert expand-abbr

    # Add a way to switch from insert to normal (command) mode.
    # Note if we are paging, we want to stay in insert mode
    # See #2871
    bind -s --preset -M insert \e "if commandline -P; commandline -f cancel; else; set fish_bind_mode default; commandline -f backward-char repaint-mode; end"

    # Default (command) mode
    bind -s --preset :q exit
    bind -s --preset -m insert \cc __fish_cancel_commandline
    bind -s --preset -M default h backward-char
    bind -s --preset -M default l forward-char
    bind -s --preset -m insert \n execute
    bind -s --preset -m insert \r execute
    bind -s --preset -m insert i repaint-mode
    bind -s --preset -m insert I beginning-of-line repaint-mode
    bind -s --preset -m insert a forward-char repaint-mode
    bind -s --preset -m insert A end-of-line repaint-mode
    bind -s --preset -m visual v begin-selection repaint-mode

    #bind -s --preset -m insert o "commandline -a \n" down-line repaint-mode
    #bind -s --preset -m insert O beginning-of-line "commandline -i \n" up-line repaint-mode # doesn't work

    bind -s --preset gg beginning-of-buffer
    bind -s --preset G end-of-buffer

    for key in $eol_keys
        bind -s --preset $key end-of-line
    end
    for key in $bol_keys
        bind -s --preset $key beginning-of-line
    end

    bind -s --preset u history-search-backward
    bind -s --preset \cr history-search-forward

    bind -s --preset [ history-token-search-backward
    bind -s --preset ] history-token-search-forward

    bind -s --preset k up-or-search
    bind -s --preset j down-or-search
    bind -s --preset b backward-word
    bind -s --preset B backward-bigword
    bind -s --preset ge backward-word
    bind -s --preset gE backward-bigword
    bind -s --preset w forward-word forward-char
    bind -s --preset W forward-bigword forward-char
    bind -s --preset e forward-char forward-word backward-char
    bind -s --preset E forward-bigword backward-char

    # OS X SnowLeopard doesn't have these keys. Don't show an annoying error message.
    # Vi/Vim doesn't support these keys in insert mode but that seems silly so we do so anyway.
    bind -s --preset -M insert -k home beginning-of-line 2>/dev/null
    bind -s --preset -M default -k home beginning-of-line 2>/dev/null
    bind -s --preset -M insert -k end end-of-line 2>/dev/null
    bind -s --preset -M default -k end end-of-line 2>/dev/null

    # Vi moves the cursor back if, after deleting, it is at EOL.
    # To emulate that, move forward, then backward, which will be a NOP
    # if there is something to move forward to.
    bind -s --preset -M default x delete-char forward-char backward-char
    bind -s --preset -M default X backward-delete-char
    bind -s --preset -M insert -k dc delete-char forward-char backward-char
    bind -s --preset -M default -k dc delete-char forward-char backward-char

    # Backspace deletes a char in insert mode, but not in normal/default mode.
    bind -s --preset -M insert -k backspace backward-delete-char
    bind -s --preset -M default -k backspace backward-char
    bind -s --preset -M insert \ch backward-delete-char
    bind -s --preset -M default \ch backward-char
    bind -s --preset -M insert \x7f backward-delete-char
    bind -s --preset -M default \x7f backward-char
    bind -s --preset -M insert \e\[3\;2~ backward-delete-char # Mavericks Terminal.app shift-ctrl-delete
    bind -s --preset -M default \e\[3\;2~ backward-delete-char # Mavericks Terminal.app shift-ctrl-delete

    bind -s --preset dd kill-whole-line
    bind -s --preset D kill-line
    bind -s --preset d\$ kill-line
    bind -s --preset d\^ backward-kill-line
    bind -s --preset d0 backward-kill-line
    bind -s --preset dw kill-word
    bind -s --preset dW kill-bigword
    bind -s --preset diw forward-char forward-char backward-word kill-word
    bind -s --preset diW forward-char forward-char backward-bigword kill-bigword
    bind -s --preset daw forward-char forward-char backward-word kill-word
    bind -s --preset daW forward-char forward-char backward-bigword kill-bigword
    bind -s --preset de kill-word
    bind -s --preset dE kill-bigword
    bind -s --preset db backward-kill-word
    bind -s --preset dB backward-kill-bigword
    bind -s --preset dge backward-kill-word
    bind -s --preset dgE backward-kill-bigword
    bind -s --preset df begin-selection forward-jump kill-selection end-selection
    bind -s --preset dt begin-selection forward-jump backward-char kill-selection end-selection
    bind -s --preset dF begin-selection backward-jump kill-selection end-selection
    bind -s --preset dT begin-selection backward-jump forward-char kill-selection end-selection

    bind -s --preset -m insert s delete-char repaint-mode
    bind -s --preset -m insert S kill-whole-line repaint-mode
    bind -s --preset -m insert cc kill-whole-line repaint-mode
    bind -s --preset -m insert C kill-line repaint-mode
    bind -s --preset -m insert c\$ kill-line repaint-mode
    bind -s --preset -m insert c\^ backward-kill-line repaint-mode
    bind -s --preset -m insert cw kill-word repaint-mode
    bind -s --preset -m insert cW kill-bigword repaint-mode
    bind -s --preset -m insert ciw forward-char forward-char backward-word kill-word repaint-mode
    bind -s --preset -m insert ciW forward-char forward-char backward-bigword kill-bigword repaint-mode
    bind -s --preset -m insert caw forward-char forward-char backward-word kill-word repaint-mode
    bind -s --preset -m insert caW forward-char forward-char backward-bigword kill-bigword repaint-mode
    bind -s --preset -m insert ce kill-word repaint-mode
    bind -s --preset -m insert cE kill-bigword repaint-mode
    bind -s --preset -m insert cb backward-kill-word repaint-mode
    bind -s --preset -m insert cB backward-kill-bigword repaint-mode
    bind -s --preset -m insert cge backward-kill-word repaint-mode
    bind -s --preset -m insert cgE backward-kill-bigword repaint-mode

    bind -s --preset '~' capitalize-word
    bind -s --preset gu downcase-word
    bind -s --preset gU upcase-word

    bind -s --preset J end-of-line delete-char
    bind -s --preset K 'man (commandline -t) 2>/dev/null; or echo -n \a'

    bind -s --preset yy kill-whole-line yank
    bind -s --preset Y kill-whole-line yank
    bind -s --preset y\$ kill-line yank
    bind -s --preset y\^ backward-kill-line yank
    bind -s --preset yw kill-word yank
    bind -s --preset yW kill-bigword yank
    bind -s --preset yiw forward-char forward-char backward-word kill-word yank
    bind -s --preset yiW forward-char forward-char backward-bigword kill-bigword yank
    bind -s --preset yaw forward-char forward-char backward-word kill-word yank
    bind -s --preset yaW forward-char forward-char backward-bigword kill-bigword yank
    bind -s --preset ye kill-word yank
    bind -s --preset yE kill-bigword yank
    bind -s --preset yb backward-kill-word yank
    bind -s --preset yB backward-kill-bigword yank
    bind -s --preset yge backward-kill-word yank
    bind -s --preset ygE backward-kill-bigword yank

    bind -s --preset f forward-jump
    bind -s --preset F backward-jump
    bind -s --preset t forward-jump-till
    bind -s --preset T backward-jump-till
    bind -s --preset ';' repeat-jump
    bind -s --preset , repeat-jump-reverse

    # in emacs yank means paste
    bind -s --preset p yank
    bind -s --preset P backward-char yank
    bind -s --preset gp yank-pop

    bind -s --preset '"*p' "commandline -i ( xsel -p; echo )[1]"
    bind -s --preset '"*P' backward-char "commandline -i ( xsel -p; echo )[1]"

    #
    # Lowercase r, enters replace_one mode
    #
    bind -s --preset -m replace_one r repaint-mode
    bind -s --preset -M replace_one -m default '' delete-char self-insert backward-char repaint-mode
    bind -s --preset -M replace_one -m default \r 'commandline -f delete-char; commandline -i \n; commandline -f backward-char; commandline -f repaint-mode'
    bind -s --preset -M replace_one -m default \e cancel repaint-mode

    #
    # Uppercase R, enters replace mode
    #
    bind -s --preset -m replace R repaint-mode
    bind -s --preset -M replace '' delete-char self-insert
    bind -s --preset -M replace -m insert \r execute repaint-mode
    bind -s --preset -M replace -m default \e cancel repaint-mode
    # in vim (and maybe in vi), <BS> deletes the changes
    # but this binding just move cursor backward, not delete the changes
    bind -s --preset -M replace -k backspace backward-char

    #
    # visual mode
    #
    bind -s --preset -M visual h backward-char
    bind -s --preset -M visual l forward-char

    bind -s --preset -M visual k up-line
    bind -s --preset -M visual j down-line

    bind -s --preset -M visual b backward-word
    bind -s --preset -M visual B backward-bigword
    bind -s --preset -M visual ge backward-word
    bind -s --preset -M visual gE backward-bigword
    bind -s --preset -M visual w forward-word
    bind -s --preset -M visual W forward-bigword
    bind -s --preset -M visual e forward-word
    bind -s --preset -M visual E forward-bigword
    bind -s --preset -M visual o swap-selection-start-stop repaint-mode

    bind -s --preset -M visual f forward-jump
    bind -s --preset -M visual t forward-jump-till
    bind -s --preset -M visual F backward-jump
    bind -s --preset -M visual T backward-jump-till

    for key in $eol_keys
        bind -s --preset -M visual $key end-of-line
    end
    for key in $bol_keys
        bind -s --preset -M visual $key beginning-of-line
    end

    bind -s --preset -M visual -m insert c kill-selection end-selection repaint-mode
    bind -s --preset -M visual -m default d kill-selection end-selection repaint-mode
    bind -s --preset -M visual -m default x kill-selection end-selection repaint-mode
    bind -s --preset -M visual -m default X kill-whole-line end-selection repaint-mode
    bind -s --preset -M visual -m default y kill-selection yank end-selection repaint-mode
    bind -s --preset -M visual -m default '"*y' "commandline -s | xsel -p; commandline -f end-selection repaint-mode"

    bind -s --preset -M visual -m default \cc end-selection repaint-mode
    bind -s --preset -M visual -m default \e end-selection repaint-mode

    # Make it easy to turn an unexecuted command into a comment in the shell history. Also, remove
    # the commenting chars so the command can be further edited then executed.
    bind -s --preset -M default \# __fish_toggle_comment_commandline
    bind -s --preset -M visual \# __fish_toggle_comment_commandline
    bind -s --preset -M replace \# __fish_toggle_comment_commandline

    # Set the cursor shape
    # After executing once, this will have defined functions listening for the variable.
    # Therefore it needs to be before setting fish_bind_mode.
    fish_vi_cursor

    set fish_bind_mode $init_mode

end
