function __fish_print_interfaces --description "Print a list of known network interfaces"
	if test -d /sys/class/net
		cd /sys/class/net
		for i in *
			echo $i
		end
	else
		netstat -i -n -a | awk 'NR>2'|awk '{print $1}'
	end
end
