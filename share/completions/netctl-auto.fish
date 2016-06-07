set -l cmds list store restore stop-all start stop restart switch-to status enable disable reenable is-enabled edit

function __fish_print_netctl-auto_profile
    for line in (netctl-auto list)
        set -l profile (string sub -s 3 $line)
        if [ (string sub -l 1 $line) = "*" ]
            printf "%s\t%s\n" $profile "Active profile"
        else
            printf "%s\t%s\n" $profile "Profile"
        end
    end
end

complete -c netctl-auto -n "not __fish_seen_subcommand_from $cmds" -l help -d "Show help"
complete -c netctl-auto -n "not __fish_seen_subcommand_from $cmds" -l version -d "Show version"

complete -c netctl-auto -n "not __fish_seen_subcommand_from $cmds" -a list -f -d "List all available profiles"
complete -c netctl-auto -n "not __fish_seen_subcommand_from $cmds" -a store -f -d "Save the profiles that are currently active"
complete -c netctl-auto -n "not __fish_seen_subcommand_from $cmds" -a restore -f -d "Load the profiles that were active during the last invocation of 'store'"
complete -c netctl-auto -n "not __fish_seen_subcommand_from $cmds" -a stop-all -f -d "Stop all active network profiles"
complete -c netctl-auto -n "not __fish_seen_subcommand_from $cmds" -a start -f -d "Start network profile"
complete -c netctl-auto -n "not __fish_seen_subcommand_from $cmds" -a stop -f -d "Stop network profile"
complete -c netctl-auto -n "not __fish_seen_subcommand_from $cmds" -a restart -f -d "Restart network profile"
complete -c netctl-auto -n "not __fish_seen_subcommand_from $cmds" -a switch-to -f -d "Start profile after stopping the others that refer to the same interface"
complete -c netctl-auto -n "not __fish_seen_subcommand_from $cmds" -a status -f -d "Show terse runtime status inforamation about a profile"
complete -c netctl-auto -n "not __fish_seen_subcommand_from $cmds" -a enable -f -d "Enable the systemd unit for the specified profile"
complete -c netctl-auto -n "not __fish_seen_subcommand_from $cmds" -a disable -f -d "Disable the systemd unit for the specified profile"
complete -c netctl-auto -n "not __fish_seen_subcommand_from $cmds" -a reenable -f -d "Renable the systemd unit for the specified profile"
complete -c netctl-auto -n "not __fish_seen_subcommand_from $cmds" -a is-enabled -f -d "Check whether the systemd unit for the specified profile is enabled"
complete -c netctl-auto -n "not __fish_seen_subcommand_from $cmds" -a edit -f -d "Open the file of the specified profile in an editor"

complete -c netctl-auto -n "__fish_seen_subcommand_from start stop restart switch-to status enable disable reenable is-enabled edit" -f -a "(__fish_print_netctl-auto_profile)" -d "Profile"

