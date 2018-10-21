# First argument is the names of the service, i.e. a file in /etc/init.d
complete -c service -n "__fish_is_first_token" -xa "(__fish_print_service_names)" -d "Service"

set -l service_commands
function __fish_complete_static_service_actions
	#The second argument is what action to take with the service
	complete -c service -n "not __fish_is_first_token" -xa "$argv"
end

# as found in __fish_print_service_names.fish
if test -d /run/systemd/system # Systemd systems
	set service_commands start stop restart status enable disable
	__fish_complete_static_service_actions $service_commands
else if type -f rc-service 2>/dev/null # OpenRC (Gentoo)
	set service_commands start stop restart
	__fish_complete_static_service_actions $service_commands
else if test -d /etc/init.d # SysV on Debian and other linuxen
	set service_commands start stop "--full-restart"
	__fish_complete_static_service_actions $service_commands
else # FreeBSD
	# Use the output of `service -v foo` to retrieve the list of service-specific verbs
	# We can safely use `sed` here because this is platform-specific
	complete -c service -n "not __fish_is_first_token" -xa "(__fish_complete_freebsd_service_actions)"
end

function __fish_complete_freebsd_service_actions
	# Use the output of `service -v foo` to retrieve the list of service-specific verbs
	# Output takes the form "[prefix1 prefix2 ..](cmd1 cmd2 cmd3)" where any combination
	# of zero or one prefixe(s) and any one command is a valid verb.
	set -l service_name (commandline --tokenize --cut-at-cursor)[-1]
	set -l results (service $service_name -v 2>| string match -r '\\[(.*)\\]\\((.*)\\)')
	set -l prefixes "" (string split '|' -- $results[2])
	set -l commands (string split '|' -- $results[3])
	printf '%s\n' $prefixes$commands
end
