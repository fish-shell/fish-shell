function __fish_print_addresses --description "Print a list of known network addresses"
	/sbin/ifconfig |sgrep 'inet addr'|cut -d : -f 2|cut -d ' ' -f 1
end

