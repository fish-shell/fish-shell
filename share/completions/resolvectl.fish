#resolvectl (systemd 248)

#variables
set -l seen __fish_seen_subcommand_from
set -l commands default-route dns dnsovertls dnssec domain flush-caches llmnr log-level mdns nta openpgp query reset-server-features reset-statistics revert service statistics status tlsa

#commands
complete -c resolvectl -x -n "not $seen $commands" -a "$commands"

#options
complete -c resolvectl -f -n "not $seen $commands" -s 4 -d "Resolve IPv4 addresses"
complete -c resolvectl -f -n "not $seen $commands" -s 6 -d "Resolve IPv6 addresses"
complete -c resolvectl -x -n "not $seen $commands" -l cache -d "Allow response from cache"
complete -c resolvectl -x -n "not $seen $commands" -l class -s c -d "Query RR with DNS class"
complete -c resolvectl -x -n "not $seen $commands" -l cname -d "Follow CNAME redirects"
complete -c resolvectl -f -n "not $seen $commands" -l help -s h -d "Show this help"
complete -c resolvectl -x -n "not $seen $commands" -l interface -s i -d "Look on interface"
complete -c resolvectl -x -n "not $seen $commands" -l legend -d "Print headers and additional info"
complete -c resolvectl -x -n "not $seen $commands" -l network -d "Allow response from network"
complete -c resolvectl -f -n "not $seen $commands" -l no-pager -d "Do not pipe output into a pager"
complete -c resolvectl -x -n "not $seen $commands" -l protocol -s p -d "Look via protocol"
complete -c resolvectl -f -n "not $seen $commands" -l raw -d "Dump the answer as binary data"
complete -c resolvectl -x -n "not $seen $commands" -l search -d "Use search domains for single-label names"
complete -c resolvectl -x -n "not $seen $commands" -l service-address -d "Resolve address for services"
complete -c resolvectl -x -n "not $seen $commands" -l service-txt -d "Resolve TXT records for services"
complete -c resolvectl -x -n "not $seen $commands" -l synthesize -d "Allow synthetic response"
complete -c resolvectl -x -n "not $seen $commands" -l trust-anchor -d "Allow response from local trust anchor"
complete -c resolvectl -x -n "not $seen $commands" -l type -s t -d "Query RR with DNS type"
complete -c resolvectl -x -n "not $seen $commands" -l validate -d "Allow DNSSEC validation"
complete -c resolvectl -f -n "not $seen $commands" -l version -d "Show package version"
complete -c resolvectl -x -n "not $seen $commands" -l zone -d "Allow response from locally registered mDNS/LLMNR records"
