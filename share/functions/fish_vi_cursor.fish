function fish_vi_cursor -d 'Set cursor shape for different vi modes'
	set -l terminal $argv[1]
	set -q terminal[1]; or set terminal auto

	set fcn
	switch "$terminal"
		case auto
			if set -q KONSOLE_PROFILE_NAME
				set fcn __fish_cursor_konsole
			else if set -q XTERM_LOCALE
				set fcn __fish_cursor_xterm
			else
				return 1
			end
	end

	set -g fish_cursor_insert  line
	set -g fish_cursor_default block
	set -g fish_cursor_unknown block blink

	echo "
	function fish_cursor_vi_handle --on-variable fish_bind_mode
		set -l varname fish_cursor_\$fish_bind_mode
		if not set -q \$varname
			set varname fish_cursor_unknown
		end
		#echo \$varname \$\$varname
		$fcn \$\$varname
	end
	" | source
end
