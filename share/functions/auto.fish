#
# auto -- autoload functions and completions
#
# Synopsis
#   auto <path> [<path>...]
#
# Description
#   Add the specified list of directories to $fish_function_path
#   or $fish_complete_path.
#
function auto -d "autoload functions and completions"
	for path in $argv
		test -d "$path"; and set -l dest fish_function_path; or continue
		switch $path
			case \*/completions; set dest fish_complete_path
		end
		set -l i (contains -i -- "$path" $$dest); and set -e $dest[1][$i]
		set $dest "$path" $$dest
	end
end

