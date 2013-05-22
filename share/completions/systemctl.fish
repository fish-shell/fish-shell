function __fish_systemctl_services
    command find /etc/systemd/system -type f -name '*.service' -printf '%f\n'
    command find /usr/lib/systemd/system -type f -name '*.service' -printf '%f\n'
end

function __fish_systemctl_sockets
    command find /etc/systemd/system -type f -name '*.socket' -printf '%f\n'
    command find /usr/lib/systemd/system -type f -name '*.socket' -printf '%f\n'
end

function __fish_systemctl_mounts
    command find /etc/systemd/system -type f -name '*.mount' -printf '%f\n'
    command find /usr/lib/systemd/system -type f -name '*.mount' -printf '%f\n'
end

function __fish_systemctl_service_paths
    command find /etc/systemd/system -type f -name '*.path' -printf '%f\n'
    command find /usr/lib/systemd/system -type f -name '*.path' -printf '%f\n'
end

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
complete -f -c systemctl -n "test (count (commandline -poc)) = 1" -a 'list-units list-sockets start stop reload restart try-restart reload-or-restart reload-or-try-restart isolate kill is-active is-failed status show get-cgroup-attr set-cgroup-attr unset-cgroup-attr set-cgroup help reset-failed list-unit-files enable disable is-enabled reenable preset mask unmask link load list-jobs cancel dump list-dependencies snapshot delete daemon-reload daemon-reexec show-environment set-environment unset-environment default rescue emergency halt poweroff reboot kexec exit suspend hibernate hybrid-sleep switch-root'

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
