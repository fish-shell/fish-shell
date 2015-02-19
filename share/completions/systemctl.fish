set -l commands list-units list-sockets start stop reload restart try-restart reload-or-restart reload-or-try-restart \
	isolate kill is-active is-failed status show get-cgroup-attr set-cgroup-attr unset-cgroup-attr set-cgroup help \
	reset-failed list-unit-files enable disable is-enabled reenable preset mask unmask link load list-jobs cancel dump \
	list-dependencies snapshot delete daemon-reload daemon-reexec show-environment set-environment unset-environment \
	default rescue emergency halt poweroff reboot kexec exit suspend hibernate hybrid-sleep switch-root

function __fish_systemd_properties
	if type -q /usr/lib/systemd/systemd
		set IFS "="
		/usr/lib/systemd/systemd --dump-configuration-items | while read key value
		if not test -z $value
			echo $key
		end
		end
	end
end

complete -f -e -c systemctl
# All systemctl commands
complete -f -c systemctl -n "not __fish_seen_subcommand_from $commands" -a "$commands"

#### Units commands
complete -f -c systemctl -n "not __fish_seen_subcommand_from $commands" -a start -d 'Start one or more units'
complete -f -c systemctl -n "not __fish_seen_subcommand_from $commands" -a stop -d 'Stop one or more units'
complete -f -c systemctl -n "not __fish_seen_subcommand_from $commands" -a restart -d 'Restart one or more units'
complete -f -c systemctl -n "not __fish_seen_subcommand_from $commands" -a status -d 'Runtime status about one or more units'
complete -f -c systemctl -n "not __fish_seen_subcommand_from $commands" -a enable -d 'Enable one or more units'
complete -f -c systemctl -n "not __fish_seen_subcommand_from $commands" -a disable -d 'Disable one or more units'
for command in start stop restart try-restart reload-or-restart reload-or-try-restart is-active is-failed is-enabled reenable mask loaded link list-dependencies show status
	complete -f -c systemctl -n "__fish_seen_subcommand_from $command" -a '(__fish_systemctl_services)' -d 'Service'
	complete -f -c systemctl -n "__fish_seen_subcommand_from $command" -a '(__fish_systemctl_sockets)' -d 'Socket'
	complete -f -c systemctl -n "__fish_seen_subcommand_from $command" -a '(__fish_systemctl_mounts)' -d 'Mount'
	complete -f -c systemctl -n "__fish_seen_subcommand_from $command" -a '(__fish_systemctl_service_paths)' -d 'Path'
	complete -f -c systemctl -n "__fish_seen_subcommand_from $command" -a '(__fish_systemctl_targets)' -d 'Target'
	complete -f -c systemctl -n "__fish_seen_subcommand_from $command" -a '(__fish_systemctl_automounts)' -d 'Automount'
	complete -f -c systemctl -n "__fish_seen_subcommand_from $command" -a '(__fish_systemctl_timers)' -d 'Timer'
end

# Enable/Disable: Only show units with matching state
complete -f -c systemctl -n "__fish_seen_subcommand_from enable" -a '(__fish_systemctl_services --state=disabled)' -d 'Service'
complete -f -c systemctl -n "__fish_seen_subcommand_from enable" -a '(__fish_systemctl_sockets --state=disabled)' -d 'Socket'
complete -f -c systemctl -n "__fish_seen_subcommand_from enable" -a '(__fish_systemctl_timers --state=disabled)' -d 'Timer'
complete -f -c systemctl -n "__fish_seen_subcommand_from enable" -a '(__fish_systemctl_service_paths --state=disabled)' -d 'Path'

complete -f -c systemctl -n "__fish_seen_subcommand_from disable" -a '(__fish_systemctl_services --state=enabled)' -d 'Service'
complete -f -c systemctl -n "__fish_seen_subcommand_from disable" -a '(__fish_systemctl_sockets --state=enabled)' -d 'Socket'
complete -f -c systemctl -n "__fish_seen_subcommand_from disable" -a '(__fish_systemctl_timers --state=enabled)' -d 'Timer'
complete -f -c systemctl -n "__fish_seen_subcommand_from disable" -a '(__fish_systemctl_service_paths --state=enabled)' -d 'Path'

