# ip(8) completion for fish

# The difficulty here is that ip allows abbreviating options, so we need to complete "ip a" like "ip address", but not "ip m" like "ip mroute"
# Also the manpage and even the grammar it accepts is utter shite (options can only be before commands, some things are only in the BNF, others only in the text)
# It also quite likes the word "dev", even though it needs it less than the BNF specifies

set -l ip_commands link address addrlabel route rule neighbour ntable tunnel tuntap maddr mroute mrule monitor xfrm netns l2tp tcp_metrics
set -l ip_addr a ad add addr addre addres address
set -l ip_link l li lin link
set -l ip_neigh n ne nei neig neigh neighb neighbo neighbor neighbour
set -l ip_route r ro rou rout route
set -l ip_rule ru rul rule
set -l ip_all_commands $ip_commands $ip_addr $ip_link $ip_neigh $ip_route $ip_rule

function __fish_ip_commandwords
    set -l skip 0
    set -l cmd (commandline -xpc | string escape)
    # Remove the first word because it's "ip" or an alias for it
    set -e cmd[1]
    set -l have_command 0
    for word in $cmd
        switch $word
            # Normalize the commands to their full form - `ip a` is `ip address`
            # This can't be just an unambiguity check because there's also `ip addrlabel`
            case a ad add addr addre addres address
                # "addr add" is a thing, so we can only echo "address" if it's actually the command
                if test $have_command = 0
                    set have_command 1
                    echo address
                else
                    echo $word
                end
            case l li lin link
                if test $have_command = 0
                    set have_command 1
                    echo link
                else
                    echo $word
                end
            case "addrl*"
                if test $have_command = 0
                    set have_command 1
                    echo addrlabel
                else
                    echo $word
                end
            case r ro rou rout route
                if test $have_command = 0
                    set have_command 1
                    echo route
                else
                    echo $word
                end
            case "ru*"
                if test $have_command = 0
                    set have_command 1
                    echo rule
                else
                    echo $word
                end
            case n ne nei neig neigh neighb neighbo neighbor neighbour
                if test $have_command = 0
                    set have_command 1
                    echo neighbour
                else
                    echo $word
                end
            case nt
                if test $have_command = 0
                    set have_command 1
                    echo ntable
                else
                    echo $word
                end
            case t tu tun tunn tunne tunnel
                if test $have_command = 0
                    set have_command 1
                    echo tunnel
                else
                    echo $word
                end
            case "tunt*"
                if test $have_command = 0
                    set have_command 1
                    echo tuntap
                else
                    echo $word
                end
            case m ma mad madd maddr maddre maddres maddress
                if test $have_command = 0
                    set have_command 1
                    echo maddress
                else
                    echo $word
                end
            case mr "mro*"
                if test $have_command = 0
                    set have_command 1
                    echo mroute
                else
                    echo $word
                end
            case "mru*"
                if test $have_command = 0
                    set have_command 1
                    echo mrule
                else
                    echo $word
                end
            case "mo*"
                if test $have_command = 0
                    set have_command 1
                    echo monitor
                else
                    echo $word
                end
            case "x*"
                if test $have_command = 0
                    set have_command 1
                    echo xfrm
                else
                    echo $word
                end
            case "net*"
                if test $have_command = 0
                    set have_command 1
                    echo netns
                else
                    echo $word
                end
            case "l*"
                if test $have_command = 0
                    set have_command 1
                    echo l2tp
                else
                    echo $word
                end
            case "tc*"
                if test $have_command = 0
                    set have_command 1
                    echo tcp_metrics
                else
                    echo $word
                end
            case "to*"
                if test $have_command = 0
                    set have_command 1
                    echo token
                else
                    echo $word
                end
            case -n -netns --netns
                if test $have_command = 0
                    set skip 1
                else
                    echo $word
                end
            case '-*'
                test $have_command = 0
                and continue
                echo $word
            case '*'
                if test $skip = 1
                    set skip 0
                    continue
                end
                echo $word
        end
    end
    # Print an empty line if the current token is empty, so we know that the one before it is finished
    # TODO: For some reason it is necessary to always print the current token - why doesn't the above loop catch it?
    set -l token (commandline -ct)
    echo $token
