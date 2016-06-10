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
		set mode "--user"
	else
		set mode "--system"
	end
    command busctl $mode $argv --no-legend --no-pager ^/dev/null
end
	
# Only get the arguments to the actual command, skipping all options and arguments to options
function __fish_busctl_get_command_args
	set -l skip
	set -l skip_next 0
	set -l cmd (commandline -opc)
	for token in $cmd
		switch $token
			# Options that take arguments - when given without "=", the next token is the arg - skip it
			case '--address' '--match' '--expect-reply' '--auto-start' '--allow-interactive-authorization' \
				'--timeout' '--augment-creds' '-H' '--host' '-M' '--machine'
				set skip_next 1
				continue
			# Skip all options themselves
			case '-*'
				continue
			# Command args only start after a command
			case 'status' 'monitor' 'capture' 'tree' 'introspect' 'call' 'get-property' 'set-property'
				set -e skip
				continue
			# These take no arguments, so abort completion
			case 'list' 'help'
				break
			case '*'
				if test "$skip_next" -eq 1; or set -q skip
					set skip_next 0
					continue
				end
				echo $token
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
	| string match ".$member *" | while read a b c d; echo $c; end
end

# This function completes service/busname, object, interface and then whatever the arguments are
# i.e. if argv[1] is "method", complete methods in the fourth place
function __fish_busctl_soi
	set -l args (__fish_busctl_get_command_args)
	set -l num (count $args)
	switch $num
		case 0 # We have nothing, need busname
			__fish_busctl_busnames
		case 1 # We have busname, need object
			__fish_busctl_objects $args
		case 2 # We have busname and object, need interface
			__fish_busctl_interfaces $args
		case '*' # We have busname and object and interface, what we need now depends on the command, so we get it as argument
			if test $num -ge 4; and set -q argv[2] # Check >= 4 to repeat the last type for get-property
				# Signatures have to be handled specially, because they're dependent on the member (method/property)
				# that's at the beginning of the line and prefixed with a "."
				# I.e. `busctl introspect` will print ".Capacity", but the argument to give to `get-property` is "Capacity"
				if test "$argv[2]" = "signature"
					__fish_busctl_signature $args
				else
					__fish_busctl_members $argv[2] $args
				end
			else if test $num -ge 3; and set -q argv[1]
				__fish_busctl_members $argv[1] $args
			else
				return 1
			end
	end
end

### Commands
set -l commands list status monitor capture tree introspect call get-property set-property help

complete -f -e -c busctl
complete -A -f -c busctl -n "not __fish_seen_subcommand_from $commands" -a "$commands"

### Arguments to commands
# "status" only takes a single service as argument
complete -f -c busctl -n "__fish_seen_subcommand_from status; and not count (__fish_busctl_get_command_args)" -a "(__fish_busctl_busnames)"

# These take multiple services
complete -x -c busctl -n "__fish_seen_subcommand_from monitor capture tree" -a "(__fish_busctl_busnames)"

# Read the busctl_soi calls as "Complete service, then object, then interface and then the arguments
# e.g. `call` takes service object interface method signature arguments
# We can't complete the arguments (without parsing the signature, which can look like "a{sv}" for an array of string-to-variant dictionaries)
complete -x -c busctl -n "__fish_seen_subcommand_from call" -a "(__fish_busctl_soi method signature)"
complete -x -c busctl -n "__fish_seen_subcommand_from get-property" -a "(__fish_busctl_soi property)"
complete -x -c busctl -n "__fish_seen_subcommand_from set-property" -a "(__fish_busctl_soi property signature)"
complete -x -c busctl -n "__fish_seen_subcommand_from introspect" -a "(__fish_busctl_soi)"

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
