function __fish_netctl_needs_command
  set cmd (commandline -opc)
  if [ (count $cmd) -eq 1 ]
    return 0
  end
  return 1
end

function __fish_netctl_using_command
  set cmd (commandline -opc)
  if [ (count $cmd) -gt 1 ]
    if [ $argv[1] = $cmd[2] ]
      return 0
    end
  end
  return 1
end

function __fish_netctl_get_profiles
  command netctl list | sed -e 's/^[ \t*]*//'
end

complete -f -c netctl -l help -d 'Display help'
complete -f -c netctl -l version -d 'Display version'
complete -f -c netctl -n '__fish_netctl_needs_command' -a list -d 'List available profiles'
complete -f -c netctl -n '__fish_netctl_needs_command' -a store -d 'Save which profiles are active'
complete -f -c netctl -n '__fish_netctl_needs_command' -a restore -d 'Load saved profiles'
complete -f -c netctl -n '__fish_netctl_needs_command' -a stop-all -d 'Stops all profiles'

complete -f -c netctl -n '__fish_netctl_needs_command' -a start -d 'Start a profile'
complete -f -c netctl -n '__fish_netctl_using_command start' -a '(__fish_netctl_get_profiles)' -d 'Profile'

complete -f -c netctl -n '__fish_netctl_needs_command' -a stop -d 'Stop a profile'
complete -f -c netctl -n '__fish_netctl_using_command stop' -a '(__fish_netctl_get_profiles)' -d 'Profile'

complete -f -c netctl -n '__fish_netctl_needs_command' -a restart -d 'Restart a profile'
complete -f -c netctl -n '__fish_netctl_using_command restart' -a '(__fish_netctl_get_profiles)' -d 'Profile'

complete -f -c netctl -n '__fish_netctl_needs_command' -a switch-to -d 'Switch to a profile'
complete -f -c netctl -n '__fish_netctl_using_command switch-to' -a '(__fish_netctl_get_profiles)' -d 'Profile'

complete -f -c netctl -n '__fish_netctl_needs_command' -a status -d 'Show runtime status of a profile'
complete -f -c netctl -n '__fish_netctl_using_command status' -a '(__fish_netctl_get_profiles)' -d 'Profile'

complete -f -c netctl -n '__fish_netctl_needs_command' -a enable -d 'Enable the systemd unit for a profile'
complete -f -c netctl -n '__fish_netctl_using_command enable' -a '(__fish_netctl_get_profiles)' -d 'Profile'

complete -f -c netctl -n '__fish_netctl_needs_command' -a disable -d 'Disable the systemd unit for a profile'
complete -f -c netctl -n '__fish_netctl_using_command disable' -a '(__fish_netctl_get_profiles)' -d 'Profile'

complete -f -c netctl -n '__fish_netctl_needs_command' -a reenable -d 'Reenable the systemd unit for a profile'
complete -f -c netctl -n '__fish_netctl_using_command reenable' -a '(__fish_netctl_get_profiles)' -d 'Profile'
