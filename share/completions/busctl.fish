# Fish completions for systemd's "busctl" dbus tool
# TODO:
# One issue is that sometimes these will come to a dead-end e.g. when a particular interface has no properties
# Another is that some busnames aren't accesible by the current user
# but this can't be predicted via the user that owns that name, e.g. `org.freedesktop.login1`
# is usually owned by a root-owned process, yet accessible (at least in part) by normal users

# A simple wrapper to call busctl with the correct mode and output
function __fish_busctl
    # TODO: If there's a "--address" argument we need to pass that
    # We also need to pass the _last_ of these (`busctl --user --system` operates on the system bus)
    set -l mode
    if __fish_contains_opt user
        set mode --user
    else
        set mode --system
    end
    command busctl $mode $argv --no-legend --no-pager 2>/dev/null
end

function _fish_busctl
    set -l args a-address= s-show-machine u-unique A-acquired ä-activatable \
        m-match= S-size= l-list q-quiet v-verbose e-expect-reply= Ä-auto-start= \
        1-allow-interactive-authorization= t-timeout= 2-augment-creds= U-user 3-system \
        H/host= M/machine= n-no-pager N-no-legend h/help V-version
    set -l cmdline (commandline -xpc) (commandline -ct)
    set -e cmdline[1]
    argparse $args -- $cmdline 2>/dev/null
    or return
    set -l cmd $argv[1]
    set -e argv[1]
    switch "$cmd"
        case list help
            # Accepts nothing
            return
        case status
            # A service
            if not set -q argv[2]
                __fish_busctl_busnames
            end
        case monitor capture tree
            # Services
            __fish_busctl_busnames
        case introspect
            # Service Object Interface
            if not set -q argv[2]
                __fish_busctl_busnames
            else if not set -q argv[3]
                __fish_busctl_objects $argv[1]
            else if not set -q argv[4]
                __fish_busctl_interfaces $argv[1..2]
            end
        case call
            # SERVICE OBJECT INTERFACE METHOD [SIGNATURE [ARGUMENT...]]
            if not set -q argv[2]
                __fish_busctl_busnames
            else if not set -q argv[3]
                __fish_busctl_objects $argv[1]
            else if not set -q argv[4]
                __fish_busctl_interfaces $argv[1..2]
            else if not set -q argv[5]
                __fish_busctl_members method $argv[1..3]
            else if not set -q argv[6]
                __fish_busctl_signature $argv[1..4]
            end
        case get-property
            # SERVICE OBJECT INTERFACE PROPERTY...
            if not set -q argv[2]
                __fish_busctl_busnames
            else if not set -q argv[3]
                __fish_busctl_objects $argv[1]
            else if not set -q argv[4]
                __fish_busctl_interfaces $argv[1..2]
            else
                __fish_busctl_members property $argv[1..3]
            end
        case set-property
            # SERVICE OBJECT INTERFACE PROPERTY SIGNATURE ARGUMENT...
            if not set -q argv[2]
                __fish_busctl_busnames
            else if not set -q argv[3]
                __fish_busctl_objects $argv[1]
            else if not set -q argv[4]
                __fish_busctl_interfaces $argv[1..2]
            else if not set -q argv[5]
                __fish_busctl_members property $argv[1..3]
            else if not set -q argv[6]
                __fish_busctl_members signature $argv[1..4]
            end
    end
end

function __fish_busctl_busnames
    __fish_busctl list --acquired | string replace -r '\s+.*$' ''
    # Describe unique names (":1.32") with their process (e.g. `:1.32\tsteam`)
    __fish_busctl list --unique | string replace -r '\s+\S+\s+(\S+)\s+.*$' '\t$1'
end

function __fish_busctl_objects -a busname
    __fish_busctl tree --list $busname | string replace -r '\s+.*$' ''
end

function __fish_busctl_interfaces -a busname -a object
    __fish_busctl introspect --list $busname $object | string replace -r '\s+.*$' ''
end

function __fish_busctl_members -a type -a busname -a object -a interface
    __fish_busctl introspect --list $busname $object $interface \
        | string match -- "* $type *" | string replace -r '.(\S+) .*' '$1'
end

function __fish_busctl_signature -a busname -a object -a interface -a member
    __fish_busctl introspect --list $busname $object $interface \
        | string match ".$member *" | while read -l a b c d
        echo $c
    end
end

### Commands
set -l commands list status monitor capture tree introspect call get-property set-property help

complete -f -c busctl
complete -f -c busctl -n "not __fish_seen_subcommand_from $commands" -a "$commands"

### Arguments to commands
complete -f -c busctl -a '(_fish_busctl)'

# Flags
# These are incomplete as I have no idea how to complete --address= or --match=
complete -f -c busctl -n "__fish_seen_subcommand_from call" -l quiet -d 'Suppress message payload display'
complete -f -c busctl -n "__fish_seen_subcommand_from call get-property" -l verbose
complete -f -c busctl -n "__fish_seen_subcommand_from tree" -l list -d 'Show a flat list instead of a tree'
complete -x -c busctl -n "__fish_seen_subcommand_from call" -l expect-reply -a "yes no"
complete -x -c busctl -n "__fish_seen_subcommand_from call" -l auto-start -a "yes no" -d 'Activate the peer if necessary'
complete -x -c busctl -n "__fish_seen_subcommand_from call" -l allow-interactive-authorization -a "yes no"
complete -x -c busctl -n "__fish_seen_subcommand_from call" -l timeout -a "(seq 0 100)"
complete -f -c busctl -n "__fish_seen_subcommand_from list" -l unique -d 'Only show unique (:X.Y) names'
complete -f -c busctl -n "__fish_seen_subcommand_from list" -l acquired -d 'Only show well-known names'
complete -f -c busctl -n "__fish_seen_subcommand_from list" -l activatable -d 'Only show peers that have not been activated yet but can be'
complete -f -c busctl -n "__fish_seen_subcommand_from list" -l show-machine -d 'Show the machine the peers belong to'
complete -x -c busctl -n "__fish_seen_subcommand_from list status" -l augment-creds -a "yes no"
complete -f -c busctl -l user
complete -f -c busctl -l system
complete -f -c busctl -s H -l host -a "(__fish_print_hostnames)"
complete -f -c busctl -s M -l machine -a "(__fish_systemd_machines)"
complete -f -c busctl -l no-pager
complete -f -c busctl -l no-legend
complete -f -c busctl -s h -l help
complete -f -c busctl -l version
