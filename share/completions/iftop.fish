function __fish_iftop_sort_orders
	echo -e "2s\tSort by 2s traffic average"
	echo -e "10s\tSort by 10s traffic average (default)"
	echo -e "40s\tSort by 40s traffic average"
	echo -e "source\tSort by source address"
	echo -e "destination\tSort by destination address"
end

complete -c iftop -f

complete -c iftop -s h -d "Print a summary of usage"
complete -c iftop -s n -d "Don't do hostname lookups"
complete -c iftop -s N -d "Don't resolve port number to service names"
complete -c iftop -s p -d "Run in promiscuous mode"
complete -c iftop -s P -d "Turn on port display"
complete -c iftop -s l -d "Include link-local IPv6 addresses"
complete -c iftop -s b -d "Don't display bar graphs of traffic"
complete -c iftop -s m -x -d "Set the upper limit for the bandwidth scale"
complete -c iftop -s B -d "Show bandwidth rates in bytes/s rather than bits/s"
complete -c iftop -s i -xa "(__fish_print_interfaces)" -d "Listen to packets on interface"
complete -c iftop -s f -x -d "Use filter code to select the packets to count"
complete -c iftop -s F -x -d "Filter to only specified IPv4 network"
complete -c iftop -s G -x -d "Filter to only specified IPv6 network"
complete -c iftop -s c -r -d "Use specified config file"
complete -c iftop -s o -xa "(__fish_iftop_sort_orders)" -d "Sort by specified column"

complete -c iftop -s t -d "Use text interface without ncurses"
complete -c iftop -s L -x -d "Number of lines to print with -t"
complete -c iftop -s s -x -d "With -t, print single output after specific number of seconds"
