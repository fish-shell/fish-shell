set -l command termux-location

complete -c $command -f

complete -c $command -s h -d 'Show help'

complete -c $command -s p -x \
    -a 'gps\tdefault network passive' \
    -d 'Specify the provider of a location'

complete -c $command -s r -x \
    -a 'once\tdefault last updates' \
    -d 'Specify the request type for a location'
