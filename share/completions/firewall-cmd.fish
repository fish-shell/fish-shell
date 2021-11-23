function __fish_print_firewalld_zones --description "Print list of predefined firewalld zones"
    firewall-cmd --get-zones | string split " "
end

function __fish_print_firewalld_policies --description "Print list of predefined firewalld policies"
    firewall-cmd --get-policies | string split " "
end

function __fish_print_firewalld_services --description "Print list of predefined firewalld services"
    firewall-cmd --get-services | string split " "
end

function __fish_print_firewalld_icmptypes --description "Print list of ICMP types supported by firewalld"
    firewall-cmd --get-icmptypes | string split " "
end

function __fish_print_firewalld_helpers --description "Print list of predefined firewalld helpers"
    firewall-cmd --get-helpers | string split " "
end

function __fish_print_firewalld_protocols --description "Print list of protocols supported by firewalld"
    string replace -f -r '^([[:alnum:]]\S*).*' '$1' </etc/protocols
end

complete -c firewall-cmd -f

# General Options
complete -c firewall-cmd -s h -l help -d "Prints a short help text and exits"
complete -c firewall-cmd -s V -l version -d "Print the version string of firewalld"
complete -c firewall-cmd -s q -l quiet -d "Do not print status messages"

# Status Options
complete -c firewall-cmd -l state -d "Check whether the firewalld daemon is active"
complete -c firewall-cmd -l reload -d "Reload firewall rules and keep state information"
complete -c firewall-cmd -l complete-reload -d "Reload firewall completely, even netfilter kernel modules"
complete -c firewall-cmd -l runtime-to-permanent -d "Save active runtime configuration and overwrite permanent configuration"
complete -c firewall-cmd -l check-config -d "Run checks on the permanent configuration"

# Log Denied Options
complete -c firewall-cmd -l get-log-denied -d "Print the log denied setting"
complete -c firewall-cmd -l set-log-denied -xa "all unicast broadcast multicast off" -d "Add logging rules right before reject and drop rules"

# Permanent Options
complete -c firewall-cmd -l permanent -d "Set options permanently"

# Zone Options
complete -c firewall-cmd -l get-default-zone -d "Print default zone for connections and interfaces"
complete -c firewall-cmd -l set-default-zone -xa "(__fish_print_firewalld_zones)" -d "Set default zone for connections and interfaces"
complete -c firewall-cmd -l get-active-zones -d "Print currently active zones altogether with interfaces and sources"
complete -c firewall-cmd -l get-zones -d "Print predefined zones"
complete -c firewall-cmd -l get-services -d "Print predefined services"
complete -c firewall-cmd -l get-icmptypes -d "Print predefined icmptypes"
complete -c firewall-cmd -l get-zone-of-interface -xa "(__fish_print_interfaces)" -d "Print the name of the zone the interface is bound to"
complete -c firewall-cmd -l get-zone-of-source -x -d "Print the name of the zone the source is bound to"
complete -c firewall-cmd -l info-zone -xa "(__fish_print_firewalld_zones)" -d "Print information about the zone"
complete -c firewall-cmd -l list-all-zones -d "List everything added for or enabled in all zones"
complete -c firewall-cmd -l new-zone -x -d "Add a new permanent and empty zone"
complete -c firewall-cmd -l new-zone-from-file -F -d "Add a new permanent zone from a prepared zone file"
complete -c firewall-cmd -l delete-zone -xa "(__fish_print_firewalld_zones)" -d "Delete an existing permanent zone"
complete -c firewall-cmd -l load-zone-defaults -xa "(__fish_print_firewalld_zones)" -d "Load zone default settings"
complete -c firewall-cmd -l path-zone -xa "(__fish_print_firewalld_zones)" -d "Print path of the zone configuration file"

