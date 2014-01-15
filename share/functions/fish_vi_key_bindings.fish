function fish_vi_key_bindings -d "vi-like key bindings for fish"

        bind --erase --all

        ##
        ## command mode
        ##
        #bind -m command : force-repaint
	    #bind -M command q exit
        #bind -M command -m default \e force-repaint
        #bind -M command -m default \cc force-repaint

	    bind :q exit

        #
        # normal (default) mode
        #

	    bind \cd exit
        bind h backward-char
        bind l forward-char
        bind \e\[C forward-char
        bind \e\[D backward-char
        bind -k right forward-char
        bind -k left backward-char
	    bind \n execute
        bind -m insert i force-repaint
        bind -m insert I beginning-of-line force-repaint
        bind -m insert a forward-char force-repaint
        bind -m insert A end-of-line force-repaint

        bind -m insert o "commandline -a \n" down-line force-repaint
        #bind -m insert O beginning-of-line "commandline -i \n" up-line force-repaint # doesn't work

        bind gg beginning-of-buffer
        bind G end-of-buffer

        bind \x24 end-of-line
        bind \x5e beginning-of-line
        bind g\x24 end-of-line
        bind g\x5e beginning-of-line
        bind \e\[H beginning-of-line
        bind \e\[F end-of-line

        # NOTE: history-search-backward and history-search-forward
        # must both be bound for `commandline -f ...' to work, and thus for up-or-search
        # and down-or-search to work, since those are actually 
        # simple shell functions that use `commandline -f ...'.
        # Generally, commandline -f can only invoke functions that have been bound previously

        bind u history-search-backward
        bind \cr history-search-forward

        bind k up-or-search
        bind j down-or-search
        bind \e\[A up-or-search
        bind \e\[B down-or-search
        bind -k down down-or-search
        bind -k up up-or-search

        bind b backward-word
        bind B backward-word
        bind w forward-word
        bind W backward-word
        bind e forward-word
        bind E backward-word

        bind x delete-char
        bind X backward-delete-char

        bind -k dc delete-char

        bind -k backspace backward-delete-char
        bind \x7f backward-delete-char
      	bind \e\[3\;2~ backward-delete-char # Mavericks Terminal.app shift-delete

        bind dd kill-whole-line
        bind D kill-line
        bind d\x24 kill-line
        bind d\x5e backward-kill-line
        bind dw kill-word
        bind dW backward-kill-word
        bind de kill-word
        bind dE backward-kill-word
        bind db backward-kill-word
        bind dB kill-word

        bind -m insert s delete-char force-repaint
        bind -m insert S kill-whole-line force-repaint
        bind -m insert cc kill-whole-line force-repaint
        bind -m insert C kill-line force-repaint
        bind -m insert c\x24 kill-line force-repaint
        bind -m insert c\x5e backward-kill-line force-repaint
        bind -m insert cw kill-word force-repaint
        bind -m insert ce kill-word force-repaint
        bind -m insert cb backward-kill-word force-repaint

        bind '~' capitalize-word 
        bind gu downcase-word 
        bind gU upcase-word 

        bind J end-of-line delete-char
        bind K 'man (basename (commandline -po; echo))[1] ^/dev/null; or echo -n \a'

        bind yy kill-whole-line yank
        bind Y  kill-whole-line yank
        bind y\x24 kill-line yank
        bind y\x5e backward-kill-line yank
        bind yw kill-word yank
        bind yW backward-kill-word yank
        bind ye kill-word yank
        bind yE backward-kill-word yank
        bind yb backward-kill-word yank
        bind yB kill-word yank

        bind p yank
        bind P backward-char yank-pop

        #
        # insert mode
        #

        bind -M insert "" self-insert
        bind -M insert \n execute

        bind -M insert -k dc delete-char

        bind -M insert -k backspace backward-delete-char
        bind -M insert \x7f backward-delete-char
        # Mavericks Terminal.app shift-delete
      	bind -M insert \e\[3\;2~ backward-delete-char 

        bind -M insert \t complete

        bind -M insert \e\[A up-or-search
        bind -M insert \e\[B down-or-search
        bind -M insert -k down down-or-search
        bind -M insert -k up up-or-search

        bind -M insert \e\[C forward-char
        bind -M insert \e\[D backward-char
        bind -M insert -k right forward-char
        bind -M insert -k left backward-char

        bind -M insert -m default \cc force-repaint
        bind -M insert -m default \e force-repaint

	    bind -M insert \cd exit

        bind -M insert \ef forward-word
end
