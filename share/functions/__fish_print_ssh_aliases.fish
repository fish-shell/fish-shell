
function __fish_print_ssh_aliases -d "Print a list of known ssh aliases"
	# Print ssh aliases
	if test -f ~/.ssh/config
		awk '/^[[:space:]]*Host\>/ {print $2}' ~/.ssh/config
	end
end