# Policy Options
complete -c firewall-cmd -l get-policies -d "Print predefined policies"
complete -c firewall-cmd -l info-policy -xa "(__fish_print_firewalld_policies)" -d "Print information about the policy"
complete -c firewall-cmd -l list-all-policies -d "List everything added for or enabled in all policies"
complete -c firewall-cmd -l new-policy -x -d "Add a new permanent policy"
complete -c firewall-cmd -l new-policy-from-file -F -d "Add a new permanent policy from a prepared policy file"
complete -c firewall-cmd -l path-policy -xa "(__fish_print_firewalld_policies)" -d "Print path of the policy configuration file"
complete -c firewall-cmd -l delete-policy -xa "(__fish_print_firewalld_policies)" -d "Delete an existing permanent policy"
complete -c firewall-cmd -l load-policy-defaults -xa "(__fish_print_firewalld_policies)" -d "Load the shipped defaults for a policy"

# Options to Adapt and Query Zones and Policies
complete -c firewall-cmd -l zone -xa "(__fish_print_firewalld_zones)" -d "Apply option to this zone"
complete -c firewall-cmd -l policy -xa "(__fish_print_firewalld_policies)" -d "Apply option to this policy"
complete -c firewall-cmd -l get-target -d "Get the target"
complete -c firewall-cmd -l set-target -xa "default CONTINUE ACCEPT DROP REJECT" -d "Set the target"
complete -c firewall-cmd -l list-all -d "List everything added for or enabled"
complete -c firewall-cmd -l list-services -d "List services added"
complete -c firewall-cmd -l add-service -xa "(__fish_print_firewalld_services)" -d "Add a service"
complete -c firewall-cmd -l remove-service -xa "(__fish_print_firewalld_services)" -d "Remove a service"
complete -c firewall-cmd -l query-service -xa "(__fish_print_firewalld_services)" -d "Return whether service has been added"
complete -c firewall-cmd -l list-ports -d "List ports added"
complete -c firewall-cmd -l list-protocols -d "List protocols added"
complete -c firewall-cmd -l list-source-ports -d "List source ports added"
complete -c firewall-cmd -l list-icmp-blocks -d "List ICMPs type blocks added"
complete -c firewall-cmd -l add-icmp-block -xa "(__fish_print_firewalld_icmptypes)" -d "Add an ICMP block for icmptype"
complete -c firewall-cmd -l remove-icmp-block -xa "(__fish_print_firewalld_icmptypes)" -d "Remove the ICMP block for icmptype"
complete -c firewall-cmd -l query-icmp-block -xa "(__fish_print_firewalld_icmptypes)" -d "Return whether an ICMP block for icmptype has been added"
complete -c firewall-cmd -l list-forward-ports -d "List IPv4 forward ports added"
complete -c firewall-cmd -l add-forward-port -x -d "Add the IPv4 forward port"
complete -c firewall-cmd -l remove-forward-port -x -d "Remove the IPv4 forward port"
complete -c firewall-cmd -l query-forward-port -x -d "Return whether the IPv4 forward port has been added"
complete -c firewall-cmd -l add-masquerade -d "Enable IPv4 masquerade"
complete -c firewall-cmd -l remove-masquerade -d "Disable IPv4 masquerade"
complete -c firewall-cmd -l query-masquerade -d "Return whether IPv4 masquerading has been enabled"
complete -c firewall-cmd -l list-rich-rules -d "List rich language rules added"
complete -c firewall-cmd -l add-rich-rule -x -d "Add rich language rule"
complete -c firewall-cmd -l remove-rich-rule -x -d "Remove rich language rule"
complete -c firewall-cmd -l query-rich-rule -x -d "Return whether the rich language rule has been added"
complete -c firewall-cmd -l timeout -x -d "The rule will be active for the specified amount of time"

