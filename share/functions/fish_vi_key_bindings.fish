function fish_vi_key_bindings --description 'vi-like key bindings for fish'
    if contains -- -h $argv
        or contains -- --help $argv
        echo "Sorry but this function doesn't support -h or --help" >&2
        return 1
    end

    # Erase all bindings if not explicitly requested otherwise to
    # allow for hybrid bindings.
    # This needs to be checked here because if we are called again
    # via the variable handler the argument will be gone.
    set -l rebind true
    if test "$argv[1]" = --no-erase
        set rebind false
        set -e argv[1]
    else
        bind --erase --all --preset # clear earlier bindings, if any
    end

    # Allow just calling this function to correctly set the bindings.
    # Because it's a rather discoverable name, users will execute it
    # and without this would then have subtly broken bindings.
    if test "$fish_key_bindings" != fish_vi_key_bindings
        and test "$rebind" = true
        # Allow the user to set the variable universally.
        set -q fish_key_bindings
        or set -g fish_key_bindings
        # This triggers the handler, which calls us again and ensures the user_key_bindings
        # are executed.
        set fish_key_bindings fish_vi_key_bindings
        # unless the handler somehow doesn't exist, which would leave us without bindings.
        # this happens in no-config mode.
        functions -q __fish_reload_key_bindings
        and return
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

    # Add a way to switch from insert to normal (command) mode.
    # Note if we are paging, we want to stay in insert mode
    # See #2871
    bind -s --preset -M insert \e "if commandline -P; commandline -f cancel; else; set fish_bind_mode default; commandline -f backward-char repaint-mode; end"

    # Default (command) mode
    bind -s --preset :q exit
    bind -s --preset -m insert \cc cancel-commandline repaint-mode
    bind -s --preset -M default h backward-char
    bind -s --preset -M default l forward-char
    bind -s --preset -m insert \n execute
    bind -s --preset -m insert \r execute
    bind -s --preset -m insert o insert-line-under repaint-mode
    bind -s --preset -m insert O insert-line-over repaint-mode
    bind -s --preset -m insert i repaint-mode
    bind -s --preset -m insert I beginning-of-line repaint-mode
    bind -s --preset -m insert a forward-single-char repaint-mode
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

    bind -s --preset u undo
    bind -s --preset \cr redo

    bind -s --preset [ history-token-search-backward
    bind -s --preset ] history-token-search-forward
    bind -s --preset -m insert / history-pager repaint-mode

    bind -s --preset k up-or-search
    bind -s --preset j down-or-search
    bind -s --preset b backward-word
    bind -s --preset B backward-bigword
    bind -s --preset ge backward-word
    bind -s --preset gE backward-bigword
    bind -s --preset w forward-word forward-single-char
    bind -s --preset W forward-bigword forward-single-char
    bind -s --preset e forward-single-char forward-word backward-char
    bind -s --preset E forward-single-char forward-bigword backward-char

    # Vi/Vim doesn't support these keys in insert mode but that seems silly so we do so anyway.
    bind -s --preset -M insert -k home beginning-of-line
    bind -s --preset -M default -k home beginning-of-line
    bind -s --preset -M insert -k end end-of-line
    bind -s --preset -M default -k end end-of-line

    # Vi moves the cursor back if, after deleting, it is at EOL.
    # To emulate that, move forward, then backward, which will be a NOP
    # if there is something to move forward to.
    bind -s --preset -M default x delete-char forward-single-char backward-char
    bind -s --preset -M default X backward-delete-char
    bind -s --preset -M insert -k dc delete-char forward-single-char backward-char
    bind -s --preset -M default -k dc delete-char forward-single-char backward-char

    # Backspace deletes a char in insert mode, but not in normal/default mode.
    bind -s --preset -M insert -k backspace backward-delete-char
    bind -s --preset -M default -k backspace backward-char
    bind -s --preset -M insert \ch backward-delete-char
    bind -s --preset -M default \ch backward-char
    bind -s --preset -M insert \x7f backward-delete-char
    bind -s --preset -M default \x7f backward-char

    bind -s --preset dd kill-whole-line
    bind -s --preset D kill-line
    bind -s --preset d\$ kill-line
    bind -s --preset d\^ backward-kill-line
    bind -s --preset d0 backward-kill-line
    bind -s --preset dw kill-word
    bind -s --preset dW kill-bigword
    bind -s --preset diw forward-single-char forward-single-char backward-word kill-word
    bind -s --preset diW forward-single-char forward-single-char backward-bigword kill-bigword
    bind -s --preset daw forward-single-char forward-single-char backward-word kill-word
    bind -s --preset daW forward-single-char forward-single-char backward-bigword kill-bigword
    bind -s --preset de kill-word
    bind -s --preset dE kill-bigword
    bind -s --preset db backward-kill-word
    bind -s --preset dB backward-kill-bigword
    bind -s --preset dge backward-kill-word
    bind -s --preset dgE backward-kill-bigword
    bind -s --preset df begin-selection forward-jump kill-selection end-selection
    bind -s --preset dt begin-selection forward-jump backward-char kill-selection end-selection
    bind -s --preset dF begin-selection backward-jump kill-selection end-selection
    bind -s --preset dT begin-selection backward-jump forward-single-char kill-selection end-selection
    bind -s --preset dh backward-char delete-char
    bind -s --preset dl delete-char
    bind -s --preset di backward-jump-till and repeat-jump-reverse and begin-selection repeat-jump kill-selection end-selection
    bind -s --preset da backward-jump and repeat-jump-reverse and begin-selection repeat-jump kill-selection end-selection
    bind -s --preset 'd;' begin-selection repeat-jump kill-selection end-selection
    bind -s --preset 'd,' begin-selection repeat-jump-reverse kill-selection end-selection

    bind -s --preset -m insert s delete-char repaint-mode
    bind -s --preset -m insert S kill-inner-line repaint-mode
    bind -s --preset -m insert cc kill-inner-line repaint-mode
    bind -s --preset -m insert C kill-line repaint-mode
    bind -s --preset -m insert c\$ kill-line repaint-mode
    bind -s --preset -m insert c\^ backward-kill-line repaint-mode
    bind -s --preset -m insert c0 backward-kill-line repaint-mode
    bind -s --preset -m insert cw kill-word repaint-mode
    bind -s --preset -m insert cW kill-bigword repaint-mode
    bind -s --preset -m insert ciw forward-single-char forward-single-char backward-word kill-word repaint-mode
    bind -s --preset -m insert ciW forward-single-char forward-single-char backward-bigword kill-bigword repaint-mode
    bind -s --preset -m insert caw forward-single-char forward-single-char backward-word kill-word repaint-mode
    bind -s --preset -m insert caW forward-single-char forward-single-char backward-bigword kill-bigword repaint-mode
    bind -s --preset -m insert ce kill-word repaint-mode
    bind -s --preset -m insert cE kill-bigword repaint-mode
    bind -s --preset -m insert cb backward-kill-word repaint-mode
    bind -s --preset -m insert cB backward-kill-bigword repaint-mode
    bind -s --preset -m insert cge backward-kill-word repaint-mode
    bind -s --preset -m insert cgE backward-kill-bigword repaint-mode
    bind -s --preset -m insert cf begin-selection forward-jump kill-selection end-selection repaint-mode
    bind -s --preset -m insert ct begin-selection forward-jump backward-char kill-selection end-selection repaint-mode
    bind -s --preset -m insert cF begin-selection backward-jump kill-selection end-selection repaint-mode
    bind -s --preset -m insert cT begin-selection backward-jump forward-single-char kill-selection end-selection repaint-mode
    bind -s --preset -m insert ch backward-char begin-selection kill-selection end-selection repaint-mode
    bind -s --preset -m insert cl begin-selection kill-selection end-selection repaint-mode
    bind -s --preset -m insert ci backward-jump-till and repeat-jump-reverse and begin-selection repeat-jump kill-selection end-selection repaint-mode
    bind -s --preset -m insert ca backward-jump and repeat-jump-reverse and begin-selection repeat-jump kill-selection end-selection repaint-mode

    bind -s --preset '~' togglecase-char forward-single-char
    bind -s --preset gu downcase-word
    bind -s --preset gU upcase-word

    bind -s --preset J end-of-line delete-char
    bind -s --preset K 'man (commandline -t) 2>/dev/null; or echo -n \a'

    bind -s --preset yy kill-whole-line yank
    bind -s --preset Y kill-whole-line yank
    bind -s --preset y\$ kill-line yank
    bind -s --preset y\^ backward-kill-line yank
    bind -s --preset y0 backward-kill-line yank
    bind -s --preset yw kill-word yank
    bind -s --preset yW kill-bigword yank
    bind -s --preset yiw forward-single-char forward-single-char backward-word kill-word yank
    bind -s --preset yiW forward-single-char forward-single-char backward-bigword kill-bigword yank
    bind -s --preset yaw forward-single-char forward-single-char backward-word kill-word yank
    bind -s --preset yaW forward-single-char forward-single-char backward-bigword kill-bigword yank
    bind -s --preset ye kill-word yank
    bind -s --preset yE kill-bigword yank
    bind -s --preset yb backward-kill-word yank
    bind -s --preset yB backward-kill-bigword yank
    bind -s --preset yge backward-kill-word yank
    bind -s --preset ygE backward-kill-bigword yank
    bind -s --preset yf begin-selection forward-jump kill-selection yank end-selection
    bind -s --preset yt begin-selection forward-jump-till kill-selection yank end-selection
    bind -s --preset yF begin-selection backward-jump kill-selection yank end-selection
    bind -s --preset yT begin-selection backward-jump-till kill-selection yank end-selection
    bind -s --preset yh backward-char begin-selection kill-selection yank end-selection
    bind -s --preset yl begin-selection kill-selection yank end-selection
    bind -s --preset yi backward-jump-till and repeat-jump-reverse and begin-selection repeat-jump kill-selection yank end-selection
    bind -s --preset ya backward-jump and repeat-jump-reverse and begin-selection repeat-jump kill-selection yank end-selection

    bind -s --preset f forward-jump
    bind -s --preset F backward-jump
    bind -s --preset t forward-jump-till
    bind -s --preset T backward-jump-till
    bind -s --preset ';' repeat-jump
    bind -s --preset , repeat-jump-reverse

    # in emacs yank means paste
    # in vim p means paste *after* current character, so go forward a char before pasting
    # also in vim, P means paste *at* current position (like at '|' with cursor = line),
    # \ so there's no need to go back a char, just paste it without moving
    bind -s --preset p forward-char yank
    bind -s --preset P yank
    bind -s --preset gp yank-pop

    # same vim 'pasting' note as upper
    bind -s --preset '"*p' forward-char "commandline -i ( xsel -p; echo )[1]"
    bind -s --preset '"*P' "commandline -i ( xsel -p; echo )[1]"

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
    bind -s --preset -M visual -m insert s kill-selection end-selection repaint-mode
    bind -s --preset -M visual -m default d kill-selection end-selection repaint-mode
    bind -s --preset -M visual -m default x kill-selection end-selection repaint-mode
    bind -s --preset -M visual -m default X kill-whole-line end-selection repaint-mode
    bind -s --preset -M visual -m default y kill-selection yank end-selection repaint-mode
    bind -s --preset -M visual -m default '"*y' "fish_clipboard_copy; commandline -f end-selection repaint-mode"
    bind -s --preset -M visual -m default '~' togglecase-selection end-selection repaint-mode

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
    set -g fish_cursor_selection_mode inclusive

    set fish_bind_mode $init_mode

end
