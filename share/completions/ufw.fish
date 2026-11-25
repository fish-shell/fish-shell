# localization: skip(not-needed)
# Fish completion for ufw.

function __fish_ufw_needs_command
    set -l cmd (commandline -opc)
    test (count $cmd) -eq 1
end

function __fish_ufw_using_command
    set -l cmd (commandline -opc)
    contains -- $argv[1] $cmd
end

function __fish_ufw_route_needs_subcommand
    set -l cmd (commandline -opc)
    test (count $cmd) -eq 2; and test "$cmd[2]" = route
end

# After "route", "route delete" or "route insert NUM": the rule action comes next
function __fish_ufw_route_needs_action
    set -l cmd (commandline -opc)
    test (count $cmd) -ge 2; or return 1
    test "$cmd[2]" = route; or return 1
    switch (count $cmd)
        case 2
            return 0
        case 3
            test "$cmd[3]" = delete
        case 4
            test "$cmd[3]" = insert
        case '*'
            return 1
    end
end

function __fish_ufw_prepend_needs_action
    set -l cmd (commandline -opc)
    test (count $cmd) -eq 2; and test "$cmd[2]" = prepend
end

complete -c ufw -f

# Global options
complete -c ufw -s h -l help -d "Show help"
complete -c ufw -l version -d "Show version"
complete -c ufw -l dry-run -d "Do not modify firewall, only show changes"

# Main commands
complete -c ufw -n __fish_ufw_needs_command -a enable -d "Enable firewall"
complete -c ufw -n __fish_ufw_needs_command -a disable -d "Disable firewall"
complete -c ufw -n __fish_ufw_needs_command -a reload -d "Reload firewall"
complete -c ufw -n __fish_ufw_needs_command -a reset -d "Reset firewall"
complete -c ufw -n __fish_ufw_needs_command -a status -d "Show firewall status"
complete -c ufw -n __fish_ufw_needs_command -a show -d "Show firewall report"
complete -c ufw -n __fish_ufw_needs_command -a allow -d "Add allow rule"
complete -c ufw -n __fish_ufw_needs_command -a deny -d "Add deny rule"
complete -c ufw -n __fish_ufw_needs_command -a reject -d "Add reject rule"
complete -c ufw -n __fish_ufw_needs_command -a limit -d "Add rate-limit rule"
complete -c ufw -n __fish_ufw_needs_command -a delete -d "Delete rule"
complete -c ufw -n __fish_ufw_needs_command -a insert -d "Insert rule"
complete -c ufw -n __fish_ufw_needs_command -a default -d "Set default policy"
complete -c ufw -n __fish_ufw_needs_command -a logging -d "Configure logging"
complete -c ufw -n __fish_ufw_needs_command -a app -d "Manage application profiles"
complete -c ufw -n __fish_ufw_needs_command -a route -d "Manage routed (forwarded) rules"
complete -c ufw -n __fish_ufw_needs_command -a prepend -d "Add rule at the top of the rule list"

# status
complete -c ufw -n '__fish_ufw_using_command status' -a verbose -d "Verbose status"
complete -c ufw -n '__fish_ufw_using_command status' -a numbered -d "Numbered rules"

# show
complete -c ufw -n '__fish_ufw_using_command show' -a raw -d "Raw rules"
complete -c ufw -n '__fish_ufw_using_command show' -a builtins -d "Builtin rules"
complete -c ufw -n '__fish_ufw_using_command show' -a before-rules -d "Rules before the user rules"
complete -c ufw -n '__fish_ufw_using_command show' -a user-rules -d "User rules"
complete -c ufw -n '__fish_ufw_using_command show' -a after-rules -d "Rules after the user rules"
complete -c ufw -n '__fish_ufw_using_command show' -a logging-rules -d "Logging rules"
complete -c ufw -n '__fish_ufw_using_command show' -a listening -d "Listening ports"
complete -c ufw -n '__fish_ufw_using_command show' -a added -d "Rules as added"

# default
complete -c ufw -n '__fish_ufw_using_command default' -a allow -d "Allow by default"
complete -c ufw -n '__fish_ufw_using_command default' -a deny -d "Deny by default"
complete -c ufw -n '__fish_ufw_using_command default' -a reject -d "Reject by default"

