set -l command termux-torch

complete -c $command -f

complete -c $command -s h -d 'Show help'

complete -c $command -a 'on off' -d 'Toggle the LED torch of a device'
