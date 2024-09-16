set command termux-vibrate

complete -c $command -f

complete -c $command \
    -s h \
    -d 'Show [h]elp'

complete -c $command \
    -a '1000\tdefault' \
    -s d \
    -d 'Specify the [d]uration of a vibration' \
    -x

complete -c $command \
    -s f \
    -d 'Vibrate in a silent mode too'
