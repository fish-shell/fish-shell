function fish_update_completions --description "Update man-page based completions"
	eval \"$__fish_datadir/tools/create_manpage_completions.py\" --manpath --progress --cleanup-in '~/.config/fish/completions'
end