# Options to Adapt and Query Zones
complete -c firewall-cmd -l add-icmp-block-inversion -d "Enable ICMP block inversion"
complete -c firewall-cmd -l remove-icmp-block-inversion -d "Disable ICMP block inversion"
complete -c firewall-cmd -l query-icmp-block-inversion -d "Return whether ICMP block inversion is enabled"
complete -c firewall-cmd -l add-forward -d "Enable intra zone forwarding"
complete -c firewall-cmd -l remove-forward -d "Disable intra zone forwarding"
complete -c firewall-cmd -l query-forward -d "Return whether intra zone forwarding is enabled"

# Options to Adapt and Query Policies
complete -c firewall-cmd -l get-priority -d "Get the priority"
complete -c firewall-cmd -l set-priority -x -d "Set the priority"
complete -c firewall-cmd -l list-ingress-zones -d "List ingress zones added"
complete -c firewall-cmd -l add-ingress-zone -xa "(__fish_print_firewalld_zones) HOST ANY" -d "Add an ingress zone"
complete -c firewall-cmd -l remove-ingress-zone -xa "(__fish_print_firewalld_zones) HOST ANY" -d "Remove an ingress zone"
complete -c firewall-cmd -l query-ingress-zone -xa "(__fish_print_firewalld_zones) HOST ANY" -d "Return whether zone has been added"
complete -c firewall-cmd -l list-egress-zones -d "List egress zones added"
complete -c firewall-cmd -l add-egress-zone -xa "(__fish_print_firewalld_zones) HOST ANY" -d "Add an egress zone"
complete -c firewall-cmd -l remove-egress-zone -xa "(__fish_print_firewalld_zones) HOST ANY" -d "Remove an egress zone"
complete -c firewall-cmd -l query-egress-zone -xa "(__fish_print_firewalld_zones) HOST ANY" -d "Return whether zone has been added"

# Options to Handle Bindings of Interfaces
complete -c firewall-cmd -l list-interfaces -d "List interfaces that are bound to zone"
complete -c firewall-cmd -l add-interface -xa "(__fish_print_interfaces)" -d "Bind interface to zone"
complete -c firewall-cmd -l change-interface -xa "(__fish_print_interfaces)" -d "Change to which zone interface is bound"
complete -c firewall-cmd -l query-interface -xa "(__fish_print_interfaces)" -d "Query whether interface is bound to zone"
complete -c firewall-cmd -l remove-interface -xa "(__fish_print_interfaces)" -d "Remove binding of interface from zone"

# Options to Handle Bindings of Sources
complete -c firewall-cmd -l list-sources -d "List sources that are bound to zone"
complete -c firewall-cmd -l add-source -x -d "Bind the source to zone"
complete -c firewall-cmd -l change-source -x -d "Change zone the source is bound to"
complete -c firewall-cmd -l query-source -x -d "Query whether the source is bound to the zone"
complete -c firewall-cmd -l remove-source -x -d "Remove binding of the source from zone"

# IPSet Options
complete -c firewall-cmd -l get-ipset-types -d "Print the supported ipset types"
complete -c firewall-cmd -l new-ipset -x -d "Add a new permanent and empty ipset"
complete -c firewall-cmd -l type -x -d "Specify ipset type"
complete -c firewall-cmd -l option -x -d "Options for new ipset"
complete -c firewall-cmd -l new-ipset-from-file -F -d "Add a new permanent ipset from a prepared ipset file"
complete -c firewall-cmd -l delete-ipset -x -d "Delete an existing permanent ipset"
complete -c firewall-cmd -l load-ipset-defaults -x -d "Load ipset default settings"
complete -c firewall-cmd -l info-ipset -x -d "Print information about the ipset"
complete -c firewall-cmd -l get-ipsets -d "Print predefined ipsets"
complete -c firewall-cmd -l ipset -x -d "Apply settings to this ipset"
complete -c firewall-cmd -l add-entry -x -d "Add a new entry to the ipset"
complete -c firewall-cmd -l remove-entry -x -d "Remove an entry from the ipset"
complete -c firewall-cmd -l query-entry -x -d "Return whether the entry has been added to an ipset"
complete -c firewall-cmd -l get-entries -d "List all entries of the ipset"
complete -c firewall-cmd -l add-entries-from-file -F -d "Add new entries to the ipset from the file"
complete -c firewall-cmd -l remove-entries-from-file -F -d "Remove existing entries from the ipset from the file"
complete -c firewall-cmd -l path-ipset -d "Print path of the ipset configuration file"

