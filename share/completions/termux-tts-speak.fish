set command termux-tts-speak

complete -c $command -f

complete -c $command \
    -s h \
    -d 'Show [h]elp'

complete -c $command \
    -a '(__fish_termux_api__complete_tts_engines)' \
    -s e \
    -d 'Specify the TTS [e]ngine for a speech' \
    -x

complete -c $command \
    -s l \
    -d 'Specify the [l]anguage of a speech' \
    -x

complete -c $command \
    -s n \
    -d 'Specify the regio[n] of a speech' \
    -x

complete -c $command \
    -s v \
    -d 'Specify the language [v]ariant of a speech' \
    -x

complete -c $command \
    -s p \
    -d 'Specify the [p]itch of a speech' \
    -x

complete -c $command \
    -s r \
    -d 'Specify the [r]ate of a speech' \
    -x

complete -c $command \
    -a 'NOTIFICATION\tdefault ALARM MUSIC RING SYSTEM VOICE_CALL' \
    -s s \
    -d 'Specify the [s]tream for a speech' \
    -x
