# Fish completions for systemd's "busctl" dbus tool

# A ton of functions, all working by the same principle
# __fish_busctl_*: Get the available options for * via busctl "list" or "introspect"
# __fish_busctl_has_*: Return 0 when a * is specified (and echo it), else return 1
# Unfortunately these become a bit slow as they stack because we don't keep state
# i.e. __fish_busctl_methods needs to know the interface, which needs the object, which needs the busname
# To speed this up, we'd need to either keep state or just assume e.g. the first non-option argument to "call" is a busname

function __fish_busctl_busnames
	set -l mode
    if __fish_contains_opt user
		set mode "--user"
	else
		set mode "--system"
	end
    command busctl $mode list --no-legend --no-pager ^/dev/null | while read a b; echo $a; end
end

function __fish_busctl_has_busname
	for busname in (__fish_busctl_busnames)
		if contains -- $busname (commandline -opc)
		   echo $busname
		   return 0
		end
	end
	return 1
end

function __fish_busctl_has_object
	for object in (__fish_busctl_objects)
		if contains -- $object (commandline -opc)
		   echo $object
		   return 0
		end
	end
	return 1
end

function __fish_busctl_has_interface
	for interface in (__fish_busctl_interfaces)
		if contains -- $interface (commandline -opc)
		   echo $interface
		   return 0
		end
	end
	return 1
end

function __fish_busctl_has_member
	for member in (__fish_busctl_members $argv)
		if contains -- $member (commandline -opc)
		   echo $member
		   return 0
		end
	end
	return 1
end

function __fish_busctl_has_method
	__fish_busctl_has_member method
end
		 
function __fish_busctl_has_property
	__fish_busctl_has_member property
end

function __fish_busctl_has_signature
	for signature in (__fish_busctl_signature)
		if contains -- $signature (commandline -opc)
		   echo $signature
		   return 0
		end
	end
	return 1
end

function __fish_busctl_objects
	set -l mode
    if __fish_contains_opt user
		set mode "--user"
	else
		set mode "--system"
	end
	set -l busname (__fish_busctl_has_busname)
    command busctl $mode tree --list --no-legend --no-pager $busname ^/dev/null | while read a b; echo $a; end
end

function __fish_busctl_interfaces
	set -l mode
    if __fish_contains_opt user
		set mode "--user"
	else
		set mode "--system"
	end
	set -l busname (__fish_busctl_has_busname)
	set -l object (__fish_busctl_has_object)
    command busctl $mode introspect --list --no-legend --no-pager $busname $object ^/dev/null | while read a b; echo $a; end
end
	
function __fish_busctl_members
	set -l mode
    if __fish_contains_opt user
		set mode "--user"
	else
		set mode "--system"
	end
	set -l busname (__fish_busctl_has_busname)
	set -l object  (__fish_busctl_has_object)
	set -l interface (__fish_busctl_has_interface)
	command busctl $mode introspect --list --no-legend --no-pager $busname $object $interface ^/dev/null | grep "$argv" | while read a b; echo $a; end | sed -e "s/^\.//"
end

function __fish_busctl_methods
	__fish_busctl_members method
end

function __fish_busctl_properties
	__fish_busctl_members property
end

function __fish_busctl_signals
	__fish_busctl_members signal
end

function __fish_busctl_signature
	set -l mode
    if __fish_contains_opt user
		set mode "--user"
	else
		set mode "--system"
	end
	set -l busname (__fish_busctl_has_busname)
	set -l object  (__fish_busctl_has_object)
	set -l interface (__fish_busctl_has_interface)
	set -l member (__fish_busctl_has_member)
	command busctl $mode introspect --list --no-legend --no-pager $busname $object $interface ^/dev/null | grep "^.$member " | while read a b c d; echo $c; end
end

### Commands
set -l commands list status monitor capture tree introspect call get-property set-property help

complete -f -e -c busctl
complete -A -f -c busctl -n "not __fish_seen_subcommand_from $commands" -a "$commands"

### Arguments to commands
# "status" only takes a single service as argument
complete -f -c busctl -n "__fish_seen_subcommand_from status; and not __fish_busctl_has_busname" -a "(__fish_busctl_busnames)"

# These take multiple services
complete -f -c busctl -n "__fish_seen_subcommand_from monitor capture tree" -a "(__fish_busctl_busnames)"

# These take "service object interface" (and then maybe something else)
set -l service_object_interface_commands introspect call get-property set-property
complete -f -c busctl -n "__fish_seen_subcommand_from $service_object_interface_commands; and not __fish_busctl_has_busname" -a "(__fish_busctl_busnames)"
complete -f -c busctl -n "__fish_seen_subcommand_from $service_object_interface_commands; and __fish_busctl_has_busname; and not __fish_busctl_has_object" -a "(__fish_busctl_objects)"
# Having an object implies having a busname, so we only need to check has_object; and not has_interface
complete -f -c busctl -n "__fish_seen_subcommand_from $service_object_interface_commands; and __fish_busctl_has_object; and not __fish_busctl_has_interface" -a "(__fish_busctl_interfaces)"

# Call takes service object interface method signature arguments
# We can't complete the arguments (or we'd need to parse the signature)
complete -f -c busctl -n "__fish_seen_subcommand_from call; and __fish_busctl_has_interface; and not __fish_busctl_has_method" -a "(__fish_busctl_methods)"
complete -f -c busctl -n "__fish_seen_subcommand_from call; and __fish_busctl_has_method; and not __fish_busctl_has_signature" -a "(__fish_busctl_signature)"

complete -f -c busctl -n "__fish_seen_subcommand_from get-property; and __fish_busctl_has_interface" -a "(__fish_busctl_properties)"
complete -f -c busctl -n "__fish_seen_subcommand_from set-property; and __fish_busctl_has_interface; and not __fish_busctl_has_property" -a "(__fish_busctl_properties)"
complete -f -c busctl -n "__fish_seen_subcommand_from set-property; and __fish_busctl_has_property; and not __fish_busctl_has_signature" -a "(__fish_busctl_signature)"


# Flags
# These are incomplete as I have no idea how to complete --address= or --match=
complete -f -c busctl -n "__fish_seen_subcommand_from call get-property" -l quiet
complete -f -c busctl -n "__fish_seen_subcommand_from call get-property" -l verbose
complete -f -c busctl -n "__fish_seen_subcommand_from tree introspect" -l list
complete -f -c busctl -n "__fish_seen_subcommand_from call" -l expect-reply= -a "yes no"
complete -f -c busctl -n "__fish_seen_subcommand_from call" -l auto-start= -a "yes no"
complete -f -c busctl -n "__fish_seen_subcommand_from call" -l allow-interactive-authorization= -a "yes no"
complete -f -c busctl -n "__fish_seen_subcommand_from call" -l timeout= -a "(seq 0 100)"
complete -f -c busctl -n "__fish_seen_subcommand_from list" -l unique
complete -f -c busctl -n "__fish_seen_subcommand_from list" -l acquired
complete -f -c busctl -n "__fish_seen_subcommand_from list" -l activatable
complete -f -c busctl -n "__fish_seen_subcommand_from list" -l show-machine
complete -f -c busctl -n "__fish_seen_subcommand_from list status" -l augment-creds= -a "yes no"
complete -f -c busctl -l user
complete -f -c busctl -l system
complete -f -c busctl -s H -l host= -a "(__fish_print_hostnames)"
complete -f -c busctl -s M -l machine= -a "(__fish_systemd_machines)"
complete -f -c busctl -l no-pager
complete -f -c busctl -l no-legend
complete -f -c busctl -s h -l help
complete -f -c busctl -l version
