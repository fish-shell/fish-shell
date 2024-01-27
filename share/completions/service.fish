# First argument is the names of the service, i.e. a file in /etc/init.d
complete -c service -n "__fish_is_nth_token 1" -xa "(__fish_print_service_names)" -d Service

# as found in __fish_print_service_names.fish
if test -d /run/systemd/system # Systemd systems
    complete -c service -n 'not __fish_is_nth_token 1' -xa "start stop restart status enable disable"
else if command -sq rc-service # OpenRC (Gentoo)
    complete -c service -n 'not __fish_is_nth_token 1' -xa "start stop restart"
else if test -d /etc/init.d # SysV on Debian and other linuxen
    complete -c service -n 'not __fish_is_nth_token 1' -xa "start stop --full-restart"
else # FreeBSD
    # Use the output of `service -v foo` to retrieve the list of service-specific verbs
    complete -c service -n 'not __fish_is_nth_token 1' -xa "(__fish_complete_freebsd_service_actions)"
end

function __fish_complete_freebsd_service_actions
    # Use the output of `service -v foo` to retrieve the list of service-specific verbs
    # Output takes the form "[prefix1 prefix2 ..](cmd1 cmd2 cmd3)" where any combination
    # of zero or one prefixe(s) and any one command is a valid verb.
    set -l service_name (commandline --tokens-expanded --cut-at-cursor)[-1]
    set -l results (service $service_name -v 2>| string match -r '\\[(.*)\\]\\((.*)\\)')
    set -l prefixes "" (string split '|' -- $results[2])
    set -l commands (string split '|' -- $results[3])
    printf '%s\n' $prefixes$commands
end
