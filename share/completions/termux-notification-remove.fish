set command termux-notification-remove

complete -c $command -f

complete -c $command \
    -s h \
    -l help \
    -d 'Show [h]elp'

complete -c $command \
    -a '(__fish_termux_api__complete_group_ids)' \
    -x
