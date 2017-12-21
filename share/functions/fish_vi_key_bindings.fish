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

    # "-s" to silence warnings about unavailable keys. See #4431, 4188
    bind -s -M insert \r execute
    bind -s -M insert \n execute

    bind -s -M insert "" self-insert

    # Add way to kill current command line while in insert mode.
    bind -s -M insert \cc __fish_cancel_commandline
    # Add a way to switch from insert to normal (command) mode.
    # Note if we are paging, we want to stay in insert mode
    # See #2871
    bind -s -M insert \e "if commandline -P; commandline -f cancel; else; set fish_bind_mode default; commandline -f backward-char force-repaint; end"

    # Default (command) mode
    bind -s :q exit
    bind -s -m insert \cc __fish_cancel_commandline
    bind -s -M default h backward-char
    bind -s -M default l forward-char
    bind -s -m insert \n execute
    bind -s -m insert \r execute
    bind -s -m insert i force-repaint
    bind -s -m insert I beginning-of-line force-repaint
    bind -s -m insert a forward-char force-repaint
    bind -s -m insert A end-of-line force-repaint
    bind -s -m visual v begin-selection force-repaint

    #bind -s -m insert o "commandline -a \n" down-line force-repaint
    #bind -s -m insert O beginning-of-line "commandline -i \n" up-line force-repaint # doesn't work

    bind -s gg beginning-of-buffer
    bind -s G end-of-buffer

    for key in $eol_keys
        bind -s $key end-of-line
    end
    for key in $bol_keys
        bind -s $key beginning-of-line
    end

    bind -s u history-search-backward
    bind -s \cr history-search-forward

    bind -s [ history-token-search-backward
    bind -s ] history-token-search-forward

    bind -s k up-or-search
    bind -s j down-or-search
    bind -s b backward-word
    bind -s B backward-bigword
    bind -s ge backward-word
    bind -s gE backward-bigword
    bind -s w forward-word forward-char
    bind -s W forward-bigword forward-char
    bind -s e forward-char forward-word backward-char
    bind -s E forward-bigword backward-char

    # OS X SnowLeopard doesn't have these keys. Don't show an annoying error message.
    # Vi/Vim doesn't support these keys in insert mode but that seems silly so we do so anyway.
    bind -s -M insert -k home beginning-of-line 2>/dev/null
    bind -s -M default -k home beginning-of-line 2>/dev/null
    bind -s -M insert -k end end-of-line 2>/dev/null
    bind -s -M default -k end end-of-line 2>/dev/null

    # Vi moves the cursor back if, after deleting, it is at EOL.
    # To emulate that, move forward, then backward, which will be a NOP
    # if there is something to move forward to.
    bind -s -M default x delete-char forward-char backward-char
    bind -s -M default X backward-delete-char
    bind -s -M insert -k dc delete-char forward-char backward-char
    bind -s -M default -k dc delete-char forward-char backward-char

    # Backspace deletes a char in insert mode, but not in normal/default mode.
    bind -s -M insert -k backspace backward-delete-char
    bind -s -M default -k backspace backward-char
    bind -s -M insert \ch backward-delete-char
    bind -s -M default \ch backward-char
    bind -s -M insert \x7f backward-delete-char
    bind -s -M default \x7f backward-char
    bind -s -M insert \e\[3\;2~ backward-delete-char # Mavericks Terminal.app shift-ctrl-delete
    bind -s -M default \e\[3\;2~ backward-delete-char # Mavericks Terminal.app shift-ctrl-delete

    bind -s dd kill-whole-line
    bind -s D kill-line
    bind -s d\$ kill-line
    bind -s d\^ backward-kill-line
    bind -s dw kill-word
    bind -s dW kill-bigword
    bind -s diw forward-char forward-char backward-word kill-word
    bind -s diW forward-char forward-char backward-bigword kill-bigword
    bind -s daw forward-char forward-char backward-word kill-word
    bind -s daW forward-char forward-char backward-bigword kill-bigword
    bind -s de kill-word
    bind -s dE kill-bigword
    bind -s db backward-kill-word
    bind -s dB backward-kill-bigword
    bind -s dge backward-kill-word
    bind -s dgE backward-kill-bigword

    bind -s -m insert s delete-char force-repaint
    bind -s -m insert S kill-whole-line force-repaint
    bind -s -m insert cc kill-whole-line force-repaint
    bind -s -m insert C kill-line force-repaint
    bind -s -m insert c\$ kill-line force-repaint
    bind -s -m insert c\^ backward-kill-line force-repaint
    bind -s -m insert cw kill-word force-repaint
    bind -s -m insert cW kill-bigword force-repaint
    bind -s -m insert ciw forward-char forward-char backward-word kill-word force-repaint
    bind -s -m insert ciW forward-char forward-char backward-bigword kill-bigword force-repaint
    bind -s -m insert caw forward-char forward-char backward-word kill-word force-repaint
    bind -s -m insert caW forward-char forward-char backward-bigword kill-bigword force-repaint
    bind -s -m insert ce kill-word force-repaint
    bind -s -m insert cE kill-bigword force-repaint
    bind -s -m insert cb backward-kill-word force-repaint
    bind -s -m insert cB backward-kill-bigword force-repaint
    bind -s -m insert cge backward-kill-word force-repaint
    bind -s -m insert cgE backward-kill-bigword force-repaint

    bind -s '~' capitalize-word
    bind -s gu downcase-word
    bind -s gU upcase-word

    bind -s J end-of-line delete-char
    bind -s K 'man (commandline -t) ^/dev/null; or echo -n \a'

    bind -s yy kill-whole-line yank
    bind -s Y kill-whole-line yank
    bind -s y\$ kill-line yank
    bind -s y\^ backward-kill-line yank
    bind -s yw kill-word yank
    bind -s yW kill-bigword yank
    bind -s yiw forward-char forward-char backward-word kill-word yank
    bind -s yiW forward-char forward-char backward-bigword kill-bigword yank
    bind -s yaw forward-char forward-char backward-word kill-word yank
    bind -s yaW forward-char forward-char backward-bigword kill-bigword yank
    bind -s ye kill-word yank
    bind -s yE kill-bigword yank
    bind -s yb backward-kill-word yank
    bind -s yB backward-kill-bigword yank
    bind -s yge backward-kill-word yank
    bind -s ygE backward-kill-bigword yank

    bind -s f forward-jump
    bind -s F backward-jump
    bind -s t forward-jump and backward-char
    bind -s T backward-jump and forward-char

    # in emacs yank means paste
    bind -s p yank
    bind -s P backward-char yank
    bind -s gp yank-pop

    bind -s '"*p' "commandline -i ( xsel -p; echo )[1]"
    bind -s '"*P' backward-char "commandline -i ( xsel -p; echo )[1]"

    #
    # Lowercase r, enters replace_one mode
    #
    bind -s -m replace_one r force-repaint
    bind -s -M replace_one -m default '' delete-char self-insert backward-char force-repaint
    bind -s -M replace_one -m default \e cancel force-repaint

    #
    # visual mode
    #
    bind -s -M visual h backward-char
    bind -s -M visual l forward-char

    bind -s -M visual k up-line
    bind -s -M visual j down-line

    bind -s -M visual b backward-word
    bind -s -M visual B backward-bigword
    bind -s -M visual ge backward-word
    bind -s -M visual gE backward-bigword
    bind -s -M visual w forward-word
    bind -s -M visual W forward-bigword
    bind -s -M visual e forward-word
    bind -s -M visual E forward-bigword
    bind -s -M visual o swap-selection-start-stop force-repaint

    for key in $eol_keys
        bind -s -M visual $key end-of-line
    end
    for key in $bol_keys
        bind -s -M visual $key beginning-of-line
    end

    bind -s -M visual -m insert c kill-selection end-selection force-repaint
    bind -s -M visual -m default d kill-selection end-selection force-repaint
    bind -s -M visual -m default x kill-selection end-selection force-repaint
    bind -s -M visual -m default X kill-whole-line end-selection force-repaint
    bind -s -M visual -m default y kill-selection yank end-selection force-repaint
    bind -s -M visual -m default '"*y' "commandline -s | xsel -p" end-selection force-repaint

    bind -s -M visual -m default \cc end-selection force-repaint
    bind -s -M visual -m default \e end-selection force-repaint

    # Make it easy to turn an unexecuted command into a comment in the shell history. Also, remove
    # the commenting chars so the command can be further edited then executed.
    bind -s -M default \# __fish_toggle_comment_commandline
    bind -s -M visual \# __fish_toggle_comment_commandline

    # Set the cursor shape
    # After executing once, this will have defined functions listening for the variable.
    # Therefore it needs to be before setting fish_bind_mode.
    fish_vi_cursor

    set fish_bind_mode $init_mode

end
