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
        bind --erase --all # clear earlier bindings, if any
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

    # The default escape timeout is 300ms. But for users of Vi bindings that can be slightly
    # annoying when trying to switch to Vi "normal" mode. So set a shorter timeout in this case
    # unless the user has explicitly set the delay.
    set -q fish_escape_delay_ms
    or set -g fish_escape_delay_ms 100

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
        __fish_shared_key_bindings -M $mode
    end

    bind -M insert \r execute
    bind -M insert \n execute

    bind -M insert "" self-insert

    # Add way to kill current command line while in insert mode.
    bind -M insert \cc __fish_cancel_commandline
    # Add a way to switch from insert to normal (command) mode.
    # Note if we are paging, we want to stay in insert mode
    # See #2871
    bind -M insert \e "if commandline -P; commandline -f cancel; else; set fish_bind_mode default; commandline -f backward-char force-repaint; end"

    # Default (command) mode
    bind :q exit
    bind -m insert \cc __fish_cancel_commandline
    bind -M default h backward-char
    bind -M default l forward-char
    bind -m insert \n execute
    bind -m insert \r execute
    bind -m insert i force-repaint
    bind -m insert I beginning-of-line force-repaint
    bind -m insert a forward-char force-repaint
    bind -m insert A end-of-line force-repaint
    bind -m visual v begin-selection force-repaint

    #bind -m insert o "commandline -a \n" down-line force-repaint
    #bind -m insert O beginning-of-line "commandline -i \n" up-line force-repaint # doesn't work

    bind gg beginning-of-buffer
    bind G end-of-buffer

    for key in $eol_keys
        bind $key end-of-line
    end
    for key in $bol_keys
        bind $key beginning-of-line
    end

    bind u history-search-backward
    bind \cr history-search-forward

    bind [ history-token-search-backward
    bind ] history-token-search-forward

    bind k up-or-search
    bind j down-or-search
    bind b backward-word
    bind B backward-bigword
    bind ge backward-word
    bind gE backward-bigword
    bind w forward-word forward-char
    bind W forward-bigword forward-char
    bind e forward-char forward-word backward-char
    bind E forward-bigword backward-char

    # OS X SnowLeopard doesn't have these keys. Don't show an annoying error message.
    # Vi/Vim doesn't support these keys in insert mode but that seems silly so we do so anyway.
    bind -M insert -k home beginning-of-line 2>/dev/null
    bind -M default -k home beginning-of-line 2>/dev/null
    bind -M insert -k end end-of-line 2>/dev/null
    bind -M default -k end end-of-line 2>/dev/null

    bind -M default x delete-char
    bind -M default X backward-delete-char
    bind -M insert -k dc delete-char
    bind -M default -k dc delete-char

    # Backspace deletes a char in insert mode, but not in normal/default mode.
    bind -M insert -k backspace backward-delete-char
    bind -M default -k backspace backward-char
    bind -M insert \ch backward-delete-char
    bind -M default \ch backward-char
    bind -M insert \x7f backward-delete-char
    bind -M default \x7f backward-char
    bind -M insert \e\[3\;2~ backward-delete-char # Mavericks Terminal.app shift-ctrl-delete
    bind -M default \e\[3\;2~ backward-delete-char # Mavericks Terminal.app shift-ctrl-delete

    bind dd kill-whole-line
    bind D kill-line
    bind d\$ kill-line
    bind d\^ backward-kill-line
    bind dw kill-word
    bind dW kill-bigword
    bind diw forward-char forward-char backward-word kill-word
    bind diW forward-char forward-char backward-bigword kill-bigword
    bind daw forward-char forward-char backward-word kill-word
    bind daW forward-char forward-char backward-bigword kill-bigword
    bind de kill-word
    bind dE kill-bigword
    bind db backward-kill-word
    bind dB backward-kill-bigword
    bind dge backward-kill-word
    bind dgE backward-kill-bigword

    bind -m insert s delete-char force-repaint
    bind -m insert S kill-whole-line force-repaint
    bind -m insert cc kill-whole-line force-repaint
    bind -m insert C kill-line force-repaint
    bind -m insert c\$ kill-line force-repaint
    bind -m insert c\^ backward-kill-line force-repaint
    bind -m insert cw kill-word force-repaint
    bind -m insert cW kill-bigword force-repaint
    bind -m insert ciw forward-char forward-char backward-word kill-word force-repaint
    bind -m insert ciW forward-char forward-char backward-bigword kill-bigword force-repaint
    bind -m insert caw forward-char forward-char backward-word kill-word force-repaint
    bind -m insert caW forward-char forward-char backward-bigword kill-bigword force-repaint
    bind -m insert ce kill-word force-repaint
    bind -m insert cE kill-bigword force-repaint
    bind -m insert cb backward-kill-word force-repaint
    bind -m insert cB backward-kill-bigword force-repaint
    bind -m insert cge backward-kill-word force-repaint
    bind -m insert cgE backward-kill-bigword force-repaint

    bind '~' capitalize-word
    bind gu downcase-word
    bind gU upcase-word

    bind J end-of-line delete-char
    bind K 'man (commandline -t) ^/dev/null; or echo -n \a'

    bind yy kill-whole-line yank
    bind Y kill-whole-line yank
    bind y\$ kill-line yank
    bind y\^ backward-kill-line yank
    bind yw kill-word yank
    bind yW kill-bigword yank
    bind yiw forward-char forward-char backward-word kill-word yank
    bind yiW forward-char forward-char backward-bigword kill-bigword yank
    bind yaw forward-char forward-char backward-word kill-word yank
    bind yaW forward-char forward-char backward-bigword kill-bigword yank
    bind ye kill-word yank
    bind yE kill-bigword yank
    bind yb backward-kill-word yank
    bind yB backward-kill-bigword yank
    bind yge backward-kill-word yank
    bind ygE backward-kill-bigword yank

    bind f forward-jump
    bind F backward-jump
    bind t forward-jump and backward-char
    bind T backward-jump and forward-char

    # in emacs yank means paste
    bind p yank
    bind P backward-char yank
    bind gp yank-pop

    bind '"*p' "commandline -i ( xsel -p; echo )[1]"
    bind '"*P' backward-char "commandline -i ( xsel -p; echo )[1]"

    #
    # Lowercase r, enters replace-one mode
    #
    bind -m replace-one r force-repaint
    bind -M replace-one -m default '' delete-char self-insert backward-char force-repaint
    bind -M replace-one -m default \e cancel force-repaint

    #
    # visual mode
    #
    bind -M visual h backward-char
    bind -M visual l forward-char

    bind -M visual k up-line
    bind -M visual j down-line

    bind -M visual b backward-word
    bind -M visual B backward-bigword
    bind -M visual ge backward-word
    bind -M visual gE backward-bigword
    bind -M visual w forward-word
    bind -M visual W forward-bigword
    bind -M visual e forward-word
    bind -M visual E forward-bigword
    bind -M visual o swap-selection-start-stop force-repaint

    for key in $eol_keys
        bind -M visual $key end-of-line
    end
    for key in $bol_keys
        bind -M visual $key beginning-of-line
    end

    bind -M visual -m insert c kill-selection end-selection force-repaint
    bind -M visual -m default d kill-selection end-selection force-repaint
    bind -M visual -m default x kill-selection end-selection force-repaint
    bind -M visual -m default X kill-whole-line end-selection force-repaint
    bind -M visual -m default y kill-selection yank end-selection force-repaint
    bind -M visual -m default '"*y' "commandline -s | xsel -p" end-selection force-repaint

    bind -M visual -m default \cc end-selection force-repaint
    bind -M visual -m default \e end-selection force-repaint

    # Make it easy to turn an unexecuted command into a comment in the shell history. Also, remove
    # the commenting chars so the command can be further edited then executed.
    bind -M default \# __fish_toggle_comment_commandline
    bind -M visual \# __fish_toggle_comment_commandline

    # Set the cursor shape
    # After executing once, this will have defined functions listening for the variable.
    # Therefore it needs to be before setting fish_bind_mode.
    fish_vi_cursor

    set fish_bind_mode $init_mode

end
