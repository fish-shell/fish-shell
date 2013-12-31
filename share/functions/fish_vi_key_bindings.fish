function nothing
end

function fish_vi_key_bindings -d "vi-like key bindings for fish"

        bind --erase --all

        #
        # command (default) mode
        #

	      bind \cd exit
	      bind : exit

        bind h backward-char
        bind l forward-char
        bind \e\[C forward-char
        bind \e\[D backward-char
        bind -k right forward-char
        bind -k left backward-char
	      bind \n execute
        bind -m insert i nothing
        bind -m insert a forward-char

        bind \x24 end-of-line
        bind \x5e beginning-of-line
        bind \e\[H beginning-of-line
        bind \e\[F end-of-line

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

        bind y yank
        bind p yank-pop

        #
        # insert mode
        #

        bind -M insert "" self-insert
	      bind -M insert \n execute

	      bind -M insert -k dc delete-char
	      bind -M insert -k backspace backward-delete-char
        bind -M insert -m default \e nothing
	      bind -M insert \t complete
end