complete -c ufw -n '__fish_ufw_using_command default' -a incoming -d "Incoming traffic"
complete -c ufw -n '__fish_ufw_using_command default' -a outgoing -d "Outgoing traffic"
complete -c ufw -n '__fish_ufw_using_command default' -a routed -d "Routed traffic"

# logging
complete -c ufw -n '__fish_ufw_using_command logging' -a on -d "Enable logging"
complete -c ufw -n '__fish_ufw_using_command logging' -a off -d "Disable logging"
complete -c ufw -n '__fish_ufw_using_command logging' -a low -d "Low logging"
complete -c ufw -n '__fish_ufw_using_command logging' -a medium -d "Medium logging"
complete -c ufw -n '__fish_ufw_using_command logging' -a high -d "High logging"
complete -c ufw -n '__fish_ufw_using_command logging' -a full -d "Full logging"

# rule modifiers
complete -c ufw -n '__fish_ufw_using_command allow; or __fish_ufw_using_command deny; or __fish_ufw_using_command reject; or __fish_ufw_using_command limit' -a in -d Incoming
complete -c ufw -n '__fish_ufw_using_command allow; or __fish_ufw_using_command deny; or __fish_ufw_using_command reject; or __fish_ufw_using_command limit' -a out -d Outgoing
complete -c ufw -n '__fish_ufw_using_command allow; or __fish_ufw_using_command deny; or __fish_ufw_using_command reject; or __fish_ufw_using_command limit' -a proto -d Protocol
complete -c ufw -n '__fish_ufw_using_command allow; or __fish_ufw_using_command deny; or __fish_ufw_using_command reject; or __fish_ufw_using_command limit' -a from -d Source
complete -c ufw -n '__fish_ufw_using_command allow; or __fish_ufw_using_command deny; or __fish_ufw_using_command reject; or __fish_ufw_using_command limit' -a to -d Destination
complete -c ufw -n '__fish_ufw_using_command allow; or __fish_ufw_using_command deny; or __fish_ufw_using_command reject; or __fish_ufw_using_command limit' -a port -d Port

# protocols
complete -c ufw -n '__fish_seen_subcommand_from proto' -a tcp -d TCP
complete -c ufw -n '__fish_seen_subcommand_from proto' -a udp -d UDP
complete -c ufw -n '__fish_seen_subcommand_from proto' -a icmp -d ICMP

# app subcommands
complete -c ufw -n '__fish_ufw_using_command app' -a list -d "List app profiles"
complete -c ufw -n '__fish_ufw_using_command app' -a info -d "Show app profile info"
complete -c ufw -n '__fish_ufw_using_command app' -a update -d "Update app profile"
complete -c ufw -n '__fish_ufw_using_command app' -a default -d "Set default app policy"

# prepend: ufw prepend RULE
complete -c ufw -n __fish_ufw_prepend_needs_action -a allow -d "Prepend allow rule"
complete -c ufw -n __fish_ufw_prepend_needs_action -a deny -d "Prepend deny rule"
complete -c ufw -n __fish_ufw_prepend_needs_action -a reject -d "Prepend reject rule"
complete -c ufw -n __fish_ufw_prepend_needs_action -a limit -d "Prepend rate-limit rule"

# route (forwarded traffic): ufw route RULE | delete RULE|NUM | insert NUM RULE
complete -c ufw -n __fish_ufw_route_needs_subcommand -a delete -d "Delete route rule"
complete -c ufw -n __fish_ufw_route_needs_subcommand -a insert -d "Insert route rule at position"
complete -c ufw -n __fish_ufw_route_needs_action -a allow -d "Add allow route rule"
complete -c ufw -n __fish_ufw_route_needs_action -a deny -d "Add deny route rule"
complete -c ufw -n __fish_ufw_route_needs_action -a reject -d "Add reject route rule"
complete -c ufw -n __fish_ufw_route_needs_action -a limit -d "Add rate-limit route rule"

# route rules: "in on IFACE out on IFACE"
complete -c ufw -n '__fish_ufw_using_command route; and __fish_prev_arg_in in out' -a on -d Interface
complete -c ufw -n '__fish_ufw_using_command route; and __fish_prev_arg_in on' -a '(__fish_print_interfaces)' -d Interface
