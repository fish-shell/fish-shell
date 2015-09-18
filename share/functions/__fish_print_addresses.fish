function __fish_print_addresses --description "Print a list of known network addresses"
	if command -s ip >/dev/null
		command ip --oneline address | cut -d" " -f7 | sed "s:\(.*\)/.*:\1:"
	else if command -s ifconfig >/dev/null
		# This is for OSX/BSD
		# There's also linux ifconfig but that has at least two different output formats
		# is basically dead, and ip is installed on everything now
		ifconfig | awk '/^\tinet/ { print $2 } '
	end
end

