function fish_indent --description 'Indenter and prettifier for fish code'
	# This is wrapped in a function so that fish_indent does not need to be found in PATH
	eval $__fish_bin_dir/fish_indent $argv
end
