set -l commands status set-hostname set-icon-name set-chassis set-deployment set-location

complete -c hostnamectl -f
complete -c hostnamectl -n "not __fish_seen_subcommand_from $commands" -a status -d 'Show current hostname settings'
complete -c hostnamectl -n "not __fish_seen_subcommand_from $commands" -a set-hostname -d 'Set system hostname'
complete -c hostnamectl -n "not __fish_seen_subcommand_from $commands" -a set-icon-name -d 'Set icon name for host'
complete -c hostnamectl -n "not __fish_seen_subcommand_from $commands" -a set-chassis -d 'Set chassis type for host'
complete -c hostnamectl -n "not __fish_seen_subcommand_from $commands" -a set-deployment -d 'Set deployment environment for host'
complete -c hostnamectl -n "not __fish_seen_subcommand_from $commands" -a set-location -d 'Set location for host'

complete -c hostnamectl -s h -l help -d 'Show this help'
complete -c hostnamectl -l version -d 'Show package version'
complete -c hostnamectl -l no-ask-password -d 'Do not prompt for password'
complete -c hostnamectl -s H -l host -r -d 'Operate on remote HOST'
complete -c hostnamectl -s M -l machine -r -d 'Operate on local CONTAINER'
complete -c hostnamectl -l transient -d 'Only set transient hostname'
complete -c hostnamectl -l static -d 'Only set static hostname'
complete -c hostnamectl -l pretty -d 'Only set pretty hostname'
