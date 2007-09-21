function __fish_print_interfaces --description "Print a list of known network interfaces"
	netstat -i -n -a | awk 'NR>2'|awk '{print $1}'
end
