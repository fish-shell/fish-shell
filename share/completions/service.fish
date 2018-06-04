set -l service_commands

# as found in __fish_print_service_names.fish
if test -d /run/systemd/system # Systemd systems
	set service_commands start stop restart status enable disable
else if type -f rc-service 2>/dev/null # OpenRC (Gentoo)
	set service_commands start stop restart
else if test -d /etc/init.d # SysV on Debian and other linuxen
	set service_commands start stop "--full-restart"
else # FreeBSD
	set service_commands start stop start_once stop_once
end

# Fist argument is the names of the service, i.e. a file in /etc/init.d
complete -c service -n "__fish_is_first_token" -xa "(__fish_print_service_names)" -d "Service name"

#The second argument is what action to take with the service
complete -c service -n "not __fish_is_first_token" -xa "$service_commands"

