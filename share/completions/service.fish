# Fist argument is the names of the service, i.e. a file in /etc/init.d
complete -c service -n "__fish_is_first_token" -xa "(__fish_print_service_names)" -d "Service name"

#The second argument is what action to take with the service
function __fish_complete_static_service_actions
	complete -c service -n "not __fish_is_first_token" -xa "(eval echo -- $service_commands)"
end

set -l service_commands

# as found in __fish_print_service_names.fish
if test -d /run/systemd/system # Systemd systems
	set service_commands start stop restart status enable disable
	__fish_complete_static_service_actions
else if type -f rc-service 2>/dev/null # OpenRC (Gentoo)
	set service_commands start stop restart
	__fish_complete_static_service_actions
else if test -d /etc/init.d # SysV on Debian and other linuxen
	set service_commands start stop "--full-restart"
	__fish_complete_static_service_actions
else # FreeBSD
	# Use the output of `service -v foo` to retrieve the list of service-specific verbs
	# We can safely use `sed` here because this is platform-specific
	complete -c service -n "not __fish_is_first_token" -xa "(set -l service_name (commandline --tokenize --cut-at-cursor)[-1]; eval printf '%s\n' (service \$service_name -v 2>| sed -rn 's/Usage.*\[/\{,/;s/\]|\)/\}/g;s/\|/,/g;s/\(/{/gp'))"
end
