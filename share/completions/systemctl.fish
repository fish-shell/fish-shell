set -l systemd_version (systemctl --version | string match "systemd*" | string replace -r "\D*(\d+)\D.*"  '$1')
set -l commands list-units list-sockets start stop reload restart try-restart reload-or-restart reload-or-try-restart \
    isolate kill is-active is-failed status show get-cgroup-attr set-cgroup-attr unset-cgroup-attr set-cgroup help \
    reset-failed list-unit-files enable disable is-enabled reenable preset mask unmask link load list-jobs cancel dump \
    list-dependencies snapshot delete daemon-reload daemon-reexec show-environment set-environment unset-environment \
    default rescue emergency halt poweroff reboot kexec exit suspend hibernate hybrid-sleep switch-root list-timers \
    set-property
if test $systemd_version -gt 208 2>/dev/null
    set commands $commands cat
    if test $systemd_version -gt 217 2>/dev/null
        set commands $commands edit
    end
end
set -l types services sockets mounts service_paths targets automounts timers

function __fish_systemd_properties
    # We need to call the main systemd binary (the thing that is run as PID1).
    # Unfortunately, it's usually not in $PATH.
    if test -f /usr/lib/systemd/systemd
        /usr/lib/systemd/systemd --dump-configuration-items | string replace -rf '(.+)=(.+)$' '$1\t$2'
    else if test -f /lib/systemd/systemd # Debian has not merged /lib and /usr/lib
        /lib/systemd/systemd --dump-configuration-items | string replace -rf '(.+)=(.+)$' '$1\t$2'
    end
end

# All systemctl commands
complete -f -c systemctl -n "not __fish_seen_subcommand_from $commands" -a "$commands"

#### Units commands
complete -f -c systemctl -n "not __fish_seen_subcommand_from $commands" -a start -d 'Start one or more units'
complete -f -c systemctl -n "not __fish_seen_subcommand_from $commands" -a stop -d 'Stop one or more units'
complete -f -c systemctl -n "not __fish_seen_subcommand_from $commands" -a restart -d 'Restart one or more units'
complete -f -c systemctl -n "not __fish_seen_subcommand_from $commands" -a status -d 'Runtime status about one or more units'
complete -f -c systemctl -n "not __fish_seen_subcommand_from $commands" -a enable -d 'Enable one or more units'
complete -f -c systemctl -n "not __fish_seen_subcommand_from $commands" -a disable -d 'Disable one or more units'
complete -f -c systemctl -n "not __fish_seen_subcommand_from $commands" -a isolate -d 'Start a unit and dependencies and disable all others'
complete -f -c systemctl -n "not __fish_seen_subcommand_from $commands" -a set-default -d 'Set the default target to boot into'
complete -f -c systemctl -n "not __fish_seen_subcommand_from $commands" -a set-property -d 'Sets one or more properties of a unit'

# Command completion done via argparse.
complete -c systemctl -a '(__fish_systemctl)' -f

# These "--x=help" outputs always have lines like "Available unit types:". We use the fact that they end in a ":" to filter them out.
complete -f -c systemctl -s t -l type -d 'List of unit types' -xa '(systemctl --type=help --no-legend --no-pager | string match -v "*:")'
complete -f -c systemctl -l state -d 'List of unit states' -xa '(systemctl --state=help --no-legend --no-pager | string match -v "*:")'
complete -f -c systemctl -s p -l property -a '(__fish_systemd_properties)'
complete -f -c systemctl -s a -l all -d 'Show all units or properties'
complete -f -c systemctl -s r -l recursive -d 'Show also units of local containers'
complete -f -c systemctl -l reverse -d 'Show reverse dependencies between units'
complete -f -c systemctl -l after -d 'Show units ordered before specified unit' -n "__fish_seen_subcommand_from list-dependencies"
complete -f -c systemctl -l before -d 'Show units ordered after specified unit' -n "__fish_seen_subcommand_from list-dependencies"
complete -f -c systemctl -s l -l full -d 'Do not ellipsize anything'
complete -f -c systemctl -l show-types -d 'Show the type of the socket'
complete -f -c systemctl -l job-mode -d 'How to deal with queued jobs' -xa 'fail replace replace-irreversibly isolate ignore-dependencies ignore-requirements flush'
complete -f -c systemctl -s i -l ignore-inhibitors -d 'Ignore inhibitor locks on shutdown or sleep'
complete -f -c systemctl -s q -l quiet -d 'Suppress output to STDOUT'
complete -f -c systemctl -l no-block -d 'Do not wait for the requested operation to finish'
complete -f -c systemctl -l no-legend -d 'Do not print header and footer'
# system and user/global are mutually exclusive
complete -f -c systemctl -l user -d 'Talk to the service manager of the calling user' -n "not __fish_contains_opt system"
complete -f -c systemctl -l system -d 'Talk to the service manager of the system.' -n "not __fish_contains_opt system global"
complete -c systemctl -l failed -d 'List units in failed state'
complete -f -c systemctl -l global -d 'Enable or disable for all users' -n "not __fish_contains_opt system"
complete -f -c systemctl -l no-wall -d 'Do not send wall message before halt'
complete -f -c systemctl -l no-reload -d 'Do not reload daemon configuration'
complete -f -c systemctl -l no-ask-password -d 'Disable asking for password'
complete -f -c systemctl -l kill-who -d 'Send signal to which process' -xa 'main control all'
complete -f -c systemctl -s s -l signal -d 'Which signal to send' -xa 'SIGTERM SIGINT SIGSTOP SIGKILL SIGHUP SIGCONT'
complete -f -c systemctl -s f -l force -d 'Overwrite conflicting existing symlinks'
# --root needs a path
complete -r -c systemctl -l root -d 'Use alternative root path'
complete -f -c systemctl -l runtime -d 'Make changes only temporarily'
complete -f -r -c systemctl -s n -l lines -d 'Number of journal lines to show' -a "(seq 1 1000)"
complete -f -c systemctl -s o -l output -d 'Control journal formatting' -xa 'short short-monotonic verbose export json json-pretty json-sse cat'
complete -f -c systemctl -l plain -d 'list-dependencies flat, not as tree'
complete -f -c systemctl -s H -l host -d 'Execute the operation on a remote host' -a "(__fish_print_hostnames)"
complete -x -c systemctl -s M -l machine -d 'Execute operation on a VM or container' -a "(__fish_systemd_machines)"
complete -f -c systemctl -s h -l help -d 'Print a short help and exit'
complete -f -c systemctl -l version -d 'Print a short version and exit'
complete -f -c systemctl -l no-pager -d 'Do not pipe output into a pager'

# New options since systemd 220
if test $systemd_version -gt 219 2>/dev/null
    complete -f -c systemctl -l firmware-setup -n "__fish_seen_subcommand_from reboot" -d "Reboot to EFI setup"
    complete -f -c systemctl -l now -n "__fish_seen_subcommand_from enable" -d "Also start unit"
    complete -f -c systemctl -l now -n "__fish_seen_subcommand_from disable mask" -d "Also stop unit"
end
