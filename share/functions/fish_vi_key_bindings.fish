function fish_vi_key_bindings --description 'vi-like key bindings for fish'
	if test "$fish_key_bindings" != "fish_vi_key_bindings"
		# Allow the user to set the variable universally
		set -q fish_key_bindings; or set -g fish_key_bindings
		set fish_key_bindings fish_vi_key_bindings # This triggers the handler, which calls us again and ensures the user_key_bindings are executed
		return
	end
    # The default escape timeout is 300ms. But for users of Vi bindings that can
    # be slightly annoying when trying to switch to Vi "normal" mode. Too,
    # vi-mode users are unlikely to use escape-as-meta. So set a much shorter
    # timeout in this case.
    set -q fish_escape_delay_ms
    or set -g fish_escape_delay_ms 10

    set -l init_mode insert
    set -l eol_keys \$ g\$ \e\[F
    set -l bol_keys \^ 0 g\^ \e\[H
    if set -q argv[1]
        set init_mode $argv[1]
    end

    # Inherit default key bindings.
    # Do this first so vi-bindings win over default.
    bind --erase --all
    fish_default_key_bindings -M insert
    fish_default_key_bindings -M default

    # Remove the default self-insert bindings in default mode
    bind -e "" -M default
    # Add way to kill current command line while in insert mode.
    bind -M insert \cc __fish_cancel_commandline
    # Add a way to switch from insert to normal (command) mode.
    bind -M insert -m default \e backward-char force-repaint

    # Default (command) mode
    bind :q exit
    bind \cd exit
    bind -m insert \cc __fish_cancel_commandline
    bind h backward-char
    bind l forward-char
    bind \e\[C forward-char
    bind \e\[D backward-char

    # Some terminals output these when they're in in keypad mode.
    bind \eOC forward-char
    bind \eOD backward-char

    bind -k right forward-char
    bind -k left backward-char
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
    bind \e\[A up-or-search
    bind \e\[B down-or-search
    bind -k down down-or-search
    bind -k up up-or-search
    bind \eOA up-or-search
    bind \eOB down-or-search

    bind b backward-word
    bind B backward-bigword
    bind ge backward-word
    bind gE backward-bigword
    bind w forward-word forward-char
    bind W forward-bigword forward-char
    bind e forward-char forward-word backward-char
    bind E forward-bigword backward-char

    bind x delete-char
    bind X backward-delete-char

    bind -k dc delete-char

    bind -k backspace backward-delete-char
    bind \x7f backward-delete-char
    bind \e\[3\;2~ backward-delete-char # Mavericks Terminal.app shift-delete

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

    ### Overrides
    # This is complete in vim
    bind -M insert \cx end-of-line

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
    bind -M visual \e\[C forward-char
    bind -M visual \e\[D backward-char
    bind -M visual -k right forward-char
    bind -M visual -k left backward-char
    bind -M insert \eOC forward-char
    bind -M insert \eOD backward-char
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

    set fish_bind_mode $init_mode

    # Make it easy to turn an unexecuted command into a comment in the shell history. Also, remove
    # the commenting chars so the command can be further edited then executed.
    bind -M default \# __fish_toggle_comment_commandline
    bind -M visual \# __fish_toggle_comment_commandline
    bind -M default \e\# __fish_toggle_comment_commandline
    bind -M insert \e\# __fish_toggle_comment_commandline
    bind -M visual \e\# __fish_toggle_comment_commandline
end
