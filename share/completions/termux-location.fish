set command termux-location

complete -c $command -f

complete -c $command \
    -s h \
    -d 'Show [h]elp'

complete -c $command \
    -a 'gps\tdefault network passive' \
    -s p \
    -d 'Specify a location provider' \
    -x

complete -c $command \
    -a 'once\tdefault last updates' \
    -s r \
    -d 'Specify a request type' \
    -x
