function __fish_print_addresses --description "Print a list of known network addresses"
	if command -s ip >/dev/null
		command ip --oneline address | cut -d" " -f7 | sed "s:\(.*\)/.*:\1:"
	else if command -s ifconfig >/dev/null
		command ifconfig |sgrep 'inet addr'|cut -d : -f 2|cut -d ' ' -f 1
	end
end

