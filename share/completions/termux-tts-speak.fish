set command termux-tts-speak

complete -c $command -f

complete -c $command \
    -s h \
    -d 'Show help'

complete -c $command \
    -a '(__fish_termux_api__complete_tts_engines)' \
    -s e \
    -d 'Specify the TTS engine for a speech' \
    -x

complete -c $command \
    -s l \
    -d 'Specify the language of a speech' \
    -x

complete -c $command \
    -s n \
    -d 'Specify the region of a speech' \
    -x

complete -c $command \
    -s v \
    -d 'Specify the language variant of a speech' \
    -x

complete -c $command \
    -s p \
    -d 'Specify the pitch of a speech' \
    -x

complete -c $command \
    -s r \
    -d 'Specify the rate of a speech' \
    -x

complete -c $command \
    -a '(__fish_termux_api__complete_stream_ids | string replace --regex "(notification)" "\$1\tdefault")' \
    -s s \
    -d 'Specify the stream for a speech' \
    -x
