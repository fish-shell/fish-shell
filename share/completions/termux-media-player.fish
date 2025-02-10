set -l command termux-media-player

complete -c $command -f

complete -c $command -s h -d 'Show help'

set subcommands_with_descriptions 'info\t"Show a current playback information"' \
    'play\t"Resume a playback if paused | Play a specific media file"' \
    'pause\t"Pause a playback"' \
    'stop\t"Quit a playback"'

set -l subcommands (string replace --regex '\\\t.+' '' -- $subcommands_with_descriptions)

complete -c $command -a "$subcommands_with_descriptions" \
    -n "not __fish_seen_subcommand_from $subcommands"

complete -c $command -F -r \
    -n "__fish_seen_subcommand_from play"
