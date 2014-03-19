function fish_update_completions --description "Update man-page based completions"
	# Set the correct configuration directory
	set -l configdir ~/.config
	if set -q XDG_CONFIG_HOME
		set configdir $XDG_CONFIG_HOME
	end

	eval \"$__fish_datadir/tools/create_manpage_completions.py\" --manpath --progress --cleanup-in "$configdir/fish/generated_completions"
end
