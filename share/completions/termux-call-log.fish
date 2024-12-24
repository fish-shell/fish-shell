set -l command termux-call-log

complete -c $command -f

complete -c $command -s h -d 'Show help'

complete -c $command -s l -x \
    -a '10\tdefault' \
    -d 'Limit the amount of listed calls'

complete -c $command -s o -x \
    -a '0\tdefault' \
    -d 'Start listing calls with the specified one'
