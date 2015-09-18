function __fish_print_interfaces --description "Print a list of known network interfaces"
	if test -d /sys/class/net
		cd /sys/class/net
		for i in *
			echo $i
		end
	else # OSX/BSD
		command ifconfig -l | tr ' ' '\n'
	end
end
