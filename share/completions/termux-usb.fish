set command termux-usb

complete -c $command -f

complete -c $command \
    -s h \
    -d 'Show help'

complete -c $command \
    -s l \
    -d 'List all devices'

complete -c $command \
    -s r \
    -d 'Show the permission request dialog for a device'

complete -c $command \
    -s e \
    -d 'execute the specified command for a device' \
    -x
    