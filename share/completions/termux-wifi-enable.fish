set -l command termux-wifi-enable

complete -c $command -f

complete -c $command -s h -d 'Show help'

complete -c $command -a 'true false' \
    -d 'Toggle the WiFi availability of a device'
