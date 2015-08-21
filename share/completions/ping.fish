complete -c ping -a "(__fish_print_hostnames)" -x
complete -c ping -s a --description "Audible ping"
complete -c ping -s A --description "Adaptive ping"
complete -c ping -s b --description "Allow pinging a broadcast address"
complete -c ping -s B --description "Do not allow ping to change source address of probes"
complete -c ping -s c --description "Stop after specified number of ECHO_REQUEST packets" -x
complete -c ping -s d --description "Set the SO_DEBUG option on the socket being used"
complete -c ping -s F --description "Allocate and set 20 bit flow label on ECHO_REQUEST packets" -x
complete -c ping -s f --description "Flood ping"
complete -c ping -s i --description "Wait specified interval of seconds between sending each packet" -x
complete -c ping -s I --description "Set source address to specified interface address" -x -a "(__fish_print_interfaces; __fish_print_addresses)"
complete -c ping -s l --description "Send the specified number of packets without waiting for reply" -x
complete -c ping -s L --description "Suppress loopback of multicast packets"
complete -c ping -s n --description "Numeric output only"
complete -c ping -s p --description "Pad packet with empty bytes" -x
complete -c ping -s Q --description "Set Quality of Service -related bits in ICMP datagrams" -x
complete -c ping -s q --description "Quiet mode"
complete -c ping -s R --description "Record route"
complete -c ping -s r --description "Bypass the normal routing tables and send directly to a host on an attached interface"
complete -c ping -s s --description "Specifies the number of data bytes to be sent" -x
complete -c ping -s S --description "Set socket buffer size" -x
complete -c ping -s t --description "Set the IP Time to Live" -x
complete -c ping -s T --description "Set special IP timestamp options" -x
complete -c ping -s M --description "Select Path MTU Discovery strategy" -x -a "do want dont"
complete -c ping -s U --description "Print full user-to-user latency"
complete -c ping -s v --description "Verbose mode"
complete -c ping -s V --description "Display version and exit"
complete -c ping -s w --description "Specify  a timeout, in seconds, before ping exits regardless of how many packets have been sent or received" -x
complete -c ping -s W --description "Time to wait for a response, in seconds" -x

