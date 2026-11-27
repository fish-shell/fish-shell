# localization: skip(not-needed)
# Fish completion for nft (nftables).
#
# Table, chain and set names are read from the ruleset. Without permission
# to run `nft list`, those names are omitted.

function __fish_nft_families
    printf '%s\t%s\n' \
        ip IPv4 \
        ip6 IPv6 \
        inet 'IPv4 and IPv6' \
        arp ARP \
        bridge Bridge \
        netdev 'Netdev (ingress/egress)'
end

function __fish_nft_commands
    printf '%s\t%s\n' \
        add 'Add a table, chain, rule, set, ...' \
        create 'Like add, but fail if it already exists' \
        insert 'Insert a rule at the beginning of a chain' \
        replace 'Replace a rule' \
        delete 'Delete an object' \
        destroy 'Delete an object, ignore if missing' \
        get 'Get set/map elements' \
        list 'List objects' \
        reset 'Reset stateful objects/rules' \
        flush 'Flush a table, chain, set or the whole ruleset' \
        rename 'Rename a chain' \
        import 'Import a ruleset' \
        export 'Export the ruleset' \
        monitor 'Monitor ruleset events' \
        describe 'Describe an expression or data type'
end

# Non-option arguments given so far (skips options and their values)
function __fish_nft_args
    set -l toks (commandline -opc)
    set -e toks[1]
    set -l skip 0
    for t in $toks
        if test $skip -eq 1
            set skip 0
            continue
        end
        switch $t
            case -f --file -D --define -I --includepath -d --debug
                set skip 1
            case '-*'
            case '*'
                echo $t
        end
    end
end

function __fish_nft_run
    command nft $argv 2>/dev/null
end

# __fish_nft_tables [family]
function __fish_nft_tables
    for line in (__fish_nft_run list tables)
        set -l p (string split ' ' -- $line)
        if test -z "$argv[1]"; or test "$p[2]" = "$argv[1]"
            echo $p[3]
        end
    end
end

# __fish_nft_names KIND FAMILY TABLE
function __fish_nft_names
    __fish_nft_run list table $argv[2] $argv[3] | string match -rg '^\t'$argv[1]' (\S+) \{$'
end

function __fish_nft_objects
    switch $argv[1]
        case add create
            printf '%s\t%s\n' \
                table Table \
                chain Chain \
                rule Rule \
                set 'Named set' \
                map 'Named map' \
                element 'Set/map element' \
                flowtable Flowtable \
                counter 'Named counter' \
                quota 'Named quota' \
                limit 'Named limit' \
                ct 'Conntrack helper/timeout/expectation' \
                secmark 'Security mark' \
                synproxy Synproxy
        case insert replace
            printf '%s\t%s\n' rule Rule
        case delete destroy
            printf '%s\t%s\n' \
                table Table \
                chain Chain \
                rule Rule \
                set 'Named set' \
                map 'Named map' \
                element 'Set/map element' \
                flowtable Flowtable \
                counter 'Named counter' \
                quota 'Named quota' \
                limit 'Named limit' \
                ct 'Conntrack helper/timeout/expectation' \
                secmark 'Security mark' \
                synproxy Synproxy
        case get
            printf '%s\t%s\n' element 'Set/map element'
        case list
            printf '%s\t%s\n' \
                ruleset 'Whole ruleset' \
                tables 'All tables' \
                table 'One table' \
                chains 'All chains' \
                chain 'One chain' \
                sets 'All sets' \
                set 'One set' \
                maps 'All maps' \
                map 'One map' \
                flowtables 'All flowtables' \
                flowtable 'One flowtable' \
                counters 'All counters' \
                counter 'One counter' \
                quotas 'All quotas' \
                quota 'One quota' \
                limits 'All limits' \
                limit 'One limit' \
                ct 'Conntrack objects' \
                secmarks 'All security marks' \
                secmark 'One security mark' \
                synproxys 'All synproxys' \
                synproxy 'One synproxy' \
                hooks 'Registered netfilter hooks'
        case flush
            printf '%s\t%s\n' \
                ruleset 'Whole ruleset' \
                table Table \
                chain Chain \
                set Set \
                map Map
        case reset
            printf '%s\t%s\n' \
                counters 'All counters' \
                counter 'One counter' \
                quotas 'All quotas' \
                quota 'One quota' \
                rules 'All rules' \
                rule 'One rule' \
                set Set \
                map Map \
                element Element
        case rename
            printf '%s\t%s\n' chain Chain
        case export import
            printf '%s\t%s\n' json 'JSON format'
        case monitor
            printf '%s\t%s\n' \
                new 'Only new events' \
                destroy 'Only destroy events' \
                tables Tables \
                chains Chains \
                sets Sets \
                rules Rules \
                elements Elements \
                ruleset 'Whole ruleset' \
                trace 'Trace events'
        case describe
            printf '%s\n' ipv4_addr ipv6_addr ether_addr inet_proto inet_service mark ifname
    end
