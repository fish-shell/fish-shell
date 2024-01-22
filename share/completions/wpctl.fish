set -l commands status get-volume inspect set-default set-volume set-mute set-profile clear-default

function __wpctl_get_nodes -a section -a type
    set -l havesection
    set -l havetype
    wpctl status | while read -l line
        if set -q havesection[1]
            test -z "$line"; and break
            if set -q havetype[1]
                string match -rq '^\s*\│\s*$' -- $line; and break
                printf '%s\t%s\n' (string match -r '\d+' $line) (string match -rg '\d+\. ([^\[]*)' $line)
            else
                string match -q "*$type*" -- $line
                and set havetype 1
            end
        else
            string match -q "$section*" -- $line
            and set havesection 1
        end
    end
end

function __wpctl_command_shape
    set -l shape $argv
    set -l command (commandline -pxc)
    set -e command[1] # Remove command name

    # Remove flags as we won't count them with the shape
    set -l command (string match -v -- '-*' $command)

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
complete -c wpctl -n "__fish_seen_subcommand_from set-volume" -s p -l pid -d "Selects all nodes associated to the given PID"
complete -c wpctl -n "__fish_seen_subcommand_from set-volume" -s l -l limit -d "Limit volume to below this value"
complete -c wpctl -n "__fish_seen_subcommand_from set-mute" -s p -l pid -d "Selects all nodes associated to the given PID"

complete -c wpctl -n __wpctl_command_shape -a "$commands"
complete -c wpctl -n '__wpctl_command_shape "*"' -n "__fish_seen_subcommand_from get-volume inspect set-volume set-mute set-profile" -a "@DEFAULT_AUDIO_SOURCE@" -d "Default Microphone"
complete -c wpctl -n '__wpctl_command_shape "*"' -n "__fish_seen_subcommand_from get-volume inspect set-volume set-mute set-profile" -a "@DEFAULT_AUDIO_SINK@" -d "Default Speakers"
complete -c wpctl -n '__wpctl_command_shape "*"' -n "__fish_seen_subcommand_from inspect set-profile" -a "@DEFAULT_VIDEO_SOURCE@" -d "Default Camera"
complete -c wpctl -n '__wpctl_command_shape "*"' -n "__fish_seen_subcommand_from get-volume inspect set-volume set-mute set-profile" -a "(__wpctl_get_nodes Audio Sources) (__wpctl_get_nodes Audio Sinks)"
complete -c wpctl -n '__wpctl_command_shape "*"' -n "__fish_seen_subcommand_from inspect set-profile" -a "(__wpctl_get_nodes Audio Sources) (__wpctl_get_nodes Audio Sinks) (__wpctl_get_nodes Video Source)"
complete -c wpctl -n '__wpctl_command_shape set-mute "*"' -a "0 1 toggle"
