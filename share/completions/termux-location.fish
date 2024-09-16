set command termux-location

complete -c $command -f

complete -c $command \
    -s h \
    -d 'Show [h]elp'

complete -c $command \
    -a 'gps\tdefault network passive' \
    -s p \
    -d 'Specify the provider of a location' \
    -x

complete -c $command \
    -a 'once\tdefault last updates' \
    -s r \
    -d 'Specify the request type for a location' \
    -x
