complete -c ping -a "(__fish_print_hostnames)" -x
complete -c ping -s a -d (_ "Audible ping")
complete -c ping -s A -d (_ "Adaptive ping")
complete -c ping -s b -d (_ "Allow pinging a broadcast address")
complete -c ping -s B -d (_ "Do not allow ping to change source address of probes")
complete -c ping -s c -d (_ "Stop after specified number of ECHO_REQUEST packets") -x
complete -c ping -s d -d (_ "Set the SO_DEBUG option on the socket being used")
complete -c ping -s F -d (_ "Allocate and set 20 bit flow label on ECHO_REQUEST packets") -x
complete -c ping -s f -d (_ "Flood ping")
complete -c ping -s i -d (_ "Wait specified interval of seconds between sending each packet") -x
complete -c ping -s I -d (_ "Set source address to specified interface address") -x -a "(__fish_print_interfaces; fish_print_addresses)"
complete -c ping -s l -d (_ "Send the specified number of packets without waiting for reply") -x
complete -c ping -s L -d (_ "Suppress loopback of multicast packets")
complete -c ping -s n -d (_ "Numeric output only")
complete -c ping -s p -d (_ "Pad packet with empty bytes") -x
complete -c ping -s Q -d (_ "Set Quality of Service -related bits in ICMP datagrams") -x 
complete -c ping -s q -d (_ "Quiet mode")
complete -c ping -s R -d (_ "Record route")
complete -c ping -s r -d (_ "Bypass the normal routing tables and send directly to a host on an attached interface")
complete -c ping -s s -d (_ "Specifies the number of data bytes to be sent") -x
complete -c ping -s S -d (_ "Set socket buffer size") -x
complete -c ping -s t -d (_ "Set the IP Time to Live") -x
complete -c ping -s T -d (_ "Set special IP timestamp options") -x
complete -c ping -s M -d (_ "Select Path MTU Discovery strategy") -x -a "do want dont"
complete -c ping -s U -d (_ "Print full user-to-user latency")
complete -c ping -s v -d (_ "Verbose mode")
complete -c ping -s V -d (_ "Display version and exit")
complete -c ping -s w -d (_ "Specify  a timeout, in seconds, before ping exits regardless of how many packets have been sent or received") -x
complete -c ping -s W -d (_ "Time to wait for a response, in seconds") -x

