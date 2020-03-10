set -l cmds list start stop switch-to enable disable enable-all disable-all is-active is-enabled

# Helper function that prints network profiles managed by netctl-auto.
# If no argument is given, it prints all profiles.
# Othewise, it only prints the ones given as arguments witch are either:
#    - active
#    - disabled
#    - other (meaning enabled but not active)
#
# For example, if you only want the enabled profiles, call it with
# the arguments active and other.
function __fish_print_netctl-auto_profile
    set -l show_active false
    set -l show_disabled false
    set -l show_other false

    for arg in $argv
        switch $arg
            case other
                set show_other true
            case disabled
                set show_disabled true
            case active
                set show_active true
        end
    end

    if not count $argv >/dev/null
        set show_active true
        set show_disabled true
        set show_other true
    end

    for line in (netctl-auto list)
        set -l profile (string sub -s 3 $line)
        if string match -q '\**' -- $line
            if test $show_active = true
                printf "%s\t%s\n" $profile "Active profile"
            end
        else if string match -q "!*" -- $line
            if test $show_disabled = true
                printf "%s\t%s\n" $profile "Disabled profile"
            end
        else
            if test $show_other = true
                printf "%s\t%s\n" $profile Profile
            end
        end
    end
end

complete -c netctl-auto -n "not __fish_seen_subcommand_from $cmds" -l help -d "Show help"
complete -c netctl-auto -n "not __fish_seen_subcommand_from $cmds" -l version -d "Show version"
complete -c netctl-auto -n "not __fish_seen_subcommand_from $cmds" -a list -f -d "List all available profiles for automatic selection"
complete -c netctl-auto -n "not __fish_seen_subcommand_from $cmds" -a start -f -d "Start automatic profile selection on interface"
complete -c netctl-auto -n "not __fish_seen_subcommand_from $cmds" -a stop -f -d "Stop automatic profile selection on interface"
complete -c netctl-auto -n "not __fish_seen_subcommand_from $cmds" -a switch-to -f -d "Switch to the given network profile"
complete -c netctl-auto -n "not __fish_seen_subcommand_from $cmds" -a enable -f -d "Enable network profile for automatic selection"
complete -c netctl-auto -n "not __fish_seen_subcommand_from $cmds" -a disable -f -d "Disable network profile for automatic selection"
complete -c netctl-auto -n "not __fish_seen_subcommand_from $cmds" -a enable-all -f -d "Enable all profiles for automatic selection"
complete -c netctl-auto -n "not __fish_seen_subcommand_from $cmds" -a disable-all -f -d "Disable all profiles for automatic selection"
complete -c netctl-auto -n "not __fish_seen_subcommand_from $cmds" -a is-active -f -d "Check whether specified profile is active"
complete -c netctl-auto -n "not __fish_seen_subcommand_from $cmds" -a is-enabled -f -d "Check whether specified profile is enabled"

complete -c netctl-auto -n "__fish_seen_subcommand_from switch-to" -f -a "(__fish_print_netctl-auto_profile other disabled)" -d Profile
complete -c netctl-auto -n "__fish_seen_subcommand_from enable" -f -a "(__fish_print_netctl-auto_profile disabled)" -d Profile
complete -c netctl-auto -n "__fish_seen_subcommand_from disable" -f -a "(__fish_print_netctl-auto_profile active other)" -d Profile
complete -c netctl-auto -n "__fish_seen_subcommand_from start stop" -f -a "(__fish_print_interfaces)"
complete -c netctl-auto -n "__fish_seen_subcommand_from is-enabled is-active" -x -a "(__fish_print_netctl-auto_profile active disabled other)"
