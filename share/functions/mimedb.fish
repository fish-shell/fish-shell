# Wrap mimedb in a function so it does not have to be found in PATH
# But only if it's installed
if test -x $__fish_bin_dir/mimedb
	eval "function mimedb --description 'Look up file information via the mimedb database'
	         \"$__fish_bin_dir/mimedb\" \$argv
	      end"
else
	function mimedb --description 'Look up file information via the mimedb database'
		# Create a function that simply fails, because mimedb not installed
		return 1
	end
end
