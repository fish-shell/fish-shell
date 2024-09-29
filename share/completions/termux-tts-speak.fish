set -l command termux-tts-speak

complete -c $command -f

complete -c $command -s h -d 'Show help'

complete -c $command -s e -x \
    -a '(__fish_termux_api__complete_tts_engines)' \
    -d 'Specify the TTS engine for a speech'

complete -c $command -s l -x -d 'Specify the language of a speech'
complete -c $command -s n -x -d 'Specify the region of a speech'
complete -c $command -s v -x -d 'Specify the language variant of a speech'
complete -c $command -s p -x -d 'Specify the pitch of a speech'
complete -c $command -s r -x -d 'Specify the rate of a speech'

complete -c $command -s s -x \
    -a '(__fish_termux_api__complete_stream_ids | string replace --regex "(notification)" "\$1\tdefault")' \
    -d 'Specify the stream for a speech'
