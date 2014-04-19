
function __fish_systemctl_using_command
  set cmd (commandline -opc)
  if [ (count $cmd) -gt 1 ]
    if [ $argv[1] = $cmd[2] ]
      return 0
    end
  end
  return 1
end

# All systemctl commands
complete -f -c systemctl -n "test (count (commandline -poc)) = 1" -a 'list-units list-sockets start stop reload restart try-restart reload-or-restart reload-or-try-restart isolate kill is-active is-failed status show get-cgroup-attr set-cgroup-attr unset-cgroup-attr set-cgroup help reset-failed list-unit-files enable disable is-enabled reenable preset mask unmask link load list-jobs cancel dump list-dependencies snapshot delete daemon-reload daemon-reexec show show-environment set-environment unset-environment default rescue emergency halt poweroff reboot kexec exit suspend hibernate hybrid-sleep switch-root'

#### Units commands

# Start
complete -f -c systemctl -n "test (count (commandline -poc)) = 1" -a start -d 'Start one or more units'
complete -f -c systemctl -n '__fish_systemctl_using_command start' -a '(__fish_systemctl_services)' -d 'Service'
complete -f -c systemctl -n '__fish_systemctl_using_command start' -a '(__fish_systemctl_sockets)' -d 'Socket'
complete -f -c systemctl -n '__fish_systemctl_using_command start' -a '(__fish_systemctl_mounts)' -d 'Mount'
complete -f -c systemctl -n '__fish_systemctl_using_command start' -a '(__fish_systemctl_service_paths)' -d 'Path'

# Stop
complete -f -c systemctl -n "test (count (commandline -poc)) = 1" -a stop -d 'Stop one or more units'
complete -f -c systemctl -n '__fish_systemctl_using_command stop' -a '(__fish_systemctl_services)' -d 'Service'
complete -f -c systemctl -n '__fish_systemctl_using_command stop' -a '(__fish_systemctl_sockets)' -d 'Socket'
complete -f -c systemctl -n '__fish_systemctl_using_command stop' -a '(__fish_systemctl_mounts)' -d 'Mount'
complete -f -c systemctl -n '__fish_systemctl_using_command stop' -a '(__fish_systemctl_service_paths)' -d 'Path'

# Restart
complete -f -c systemctl -n "test (count (commandline -poc)) = 1" -a restart -d 'Restart one or more units'
complete -f -c systemctl -n '__fish_systemctl_using_command restart' -a '(__fish_systemctl_services)' -d 'Service'
complete -f -c systemctl -n '__fish_systemctl_using_command restart' -a '(__fish_systemctl_sockets)' -d 'Socket'
complete -f -c systemctl -n '__fish_systemctl_using_command restart' -a '(__fish_systemctl_mounts)' -d 'Mount'
complete -f -c systemctl -n '__fish_systemctl_using_command restart' -a '(__fish_systemctl_service_paths)' -d 'Path'

# Status
complete -f -c systemctl -n "test (count (commandline -poc)) = 1" -a status -d 'Runtime status about one or more units'
complete -f -c systemctl -n '__fish_systemctl_using_command status' -a '(__fish_systemctl_services)' -d 'Service'
complete -f -c systemctl -n '__fish_systemctl_using_command status' -a '(__fish_systemctl_sockets)' -d 'Socket'
complete -f -c systemctl -n '__fish_systemctl_using_command status' -a '(__fish_systemctl_mounts)' -d 'Mount'
complete -f -c systemctl -n '__fish_systemctl_using_command status' -a '(__fish_systemctl_service_paths)' -d 'Path'

# Enable
complete -f -c systemctl -n "test (count (commandline -poc)) = 1" -a enable -d 'Enable one or more units'
complete -f -c systemctl -n '__fish_systemctl_using_command enable' -a '(__fish_systemctl_services)' -d 'Service'
complete -f -c systemctl -n '__fish_systemctl_using_command enable' -a '(__fish_systemctl_sockets)' -d 'Socket'
complete -f -c systemctl -n '__fish_systemctl_using_command enable' -a '(__fish_systemctl_mounts)' -d 'Mount'
complete -f -c systemctl -n '__fish_systemctl_using_command enable' -a '(__fish_systemctl_service_paths)' -d 'Path'

