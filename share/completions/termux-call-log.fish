set command termux-call-log

complete -c $command -f

complete -c $command \
    -s h \
    -d 'Show [h]elp'

complete -c $command \
    -a '10\tdefault' \
    -s l \
    -d '[l]imit the amount of listed calls' \
    -x

complete -c $command \
    -a '0\tdefault' \
    -s o \
    -d 'Start listing calls with the specified one' \
    -x
