# Completions for networkctl (a part of systemd)

set -l command_with_devices list status lldp delete up down renew forcerenew reconfigure
set -l commands label reload $command_with_devices
set -l devices "(networkctl list --no-legend 2>/dev/null | string trim | string split -f 2 -- ' ')"

# Commands
complete -c networkctl -f
complete -c networkctl -n "not __fish_seen_subcommand_from $commands" -a list -d 'List links'
complete -c networkctl -n "not __fish_seen_subcommand_from $commands" -a status -d 'Show link status'
complete -c networkctl -n "not __fish_seen_subcommand_from $commands" -a lldp -d 'Show LLDP neighbors'
complete -c networkctl -n "not __fish_seen_subcommand_from $commands" -a label -d 'Show current address label'
complete -c networkctl -n "not __fish_seen_subcommand_from $commands" -a delete -d 'Delete virtual netdevs'
complete -c networkctl -n "not __fish_seen_subcommand_from $commands" -a up -d 'Bring devices up'
complete -c networkctl -n "not __fish_seen_subcommand_from $commands" -a down -d 'Bring devices down'
complete -c networkctl -n "not __fish_seen_subcommand_from $commands" -a renew -d 'Renew dynamic configurations'
complete -c networkctl -n "not __fish_seen_subcommand_from $commands" -a forcerenew -d 'Trigger DHCP reconfiguration of all connected clients'
complete -c networkctl -n "not __fish_seen_subcommand_from $commands" -a reconfigure -d 'Reconfigure interfaces'
complete -c networkctl -n "not __fish_seen_subcommand_from $commands" -a reload -d 'Reload .network and .netdev files'
complete -c networkctl -n "__fish_seen_subcommand_from $command_with_devices" -a "$devices"

# Options
complete -c networkctl -s h -l help -d 'Show help'
complete -c networkctl -l version -d 'Show version'
complete -c networkctl -l no-pager -d 'Do not pipe output into a pager'
complete -c networkctl -l no-legend -d 'Do not show the headers and footers'
complete -c networkctl -s a -l all -d 'Show status for all links'
complete -c networkctl -s s -l stats -d 'Show detailed link statics'
complete -c networkctl -s l -l full -d 'Do not ellipsize output'
complete -c networkctl -s n -l lines -d 'Number of journal entries to show' -xa '(seq 1 10)'