end

function __fish_ip_device
    command ip -o link show | while read -l a b c
        printf '%s\t%s\n' (string replace -r '(@.*)?:' '' -- $b) Device
    end
end

function __fish_ip_scope
    if test -r /etc/iproute2/rt_scopes
        string replace -r '#.*' '' </etc/iproute2/rt_scopes \
            | string match -v '^\s*$' \
            | string replace -r '(\S+)\s*(\S+)' '$1\t$2\n$2\t$1' \
            | string match -rv '^(global|link|host).*' # Ignore scopes with better descriptions
    end
    # Predefined scopes
    printf '%s\t%s\n' global "Address is globally valid" \
        link "Address is link-local, only valid on this device" \
        host "Address is only valid on this host"
end

function __fish_ip_netns_list
    command ip netns list | while read -l a b c
        echo -- $a
    end
end

function __fish_ip_routing_tables
    command ip -d route show table all 2>/dev/null | string replace -fr '.*\stable\s(\S+).*' '$1'
end

function __fish_ip_types
    printf '%s\t%s\n' \
        bridge "Ethernet Bridge device" \
        bond "Bonding device" \
        dummy "Dummy network interface" \
        hsr "High-availability Seamless Redundancy device" \
        ifb "Intermediate Functional Block device" \
        ipoib "IP over Infiniband device" \
        macvlan "Virtual interface base on link layer address (MAC)" \
        macvtap "Virtual interface based on link layer address (MAC) and TAP." \
        vcan "Virtual Controller Area Network interface" \
        vxcan "Virtual Controller Area Network tunnel interface" \
        veth "Virtual ethernet interface" \
        vlan "802.1q tagged virtual LAN interface" \
        vxlan "Virtual eXtended LAN" \
        ip6tnl "Virtual tunnel interface IPv4|IPv6 over IPv6" \
        ipip "Virtual tunnel interface IPv4 over IPv4" \
        sit "Virtual tunnel interface IPv6 over IPv4" \
        gre "Virtual tunnel interface GRE over IPv4" \
        gretap "Virtual L2 tunnel interface GRE over IPv4" \
        erspan "Encapsulated Remote SPAN over GRE and IPv4" \
        ip6gre "Virtual tunnel interface GRE over IPv6" \
        ip6gretap "Virtual L2 tunnel interface GRE over IPv6" \
        ip6erspan "Encapsulated Remote SPAN over GRE and IPv6" \
        vti "Virtual tunnel interface" \
        nlmon "Netlink monitoring device" \
        ipvlan "Interface for L3 (IPv6/IPv4) based VLANs" \
        ipvtap "Interface for L3 (IPv6/IPv4) based VLANs and TAP" \
        lowpan "Interface for 6LoWPAN (IPv6) over IEEE 802.15.4 / Bluetooth" \
        geneve "GEneric NEtwork Virtualization Encapsulation" \
        bareudp "Bare UDP L3 encapsulation support" \
        macsec "Interface for IEEE 802.1AE MAC Security (MACsec)" \
        vrf "Interface for L3 VRF domains" \
        netdevsim "Interface for netdev API tests" \
        rmnet "Qualcomm rmnet device" \
        xfrm "Virtual xfrm interface"
end

function __fish_ip_neigh_states
    printf '%s\t%s\n' \
        permanent "entry is valid forever" \
        noarp "entry is valid without validation" \
        reachable "entry is valid until timeout" \
        stale "entry is valid but suspicious" \
        none "pseudo state" \
        incomplete "entry has not yet been validated" \
        delay "entry validation is currently delayed" \
        probe "neighbor is being probed" \
        failed "neighbor validation has ultimately failed"
end

