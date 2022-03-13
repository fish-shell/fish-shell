# Completions for nmap (https://www.nmap.org)

complete -c nmap -f -a "(__fish_print_hostnames)"

# TARGET SPECIFICATION
complete -c nmap -o iL -F -d 'Input target from file'
complete -c nmap -o iR -x -d 'Choose random targets'
complete -c nmap -l exclude -x -a "(__fish_print_hostnames)" -d 'Exclude hosts/networks'
complete -c nmap -l excludefile -r -d 'Exclude list from file'

# HOST DISCOVERY
complete -c nmap -o sL -d 'Scan: List Scan'
complete -c nmap -o sn -d 'Scan: No port scan'
complete -c nmap -o Pn -d 'Probe: No Ping'
complete -c nmap -o PS -x -d 'Probe: TCP Syn Ping'
complete -c nmap -o PA -x -d 'Probe: TCP ACK Ping'
complete -c nmap -o PU -x -d 'Probe: UDP Ping'
complete -c nmap -o PY -x -d 'Probe: SCTP INIT Ping'
complete -c nmap -o PE -d 'Probe: ICMP Echo Ping'
complete -c nmap -o PP -d 'Probe: ICMP timestamp request'
complete -c nmap -o PM -d 'Probe: ICMP netmask Ping'
complete -c nmap -o PO -x -d 'Probe: IP Protocol Ping'
complete -c nmap -o PR -d 'Probe: ARP Ping'
complete -c nmap -l disable-arp-ping -d 'No ARP or ND Ping'
complete -c nmap -l traceroute -d 'Trace path to host'
complete -c nmap -s n -d 'No DNS resolution'
complete -c nmap -s R -d 'DNS resolution for all targets'
complete -c nmap -l system-dns -d 'Use system DNS resolver'
complete -c nmap -l dns-servers -x -a "(__fish_print_hostnames)" -d 'Servers to use for reverse DNS queries'

# PORT SCANNING TECHNIQUES
complete -c nmap -o sS -d 'Scan: TCP SYN'
complete -c nmap -o sT -d 'Scan: TCP connect'
complete -c nmap -o sU -d 'Scan: UDP'
complete -c nmap -o sY -d 'Scan: SCTP INIT'
complete -c nmap -o sN -d 'Scan: TCP NULL'
complete -c nmap -o sF -d 'Scan: FIN'
complete -c nmap -o sX -d 'Scan: Xmas'
complete -c nmap -o sA -d 'Scan: ACK'
complete -c nmap -o sW -d 'Scan: Window'
complete -c nmap -o sM -d 'Scan: Mainmon'
complete -c nmap -l scanflags -d 'Custom TCP scan flags'
complete -c nmap -o sZ -d 'Scan: SCTP COOKIE ECHO'
complete -c nmap -o sI -x -a"(__fish_print_hostnames)" -d 'Scan: Idle Scan'
complete -c nmap -o sO -d 'Scan: IP protocol'
complete -c nmap -s b -x -a"(__fish_print_hostnames)" -d 'FTP bounce scan'

# PORT SPECIFICATION AND SCAN ORDER
complete -c nmap -s p -d 'Only scan specified ports'
complete -c nmap -l exclude-ports -d 'Exclude the specified ports from scanning'
complete -c nmap -s F -d 'Fast (limited port) scan'
complete -c nmap -s r -d "Don't randomize ports"
complete -c nmap -l port-ratio -x -d 'Scan ports with ratio greater then'
complete -c nmap -l top-ports -x -d 'Scan the n highest-ratio ports'

# SERVICE AND VERSION DETECTION
complete -c nmap -o sV -d 'Scan: Version'
complete -c nmap -l allports -d "Don't exclude any ports from version detection"
complete -c nmap -l version-intensity -x -d 'Set version scan intensity'
complete -c nmap -l version-light -d 'Enable light mode'
complete -c nmap -l version-all -d 'Try every single probe'
complete -c nmap -l version-trace -d 'Trace version scan activity'

