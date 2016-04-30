set -l cmds list store restore stop-all start stop restart switch-to status enable disable reenable is-enabled edit

function __fish_netctl_get_profiles
  command netctl list | sed -e 's/^[ \t*]*//'
end

complete -f -c netctl -l help -d 'Display help'
complete -f -c netctl -l version -d 'Display version'
complete -f -c netctl -n "not __fish_seen_subcommand_from $cmds" -a list -d 'List available profiles'
complete -f -c netctl -n "not __fish_seen_subcommand_from $cmds" -a store -d 'Save which profiles are active'
complete -f -c netctl -n "not __fish_seen_subcommand_from $cmds" -a restore -d 'Load saved profiles'
complete -f -c netctl -n "not __fish_seen_subcommand_from $cmds" -a stop-all -d 'Stops all profiles'
complete -f -c netctl -n "not __fish_seen_subcommand_from $cmds" -a start -d 'Start a profile'
complete -f -c netctl -n "not __fish_seen_subcommand_from $cmds" -a stop -d 'Stop a profile'
complete -f -c netctl -n "not __fish_seen_subcommand_from $cmds" -a restart -d 'Restart a profile'
complete -f -c netctl -n "not __fish_seen_subcommand_from $cmds" -a switch-to -d 'Switch to a profile'
complete -f -c netctl -n "not __fish_seen_subcommand_from $cmds" -a status -d 'Show runtime status of a profile'
complete -f -c netctl -n "not __fish_seen_subcommand_from $cmds" -a enable -d 'Enable the systemd unit for a profile'
complete -f -c netctl -n "not __fish_seen_subcommand_from $cmds" -a disable -d 'Disable the systemd unit for a profile'
complete -f -c netctl -n "not __fish_seen_subcommand_from $cmds" -a reenable -d 'Reenable the systemd unit for a profile'
complete -f -c netctl -n "not __fish_seen_subcommand_from $cmds" -a is-enabled -d 'Check whether the unit is enabled'
complete -f -c netctl -n "not __fish_seen_subcommand_from $cmds" -a edit -d 'Open the specified profile in an editor'
complete -f -c netctl -n "__fish_seen_subcommand_from start stop restart switch-to status enable disable reenable is-enabled edit" -a '(__fish_netctl_get_profiles)' -d 'Profile'