# Service Options
complete -c firewall-cmd -l info-service -xa "(__fish_print_firewalld_services)" -d "Print information about the service"
complete -c firewall-cmd -l new-service -x -d "Add a new permanent and empty service"
complete -c firewall-cmd -l new-service-from-file -F -d "Add a new permanent service from a prepared service file"
complete -c firewall-cmd -l delete-service -xa "(__fish_print_firewalld_services)" -d "Delete an existing permanent service"
complete -c firewall-cmd -l load-service-defaults -xa "(__fish_print_firewalld_services)" -d "Load service default settings"
complete -c firewall-cmd -l path-service -xa "(__fish_print_firewalld_services)" -d "Print path of the service configuration file"
complete -c firewall-cmd -l service -xa "(__fish_print_firewalld_services)" -d "Apply settings to this service"
complete -c firewall-cmd -l get-protocols -d "List protocols added to the permanent service"
complete -c firewall-cmd -l get-source-ports -d "List source ports added to the permanent service"
complete -c firewall-cmd -l add-helper -xa "(__fish_print_firewalld_helpers)" -d "Add a new helper to the permanent service"
complete -c firewall-cmd -l remove-helper -xa "(__fish_print_firewalld_helpers)" -d "Remove a helper from the permanent service"
complete -c firewall-cmd -l query-helper -xa "(__fish_print_firewalld_helpers)" -d "Return wether the helper has been added to the permanent service"
complete -c firewall-cmd -l get-service-helpers -d "List helpers added to the permanent service"
complete -c firewall-cmd -l set-destination -x -d "Set destination for ipv to address[/mask] in the permanent service"
complete -c firewall-cmd -l add-include -xa "(__fish_print_firewalld_services)" -d "Add a new include to the permanent service"
complete -c firewall-cmd -l remove-include -xa "(__fish_print_firewalld_services)" -d "Remove a include from the permanent service"
complete -c firewall-cmd -l query-include -xa "(__fish_print_firewalld_services)" -d "Return wether the include has been added to the permanent service"
complete -c firewall-cmd -l get-includes -d "List includes added to the permanent service"

# Helper Options
complete -c firewall-cmd -l info-helper -xa "(__fish_print_firewalld_helpers)" -d "Print information about the helper"
complete -c firewall-cmd -l new-helper -x -d "Add a new permanent helper"
complete -c firewall-cmd -l module -x -d "Specify module for new helper"
complete -c firewall-cmd -l new-helper-from-file -F -d "Add a new permanent helper from a prepared helper file"
complete -c firewall-cmd -l delete-helper -xa "(__fish_print_firewalld_helpers)" -d "Delete an existing permanent helper"
complete -c firewall-cmd -l load-helper-defaults -xa "(__fish_print_firewalld_helpers)" -d "Load helper default settings"
complete -c firewall-cmd -l path-helper -xa "(__fish_print_firewalld_helpers)" -d "Print path of the helper configuration file"
complete -c firewall-cmd -l get-helpers -d "Print predefined helpers as a space separated list"
complete -c firewall-cmd -l helper -xa "(__fish_print_firewalld_helpers)" -d "Apply settings to this helper"
complete -c firewall-cmd -l set-module -x -d "Set module description for helper"
complete -c firewall-cmd -l get-module -d "Print module description for helper"
complete -c firewall-cmd -l set-family -x -d "Set family description for helper"
complete -c firewall-cmd -l get-family -d "Print family description of helper"

