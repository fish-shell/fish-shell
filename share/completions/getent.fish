# localization: skip(not-needed)
# Fish completion for getent.
#
# Database names and options differ between glibc and the BSDs. glibc accepts
# --version; the BSD commands do not.

function __fish_getent_is_glibc
    command getent --version &>/dev/null
end

function __fish_getent_databases
    # Present on glibc, FreeBSD, NetBSD and OpenBSD.
    printf '%s\t%s\n' \
        ethers 'Ethernet addresses' \
        group Groups \
        hosts Hosts \
        passwd Users \
        protocols Protocols \
        rpc 'RPC programs' \
        services 'Network services'
    if __fish_getent_is_glibc
        printf '%s\t%s\n' \
            ahosts 'Hosts via getaddrinfo (IPv4 and IPv6)' \
            ahostsv4 'Hosts via getaddrinfo (IPv4 only)' \
            ahostsv6 'Hosts via getaddrinfo (IPv6 only)' \
            aliases 'Mail aliases' \
            gshadow 'Shadow groups' \
            initgroups 'Supplementary groups of a user' \
            netgroup 'Network groups' \
            networks Networks \
            shadow 'Shadow passwords'
        return
    end
    switch (uname)
        case FreeBSD
            printf '%s\t%s\n' \
                netgroup 'Network groups' \
                networks Networks \
                shells 'User shells' \
                utmpx 'Login records'
        case NetBSD
            printf '%s\t%s\n' \
                disktab 'Disk descriptions' \
                gettytab 'Terminal descriptions' \
                netgroup 'Network groups' \
                networks Networks \
                printcap 'Printer descriptions' \
                shells 'User shells'
        case OpenBSD
            printf '%s\t%s\n' \
                shells 'User shells'
    end
end

# Non-option arguments given so far (skips the value of -s/--service).
function __fish_getent_args
    set -l toks (commandline -opc)
    set -e toks[1]
    set -l skip 0
    for t in $toks
        if test $skip -eq 1
            set skip 0
            continue
        end
        switch $t
            case -s --service
                set skip 1
            case '-*'
            case '*'
                echo $t
        end
    end
end

# NSS service names, taken from the installed libnss_*.so.2 modules.
function __fish_getent_nss_services
    command ls -1 /usr/lib /usr/lib64 /usr/lib/(uname -m)-linux-gnu 2>/dev/null \
        | string replace -rf '^libnss_(.*)\.so\.2$' '$1' | sort -u
end

function __fish_getent_keys
    set -l a (__fish_getent_args)
    test (count $a) -ge 1; or return
    switch $a[1]
        case passwd shadow initgroups
            __fish_complete_users
        case group gshadow
            __fish_complete_groups
        case hosts ahosts ahostsv4 ahostsv6
            __fish_print_hostnames
        case services
            command getent services 2>/dev/null | string replace -rf '^(\S+)\s+(\S+).*' '$1\t$2' | sort -u
        case protocols
            command getent protocols 2>/dev/null | string replace -rf '^(\S+)\s+(\S+).*' '$1\tprotocol $2'
        case networks
            command getent networks 2>/dev/null | string replace -rf '^(\S+)\s+(\S+).*' '$1\t$2'
        case rpc
            command getent rpc 2>/dev/null | string replace -rf '^(\S+)\s+(\S+).*' '$1\tprogram $2'
        case aliases
            command getent aliases 2>/dev/null | string replace -rf '^([^:]+):.*' '$1'
    end
end

function __fish_getent_needs_database
    test (count (__fish_getent_args)) -eq 0
end

complete -c getent -f
complete -c getent -n __fish_getent_needs_database -a '(__fish_getent_databases)'
complete -c getent -n 'not __fish_getent_needs_database' -a '(__fish_getent_keys)'

if __fish_getent_is_glibc
    complete -c getent -s A -l no-addrconfig -d 'Do not filter out unsupported IPv4/IPv6 addresses (with ahosts*)'
    complete -c getent -s i -l no-idn -d 'Disable IDN encoding'
    complete -c getent -s s -l service -x -a '(__fish_getent_nss_services)' -d 'Service configuration to be used'
    complete -c getent -s v -l verbose -d 'Print getaddrinfo error details'
    complete -c getent -s '?' -l help -d 'Show help'
    complete -c getent -l usage -d 'Show a short usage message'
    complete -c getent -s V -l version -d 'Print program version'
end
