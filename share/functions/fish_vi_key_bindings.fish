function fish_vi_key_bindings -d "vi-like key bindings for fish"

        bind --erase --all

        #
        # command (default) mode
        #

	      bind \cd exit
	      bind :q exit

        bind h backward-char
        bind l forward-char
        bind \e\[C forward-char
        bind \e\[D backward-char
        bind -k right forward-char
        bind -k left backward-char
	      bind \n execute
        bind -m insert i force-repaint
        bind -m insert a forward-char force-repaint

        bind \x24 end-of-line
        bind \x5e beginning-of-line
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

        bind dd kill-line

        bind y yank
        bind p yank-pop

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
end
