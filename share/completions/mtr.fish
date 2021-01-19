complete -c mtr -x -a "(__fish_print_hostnames)"

complete -c mtr -s h -l help -d 'display this help and exit'
complete -c mtr -s v -l version -d 'output version information and exit'
complete -c mtr -s 4 -d 'use IPv4 only'
complete -c mtr -s 6 -d 'use IPv6 only'
complete -c mtr -s F -l filename -d 'read hostname(s) from a file' -rF
complete -c mtr -s r -l report -d 'output using report mode'
complete -c mtr -s w -l report-wide -d 'output wide report'
complete -c mtr -s x -l xml -d 'output xml'
complete -c mtr -s t -l curses -d 'use curses terminal interface'
complete -c mtr -l displaymode -d 'select initial display mode' -x -a '0 1 2'
complete -c mtr -s g -l gtk -d 'use the GTK+ interface (if available)'
complete -c mtr -s l -l raw -d 'output raw format'
complete -c mtr -s C -l csv -d 'output comma separated values'
complete -c mtr -s j -l json -d 'output json'
complete -c mtr -s p -l split -d 'split output'
complete -c mtr -s n -l no-dns -d 'do not resolve host names'
complete -c mtr -s b -l show-ips -d 'show IP numbers and host names'
complete -c mtr -s o -l order -d 'select output fields' -x
complete -c mtr -s y -l ipinfo -d 'select IP information in output' -x -a '0 1 2 3 4'
complete -c mtr -s z -l aslookup -d 'display AS number'
complete -c mtr -s i -l interval -d 'ICMP echo request interval' -x
complete -c mtr -s c -l report-cycles -d 'set the number of pings sent' -x
complete -c mtr -s s -l psize -d 'set the packet size used for probing' -x
complete -c mtr -s B -l bitpattern -d 'set bit pattern to use in payload' -x
complete -c mtr -s G -l gracetime -d 'number of seconds to wait for responses' -x
complete -c mtr -s Q -l tos -d 'type of service field in IP header' -x
complete -c mtr -s e -l mpls -d 'display information from ICMP extensions'
complete -c mtr -s I -l interface -d 'use named network interface' -x
complete -c mtr -s a -l address -d 'bind the outgoing socket to ADDRESS' -x
complete -c mtr -s f -l first-ttl -d 'set what TTL to start' -x
complete -c mtr -s m -l max-ttl -d 'maximum number of hops' -x
complete -c mtr -s U -l max-unknown -d 'maximum unknown host' -x
complete -c mtr -s u -l udp -d 'use UDP instead of ICMP echo'
complete -c mtr -s T -l tcp -d 'use TCP instead of ICMP echo'
complete -c mtr -s S -l sctp -d 'Use Stream Control Transmission instead of ICMP echo'
complete -c mtr -s P -l port -d 'target port number for TCP SCTP or UDP' -x
complete -c mtr -s L -l localport -d 'source port number for UDP' -x
complete -c mtr -s Z -l timeout -d 'seconds to keep probe sockets open' -x
complete -c mtr -s M -l mark -d 'mark each sent packet' -x
