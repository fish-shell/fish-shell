set -l commands status get-volume inspect set-default set-volume set-mute set-profile clear-default

function __wpctl_get_nodes -a section -a type
    for node in (string match -r " ├─ $type:.*(?:\n │.*)+" "$(string match -r "$section*(?:\n [├│└].*)*" "$(wpctl status)")" | string match -rg '^ │[ \*]{6}([^\[]*)')
        printf '%s\t%s\n' (string match -r '\d+' "$node") (string match -rg '\d+\. (.*)' "$node")
    end
end

function __wpctl_command_shape
    set -l shape (string split --no-empty " " -- $argv)
    set command (commandline -poc)
    set -e command[1] # Remove command name

    # Remove flags as we won't count them with the shape
    set -l i 1
    for arg in $command
        if string match -- '-*' $arg
            set -e command[$i]
        else
            set i (math $i + 1)
        end
    end

    if test (count $command) != (count $shape)
        return 1
    end

    while set -q command[1]
        string match -q -- $shape[1] $command[1]; or return 1
        set -e shape[1] command[1]
    end
end

complete -c wpctl -f

complete -c wpctl -s h -l help -d "Show help options"
complete -c wpctl -n "__fish_seen_subcommand_from status" -s k -l nick -d "Display device and node nicknames instead of descriptions"
complete -c wpctl -n "__fish_seen_subcommand_from status" -s n -l name -d "Display device and node names instead of descriptions"
complete -c wpctl -n "__fish_seen_subcommand_from inspect" -s r -l referenced -d "Show objects that are referenced in properties"
complete -c wpctl -n "__fish_seen_subcommand_from inspect" -s a -l associated -d "Show associated objects"
complete -c wpctl -n "__fish_seen_subcommand_from set-volume" -s p -l pid -d "Selects all nodes associated to the given PID number"
complete -c wpctl -n "__fish_seen_subcommand_from set-volume" -s l -l limit -d "Limits the final volume percentage to below this value. (floating point, 1.0 is 100%)"
complete -c wpctl -n "__fish_seen_subcommand_from set-mute" -s p -l pid -d "Selects all nodes associated to the given PID number"

complete -c wpctl -n __wpctl_command_shape -a "$commands"
complete -c wpctl -n '__wpctl_command_shape "*"' -n "__fish_seen_subcommand_from get-volume inspect set-volume set-mute set-profile" -a "@DEFAULT_AUDIO_SOURCE@" -d "Default Microphone"
complete -c wpctl -n '__wpctl_command_shape "*"' -n "__fish_seen_subcommand_from get-volume inspect set-volume set-mute set-profile" -a "@DEFAULT_AUDIO_SINK@" -d "Default Speakers"
complete -c wpctl -n '__wpctl_command_shape "*"' -n "__fish_seen_subcommand_from inspect set-profile" -a "@DEFAULT_VIDEO_SOURCE@" -d "Default Camera"
complete -c wpctl -n '__wpctl_command_shape "*"' -n "__fish_seen_subcommand_from get-volume inspect set-volume set-mute set-profile" -a "(__wpctl_get_nodes Audio Sources) (__wpctl_get_nodes Audio Sinks)"
complete -c wpctl -n '__wpctl_command_shape "*"' -n "__fish_seen_subcommand_from inspect set-profile" -a "(__wpctl_get_nodes Audio Sources) (__wpctl_get_nodes Audio Sinks) (__wpctl_get_nodes Video Source)"
complete -c wpctl -n '__wpctl_command_shape set-mute "*"' -a 0\n1\ntoggle