# Disable
complete -f -c systemctl -n "test (count (commandline -poc)) = 1" -a disable -d 'Disable one or more units'
complete -f -c systemctl -n '__fish_systemctl_using_command disable' -a '(__fish_systemctl_services)' -d 'Service'
complete -f -c systemctl -n '__fish_systemctl_using_command disable' -a '(__fish_systemctl_sockets)' -d 'Socket'
complete -f -c systemctl -n '__fish_systemctl_using_command disable' -a '(__fish_systemctl_mounts)' -d 'Mount'
complete -f -c systemctl -n '__fish_systemctl_using_command disable' -a '(__fish_systemctl_service_paths)' -d 'Path'

complete -c systemctl -s t -l type -d 'List of unit types' -xa 'help service, mount, socket, target,'
complete -c systemctl -l state -d 'List of unit states' -xa 'LOAD, SUB, ACTIVE,'
complete -c systemctl -s p -l property -d 'Properties displayed in the "show" command'
complete -c systemctl -s a -l all -d 'Show all units or properties'
complete -c systemctl -s r -l recursive -d 'Show also units of local containers'
complete -c systemctl -l reverse -d 'Show reverse dependencies between units'
complete -c systemctl -l after -d 'Show units ordered before specified unit'
complete -c systemctl -l before -d 'Show units ordered after specified unit'
complete -c systemctl -s l -l full -d 'Do not ellipsize anything'
complete -c systemctl -l show-types -d 'Show the type of the socket'
complete -c systemctl -l job-mode -d 'How to deal with queued jobs' -xa 'fail replace replace-irreversibly isolate ignore-dependencies ignore-requirements flush'
complete -c systemctl -s i -l ignore-inhibitors -d 'Ignore inhibitor locks on shutdown or sleep'
complete -c systemctl -s q -l quiet -d 'Suppress output to STDOUT'
complete -c systemctl -l no-block -d 'Do not wait for the requested operation to finish'
complete -c systemctl -l no-legend -d 'Do not print header and footer'
complete -c systemctl -l user -d 'Talk to the service manager of the calling user'
complete -c systemctl -l system -d 'Talk to the service manager of the system.'
complete -c systemctl -l no-wall -d 'Do not send wall message before halt'
complete -c systemctl -l global -d 'Enable or disable for all users'
complete -c systemctl -l no-reload -d 'Do not reload daemon configuration'
complete -c systemctl -l no-ask-password -d 'Disable asking for password'
complete -c systemctl -l kill-who -d 'Send signal to which process' -xa 'main control all'
complete -c systemctl -s s -l signal -d 'Which signal to send' -xa 'SIGTERM SIGINT SIGSTOP SIGKILL SIGHUP SIGCONT'
complete -c systemctl -s f -l force -d 'Overwrite conflicting existing symlinks'
complete -c systemctl -l root -d 'Use alternative root path'
complete -c systemctl -l runtime -d 'Make changes only temporarily'
complete -c systemctl -s n -l lines -d 'Number of journal lines to show'
complete -c systemctl -s o -l output -d 'Control journal formatting' -xa 'short short-monotonic verbose export json json-pretty json-sse cat'
complete -c systemctl -l plain -d 'list-dependencies flat, not as tree'
complete -c systemctl -s H -l host -d 'Execute the operation remotely'
complete -c systemctl -s M -l machine -d 'Execute operation locally'
complete -c systemctl -s h -l help -d 'Print a short help and exit'
complete -c systemctl -l version -d 'Print a short version and exit'
complete -c systemctl -l no-pager -d 'Do not pipe output into a pager'
