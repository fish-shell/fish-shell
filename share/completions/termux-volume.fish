set -l command termux-volume

complete -c $command -f

complete -c $command -a '(__fish_termux_api__complete_stream_ids)' \
    -d 'Specify the stream for a volume'