end

function __fish_nft_complete
    set -l a (__fish_nft_args)
    set -l n (count $a)

    if test $n -eq 0
        __fish_nft_commands
        return
    end

    set -l cmd $a[1]
    if test $n -eq 1
        __fish_nft_objects $cmd
        return
    end

    set -l obj $a[2]
    switch $cmd
        case export import describe
            return
        case monitor
            if test $n -eq 2; and contains -- $obj new destroy
                __fish_nft_objects monitor | string match -rv '^(new|destroy)\t'
            end
            return
    end

    switch $obj
        case ruleset tables chains sets maps flowtables counters quotas limits secmarks synproxys hooks rules
            test $n -eq 2; and __fish_nft_families
            return
    end

    # Conntrack objects have an extra type word before the table
    set -l off 0
    if test $obj = ct
        if test $n -eq 2
            printf '%s\n' helper timeout expectation
            return
        end
        set off 1
    end

    set -l p
    if test $n -ge (math 3 + $off)
        set p $a[(math 3 + $off)..-1]
    end

    # Optional family before the table name
    set -l fam
    if contains -- "$p[1]" ip ip6 inet arp bridge netdev
        set fam $p[1]
        set -e p[1]
    end
    set -l famarg $fam
    test -z "$fam"; and set famarg ip
    set -l np (count $p)

    # Table name (or family)
    if test $np -eq 0
        test -z "$fam"; and __fish_nft_families
        if not contains -- $cmd add create; or test $obj != table
            __fish_nft_tables $famarg
        end
        return
    end

    set -l tbl $p[1]
    switch $obj
        case chain
            if test $np -eq 1; and not contains -- $cmd add create
                __fish_nft_names chain $famarg $tbl
            end
        case rule
            if test $np -eq 1
                __fish_nft_names chain $famarg $tbl
            else if test $np -eq 2
                if contains -- $cmd add insert
                    printf '%s\n' position index handle
                else
                    echo handle
                end
            end
        case set map flowtable counter quota limit secmark synproxy
            if test $np -eq 1; and not contains -- $cmd add create
                __fish_nft_names $obj $famarg $tbl
            end
        case element
            if test $np -eq 1
                __fish_nft_names set $famarg $tbl
                __fish_nft_names map $famarg $tbl
            end
    end
end

complete -c nft -f
complete -c nft -a '(__fish_nft_complete)'

# Options
complete -c nft -s h -l help -d 'Show help'
complete -c nft -s v -l version -d 'Show version information'
complete -c nft -s V -d 'Show extended version information'
complete -c nft -s f -l file -r -F -d 'Read input from file'
complete -c nft -s D -l define -x -d 'Define variable (name=value)'
complete -c nft -s i -l interactive -d 'Read input from interactive CLI'
complete -c nft -s I -l includepath -r -a '(__fish_complete_directories)' -d 'Add include search directory'
complete -c nft -s c -l check -d 'Check commands validity without applying them'
complete -c nft -s o -l optimize -d 'Optimize ruleset'
complete -c nft -s a -l handle -d 'Output rule handle'
complete -c nft -s s -l stateless -d 'Omit stateful information'
complete -c nft -s t -l terse -d 'Omit contents of sets'
complete -c nft -s S -l service -d 'Translate ports to service names'
complete -c nft -s N -l reversedns -d 'Translate IP addresses to names'
complete -c nft -s u -l guid -d 'Print UID/GID as defined in passwd/group'
complete -c nft -s n -l numeric -d 'Print fully numerical output'
complete -c nft -s y -l numeric-priority -d 'Print chain priority numerically'
complete -c nft -s p -l numeric-protocol -d 'Print layer 4 protocols numerically'
complete -c nft -s T -l numeric-time -d 'Print time values numerically'
complete -c nft -s e -l echo -d 'Echo what has been added, inserted or replaced'
complete -c nft -s j -l json -d 'Format output in JSON'
complete -c nft -s d -l debug -x -a 'scanner parser eval netlink mnl proto-ctx segtree all' -d 'Specify debugging level'