# OS DETECTION
complete -c nmap -s O -d 'Enable OS detection'
complete -c nmap -l osscan-limit -d 'Limit OS detection to promising targets'
complete -c nmap -l osscan-guess -d 'Guess OS detection results'
complete -c nmap -l fuzzy -d 'Guess OS detection results'
complete -c nmap -l max-os-tries -d 'Set the maximum number of OS detection tries against a target'

# NMAP SCRIPTING ENGINE (NSE)
complete -c nmap -o sC -d 'Scan: Scripts (default)'
function __fish_complete_nmap_script
    set -l now (date "+%s")

    # cache completion for 5 minutes (`nmap --script-help all` is slow)
    # must use `math` because of differences between BSD and GNU `date`
    if test -z "$__fish_nmap_script_completion_cache" -o (math $now - 5 "*" 60) -gt "$__fish_nmap_script_completion_cache_time"
        set -g __fish_nmap_script_completion_cache_time $now
        set -g __fish_nmap_script_completion_cache ""
        set -l cmd
        for l in (nmap --script-help all 2> /dev/null | grep -A2 -B1 Categories: | grep -v '^\\(--\\|Categories:\\|https:\\)')
            if string match -q -v --regex "^ " $l
                set cmd $l
            else
                set __fish_nmap_script_completion_cache $__fish_nmap_script_completion_cache\n$cmd\t(string trim -l $l)
            end
        end
        for cat in all auth broadcast brute default discovery dos exploit external fuzzer intrusive malware safe version vuln
            set __fish_nmap_script_completion_cache $__fish_nmap_script_completion_cache\n$cat\tCategory\n
        end
    end
    echo -e $__fish_nmap_script_completion_cache
end
complete -c nmap -l script -r -a "(__fish_complete_list , __fish_complete_nmap_script)"
complete -c nmap -l script -r -d 'Runs a script scan'
complete -c nmap -l script-args -d 'provide arguments to NSE scripts'
complete -c nmap -l script-args-file -r -d 'load arguments to NSE scripts from a file'
complete -c nmap -l script-help -r -a "(__fish_complete_list , __fish_complete_nmap_script)"
complete -c nmap -l script-help -r -d "Shows help about scripts"
complete -c nmap -l script-trace
complete -c nmap -l script-updatedb

# TIMING AND PERFORMANCE
complete -c nmap -l min-hostgroup -l max-hostgroup -x -d 'Adjust paralel scan group size'
complete -c nmap -l min-parallelism -l max-parallelism -x -d 'Adjust probe parallelization'
complete -c nmap -l min-rtt-timeout -l max-rtt-timeout -l initial-rtt-timeout -x -d 'Adjust probe timeouts'
complete -c nmap -l max-retries -x -d 'Specify the maximum number of port scan probe retransmissions'
complete -c nmap -l host-timeout -d 'to skip slow hosts'
complete -c nmap -l script-timeout -x
complete -c nmap -l scan-delay -l max-scan-delay -x -d 'Adjust delay between probes'
complete -c nmap -l min-rate -l max-rate -x -d 'Directly control the scanning rate'
complete -c nmap -l defeat-rst-ratelimit -d 'ignore ICMP-RST rate limits'
complete -c nmap -l defeat-icmp-ratelimit -d 'ignore ICMP unreachable in UDP'
complete -c nmap -l nsock-engine -x -d 'Enforce use of a given nsock IO multiplexing engine' -a "epoll kqueue poll select"
function __fish_complete_nmap_timing-template
    set -l i 0
    for t in paranoid sneaky polite normal aggressive insane
        printf "%i\t%s timing\n" $i $t
        printf "%s\tTemplate %i\n" $t $i
        set i (math $i + 1)
    end
end
complete -c nmap -s T -x -a "(__fish_complete_nmap_timing-template)" -d 'Set a timing template'

