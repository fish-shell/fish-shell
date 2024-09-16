set command termux-volume

complete -c $command -f

complete -c $command \
    -a '(__fish_termux_api__complete_stream_ids)' \
    -d 'Specify the [s]tream for a volume'