# These are useless for the other commands
# .device in particular creates too much noise
complete -f -c systemctl -n "__fish_seen_subcommand_from status" -a '(__fish_systemctl_devices)' -d 'Device'
complete -f -c systemctl -n "__fish_seen_subcommand_from status" -a '(__fish_systemctl_slices)' -d 'Slice'
complete -f -c systemctl -n "__fish_seen_subcommand_from status" -a '(__fish_systemctl_scopes)' -d 'Scope'
complete -f -c systemctl -n "__fish_seen_subcommand_from status" -a '(__fish_systemctl_swaps)' -d 'Swap'

complete -f -c systemctl -n "not __fish_seen_subcommand_from $commands" -a isolate -d 'Disable one or more units'
complete -f -c systemctl -n "__fish_seen_subcommand_from isolate" -a '(__fish_systemctl_targets)' -d 'Target'
complete -f -c systemctl -n "__fish_seen_subcommand_from isolate" -a '(__fish_systemctl_snapshots)' -d 'Snapshot'

complete -f -c systemctl -n "__fish_seen_subcommand_from set-default" -a '(__fish_systemctl_targets)' -d 'Target'
complete -f -c systemctl -n "__fish_seen_subcommand_from set-default" -a '(__fish_systemctl_services)' -d 'Service'

complete -f -c systemctl -s t -l type -d 'List of unit types' -xa 'help service, mount, socket, target,'
complete -f -c systemctl -l state -d 'List of unit states' -xa 'LOAD, SUB, ACTIVE,'
complete -f -c systemctl -s p -l property -d 'Properties displayed in the "show" command' -a '(__fish_systemd_properties)'
complete -f -c systemctl -s a -l all -d 'Show all units or properties'
complete -f -c systemctl -s r -l recursive -d 'Show also units of local containers'
complete -f -c systemctl -l reverse -d 'Show reverse dependencies between units'
complete -f -c systemctl -l after -d 'Show units ordered before specified unit'
complete -f -c systemctl -l before -d 'Show units ordered after specified unit'
complete -f -c systemctl -s l -l full -d 'Do not ellipsize anything'
complete -f -c systemctl -l show-types -d 'Show the type of the socket'
complete -f -c systemctl -l job-mode -d 'How to deal with queued jobs' -xa 'fail replace replace-irreversibly isolate ignore-dependencies ignore-requirements flush'
complete -f -c systemctl -s i -l ignore-inhibitors -d 'Ignore inhibitor locks on shutdown or sleep'
complete -f -c systemctl -s q -l quiet -d 'Suppress output to STDOUT'
complete -f -c systemctl -l no-block -d 'Do not wait for the requested operation to finish'
complete -f -c systemctl -l no-legend -d 'Do not print header and footer'
complete -f -c systemctl -l user -d 'Talk to the service manager of the calling user'
complete -f -c systemctl -l system -d 'Talk to the service manager of the system.'
complete -f -c systemctl -l no-wall -d 'Do not send wall message before halt'
complete -f -c systemctl -l global -d 'Enable or disable for all users'
complete -f -c systemctl -l no-reload -d 'Do not reload daemon configuration'
complete -f -c systemctl -l no-ask-password -d 'Disable asking for password'
complete -f -c systemctl -l kill-who -d 'Send signal to which process' -xa 'main control all'
complete -f -c systemctl -s s -l signal -d 'Which signal to send' -xa 'SIGTERM SIGINT SIGSTOP SIGKILL SIGHUP SIGCONT'
complete -f -c systemctl -s f -l force -d 'Overwrite conflicting existing symlinks'
complete -f -c systemctl -l root -d 'Use alternative root path'
complete -f -c systemctl -l runtime -d 'Make changes only temporarily'
complete -f -c systemctl -s n -l lines -d 'Number of journal lines to show'
complete -f -c systemctl -s o -l output -d 'Control journal formatting' -xa 'short short-monotonic verbose export json json-pretty json-sse cat'
complete -f -c systemctl -l plain -d 'list-dependencies flat, not as tree'
complete -f -c systemctl -s H -l host -d 'Execute the operation remotely'
complete -f -c systemctl -s M -l machine -d 'Execute operation locally'
complete -f -c systemctl -s h -l help -d 'Print a short help and exit'
complete -f -c systemctl -l version -d 'Print a short version and exit'
complete -f -c systemctl -l no-pager -d 'Do not pipe output into a pager'
