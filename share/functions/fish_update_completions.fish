function fish_update_completions --description "Update man-page based completions"
	# Clean up old paths
	eval (string escape $__fish_datadir/tools/create_manpage_completions.py) --manpath --progress --cleanup-in '~/.config/fish/completions' --cleanup-in '~/.config/fish/generated_completions'
end