# FIREWALL/IDS EVASION AND SPOOFING
complete -c nmap -s f -d 'fragment packets' -n "not __fish_contains_opt -s f"
complete -c nmap -s f -d 'use 16 bytes per fragment' -n "__fish_contains_opt -s f"
complete -c nmap -l mtu -d 'use specified mtu' -n "__fish_contains_opt -s f"
complete -c nmap -s D -x -d 'Cloak a scan with decoys'
complete -c nmap -s S -x -d 'Spoof source address'
complete -c nmap -s e -x -d 'Use specified interface' -a "(__fish_print_interfaces)"
complete -c nmap -l source-port -s g -x -d 'Spoof source port number'
complete -c nmap -l data -x -d 'Append custom binary data to sent packets'
complete -c nmap -l data-string -x -d 'Append custom string to sent packets'
complete -c nmap -l data-length -x -d 'Append random data to sent packets'
function __fish_complete_nmap_ip-options
    printf "S\tstrict source routing\n" # may be followed by ip addresses
    printf "R\trecord route\n" # may be followed by ip addresses
    printf "L\tloose source routing\n" # may be followed by ip addresses
    printf "T\trecord internet timestamps\n"
    printf "U\trecord timestamps and ip addresses\n"
end
complete -c nmap -l ip-options -x -a "(__fish_complete_nmap_ip-options)" -d 'Send packets with specified ip options'
complete -c nmap -l ttl -x -d 'Set IP time-to-live field'
complete -c nmap -l randomize-hosts -d 'Randomize target host order'
complete -c nmap -l spoof-mac -x -d 'Spoof MAC address'
complete -c nmap -l proxies -x -d 'Relay TCP connections through a chain of proxies'
complete -c nmap -l badsum -d 'Send packets with bogus TCP/UDP checksums'
complete -c nmap -l adler32 -d 'Use deprecated Adler32 instead of CRC32C for SCTP checksums'

# OUTPUT
complete -c nmap -o oN -r -d 'normal output'
complete -c nmap -o oX -r -d 'XML output'
complete -c nmap -o oS -r -d 'ScRipT KIdd|3 oUTpuT'
complete -c nmap -o oG -r -d 'grepable output'
complete -c nmap -o oA -r -d 'Output in the three major formats'
complete -c nmap -s v -d 'Increase/Set verbosity level'
complete -c nmap -s d -d 'Increase/Set debugging level' -a '0 1 2 3 4 5 6 7 8 9'
complete -c nmap -l reason -d 'Host and port state reasons'
complete -c nmap -l stats-every -x -d 'Print periodic timing stats'
complete -c nmap -l packet-trace -d 'Trace packets and data sent and received'
complete -c nmap -l open -d 'Show only open (or possibly open) ports'
complete -c nmap -l iflist -d 'List interfaces and routes'
complete -c nmap -l append-output -d 'Append to rather than clobber output files'
complete -c nmap -l resume -r -d 'Resume aborted scan'
complete -c nmap -l stylesheet -r -d 'Set XSL stylesheet to transform XML output'
complete -c nmap -l webxml -d 'Load stylesheet from Nmap.Org'
complete -c nmap -l no-stylesheet -d 'Omit XSL stylesheet declaration from XML'

# MISCELLANEOUS OPTIONS
complete -c nmap -s 6 -d 'Enable IPv6 scanning'
complete -c nmap -s A -d 'Aggressive scan options'
complete -c nmap -l datadir -x -a "(__fish_complete_directories)" -d 'Specify custom Nmap data file location'
complete -c nmap -l servicedb -r -d 'Specify custom services file'
complete -c nmap -l versiondb -r -d 'Specify custom service probes file'
complete -c nmap -l send-eth -d 'Use raw ethernet sending'
complete -c nmap -l send-ip -d 'Send at raw IP level'
complete -c nmap -l privileged -d 'Assume that the user is fully privileged'
complete -c nmap -l unprivileged -d 'Assume that the user lacks raw socket privileges'
complete -c nmap -l release-memory -d 'Release memory before quitting'
complete -c nmap -s V -l version -d 'Print version number'
complete -c nmap -s h -l help -d 'Print help summary page'