# Internet Control Message Protocol (ICMP) type Options
complete -c firewall-cmd -l info-icmptype -xa "(__fish_print_firewalld_icmptypes)" -d "Print information about the icmptype"
complete -c firewall-cmd -l new-icmptype -x -d "Add a new permanent and empty icmptype"
complete -c firewall-cmd -l new-icmptype-from-file -F -d "Add a new permanent icmptype from a prepared icmptype file"
complete -c firewall-cmd -l delete-icmptype -xa "(__fish_print_firewalld_icmptypes)" -d "Delete an existing permanent icmptype"
complete -c firewall-cmd -l load-icmptype-defaults -xa "(__fish_print_firewalld_icmptypes)" -d "Load icmptype default settings"
complete -c firewall-cmd -l icmptype -xa "(__fish_print_firewalld_icmptypes)" -d "Apply settings to this icmptype"
complete -c firewall-cmd -l add-destination -xa "ipv4 ipv6" -d "Enable destination for ipv in permanent icmptype"
complete -c firewall-cmd -l path-icmptype -xa "(__fish_print_firewalld_icmptypes)" -d "Print path of the icmptype configuration file"

# Direct Options
complete -c firewall-cmd -l direct -d "Give a more direct access to the firewall"
complete -c firewall-cmd -l get-all-chains -d "Get all chains added to all tables"
complete -c firewall-cmd -l get-chains -xa "ipv4 ipv6 eb" -d "Get all chains added to table"
complete -c firewall-cmd -l add-chain -xa "ipv4 ipv6 eb" -d "Add a new chain to table"
complete -c firewall-cmd -l remove-chain -xa "ipv4 ipv6 eb" -d "Remove chain from table"
complete -c firewall-cmd -l query-chain -xa "ipv4 ipv6 eb" -d "Return whether the chain with given name exists in table"
complete -c firewall-cmd -l get-all-rules -d "Get all rules added to all chains in all tables"
complete -c firewall-cmd -l get-rules -xa "ipv4 ipv6 eb" -d "Get all rules added to chain in table"
complete -c firewall-cmd -l add-rule -xa "ipv4 ipv6 eb" -d "Add a rule to chain in table"
complete -c firewall-cmd -l remove-rule -xa "ipv4 ipv6 eb" -d "Remove a rule from chain in table"
complete -c firewall-cmd -l remove-rules -xa "ipv4 ipv6 eb" -d "Remove all rules in the chain in table"
complete -c firewall-cmd -l query-rule -xa "ipv4 ipv6 eb" -d "Return whether the rule exists in chain in table"
complete -c firewall-cmd -l passthrough -xa "ipv4 ipv6 eb" -d "Pass a command through to the firewall"
complete -c firewall-cmd -l get-all-passthroughs -d "Get all passthrough rules"
complete -c firewall-cmd -l get-passthroughs -xa "ipv4 ipv6 eb" -d "Get all passthrough rules for the ipv value"
complete -c firewall-cmd -l add-passthrough -xa "ipv4 ipv6 eb" -d "Add a passthrough rule"
complete -c firewall-cmd -l remove-passthrough -xa "ipv4 ipv6 eb" -d "Remove a passthrough rule"
complete -c firewall-cmd -l query-passthrough -xa "ipv4 ipv6 eb" -d "Return whether the passthrough rule exists"

# Lockdown Options
complete -c firewall-cmd -l lockdown-on -d "Enable lockdown"
complete -c firewall-cmd -l lockdown-off -d "Disable lockdown"
complete -c firewall-cmd -l query-lockdown -d "Query whether lockdown is enabled"

