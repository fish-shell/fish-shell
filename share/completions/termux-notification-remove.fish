set -l command termux-notification-remove

complete -c $command -f

complete -c $command -s h -l help -d 'Show help'

complete -c $command -a '(__fish_termux_api__complete_group_ids)' -x
