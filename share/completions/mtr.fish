complete -c mtr -x -a '(__fish_print_hostnames)'

complete -c mtr -s h -l help -d 'Display help and exit'
complete -c mtr -s v -l version -d 'Output version information and exit'
complete -c mtr -s 4 -d 'Use IPv4 only'
complete -c mtr -s 6 -d 'Use IPv6 only'
complete -c mtr -s F -l filename -d 'Read hostname(s) from a file' -rF
complete -c mtr -s r -l report -d 'Output a statistic report'
complete -c mtr -s w -l report-wide -d 'Output report with full hostnames'
complete -c mtr -s x -l xml -d 'Output xml format'
complete -c mtr -s t -l curses -d 'Use curses terminal interface'
complete -c mtr -l displaymode -d 'Select initial display mode' -x -a \
    '0\t"Statistics" 1\t"Stripchart without latency" 2\t"Stripchart with latency"'
complete -c mtr -s g -l gtk -d 'Use the GTK+ interface (if available)'
complete -c mtr -s l -l raw -d 'Output raw format'
complete -c mtr -s C -l csv -d 'Output csv format'
complete -c mtr -s j -l json -d 'Output json format'
complete -c mtr -s p -l split -d 'Split output'
complete -c mtr -s n -l no-dns -d 'Do not resolve host names'
complete -c mtr -s b -l show-ips -d 'Show IP numbers and host names'
complete -c mtr -s o -l order -d 'Select output fields' -x
complete -c mtr -s y -l ipinfo -d 'Select IP information in output' -x -a \
    '0\t"AS number" 1\t"IP prefix" 2\t"Country code" 3\t"RIR organization"\
     4\t"Allocation date of the IP prefix"'
complete -c mtr -s z -l aslookup -d 'Display AS number'
complete -c mtr -s i -l interval -d 'ICMP echo request interval (sec)' -x
complete -c mtr -s c -l report-cycles -d 'Set the number of pings sent' -x
complete -c mtr -s s -l psize -d 'Set the packet size used for probing (bytes)' -x
complete -c mtr -s B -l bitpattern -d 'Set bit pattern to use in payload' -x -a '(seq 0 256)'
complete -c mtr -s G -l gracetime -d 'Number of seconds to wait for responses' -x
complete -c mtr -s Q -l tos -d 'Type of service field in IP header' -x -a '(seq 0 255)'
complete -c mtr -s e -l mpls -d 'Display information from ICMP extensions'
complete -c mtr -s I -l interface -d 'Use named network interface' -x
complete -c mtr -s a -l address -d 'Bind the outgoing socket to ADDRESS' -x
complete -c mtr -s f -l first-ttl -d 'Set the TTL start number' -x
complete -c mtr -s m -l max-ttl -d 'Maximum number of hops' -x
complete -c mtr -s U -l max-unknown -d 'Maximum number of unknown host' -x
complete -c mtr -s u -l udp -d 'use UDP instead of ICMP echo'
complete -c mtr -s T -l tcp -d 'use TCP instead of ICMP echo'
complete -c mtr -s S -l sctp -d 'Use Stream Control Transmission instead of ICMP echo'
complete -c mtr -s P -l port -d 'Target port number for TCP SCTP or UDP' -x
complete -c mtr -s L -l localport -d 'Source port number for UDP' -x
complete -c mtr -s Z -l timeout -d 'Seconds to keep probe sockets open' -x
complete -c mtr -s M -l mark -d 'Mark each sent packet' -x
