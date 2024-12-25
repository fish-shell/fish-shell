# resolvectl (systemd 250)

function __resolvectl_interfaces
    resolvectl status | string replace -fr '^Link\s+\d+\s+\((.*)\)$' '$1'
end

function __resolvectl_protocols
    resolvectl --protocol help | string match -rv ':$'
end

function __resolvectl_types
    resolvectl --type help | string match -rv ':$'
end

function __resolvectl_classes
    resolvectl --class help | string match -rv ':$'
end

function __resolvectl_commands
    printf "%b\n" "query\tResolve domain names or IP addresses" \
        "query\tResolve domain names, IPv4 and IPv6 addresses" \
        "service\tResolve service records" \
        "openpgp\tQuery PGP keys for email" \
        "tlsa\tQuery TLS public keys" \
        "status\tShow current DNS settings" \
        "statistics\tShow resolver statistics" \
        "reset-statistics\tReset statistics counters" \
        "flush-caches\tFlush DNS RR caches" \
        "reset-server-features\tFlushe all feature level information" \
        "monitor\tMonitor DNS queries" \
        "show-cache\tShow cache contents" \
        "show-server-state\tShow server state" \
        "dns\tSet per-interface DNS servers" \
        "domain\tSet per-interface search or routing domains" \
        "default-route\tSet per-interface default route flag" \
        "llmnr\tSet per-interface LLMNR settings" \
        "mdns\tSet per-interface MulticastDNS settings" \
        "dnssec\tSet per-interface DNSSEC settings" \
        "dnsovertls\tSet per-interface DNS-over-TLS settings" \
        "nta\tSet per-interface DNSSEC NTA domains" \
        "revert\tRevert the per-interface DNS configuration" \
        "log-level\tSet the log-level"
end

# variables
set -l seen __fish_seen_subcommand_from
set -l commands (__resolvectl_commands | string split -f 1 '\t')

# commands
complete -c resolvectl -f
complete -c resolvectl -n "not $seen $commands" -xa "(__resolvectl_commands)"
complete -c resolvectl -n "__fish_is_nth_token 2" -n "$seen status dns domain default-route llmnr mdns dnssec dnsovertls nta revert" -xa "(__resolvectl_interfaces)"

# global options
complete -c resolvectl -s 4 -d "Resolve only IPv4"
complete -c resolvectl -s 6 -d "Resolve only IPv6"
complete -c resolvectl -l interface -s i -xa "(__resolvectl_interfaces)" -d "Interface to execute the query on"
complete -c resolvectl -l protocol -s p -xa "(__resolvectl_protocols)" -d "Network protocol for the query"
complete -c resolvectl -l type -s t -xa "(__resolvectl_types)" -d "DNS RR type for query"
complete -c resolvectl -l class -s c -xa "(__resolvectl_classes)" -d "DNS class for query"
complete -c resolvectl -l service-address -xa "true false" -d "Resolve address for SRV record"
complete -c resolvectl -l service-txt -xa "true false" -d "Resolve TXT records for services"
complete -c resolvectl -l cname -xa "true false" -d "Follow CNAME redirects"
complete -c resolvectl -l validate -xa "true false" -d "Allow DNSSEC validation"
complete -c resolvectl -l synthesize -xa "true false" -d "Allow synthetic response"
complete -c resolvectl -l cache -xa "true false" -d "Allow response from cache"
complete -c resolvectl -l zone -xa "true false" -d "Allow response from locally registered mDNS/LLMNR records"
complete -c resolvectl -l trust-anchor -xa "true false" -d "Use local trust anchors"
complete -c resolvectl -l network -xa "true false" -d "Allow response from network"
complete -c resolvectl -l search -xa "true false" -d "Use search domains for single-label names"
complete -c resolvectl -l raw -xa "payload packet" -d "Dump answer as binary data"
complete -c resolvectl -l legend -xa "true false" -d "Print headers and meta info"
complete -c resolvectl -l help -s h -d "Show help"
complete -c resolvectl -l version -d "Show version"
complete -c resolvectl -l no-pager -d "Do not pipe output into a pager"