function __fish_complete_ip
    set -l cmd (__fish_ip_commandwords)
    set -l count (count $cmd)
    switch "$cmd[1]"
        case address
            # We're still _on_ the second word, which is the subcommand
            if not set -q cmd[3]
                printf '%s\t%s\n' add "Add new protocol address" \
                    delete "Delete protocol address" \
                    show "Look at protocol addresses" \
                    flush "Flush protocol addresses"
            else
                switch $cmd[2]
                    # Change and replace are undocumented (apart from mentions in the BNF)
                    case add change replace
                        switch $count
                            case 3
                                # __fish_ip_complete_ip
                            case '*'
                                switch $cmd[-2]
                                    case dev
                                        __fish_ip_device
                                    case scope
                                        __fish_ip_scope
                                        # TODO: Figure out how to complete these
                                    case label
                                        # Prefix
                                    case local peer broadcast
                                        # Address
                                    case valid_lft preferred_lft
                                        # Lifetime
                                    case '*'
                                        printf '%s\t%s\n' forever "Keep address valid forever" \
                                            home "(Ipv6 only) Designate address as home address" \
                                            nodad "(Ipv6 only) Don't perform duplicate address detection" \
                                            dev "Add address to specified device" \
                                            scope "Set scope of address" \
                                            label "Tag address with label"
                                end
                        end
                    case delete
                        switch $count
                            case 3
                                command ip -o addr show | while read -l a b c d e
                                    echo $d
                                end
                            case 4
                                # A dev argument is mandatory, but contrary to the BNF, other things (like "scope") are also valid here
                                # And yes, unlike e.g. show, this _needs_ the "dev" before the device
                                # Otherwise it barfs and says "??? prefix is expected"
                                # Anyway, try to steer the user towards supplying a device
                                echo dev
                            case 5
                                switch $cmd[-2]
                                    case dev
                                        command ip -o addr show | string match "*$cmd[3]*" | while read -l a b c
                                            echo $b
                                        end
                                        # TODO: Moar
                                end
                        end
                    case show save flush # These take the same args
                        switch $cmd[-2]
                            case dev
                                __fish_ip_device
                            case scope
                                __fish_ip_scope
                            case to
                                # Prefix
                            case label
                                # Label-pattern
                            case '*'
                                printf '%s\t%s\n' up "Only active devices" \
                                    dev "Limit to a certain device" \
                                    scope "Limit scope" \
                                    to "Limit prefix" \
                                    label "Limit by label" \
                                    dynamic "(Ipv6 only) Limit to dynamic addresses" \
                                    permanent "(Ipv6 only) Limit to permanent addresses"
                                __fish_ip_device
                                # TODO: Moar
                        end
                end
            end
        case link
            if not set -q cmd[3]
                printf '%s\t%s\n' add "Add virtual link" \
                    delete "Delete virtual link" \
                    set "Change device attributes" \
                    show "Display device attributes" \
                    help "Display help"
            else
                # TODO: Add moar
                switch $cmd[2]
                    case add
                        switch $cmd[-2]
                            case link
                                __fish_ip_device
                            case name # freeform, uncompleteable.
                            case type
                                __fish_ip_types
                            case txqueuelen
                            case address
                            case broadcast
                            case mtu index numtxqueues numrxqueues gso_max_size gso_max_segs
                                # These all take some kind of number?
                                seq 0 50
                            case add '*'
                                # We assume if we haven't checked it above, we're back to `ip link add`, with any optionals completed.
                                # So we now suggest optionals.
                                printf '%s\t%s\n' \
                                    link "" \
                                    type "" \
                                    txqueuelen PACKETS \
                                    address LLADDR \
                                    broadcast LLADDR \
                                    mtu MTU \
                                    index IDX \
                                    numtxqueues QUEUE_COUNT \
                                    numrxqueues QUEUE_COUNT \
                                    gso_max_size BYTES \
                                    gso_max_segs
                        end
                    case delete
                        switch $cmd[-2]
                            case delete
                                __fish_ip_device
                                echo dev
                                echo group
                            case dev
                                __fish_ip_device
                            case group
                            case type
                                __fish_ip_types
                            case '*'
                                if not set -q cmd[6]
                                    echo type
                                end
                        end
                    case set
                        switch $cmd[-2]
                            case type
                                __fish_ip_types
                                echo bridge_slave
                                echo bond_slave
                            case arp dynamic {all,}multicast pro{misc,todown} trailers spoofchk query_rss trust
                                echo on
                                echo off
                            case name alias
                            case address broadcast
                            case mtu txqueuelen
                            case netns
                            case link-netnsid
                            case vf mac
                            case {{max,min}_tx_,}rate
                            case node_guid port_guid
                            case state
                                echo auto
                                echo enable
                                echo disable
                            case master dev # Yes, the word "dev" is allowed here!
                                __fish_ip_device
                            case nomaster
                            case vrf
                            case xdp'*'
                            case object section pinned
                            case addrgenmode
                            case macaddr
                            case group
                            case set '*'
                                __fish_ip_device
                                echo group
                                echo up
                                echo down
                        end
                    case show
                    case help
                end
            end
        case neighbour
            if not set -q cmd[3]
                printf '%s\t%s\n' help "Show help" \
                    add "Add new neighbour entry" \
                    delete "Delete neighbour entry" \
                    change "Change neighbour entry" \
                    replace "Add or change neighbour entry" \
                    show "List neighbour entries" \
                    flush "Flush neighbour entries" \
                    get "Lookup neighbour entry"
            else
                switch $cmd[2]
                    case add del delete change replace
                        switch $cmd[-2]
                            case lladdr
                            case nud
                                __fish_ip_neigh_states
                            case proxy
                            case dev
                                __fish_ip_device
                            case '*'
                                echo lladdr
                                echo nud
                                echo proxy
                                echo dev
                                echo router
                                echo use
                                echo managed
                                echo extern_learn
                        end
                    case show flush
                        switch $cmd[-2]
                            case to
                            case dev
                                __fish_ip_device
                            case vrf
                            case nud
                                __fish_ip_neigh_states
                                echo all
                            case '*'
                                echo to
                                echo dev
                                echo vrf
                                echo nomaster
                                echo proxy
                                echo unused
                                echo nud
                        end
                    case get
                        switch $cmd[-2]
                            case to
                            case dev
                                __fish_ip_device
                            case '*'
                                echo proxy
                                echo to
                                echo dev
                        end
                end
            end
        case route
            if not set -q cmd[3]
                printf '%s\t%s\n' \
                    add "Add new route" \
                    change "Change route" \
                    append "Append route" \
                    replace "Change or add new route" \
                    delete "Delete route" \
                    show "List routes" \
                    flush "Flush routing tables" \
                    get "Get a single route" \
                    save "Save routing table to stdout" \
                    showdump "Show saved routing table from stdin" \
                    restore "Restore routing table from stdin"
            else
                # TODO: switch on $cmd[2] and complete subcommand specific arguments
                # for now just complete most useful arguments for the last token
                switch $cmd[-2]
                    case dev iif oif
                        __fish_ip_device
                    case table
                        __fish_ip_routing_tables
                end
            end
        case rule
            if not set -q cmd[3]
                printf '%s\t%s\n' \
                    add "Add new rule" \
                    delete "Delete rule" \
                    flush "Flush rules" \
                    show "List rules" \
                    save "Save rules to stdout" \
                    restore "Restore rules from stdin"
            else
                # TODO: switch on $cmd[2] and complete subcommand specific arguments
                # for now just complete most useful arguments for the last token
                switch $cmd[-2]
                    case iif oif
                        __fish_ip_device
                    case table
                        __fish_ip_routing_tables
                end
            end
        case netns
            if not set -q cmd[3]
                printf '%s\t%s\n' add "Add network namespace" \
                    attach "Attach process to network namespace" \
                    delete "Delete network namespace" \
                    set "Change network namespace attributes" \
                    identify "Display network namespace for a process id" \
                    pids "Display process ids of processes running in network namespace" \
                    monitor "Report as network namespace names are added and deleted" \
                    exec "Execute command in network namespace" \
                    help "Display help" \
                    list "List network namespaces"
            else
                switch $cmd[2]
                    case delete
                        if not set -q cmd[4]
                            __fish_ip_netns_list
                        end
                    case exec
                        if not set -q cmd[4]
                            __fish_ip_netns_list
                        else
                            __fish_complete_subcommand --commandline $cmd[4..-1]
                        end
                    case pids
                        if not set -q cmd[4]
                            __fish_ip_netns_list
                        end
                    case set
                        if not set -q cmd[4]
                            __fish_ip_netns_list
                        end
                    case attach
                        if not set -q cmd[4]
                            __fish_ip_netns_list
                        else
                            __fish_complete_pids
                        end
                    case identify
                        if not set -q cmd[4]
                            __fish_complete_pids
                        end
                end
            end
    end
