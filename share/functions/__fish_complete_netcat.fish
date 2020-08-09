function __fish_complete_netcat
    set -l nc $argv[1]
    set -l flavor $argv[-1]

    switch $flavor
        case busybox
            complete -c $nc -s l -d "Listen mode, for inbound connects"
            complete -c $nc -s p -x -d "Local port"
            complete -c $nc -s w -x -d "Connect timeout"
            complete -c $nc -s i -x -d "Delay interval for lines sent"
            complete -c $nc -s f -r -d "Use file (ala /dev/ttyS0) instead of network"
            complete -c $nc -s e -r -d "Run PROG after connect"

        case ncat
            complete -c $nc -w ncat

        case nc.openbsd
            complete -c $nc -s 4 -d "Forces nc to use IPv4 addresses only"
            complete -c $nc -s 6 -d "Forces nc to use IPv6 addresses only"
            complete -c $nc -s b -d "Allow broadcast"
            complete -c $nc -s C -d "Send CRLF as line-ending"
            complete -c $nc -s D -d "Enable debugging on the socket"
            complete -c $nc -s d -d "Do not attempt to read from stdin"
            complete -c $nc -s F -d "Pass the first connected socket using sendmsg(2) to stdout and exit"
            complete -c $nc -s h -d "Prints out nc help"
            complete -c $nc -s I -x -d "Specifies the size of the TCP receive buffer"
            complete -c $nc -s i -x -d "Specifies a delay time interval between lines of text sent and received"
            complete -c $nc -s k -d "Forces nc to stay listening for another connection after its current connection is completed"
            complete -c $nc -s l -d "Used to specify that nc should listen for an incoming connection rather than initiate a connection to a remote host"
            complete -c $nc -s M -x -d "Set the TTL / hop limit of outgoing packets"
            complete -c $nc -s m -x -d "Ask the kernel to drop incoming packets whose TTL / hop limit is under minttl"
            complete -c $nc -s N -d "shutdown(2) the network socket after EOF on the input"
            complete -c $nc -s n -d "Do not do any DNS or service lookups on any specified addresses, hostnames or ports"
            complete -c $nc -s O -x -d "Specifies the size of the TCP send buffer"
            complete -c $nc -s P -x -d "Specifies a username to present to a proxy server that requires authentication"
            complete -c $nc -s p -x -d "Specifies the source port nc should use, subject to privilege restrictions and availability"
            complete -c $nc -s q -x -d "after EOF on stdin, wait the specified number of seconds and then quit"
            complete -c $nc -s r -d "Specifies that source and/or destination ports should be chosen randomly instead of sequentially within a range or in the order that the system assigns them"
            complete -c $nc -s S -d "Enables the RFC 2385 TCP MD5 signature option"
            complete -c $nc -s s -x -d "Specifies the IP of the interface which is used to send the packets"
            complete -c $nc -s T -x -a "critical inetcontrol lowcost lowdelay netcontrol throughput reliability ef af cs0 cs1 cs2 cs3 cs4 cs5 cs6 cs7" -d "Change IPv4 TOS value"
            complete -c $nc -s t -d "Causes nc to send RFC 854 DON'T and WON'T responses to RFC 854 DO and WILL requests"
            complete -c $nc -s U -d "Specifies to use UNIX-domain sockets"
            complete -c $nc -s u -d "Use UDP instead of the default option of TCP"
            complete -c $nc -s V -x -d "Set the routing table to be used"
            complete -c $nc -s v -d "Have nc give more verbose output"
            complete -c $nc -s W -x -d "Terminate after receiving recvlimit packets from the network"
            complete -c $nc -s w -x -d "Connections which cannot be established or are idle timeout after timeout seconds"
            function __fish_complete_nc-connect-openbsd
                printf "connect\tHTTPS proxy\n"
                printf "4\tSOCKS v.4\n"
                printf "5\tSOCKS v.5\n"
            end
            complete -c $nc -s X -x -a "(__fish_complete_nc-connect-openbsd)" -d "Requests that nc should use the specified protocol when talking to the proxy server"
            complete -c $nc -s x -x -a "(__fish_print_hostnames)" -d "Requests that nc should connect to destination using a proxy at proxy_address and port"
            complete -c $nc -s Z -d "DCCP mode"
            complete -c $nc -s z -d "Specifies that nc should just scan for listening daemons, without sending any data to them"

        case nc.traditional '*' # fallback to the most restricted one
            complete -c $nc -s c -r -d "specify shell commands to exec after connect"
            complete -c $nc -s e -r -d "specify filename to exec after connect"
            complete -c $nc -s g -x -d "source-routing hop point[s], up to 8"
            complete -c $nc -s G -x -d "source-routing pointer: 4, 8, 12, ..."
            complete -c $nc -s h -d "display help"
            complete -c $nc -s i -x -d "delay interval for lines sent, ports scanned"
            complete -c $nc -s l -d "listen mode, for inbound connects"
            complete -c $nc -s n -d "numeric-only IP addresses, no DNS"
            complete -c $nc -s o -r -d "hex dump of traffic"
            complete -c $nc -s p -x -d "local port number (port numbers can be individual or ranges: lo-hi [inclusive])"
            complete -c $nc -s q -x -d "after EOF on stdin, wait the specified number of seconds and then quit"
            complete -c $nc -s b -d "allow UDP broadcasts"
            complete -c $nc -s r -d "randomize local and remote ports"
            complete -c $nc -s s -x -d "local source address"
            complete -c $nc -s t -d "enable telnet negotiation"
            complete -c $nc -s u -d "UDP mode"
            complete -c $nc -s v -d "verbose [use twice to be more verbose]"
            complete -c $nc -s w -x -d "timeout for connects and final net reads"
            complete -c $nc -s C -d "Send CRLF as line-ending"
            complete -c $nc -s z -d "zero-I/O mode [used for scanning]"
            complete -c $nc -s T -x -a "Minimize-Delay Maximize-Throughput Maximize-Reliability Minimize-Cost" -x -d "set TOS flag"
    end
end
