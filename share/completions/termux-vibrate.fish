set -l command termux-vibrate

complete -c $command -f

complete -c $command -s h -d 'Show help'

complete -c $command -s d -x \
    -a '1000\tdefault' \
    -d 'Specify the duration of a vibration'

complete -c $command -s f -d 'Vibrate in a silent mode too'
