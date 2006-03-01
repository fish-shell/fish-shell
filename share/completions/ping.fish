complete -c ping -a "(__fish_print_hostnames)" -x
complete -c ping -s a -d (N_ "Audible ping")
complete -c ping -s A -d (N_ "Adaptive ping")
complete -c ping -s b -d (N_ "Allow pinging a broadcast address")
complete -c ping -s B -d (N_ "Do not allow ping to change source address of probes")
complete -c ping -s c -d (N_ "Stop after specified number of ECHO_REQUEST packets") -x
complete -c ping -s d -d (N_ "Set the SO_DEBUG option on the socket being used")
complete -c ping -s F -d (N_ "Allocate and set 20 bit flow label on ECHO_REQUEST packets") -x
complete -c ping -s f -d (N_ "Flood ping")
complete -c ping -s i -d (N_ "Wait specified interval of seconds between sending each packet") -x
complete -c ping -s I -d (N_ "Set source address to specified interface address") -x -a "(__fish_print_interfaces; fish_print_addresses)"
complete -c ping -s l -d (N_ "Send the specified number of packets without waiting for reply") -x
complete -c ping -s L -d (N_ "Suppress loopback of multicast packets")
complete -c ping -s n -d (N_ "Numeric output only")
complete -c ping -s p -d (N_ "Pad packet with empty bytes") -x
complete -c ping -s Q -d (N_ "Set Quality of Service -related bits in ICMP datagrams") -x 
complete -c ping -s q -d (N_ "Quiet mode")
complete -c ping -s R -d (N_ "Record route")
complete -c ping -s r -d (N_ "Bypass the normal routing tables and send directly to a host on an attached interface")
complete -c ping -s s -d (N_ "Specifies the number of data bytes to be sent") -x
complete -c ping -s S -d (N_ "Set socket buffer size") -x
complete -c ping -s t -d (N_ "Set the IP Time to Live") -x
complete -c ping -s T -d (N_ "Set special IP timestamp options") -x
complete -c ping -s M -d (N_ "Select Path MTU Discovery strategy") -x -a "do want dont"
complete -c ping -s U -d (N_ "Print full user-to-user latency")
complete -c ping -s v -d (N_ "Verbose mode")
complete -c ping -s V -d (N_ "Display version and exit")
complete -c ping -s w -d (N_ "Specify  a timeout, in seconds, before ping exits regardless of how many packets have been sent or received") -x
complete -c ping -s W -d (N_ "Time to wait for a response, in seconds") -x

