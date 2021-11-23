complete -c ping -xa "(__fish_print_hostnames)"

set -l ping_version (ping -V 2>&1 | string collect)

switch $ping_version
    case '*iputils*'
        # completions for ping from iputils
        complete -c ping -s 4 -d "Use IPv4"
        complete -c ping -s 6 -d "Use IPv6"
        complete -c ping -s a -d "Audible ping"
        complete -c ping -s A -d "Adaptive ping"
        complete -c ping -s b -d "Allow pinging a broadcast address (IPv4 only)"
        complete -c ping -s B -d "Sticky source address"
        complete -c ping -s c -d "Stop after specified number of packets" -x
        complete -c ping -s D -d "Print timestamps"
        complete -c ping -s d -d "Set SO_DEBUG socket option"
        complete -c ping -s f -d "Flood ping"
        complete -c ping -s F -d "Define flow label (IPv6 only)" -x
        complete -c ping -s h -d "Print help and exit"
        complete -c ping -s I -d "Set source address to specified interface address" -xa "(__fish_print_interfaces; __fish_print_addresses)"
        complete -c ping -s i -d "Seconds between sending each packet" -x
        complete -c ping -s L -d "Suppress loopback of multicast packets"
        complete -c ping -s l -d "Send the specified number of packets without waiting for reply" -x
        complete -c ping -s m -d "Tag the packets going out" -x
        complete -c ping -s M -d "Select Path MTU Discovery strategy" -xa "do want dont"
        complete -c ping -s n -d "No dns name resolution"
        complete -c ping -s N -d "Send ICMPv6 Node Information Queries (IPv6 only)" -x
        complete -c ping -s O -d "Report outstanding replies"
        complete -c ping -s p -d "Contents of padding byte" -x
        complete -c ping -s R -d "Record route (IPv4 only)"
        complete -c ping -s q -d "Quiet mode"
        complete -c ping -s Q -d "Set QoS bits in ICMP datagrams" -x
        complete -c ping -s s -d "Number of data bytes to be sent" -x
        complete -c ping -s S -d "Set socket buffer size" -x
        complete -c ping -s t -d "Set the IP Time to Live" -x
        complete -c ping -s T -d "Set IP timestamp options (IPv4 only)" -xa "tsonly tsandaddr tsprespec"
        complete -c ping -s U -d "Print full user-to-user latency"
        complete -c ping -s v -d "Verbose mode"
        complete -c ping -s V -d "Display version and exit"
        complete -c ping -s w -d "Specify a timeout, in seconds, before ping exits" -x
        complete -c ping -s W -d "Time to wait for a response, in seconds" -x

    case '*inetutils*'
        # completions for ping from GNU inetutils
        complete -c ping -l address -l mask -d "Send ICMP_ADDRESS packets"
        complete -c ping -l echo -d "Send ICMP_ECHO packets"
        complete -c ping -l timestamp -d "Send ICMP_TIMESTAMP packets"
        complete -c ping -s t -l type -d "Set packet type" -x
        complete -c ping -s c -l count -d "Set number of packets to send" -x
        complete -c ping -s d -l debug -d "Set the SO_DEBUG option"
        complete -c ping -s i -l interval -d "Set seconds to wait between sending each packet" -x
        complete -c ping -s n -l numeric -d "Do not resolve host addresses"
        complete -c ping -s r -l ignore-routing -d "Send directly to a host on an attached network"
        complete -c ping -l ttl -d "Set time-to-live" -x
        complete -c ping -s T -l tos -d "Set type of service" -x
        complete -c ping -s v -l verbose -d "Verbose output"
        complete -c ping -s w -l timeout -d "Stop after N seconds" -x
        complete -c ping -s W -l linger -d "Number of seconds to wait for response" -x
        complete -c ping -s f -l flood -d "Flood ping"
        complete -c ping -l ip-timestamp -d "Set IP timestamp type" -xa "tsonly tsaddr"
        complete -c ping -s l -l preload -d "Send number of packets as fast as possible" -x
        complete -c ping -s p -l pattern -d "Fill ICMP packet with given pattern" -x
        complete -c ping -s q -l quiet -d "Quiet output"
        complete -c ping -s R -l route -d "Record route"
        complete -c ping -s s -l size -d "Set number of data octets" -x
        complete -c ping -s \? -l help -d "Show help"
        complete -c ping -l usage -d "Short usage message"
        complete -c ping -s V -l version -d "Print program version"

    case '*BusyBox*'
        # completions for ping from busybox
        complete -c ping -s 4 -d "Use IPv4"
        complete -c ping -s 6 -d "Use IPv6"
        complete -c ping -s c -d "Set number of pings to send" -x
        complete -c ping -s s -d "Set data size" -x
        complete -c ping -s i -d "Interval (seconds)" -x
        complete -c ping -s A -d "Ping as soon as reply is received"
        complete -c ping -s t -d "Set TTL" -x
        complete -c ping -s I -d "Source interface or IP address" -xa "(__fish_print_interfaces; __fish_print_addresses)"
        complete -c ping -s W -d "Seconds to wait for the first response" -x
        complete -c ping -s w -d "Seconds until ping exits" -x
        complete -c ping -s q -d Quiet
        complete -c ping -s p -d "Payload pattern" -x

    case '*'
        set -l ping_os_version (uname)
        switch $ping_os_version
            case '*FreeBSD*' '*Darwin*'
                # In common with FreeBSD and Apple
                complete -c ping -s A -d "Audible ping"
                complete -c ping -s a -d "Audible ping"
                complete -c ping -s C -d "Prohibit the socket from using the cellular network interface"
                complete -c ping -s c -d "Stop after specified number of packets" -x
                complete -c ping -s D -d "Set the Don't Fragment bit"
                complete -c ping -s d -d "Set the SO_DEBUG socket option"
                complete -c ping -s f -d "Flood ping"
                complete -c ping -s G -d "Maximum payload size for sweeping pings" -x
                complete -c ping -s g -d "Minimum payload size for sweeping pings" -x
                complete -c ping -s h -d "Payload size increment for sweeping pings" -x
                complete -c ping -s I -d "Source multicast packets with the given interface address" -x
                complete -c ping -s i -d "Seconds between sending each packet" -x
                complete -c ping -s L -d "Suppress loopback of multicast packets"
                complete -c ping -s l -d "Send specified number of packets as fast as possible" -x
                complete -c ping -s M -d "Use ICMP_MASKREQ or ICMP_TSTAMP instead of ICMP_ECHO" -xa "mask time"
                complete -c ping -s m -d "Time To Live for outgoing packets" -x
                complete -c ping -s n -d "Numeric output only"
                complete -c ping -s o -d "Exit successfully after receiving one reply packet"
                complete -c ping -s P -d "Specify IPsec policy for the ping session" -x
                complete -c ping -s p -d "Pattern to fill the sent packets" -x
                complete -c ping -s Q -d "Somewhat quiet output"
                complete -c ping -s q -d "Quiet output"
                complete -c ping -s R -d "Record route"
                complete -c ping -s r -d "Bypass the normal routing tables"
                complete -c ping -s S -d "Source address for outgoing packets" -xa "(__fish_print_addresses)"
                complete -c ping -s s -d "The number of data bytes to be sent" -x
                complete -c ping -s T -d "Time To Live for multicasted packets" -x
                complete -c ping -s t -d "Timeout (seconds) before ping exits" -x
                complete -c ping -s v -d "Verbose output"
                complete -c ping -s W -d "Time (milliseconds) to wait for a reply for each packet sent" -x
                complete -c ping -s z -d "Use the specified type of service" -x

                if string match -q '*Darwin*' $ping_os_version
                    # Apple specific
                    complete -c ping -s b -d "Bind the socket to specified interface for sending" -xa "(__fish_print_interfaces)"
                    complete -c ping -s k -d "Traffic class to use for sending ICMP packets" -xa "BK_SYS BK BE RD OAM AV RV VI VO CTL"
                    complete -c ping -s K -d "Network service type to use for sending ICMP packets" -xa "BK_SYS BK BE RV AV RD OAM VI SIG VO"
                    complete -c ping -l apple-connect -d "Connects the socket to the destination address"
                    complete -c ping -l apple-time -d "Prints the time a packet was received"
                end
        end
end