end

complete -f -c ip
complete -f -c ip -a '(__fish_complete_ip)'
complete -f -c ip -n "not __fish_seen_subcommand_from $ip_all_commands" -a "$ip_commands"
# Yes, ip only takes options before "objects"
complete -c ip -s h -l human -d "Output statistics with human readable values" -n "not __fish_seen_subcommand_from $ip_commands"
complete -c ip -s b -l batch -d "Read commands from file or stdin" -n "not __fish_seen_subcommand_from $ip_commands"
complete -c ip -l force -d "Don't terminate on errors in batch mode" -n "not __fish_seen_subcommand_from $ip_commands"
complete -c ip -s V -l Version -d "Print the version" -n "not __fish_seen_subcommand_from $ip_commands"
complete -c ip -f -s s -l stats -d "Output more information" -n "not __fish_seen_subcommand_from $ip_commands"
complete -c ip -f -s d -l details -d "Output more detailed information" -n "not __fish_seen_subcommand_from $ip_commands"
complete -c ip -f -s l -l loops -d "Specify maximum number of loops for 'ip addr flush'" -n "not __fish_seen_subcommand_from $ip_commands"
complete -c ip -f -s f -l family -d "The protocol family to use" -a "inet inet6 bridge ipx dnet link any" -n "not __fish_seen_subcommand_from $ip_commands"
complete -c ip -f -s 4 -d "Short for --family inet" -n "not __fish_seen_subcommand_from $ip_commands"
complete -c ip -f -s 6 -d "Short for --family inet6" -n "not __fish_seen_subcommand_from $ip_commands"
complete -c ip -f -s B -d "Short for --family bridge" -n "not __fish_seen_subcommand_from $ip_commands"
complete -c ip -f -s M -d "Short for --family mpls" -n "not __fish_seen_subcommand_from $ip_commands"
complete -c ip -f -s 0 -d "Short for --family link" -n "not __fish_seen_subcommand_from $ip_commands"
complete -c ip -f -s o -l oneline -d "Output on one line" -n "not __fish_seen_subcommand_from $ip_commands"
complete -c ip -f -s r -l resolve -d "Resolve names and print them instead of addresses" -n "not __fish_seen_subcommand_from $ip_commands"
complete -c ip -f -s n -l netns -d "Use specified network namespace" -n "not __fish_seen_subcommand_from $ip_commands"
complete -c ip -f -s a -l all -d "Execute command for all objects" -n "not __fish_seen_subcommand_from $ip_commands"
complete -c ip -f -s c -l color -d "Configure color output" -n "not __fish_seen_subcommand_from $ip_commands"
complete -c ip -f -s t -l timestamp -d "Display current time when using monitor" -n "not __fish_seen_subcommand_from $ip_commands"
complete -c ip -f -o ts -l tshort -d "Like -timestamp, but shorter format" -n "not __fish_seen_subcommand_from $ip_commands"
complete -c ip -f -o rc -l rcvbuf -d "Set the netlink socket receive buffer size" -n "not __fish_seen_subcommand_from $ip_commands"
complete -c ip -f -o iec -d "Print human readable rates in IEC units" -n "not __fish_seen_subcommand_from $ip_commands"
complete -c ip -f -o br -l brief -d "Print only basic information in a tabular format" -n "not __fish_seen_subcommand_from $ip_commands"
complete -c ip -f -s j -l json -d "Output results in JSON" -n "not __fish_seen_subcommand_from $ip_commands"
complete -c ip -f -s p -l pretty -d "Output results in pretty JSON" -n "not __fish_seen_subcommand_from $ip_commands"
