if not contains string (builtin -n)
	function string
		if not set -q __is_launched_without_string
			if status --is-interactive
				# We've been autoloaded after fish < 2.3.0 upgraded to >= 2.3.1 - no string builtin
				set_color --bold
				echo "Fish has been upgraded, and the scripts on your system are not compatible"
				echo "with this prior instance of fish. You can probably run:"
				set_color green
				echo "\n exec fish"
				set_color normal
				echo "… to replace this process with a new one in-place."
			end
	     end
	end                                                                  
end
