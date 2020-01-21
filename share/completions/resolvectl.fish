#resolvectl (systemd 244)

#variables
set -l seen __fish_seen_subcommand_from
set -l commands default-route dns dnsovertls dnssec domain flush-caches llmnr mdns nta openpgp query reset-server-features reset-statistics revert service statistics status tlsa

#commands
complete -c resolvectl -x -n "not $seen $commands" -a "$commands"

#options
complete -c resolvectl -x -n "not $seen $commands" -l class -a "CLASS" -s c -d "Query RR with DNS class"
complete -c resolvectl -x -n "not $seen $commands" -l cname -a "yes" -d "Follow CNAME redirects"
complete -c resolvectl -x -n "not $seen $commands" -l help -s h -d "Show this help"
complete -c resolvectl -x -n "not $seen $commands" -l interface -a "INTERFACE" -s i -d "Look on interface"
complete -c resolvectl -x -n "not $seen $commands" -l legend -a "yes" -d "Print headers and additional info"
complete -c resolvectl -x -n "not $seen $commands" -l no-pager -d "Do not pipe output into a pager"
complete -c resolvectl -x -n "not $seen $commands" -l protocol -a "PROTO" -s p -d "Look via protocol"
complete -c resolvectl -f -n "not $seen $commands" -l raw -a "packet payload" -d "Dump the answer as binary data"
complete -c resolvectl -x -n "not $seen $commands" -l search -a "yes" -d "Use search domains for single-label names"
complete -c resolvectl -x -n "not $seen $commands" -l service-address -a "yes" -d "Resolve address for services"
complete -c resolvectl -x -n "not $seen $commands" -l service-txt -a "yes" -d "Resolve TXT records for services"
complete -c resolvectl -x -n "not $seen $commands" -l type -a "TYPE" -s t -d "Query RR with DNS type"
complete -c resolvectl -x -n "not $seen $commands" -l version -d "Show package version"
complete -c resolvectl -x -n "not $seen $commands" -s 4 -d "Resolve IPv4 addresses"
complete -c resolvectl -x -n "not $seen $commands" -s 6 -d "Resolve IPv6 addresses"
