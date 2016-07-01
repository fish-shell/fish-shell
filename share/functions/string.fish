# XXX nostring
if not contains string (builtin -n)
	function string
		if not set -q __is_launched_without_string
			if status --is-interactive
				# We've been autoloaded after fish < 2.3.0 upgraded to >= 2.3.1 - no string builtin
				set_color --bold >&2
				echo "Fish has been upgraded, and the scripts on your system are not compatible" >&2
				echo "with this prior instance of fish. You can probably run:" >&2
				set_color green >&2
				echo -e "\n exec fish" >&2
				set_color normal >&2
				echo "… to replace this process with a new one in-place." >&2
				set -g __is_launched_without_string 1
			end
	    end
	    # We hope that in $__fish_bin_path is a newer fish that can do `string` for us.
	    
	    set fish_user_paths $__fish_bin_dir $fish_user_paths
	    set string_cmd string \'$argv\'
	    fish -c "$string_cmd"
	end                                                                  
end
