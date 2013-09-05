
function fish_default_key_bindings -d "Default (Emacs-like) key bindings for fish"

	# Clear earlier bindings, if any
	bind --erase --all

	# This is the default binding, i.e. the one used if no other binding matches
	bind "" self-insert

	bind \n execute

	bind \ck kill-line
	bind \cy yank
	bind \t complete

	bind \e\n "commandline -i \n"

	bind \e\[A up-or-search
	bind \e\[B down-or-search
	bind -k down down-or-search
	bind -k up up-or-search

	bind \e\[C forward-char
	bind \e\[D backward-char
	bind -k right forward-char
	bind -k left backward-char

	bind -k dc delete-char
	bind -k backspace backward-delete-char
	bind \x7f backward-delete-char

	bind \e\[H beginning-of-line
	bind \e\[F end-of-line

	# OS X SnowLeopard doesn't have these keys. Don't show an annoying error message.
	bind -k home beginning-of-line 2> /dev/null
	bind -k end end-of-line 2> /dev/null
	bind \e\[3\;2~ backward-delete-char # Mavericks Terminal.app shift-delete

	bind \e\eOC nextd-or-forward-word
	bind \e\eOD prevd-or-backward-word
	bind \e\e\[C nextd-or-forward-word
	bind \e\e\[D prevd-or-backward-word
	bind \eO3C nextd-or-forward-word
	bind \eO3D prevd-or-backward-word
	bind \e\[3C nextd-or-forward-word
	bind \e\[3D prevd-or-backward-word
	bind \e\[1\;3C nextd-or-forward-word
	bind \e\[1\;3D prevd-or-backward-word

	bind \e\eOA history-token-search-backward
	bind \e\eOB history-token-search-forward
	bind \e\e\[A history-token-search-backward
	bind \e\e\[B history-token-search-forward
	bind \eO3A history-token-search-backward
	bind \eO3B history-token-search-forward
	bind \e\[3A history-token-search-backward
	bind \e\[3B history-token-search-forward
	bind \e\[1\;3A history-token-search-backward
	bind \e\[1\;3B history-token-search-forward

	bind \ca beginning-of-line
	bind \ce end-of-line
	bind \ey yank-pop
	bind \ch backward-delete-char
	bind \cw backward-kill-word
	bind \cp history-search-backward
	bind \cn history-search-forward
	bind \cf forward-char
	bind \cb backward-char
	bind \ct transpose-chars
	bind \et transpose-words
	bind \e\x7f backward-kill-word
	bind \eb backward-word
	bind \ef forward-word
	bind \e\[1\;5C forward-word
	bind \e\[1\;5D backward-word
	bind \e\[1\;9C forward-word #iTerm2
	bind \e\[1\;9D backward-word #iTerm2
	bind \ed forward-kill-word
	bind -k ppage beginning-of-history
	bind -k npage end-of-history
	bind \e\< beginning-of-buffer
	bind \e\> end-of-buffer

	bind \el __fish_list_current_token
	bind \ew 'set tok (commandline -pt); if test $tok[1]; echo; whatis $tok[1]; commandline -f repaint; end'
	bind \cl 'clear; commandline -f repaint'
	bind \cc 'commandline ""'
	bind \cu backward-kill-line
	bind \ed kill-word
	bind \cw backward-kill-path-component
	bind \ed 'set -l cmd (commandline); if test -z "$cmd"; echo; dirh; commandline -f repaint; else; commandline -f kill-word; end'
	bind \cd delete-or-exit

	# This will make sure the output of the current command is paged using the less pager when you press Meta-p
	bind \ep '__fish_paginate'
	
	# term-specific special bindings
	switch "$TERM"
		case 'rxvt*'
			bind \e\[8~ end-of-line
			bind \eOc forward-word
			bind \eOd backward-word
	end
end
