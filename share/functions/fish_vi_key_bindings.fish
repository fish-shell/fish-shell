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

    bind --preset $argv insert \r execute
    bind --preset $argv insert \n execute

    bind --preset $argv insert "" self-insert

    # Add way to kill current command line while in insert mode.
    bind --preset $argv insert \cc __fish_cancel_commandline
    # Add a way to switch from insert to normal (command) mode.
    # Note if we are paging, we want to stay in insert mode
    # See #2871
    bind --preset $argv insert \e "if commandline -P; commandline -f cancel; else; set fish_bind_mode default; commandline -f backward-char force-repaint; end"

    # Default (command) mode
    bind --preset :q exit
    bind --preset -m insert \cc __fish_cancel_commandline
    bind --preset $argv default h backward-char
    bind --preset $argv default l forward-char
    bind --preset -m insert \n execute
    bind --preset -m insert \r execute
    bind --preset -m insert i force-repaint
    bind --preset -m insert I beginning-of-line force-repaint
    bind --preset -m insert a forward-char force-repaint
    bind --preset -m insert A end-of-line force-repaint
    bind --preset -m visual v begin-selection force-repaint

    #bind --preset -m insert o "commandline -a \n" down-line force-repaint
    #bind --preset -m insert O beginning-of-line "commandline -i \n" up-line force-repaint # doesn't work

    bind --preset gg beginning-of-buffer
    bind --preset G end-of-buffer

    for key in $eol_keys
        bind --preset $key end-of-line
    end
    for key in $bol_keys
        bind --preset $key beginning-of-line
    end

    bind --preset u history-search-backward
    bind --preset \cr history-search-forward

    bind --preset [ history-token-search-backward
    bind --preset ] history-token-search-forward

    bind --preset k up-or-search
    bind --preset j down-or-search
    bind --preset b backward-word
    bind --preset B backward-bigword
    bind --preset ge backward-word
    bind --preset gE backward-bigword
    bind --preset w forward-word forward-char
    bind --preset W forward-bigword forward-char
    bind --preset e forward-char forward-word backward-char
    bind --preset E forward-bigword backward-char

    # OS X SnowLeopard doesn't have these keys. Don't show an annoying error message.
    # Vi/Vim doesn't support these keys in insert mode but that seems silly so we do so anyway.
    bind --preset $argv insert -k home beginning-of-line 2>/dev/null
    bind --preset $argv default -k home beginning-of-line 2>/dev/null
    bind --preset $argv insert -k end end-of-line 2>/dev/null
    bind --preset $argv default -k end end-of-line 2>/dev/null

    # Vi moves the cursor back if, after deleting, it is at EOL.
    # To emulate that, move forward, then backward, which will be a NOP
    # if there is something to move forward to.
    bind --preset $argv default x delete-char forward-char backward-char
    bind --preset $argv default X backward-delete-char
    bind --preset $argv insert -k dc delete-char forward-char backward-char
    bind --preset $argv default -k dc delete-char forward-char backward-char

    # Backspace deletes a char in insert mode, but not in normal/default mode.
    bind --preset $argv insert -k backspace backward-delete-char
    bind --preset $argv default -k backspace backward-char
    bind --preset $argv insert \ch backward-delete-char
    bind --preset $argv default \ch backward-char
    bind --preset $argv insert \x7f backward-delete-char
    bind --preset $argv default \x7f backward-char
    bind --preset $argv insert \e\[3\;2~ backward-delete-char # Mavericks Terminal.app shift-ctrl-delete
    bind --preset $argv default \e\[3\;2~ backward-delete-char # Mavericks Terminal.app shift-ctrl-delete

    bind --preset dd kill-whole-line
    bind --preset D kill-line
    bind --preset d\$ kill-line
    bind --preset d\^ backward-kill-line
    bind --preset dw kill-word
    bind --preset dW kill-bigword
    bind --preset diw forward-char forward-char backward-word kill-word
    bind --preset diW forward-char forward-char backward-bigword kill-bigword
    bind --preset daw forward-char forward-char backward-word kill-word
    bind --preset daW forward-char forward-char backward-bigword kill-bigword
    bind --preset de kill-word
    bind --preset dE kill-bigword
    bind --preset db backward-kill-word
    bind --preset dB backward-kill-bigword
    bind --preset dge backward-kill-word
    bind --preset dgE backward-kill-bigword
    bind --preset df begin-selection forward-jump kill-selection end-selection
    bind --preset dt begin-selection forward-jump backward-char kill-selection end-selection
    bind --preset dF begin-selection backward-jump kill-selection end-selection
    bind --preset dT begin-selection backward-jump forward-char kill-selection end-selection

    bind --preset -m insert s delete-char force-repaint
    bind --preset -m insert S kill-whole-line force-repaint
    bind --preset -m insert cc kill-whole-line force-repaint
    bind --preset -m insert C kill-line force-repaint
    bind --preset -m insert c\$ kill-line force-repaint
    bind --preset -m insert c\^ backward-kill-line force-repaint
    bind --preset -m insert cw kill-word force-repaint
    bind --preset -m insert cW kill-bigword force-repaint
    bind --preset -m insert ciw forward-char forward-char backward-word kill-word force-repaint
    bind --preset -m insert ciW forward-char forward-char backward-bigword kill-bigword force-repaint
    bind --preset -m insert caw forward-char forward-char backward-word kill-word force-repaint
    bind --preset -m insert caW forward-char forward-char backward-bigword kill-bigword force-repaint
    bind --preset -m insert ce kill-word force-repaint
    bind --preset -m insert cE kill-bigword force-repaint
    bind --preset -m insert cb backward-kill-word force-repaint
    bind --preset -m insert cB backward-kill-bigword force-repaint
    bind --preset -m insert cge backward-kill-word force-repaint
    bind --preset -m insert cgE backward-kill-bigword force-repaint

    bind --preset '~' capitalize-word
    bind --preset gu downcase-word
    bind --preset gU upcase-word

    bind --preset J end-of-line delete-char
    bind --preset K 'man (commandline -t) 2>/dev/null; or echo -n \a'

    bind --preset yy kill-whole-line yank
    bind --preset Y kill-whole-line yank
    bind --preset y\$ kill-line yank
    bind --preset y\^ backward-kill-line yank
    bind --preset yw kill-word yank
    bind --preset yW kill-bigword yank
    bind --preset yiw forward-char forward-char backward-word kill-word yank
    bind --preset yiW forward-char forward-char backward-bigword kill-bigword yank
    bind --preset yaw forward-char forward-char backward-word kill-word yank
    bind --preset yaW forward-char forward-char backward-bigword kill-bigword yank
    bind --preset ye kill-word yank
    bind --preset yE kill-bigword yank
    bind --preset yb backward-kill-word yank
    bind --preset yB backward-kill-bigword yank
    bind --preset yge backward-kill-word yank
    bind --preset ygE backward-kill-bigword yank

    bind --preset f forward-jump
    bind --preset F backward-jump
    bind --preset t forward-jump-till
    bind --preset T backward-jump-till
    bind --preset ';' repeat-jump
    bind --preset , repeat-jump-reverse

    # in emacs yank means paste
    bind --preset p yank
    bind --preset P backward-char yank
    bind --preset gp yank-pop

    bind --preset '"*p' "commandline -i ( xsel -p; echo )[1]"
    bind --preset '"*P' backward-char "commandline -i ( xsel -p; echo )[1]"

    #
    # Lowercase r, enters replace_one mode
    #
    bind --preset -m replace_one r force-repaint
    bind --preset $argv replace_one -m default '' delete-char self-insert backward-char force-repaint
    bind --preset $argv replace_one -m default \e cancel force-repaint

    #
    # visual mode
    #
    bind --preset $argv visual h backward-char
    bind --preset $argv visual l forward-char

    bind --preset $argv visual k up-line
    bind --preset $argv visual j down-line

    bind --preset $argv visual b backward-word
    bind --preset $argv visual B backward-bigword
    bind --preset $argv visual ge backward-word
    bind --preset $argv visual gE backward-bigword
    bind --preset $argv visual w forward-word
    bind --preset $argv visual W forward-bigword
    bind --preset $argv visual e forward-word
    bind --preset $argv visual E forward-bigword
    bind --preset $argv visual o swap-selection-start-stop force-repaint

    bind --preset $argv visual f forward-jump
    bind --preset $argv visual t forward-jump-till
    bind --preset $argv visual F backward-jump
    bind --preset $argv visual T backward-jump-till

    for key in $eol_keys
        bind --preset $argv visual $key end-of-line
    end
    for key in $bol_keys
        bind --preset $argv visual $key beginning-of-line
    end

    bind --preset $argv visual -m insert c kill-selection end-selection force-repaint
    bind --preset $argv visual -m default d kill-selection end-selection force-repaint
    bind --preset $argv visual -m default x kill-selection end-selection force-repaint
    bind --preset $argv visual -m default X kill-whole-line end-selection force-repaint
    bind --preset $argv visual -m default y kill-selection yank end-selection force-repaint
    bind --preset $argv visual -m default '"*y' "commandline -s | xsel -p; commandline -f end-selection force-repaint"

    bind --preset $argv visual -m default \cc end-selection force-repaint
    bind --preset $argv visual -m default \e end-selection force-repaint

    # Make it easy to turn an unexecuted command into a comment in the shell history. Also, remove
    # the commenting chars so the command can be further edited then executed.
    bind --preset $argv default \# __fish_toggle_comment_commandline
    bind --preset $argv visual \# __fish_toggle_comment_commandline

    # Set the cursor shape
    # After executing once, this will have defined functions listening for the variable.
    # Therefore it needs to be before setting fish_bind_mode.
    fish_vi_cursor

    set fish_bind_mode $init_mode

end