# Lockdown Whitelist Options
complete -c firewall-cmd -l list-lockdown-whitelist-commands -d "List all command lines that are on the whitelist"
complete -c firewall-cmd -l add-lockdown-whitelist-command -x -d "Add the command to the whitelist"
complete -c firewall-cmd -l remove-lockdown-whitelist-command -x -d "Remove the command from the whitelist"
complete -c firewall-cmd -l query-lockdown-whitelist-command -x -d "Query whether the command is on the whitelist"
complete -c firewall-cmd -l list-lockdown-whitelist-contexts -d "List all contexts that are on the whitelist"
complete -c firewall-cmd -l add-lockdown-whitelist-context -x -d "Add the context to the whitelist"
complete -c firewall-cmd -l remove-lockdown-whitelist-context -x -d "Remove the context from the whitelist"
complete -c firewall-cmd -l query-lockdown-whitelist-context -x -d "Query whether the context is on the whitelist"
complete -c firewall-cmd -l list-lockdown-whitelist-uids -d "List all user ids that are on the whitelist"
complete -c firewall-cmd -l add-lockdown-whitelist-uid -x -d "Add the user id to the whitelist"
complete -c firewall-cmd -l remove-lockdown-whitelist-uid -x -d "Remove the user id from the whitelist"
complete -c firewall-cmd -l query-lockdown-whitelist-uid -x -d "Query whether the user id is on the whitelist"
complete -c firewall-cmd -l list-lockdown-whitelist-users -d "List all user names that are on the whitelist"
complete -c firewall-cmd -l add-lockdown-whitelist-user -xa "(__fish_complete_users)" -d "Add the user to the whitelist"
complete -c firewall-cmd -l remove-lockdown-whitelist-user -xa "(__fish_complete_users)" -d "Remove the user from the whitelist"
complete -c firewall-cmd -l query-lockdown-whitelist-user -xa "(__fish_complete_users)" -d "Query whether the user is on the whitelist"

# Panic Options
complete -c firewall-cmd -l panic-on -d "Enable panic mode"
complete -c firewall-cmd -l panic-off -d "Disable panic mode"
complete -c firewall-cmd -l query-panic -d "Returns 0 if panic mode is enabled, 1 otherwise"

# Object independent options
complete -c firewall-cmd -l set-description -x -d "Set new description for zone|policy|ipset|service|helper|icmptype"
complete -c firewall-cmd -l get-description -d "Print description of zone|policy|ipset|service|helper|icmptype"
complete -c firewall-cmd -l set-short -x -d "Set short description for zone|policy|ipset|service|helper|icmptype"
complete -c firewall-cmd -l get-short -d "Print short description of zone|policy|ipset|service|helper|icmptype"
complete -c firewall-cmd -l name -x -d "Name for new zone|policy|ipset|service|helper|icmptype"
complete -c firewall-cmd -l add-port -x -d "Add the port to zone|policy|service|helper"
complete -c firewall-cmd -l remove-port -x -d "Remove the port from zone|policy|service|helper"
complete -c firewall-cmd -l query-port -x -d "Return whether the port has been added for zone|policy|service|helper"
complete -c firewall-cmd -l add-protocol -xa "(__fish_print_firewalld_protocols)" -d "Add the protocol to zone|policy|service"
complete -c firewall-cmd -l remove-protocol -xa "(__fish_print_firewalld_protocols)" -d "Remove the protocol from zone|policy|service"
complete -c firewall-cmd -l query-protocol -xa "(__fish_print_firewalld_protocols)" -d "Return whether the protocol has been added to zone|policy|service"
complete -c firewall-cmd -l add-source-port -x -d "Add the source port to zone|policy|service"
complete -c firewall-cmd -l remove-source-port -x -d "Remove the source port from zone|policy|service"
complete -c firewall-cmd -l query-source-port -x -d "Return whether the source port has been added for zone|policy|service"
complete -c firewall-cmd -l get-ports -d "List ports added to the permanent service|helper"
complete -c firewall-cmd -l remove-destination -xa "ipv4 ipv6" -d "Disable destination for ipv in permanent service|icmptype"
complete -c firewall-cmd -l query-destination -xa "ipv4 ipv6" -d "Return whether destination for ipv is enabled in permanent service|icmptype"
complete -c firewall-cmd -l get-destinations -d "List destinations in permanent service|icmptype"
complete -c firewall-cmd -l family -x -d "Inet family"
