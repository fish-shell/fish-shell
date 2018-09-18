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

    # Silence warnings about unavailable keys. See #4431, 4188
    if not contains -- -s $argv
        set argv "-s" "-M" $argv
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

    bind --default $argv insert \r execute
    bind --default $argv insert \n execute

    bind --default $argv insert "" self-insert

    # Add way to kill current command line while in insert mode.
    bind --default $argv insert \cc __fish_cancel_commandline
    # Add a way to switch from insert to normal (command) mode.
    # Note if we are paging, we want to stay in insert mode
    # See #2871
    bind --default $argv insert \e "if commandline -P; commandline -f cancel; else; set fish_bind_mode default; commandline -f backward-char force-repaint; end"

    # Default (command) mode
    bind --default :q exit
    bind --default -m insert \cc __fish_cancel_commandline
    bind --default $argv default h backward-char
    bind --default $argv default l forward-char
    bind --default -m insert \n execute
    bind --default -m insert \r execute
    bind --default -m insert i force-repaint
    bind --default -m insert I beginning-of-line force-repaint
    bind --default -m insert a forward-char force-repaint
    bind --default -m insert A end-of-line force-repaint
    bind --default -m visual v begin-selection force-repaint

    #bind --default -m insert o "commandline -a \n" down-line force-repaint
    #bind --default -m insert O beginning-of-line "commandline -i \n" up-line force-repaint # doesn't work

    bind --default gg beginning-of-buffer
    bind --default G end-of-buffer

    for key in $eol_keys
        bind --default $key end-of-line
    end
    for key in $bol_keys
        bind --default $key beginning-of-line
    end

    bind --default u history-search-backward
    bind --default \cr history-search-forward

    bind --default [ history-token-search-backward
    bind --default ] history-token-search-forward

    bind --default k up-or-search
    bind --default j down-or-search
    bind --default b backward-word
    bind --default B backward-bigword
    bind --default ge backward-word
    bind --default gE backward-bigword
    bind --default w forward-word forward-char
    bind --default W forward-bigword forward-char
    bind --default e forward-char forward-word backward-char
    bind --default E forward-bigword backward-char

    # OS X SnowLeopard doesn't have these keys. Don't show an annoying error message.
    # Vi/Vim doesn't support these keys in insert mode but that seems silly so we do so anyway.
    bind --default $argv insert -k home beginning-of-line 2>/dev/null
    bind --default $argv default -k home beginning-of-line 2>/dev/null
    bind --default $argv insert -k end end-of-line 2>/dev/null
    bind --default $argv default -k end end-of-line 2>/dev/null

    # Vi moves the cursor back if, after deleting, it is at EOL.
    # To emulate that, move forward, then backward, which will be a NOP
    # if there is something to move forward to.
    bind --default $argv default x delete-char forward-char backward-char
    bind --default $argv default X backward-delete-char
    bind --default $argv insert -k dc delete-char forward-char backward-char
    bind --default $argv default -k dc delete-char forward-char backward-char

    # Backspace deletes a char in insert mode, but not in normal/default mode.
    bind --default $argv insert -k backspace backward-delete-char
    bind --default $argv default -k backspace backward-char
    bind --default $argv insert \ch backward-delete-char
    bind --default $argv default \ch backward-char
    bind --default $argv insert \x7f backward-delete-char
    bind --default $argv default \x7f backward-char
    bind --default $argv insert \e\[3\;2~ backward-delete-char # Mavericks Terminal.app shift-ctrl-delete
    bind --default $argv default \e\[3\;2~ backward-delete-char # Mavericks Terminal.app shift-ctrl-delete

    bind --default dd kill-whole-line
    bind --default D kill-line
    bind --default d\$ kill-line
    bind --default d\^ backward-kill-line
    bind --default dw kill-word
    bind --default dW kill-bigword
    bind --default diw forward-char forward-char backward-word kill-word
    bind --default diW forward-char forward-char backward-bigword kill-bigword
    bind --default daw forward-char forward-char backward-word kill-word
    bind --default daW forward-char forward-char backward-bigword kill-bigword
    bind --default de kill-word
    bind --default dE kill-bigword
    bind --default db backward-kill-word
    bind --default dB backward-kill-bigword
    bind --default dge backward-kill-word
    bind --default dgE backward-kill-bigword
    bind --default df begin-selection forward-jump kill-selection end-selection
    bind --default dt begin-selection forward-jump backward-char kill-selection end-selection
    bind --default dF begin-selection backward-jump kill-selection end-selection
    bind --default dT begin-selection backward-jump forward-char kill-selection end-selection

    bind --default -m insert s delete-char force-repaint
    bind --default -m insert S kill-whole-line force-repaint
    bind --default -m insert cc kill-whole-line force-repaint
    bind --default -m insert C kill-line force-repaint
    bind --default -m insert c\$ kill-line force-repaint
    bind --default -m insert c\^ backward-kill-line force-repaint
    bind --default -m insert cw kill-word force-repaint
    bind --default -m insert cW kill-bigword force-repaint
    bind --default -m insert ciw forward-char forward-char backward-word kill-word force-repaint
    bind --default -m insert ciW forward-char forward-char backward-bigword kill-bigword force-repaint
    bind --default -m insert caw forward-char forward-char backward-word kill-word force-repaint
    bind --default -m insert caW forward-char forward-char backward-bigword kill-bigword force-repaint
    bind --default -m insert ce kill-word force-repaint
    bind --default -m insert cE kill-bigword force-repaint
    bind --default -m insert cb backward-kill-word force-repaint
    bind --default -m insert cB backward-kill-bigword force-repaint
    bind --default -m insert cge backward-kill-word force-repaint
    bind --default -m insert cgE backward-kill-bigword force-repaint

    bind --default '~' capitalize-word
    bind --default gu downcase-word
    bind --default gU upcase-word

    bind --default J end-of-line delete-char
    bind --default K 'man (commandline -t) 2>/dev/null; or echo -n \a'

    bind --default yy kill-whole-line yank
    bind --default Y kill-whole-line yank
    bind --default y\$ kill-line yank
    bind --default y\^ backward-kill-line yank
    bind --default yw kill-word yank
    bind --default yW kill-bigword yank
    bind --default yiw forward-char forward-char backward-word kill-word yank
    bind --default yiW forward-char forward-char backward-bigword kill-bigword yank
    bind --default yaw forward-char forward-char backward-word kill-word yank
    bind --default yaW forward-char forward-char backward-bigword kill-bigword yank
    bind --default ye kill-word yank
    bind --default yE kill-bigword yank
    bind --default yb backward-kill-word yank
    bind --default yB backward-kill-bigword yank
    bind --default yge backward-kill-word yank
    bind --default ygE backward-kill-bigword yank

    bind --default f forward-jump
    bind --default F backward-jump
    bind --default t forward-jump-till
    bind --default T backward-jump-till
    bind --default ';' repeat-jump
    bind --default , repeat-jump-reverse

    # in emacs yank means paste
    bind --default p yank
    bind --default P backward-char yank
    bind --default gp yank-pop

    bind --default '"*p' "commandline -i ( xsel -p; echo )[1]"
    bind --default '"*P' backward-char "commandline -i ( xsel -p; echo )[1]"

    #
    # Lowercase r, enters replace_one mode
    #
    bind --default -m replace_one r force-repaint
    bind --default $argv replace_one -m default '' delete-char self-insert backward-char force-repaint
    bind --default $argv replace_one -m default \e cancel force-repaint

    #
    # visual mode
    #
    bind --default $argv visual h backward-char
    bind --default $argv visual l forward-char

    bind --default $argv visual k up-line
    bind --default $argv visual j down-line

    bind --default $argv visual b backward-word
    bind --default $argv visual B backward-bigword
    bind --default $argv visual ge backward-word
    bind --default $argv visual gE backward-bigword
    bind --default $argv visual w forward-word
    bind --default $argv visual W forward-bigword
    bind --default $argv visual e forward-word
    bind --default $argv visual E forward-bigword
    bind --default $argv visual o swap-selection-start-stop force-repaint

    bind --default $argv visual f forward-jump
    bind --default $argv visual t forward-jump-till
    bind --default $argv visual F backward-jump
    bind --default $argv visual T backward-jump-till

    for key in $eol_keys
        bind --default $argv visual $key end-of-line
    end
    for key in $bol_keys
        bind --default $argv visual $key beginning-of-line
    end

    bind --default $argv visual -m insert c kill-selection end-selection force-repaint
    bind --default $argv visual -m default d kill-selection end-selection force-repaint
    bind --default $argv visual -m default x kill-selection end-selection force-repaint
    bind --default $argv visual -m default X kill-whole-line end-selection force-repaint
    bind --default $argv visual -m default y kill-selection yank end-selection force-repaint
    bind --default $argv visual -m default '"*y' "commandline -s | xsel -p; commandline -f end-selection force-repaint"

    bind --default $argv visual -m default \cc end-selection force-repaint
    bind --default $argv visual -m default \e end-selection force-repaint

    # Make it easy to turn an unexecuted command into a comment in the shell history. Also, remove
    # the commenting chars so the command can be further edited then executed.
    bind --default $argv default \# __fish_toggle_comment_commandline
    bind --default $argv visual \# __fish_toggle_comment_commandline

    # Set the cursor shape
    # After executing once, this will have defined functions listening for the variable.
    # Therefore it needs to be before setting fish_bind_mode.
    fish_vi_cursor

    set fish_bind_mode $init_mode

end
